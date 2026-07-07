// Copyright Potionify. Apache-2.0.

#include "ConvexClient.h"

#include "ConvexClientModule.h"
#include "ConvexSubscription.h"
#include "ConvexSubscriptionHandle.h"
#include "ConvexUtils.h"
#include "UEHttpTransport.h"
#include "UEWebSocketTransport.h"

#include <convex/client.h>
#include <convex/file_storage.h>
#include <convex/http_client.h>
#include <convex/protocol.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

using ConvexUtils::FStringToUtf8;

// ---------------------------------------------------------------------------
// PIMPL: owns the native clients and transports. Kept out of the header so the
// UObject stays free of std::unique_ptr-of-incomplete-type concerns.
// ---------------------------------------------------------------------------
struct FConvexClientImpl
{
	std::shared_ptr<FUEWebSocketTransport> WebSocketTransport;
	std::shared_ptr<FUEHttpTransport> HttpTransport;
	std::unique_ptr<convex::client> Client;
	std::unique_ptr<convex::http_client> HttpClient;
};

namespace
{
	EConvexConnectionState ToConnectionState(convex::connection_state State)
	{
		switch (State)
		{
		case convex::connection_state::connecting: return EConvexConnectionState::Connecting;
		case convex::connection_state::connected:  return EConvexConnectionState::Connected;
		case convex::connection_state::disconnected:
		default:                                   return EConvexConnectionState::Disconnected;
		}
	}
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void UConvexClient::Initialize(const FString& DeploymentUrl)
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	Impl = MakePimpl<FConvexClientImpl>();
	Impl->WebSocketTransport = std::make_shared<FUEWebSocketTransport>();
	Impl->HttpTransport = std::make_shared<FUEHttpTransport>();

	const std::string Url = FStringToUtf8(DeploymentUrl);

	try
	{
		convex::client_options Options;
		Options.deployment_url = Url;
		Options.websocket = Impl->WebSocketTransport;
		Options.delivery_mode = convex::client_options::delivery::pumped;
		Options.client_id = "ue-0.1.0";
		Impl->Client = std::make_unique<convex::client>(std::move(Options));

		Impl->HttpClient = std::make_unique<convex::http_client>(Url, Impl->HttpTransport);

		TWeakObjectPtr<UConvexClient> WeakThis(this);
		Impl->Client->on_state_change([WeakThis](convex::connection_state State)
		{
			// Delivered on the game thread (pumped).
			if (UConvexClient* Self = WeakThis.Get())
			{
				const EConvexConnectionState Mapped = ToConnectionState(State);
				Self->OnConnectionStateChanged.Broadcast(Mapped);
				Self->OnConnectionStateChangedNative.Broadcast(Mapped);
			}
		});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UConvexClient::Initialize failed for '%s': %hs"),
			*DeploymentUrl, Error.what());
		Impl->Client.reset();
		Impl->HttpClient.reset();
	}

	// Pump queued callbacks on the game thread every tick.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UConvexClient::Tick));
}

bool UConvexClient::Tick(float /*DeltaTime*/)
{
	if (Impl && Impl->Client)
	{
		try
		{
			Impl->Client->process_events();
		}
		catch (const std::exception& Error)
		{
			UE_LOG(LogConvex, Error, TEXT("UConvexClient::Tick process_events failed: %hs"), Error.what());
		}
	}
	return true; // keep ticking
}

void UConvexClient::Shutdown()
{
	if (bShutDown)
	{
		return;
	}
	bShutDown = true;

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (Impl)
	{
		try
		{
			// Destroy the realtime client first (joins worker threads and
			// completes pending callbacks), then the HTTP client, then the
			// transports.
			Impl->Client.reset();
			Impl->HttpClient.reset();
			Impl->WebSocketTransport.reset();
			Impl->HttpTransport.reset();
		}
		catch (const std::exception& Error)
		{
			UE_LOG(LogConvex, Error, TEXT("UConvexClient::Shutdown failed: %hs"), Error.what());
		}
	}

	ActiveSubscriptions.Reset();
}

void UConvexClient::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// ===========================================================================
// Subscriptions
// ===========================================================================

UConvexSubscription* UConvexClient::SubscribeNative(const FString& Path,
	const TMap<FString, FConvexValue>& Args, FConvexResultNative OnUpdate)
{
	if (!Impl || !Impl->Client)
	{
		UE_LOG(LogConvex, Error, TEXT("Subscribe called before Initialize (path '%s')"), *Path);
		return nullptr;
	}

	UConvexSubscription* Subscription = NewObject<UConvexSubscription>(this);
	if (OnUpdate)
	{
		Subscription->OnUpdateNative.AddLambda(
			[Fn = MoveTemp(OnUpdate)](const FConvexResult& Result) { Fn(Result); });
	}

	TWeakObjectPtr<UConvexSubscription> WeakSub(Subscription);
	try
	{
		convex::client::subscription Handle = Impl->Client->subscribe(
			FStringToUtf8(Path), ConvexMakeArgs(Args),
			[WeakSub](const convex::function_result& Result)
			{
				// Pumped: fires on the game thread.
				if (UConvexSubscription* Sub = WeakSub.Get())
				{
					Sub->BroadcastUpdate(FConvexResult::FromNative(Result));
				}
			});
		Subscription->SetHandle(FConvexSubscriptionHandle(std::move(Handle)), this);
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("Subscribe failed for '%s': %hs"), *Path, Error.what());
		return nullptr;
	}

	ActiveSubscriptions.Add(Subscription);
	return Subscription;
}

UConvexSubscription* UConvexClient::Subscribe(const FString& Path,
	const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnUpdate)
{
	return SubscribeNative(Path, Args,
		[OnUpdate](const FConvexResult& Result) { OnUpdate.ExecuteIfBound(Result); });
}

void UConvexClient::ForgetSubscription(UConvexSubscription* Subscription)
{
	ActiveSubscriptions.Remove(Subscription);
}

// ===========================================================================
// One-shot operations (realtime connection)
// ===========================================================================

void UConvexClient::QueryNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->Client)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->Client->query(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("Query failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: query dispatch failed"))); }
	}
}

void UConvexClient::MutationNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->Client)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->Client->mutation(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("Mutation failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: mutation dispatch failed"))); }
	}
}

void UConvexClient::ActionNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->Client)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->Client->action(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("Action failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: action dispatch failed"))); }
	}
}

void UConvexClient::Query(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	QueryNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

void UConvexClient::Mutation(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	MutationNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

void UConvexClient::Action(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	ActionNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

// ===========================================================================
// One-shot operations (plain HTTP)
// ===========================================================================

void UConvexClient::HttpQueryNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->HttpClient)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->HttpClient->query(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("HttpQuery failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: http query dispatch failed"))); }
	}
}

void UConvexClient::HttpMutationNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->HttpClient)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->HttpClient->mutation(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("HttpMutation failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: http mutation dispatch failed"))); }
	}
}

void UConvexClient::HttpActionNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultNative OnResult)
{
	if (!Impl || !Impl->HttpClient)
	{
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		Impl->HttpClient->action(FStringToUtf8(Path), ConvexMakeArgs(Args),
			[OnResult](convex::function_result Result)
			{
				if (OnResult) { OnResult(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("HttpAction failed for '%s': %hs"), *Path, Error.what());
		if (OnResult) { OnResult(FConvexResult::MakeError(TEXT("convex: http action dispatch failed"))); }
	}
}

void UConvexClient::HttpQuery(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	HttpQueryNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

void UConvexClient::HttpMutation(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	HttpMutationNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

void UConvexClient::HttpAction(const FString& Path, const TMap<FString, FConvexValue>& Args,
	FConvexResultDelegate OnResult)
{
	HttpActionNative(Path, Args, [OnResult](const FConvexResult& Result) { OnResult.ExecuteIfBound(Result); });
}

// ===========================================================================
// File storage
// ===========================================================================

void UConvexClient::UploadFileNative(const FString& UploadUrl, const TArray<uint8>& Data,
	const FString& ContentType, FConvexResultNative OnDone)
{
	if (!Impl || !Impl->HttpTransport)
	{
		if (OnDone) { OnDone(FConvexResult::MakeError(TEXT("convex: client not initialized"))); }
		return;
	}
	try
	{
		convex::bytes Bytes(Data.GetData(), Data.GetData() + Data.Num());
		convex::store_file(*Impl->HttpTransport, FStringToUtf8(UploadUrl), FStringToUtf8(ContentType),
			std::move(Bytes),
			[OnDone](convex::function_result Result)
			{
				if (OnDone) { OnDone(FConvexResult::FromNative(Result)); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UploadFile failed: %hs"), Error.what());
		if (OnDone) { OnDone(FConvexResult::MakeError(TEXT("convex: upload dispatch failed"))); }
	}
}

void UConvexClient::UploadFile(const FString& UploadUrl, const TArray<uint8>& Data,
	const FString& ContentType, FConvexResultDelegate OnDone)
{
	UploadFileNative(UploadUrl, Data, ContentType,
		[OnDone](const FConvexResult& Result) { OnDone.ExecuteIfBound(Result); });
}

void UConvexClient::DownloadFileNative(const FString& Url, FConvexDownloadNative OnDone)
{
	if (!Impl || !Impl->HttpTransport)
	{
		if (OnDone) { OnDone(false, TArray<uint8>()); }
		return;
	}
	try
	{
		convex::fetch_file(*Impl->HttpTransport, FStringToUtf8(Url),
			[OnDone](convex::function_result Result)
			{
				TArray<uint8> Out;
				bool bSuccess = false;
				try
				{
					if (Result.ok())
					{
						const convex::value& Value = Result.get_value();
						if (Value.is_bytes())
						{
							const convex::bytes& Buffer = Value.as_bytes();
							Out.Append(Buffer.data(), static_cast<int32>(Buffer.size()));
						}
						bSuccess = true;
					}
				}
				catch (const std::exception& Error)
				{
					UE_LOG(LogConvex, Error, TEXT("DownloadFile decode failed: %hs"), Error.what());
					bSuccess = false;
				}
				if (OnDone) { OnDone(bSuccess, Out); }
			});
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("DownloadFile failed: %hs"), Error.what());
		if (OnDone) { OnDone(false, TArray<uint8>()); }
	}
}

void UConvexClient::DownloadFile(const FString& Url, FConvexDownloadDelegate OnDone)
{
	DownloadFileNative(Url,
		[OnDone](bool bSuccess, const TArray<uint8>& Data) { OnDone.ExecuteIfBound(bSuccess, Data); });
}

// ===========================================================================
// Auth
// ===========================================================================

void UConvexClient::SetUserAuth(const FString& Jwt)
{
	try
	{
		const convex::auth_token Token = convex::auth_token::user(FStringToUtf8(Jwt));
		if (Impl && Impl->Client) { Impl->Client->set_auth(Token); }
		if (Impl && Impl->HttpClient) { Impl->HttpClient->set_auth(Token); }
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("SetUserAuth failed: %hs"), Error.what());
	}
}

void UConvexClient::SetAdminAuth(const FString& Key)
{
	try
	{
		const convex::auth_token Token = convex::auth_token::admin(FStringToUtf8(Key));
		if (Impl && Impl->Client) { Impl->Client->set_auth(Token); }
		if (Impl && Impl->HttpClient) { Impl->HttpClient->set_auth(Token); }
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("SetAdminAuth failed: %hs"), Error.what());
	}
}

void UConvexClient::ClearAuth()
{
	try
	{
		const convex::auth_token Token = convex::auth_token::none();
		if (Impl && Impl->Client) { Impl->Client->set_auth(Token); }
		if (Impl && Impl->HttpClient) { Impl->HttpClient->set_auth(Token); }
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("ClearAuth failed: %hs"), Error.what());
	}
}

// ===========================================================================
// Connection
// ===========================================================================

EConvexConnectionState UConvexClient::GetConnectionState() const
{
	if (Impl && Impl->Client)
	{
		try
		{
			return ToConnectionState(Impl->Client->state());
		}
		catch (const std::exception& Error)
		{
			UE_LOG(LogConvex, Error, TEXT("GetConnectionState failed: %hs"), Error.what());
		}
	}
	return EConvexConnectionState::Disconnected;
}

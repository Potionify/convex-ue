// Copyright Potionify. Apache-2.0.

#include "ConvexAdminSession.h"

#include "ConvexClient.h"
#include "ConvexEditorSettings.h"
#include "ConvexSubscription.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogConvexEditor, Log, All);

namespace
{
	FString GetStringField(const FConvexValue& Object, const TCHAR* Field)
	{
		TMap<FString, FConvexValue> Fields;
		if (!Object.TryGetObject(Fields))
		{
			return FString();
		}
		if (const FConvexValue* Value = Fields.Find(Field))
		{
			FString Out;
			if (Value->TryGetString(Out))
			{
				return Out;
			}
		}
		return FString();
	}
}

FConvexAdminSession::~FConvexAdminSession()
{
	Disconnect();
}

void FConvexAdminSession::Disconnect()
{
	++Generation;
	if (StateSubscription.IsValid())
	{
		StateSubscription->Unsubscribe();
		StateSubscription.Reset();
	}
	if (VersionSubscription.IsValid())
	{
		VersionSubscription->Unsubscribe();
		VersionSubscription.Reset();
	}
	if (ApiSpecSubscription.IsValid())
	{
		ApiSpecSubscription->Unsubscribe();
		ApiSpecSubscription.Reset();
	}
	if (Client.IsValid())
	{
		if (ConnectionStateHandle.IsValid())
		{
			Client->OnConnectionStateChangedNative.Remove(ConnectionStateHandle);
			ConnectionStateHandle.Reset();
		}
		Client->Shutdown();
		Client.Reset();
	}
	KeyInfo = FConvexAdminKeyInfo{};
	DeploymentState.Reset();
	ServerVersion.Reset();
	Functions.Reset();
	NotifyChanged();
}

void FConvexAdminSession::RefreshAndConnect()
{
	const UConvexEditorSettings* Settings = GetDefault<UConvexEditorSettings>();
	ConnectWithConfig(ConvexDeploymentResolver::Resolve(
		Settings->EnvFile.FilePath, Settings->EnvSearchParentDepth));
}

void FConvexAdminSession::ConnectWithConfig(const FConvexDeploymentConfig& InConfig)
{
	Disconnect();
	const uint64 Gen = ++Generation;

	Config = InConfig;
	if (!Config.IsValid())
	{
		UE_LOG(LogConvexEditor, Warning, TEXT("Convex deployment resolution failed: %s"),
			*Config.Error);
		NotifyChanged();
		return;
	}

	UE_LOG(LogConvexEditor, Log, TEXT("Connecting to Convex deployment %s (%s) from %s"),
		*Config.DeploymentName, ConvexDeploymentTypeToString(Config.Type), *Config.Source);

	Client = TStrongObjectPtr<UConvexClient>(NewObject<UConvexClient>(GetTransientPackage()));
	Client->Initialize(Config.DeploymentUrl);
	if (!Client->IsInitialized())
	{
		Config.Error = FString::Printf(TEXT("Client failed to initialize for URL '%s'."),
			*Config.DeploymentUrl);
		Client.Reset();
		NotifyChanged();
		return;
	}
	Client->SetAdminAuth(Config.AdminKey);

	ConnectionStateHandle = Client->OnConnectionStateChangedNative.AddLambda(
		[WeakThis = AsWeak(), Gen](EConvexConnectionState)
		{
			if (const TSharedPtr<FConvexAdminSession> This = WeakThis.Pin();
				This.IsValid() && This->Generation == Gen)
			{
				This->NotifyChanged();
			}
		});

	CheckAdminKey();
	SubscribeSystemQueries();
	NotifyChanged();
}

EConvexConnectionState FConvexAdminSession::GetConnectionState() const
{
	return Client.IsValid() ? Client->GetConnectionState() : EConvexConnectionState::Disconnected;
}

FConvexConnectionInfo FConvexAdminSession::GetConnectionInfo() const
{
	return Client.IsValid() ? Client->GetConnectionInfo() : FConvexConnectionInfo{};
}

void FConvexAdminSession::CheckAdminKey()
{
	const uint64 Gen = Generation;
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();
	Request->SetURL(Config.DeploymentUrl / TEXT("api/check_admin_key"));
	Request->SetVerb(TEXT("GET"));
	// Admin scheme is literally "Convex", not "Bearer".
	Request->SetHeader(TEXT("Authorization"), TEXT("Convex ") + Config.AdminKey);
	Request->SetHeader(TEXT("Convex-Client"), TEXT("unreal-0.1.0"));
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis = AsWeak(), Gen](
			FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			const TSharedPtr<FConvexAdminSession> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen)
			{
				return;
			}
			FConvexAdminKeyInfo& Info = This->KeyInfo;
			Info = FConvexAdminKeyInfo{};
			Info.bChecked = true;

			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				Info.Error = TEXT("Could not reach the deployment.");
				This->NotifyChanged();
				return;
			}

			TSharedPtr<FJsonObject> Json;
			const TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Response->GetContentAsString());
			FJsonSerializer::Deserialize(Reader, Json);

			if (Response->GetResponseCode() == 200 && Json.IsValid())
			{
				Info.bValid = Json->HasField(TEXT("success")) &&
					Json->GetBoolField(TEXT("success"));
				// Older backends return only {success}; both extras optional.
				Json->TryGetBoolField(TEXT("isReadOnly"), Info.bIsReadOnly);
				const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
				if (Json->TryGetArrayField(TEXT("allowedOps"), Ops))
				{
					for (const TSharedPtr<FJsonValue>& Op : *Ops)
					{
						FString Name;
						if (Op.IsValid() && Op->TryGetString(Name))
						{
							Info.AllowedOps.Add(MoveTemp(Name));
						}
					}
				}
			}
			else
			{
				// Error body is {code, message}.
				FString Message = TEXT("HTTP ") + FString::FromInt(Response->GetResponseCode());
				if (Json.IsValid())
				{
					FString Code, Detail;
					Json->TryGetStringField(TEXT("code"), Code);
					Json->TryGetStringField(TEXT("message"), Detail);
					if (!Code.IsEmpty() || !Detail.IsEmpty())
					{
						Message = Code + TEXT(": ") + Detail;
					}
				}
				Info.Error = Message;
			}
			This->NotifyChanged();
		});
	Request->ProcessRequest();
}

void FConvexAdminSession::SubscribeSystemQueries()
{
	const uint64 Gen = Generation;
	const TMap<FString, FConvexValue> NoArgs;

	// Deployment runtime state: {state: running|paused|disabled|suspended}.
	StateSubscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
		TEXT("_system/frontend/deploymentState:deploymentState"), NoArgs,
		[WeakThis = AsWeak(), Gen](const FConvexResult& Result)
		{
			const TSharedPtr<FConvexAdminSession> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen)
			{
				return;
			}
			This->DeploymentState =
				Result.bSuccess ? GetStringField(Result.Value, TEXT("state")) : FString();
			This->NotifyChanged();
		}));

	// Server version string.
	VersionSubscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
		TEXT("_system/frontend/getVersion"), NoArgs,
		[WeakThis = AsWeak(), Gen](const FConvexResult& Result)
		{
			const TSharedPtr<FConvexAdminSession> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen)
			{
				return;
			}
			FString Version;
			if (Result.bSuccess)
			{
				Result.Value.TryGetString(Version);
			}
			This->ServerVersion = Version;
			This->NotifyChanged();
		}));

	// Function specs (live: updates on every deploy).
	ApiSpecSubscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
		TEXT("_system/cli/modules:apiSpec"), NoArgs,
		[WeakThis = AsWeak(), Gen](const FConvexResult& Result)
		{
			const TSharedPtr<FConvexAdminSession> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen)
			{
				return;
			}
			This->Functions.Reset();
			TArray<FConvexValue> Specs;
			if (Result.bSuccess && Result.Value.TryGetArray(Specs))
			{
				for (const FConvexValue& SpecValue : Specs)
				{
					TMap<FString, FConvexValue> Fields;
					if (!SpecValue.TryGetObject(Fields))
					{
						continue;
					}
					FConvexFunctionSpec Spec;
					Spec.FunctionType = GetStringField(SpecValue, TEXT("functionType"));
					if (Spec.FunctionType == TEXT("HttpAction"))
					{
						const FString Method = GetStringField(SpecValue, TEXT("method"));
						const FString Path = GetStringField(SpecValue, TEXT("path"));
						Spec.HttpRoute = Method + TEXT(" ") + Path;
						Spec.Identifier = Spec.HttpRoute;
						Spec.Visibility = TEXT("public");
					}
					else
					{
						// apiSpec identifiers carry the bundled module
						// extension ("counters.js:get"); canonicalize to the
						// callable path the sync protocol uses.
						Spec.Identifier = GetStringField(SpecValue, TEXT("identifier"));
						Spec.Identifier.ReplaceInline(TEXT(".js:"), TEXT(":"));
						if (const FConvexValue* Visibility = Fields.Find(TEXT("visibility")))
						{
							Spec.Visibility = GetStringField(*Visibility, TEXT("kind"));
						}
						if (const FConvexValue* Args = Fields.Find(TEXT("args")))
						{
							Spec.ArgsValidator = *Args;
						}
						if (const FConvexValue* Returns = Fields.Find(TEXT("returns")))
						{
							Spec.ReturnsValidator = *Returns;
						}
					}
					if (!Spec.Identifier.IsEmpty())
					{
						This->Functions.Add(MoveTemp(Spec));
					}
				}
				This->Functions.Sort([](const FConvexFunctionSpec& A, const FConvexFunctionSpec& B)
					{ return A.Identifier < B.Identifier; });
			}
			else if (!Result.bSuccess)
			{
				UE_LOG(LogConvexEditor, Warning, TEXT("apiSpec query failed: %s"),
					*Result.ErrorMessage);
			}
			This->NotifyChanged();
		}));
}

void FConvexAdminSession::RunFunction(const FConvexFunctionSpec& Spec,
	const FString& ArgsWireJson, FConvexResultNative OnResult)
{
	if (!Client.IsValid())
	{
		OnResult(FConvexResult::MakeError(TEXT("Not connected.")));
		return;
	}
	if (Spec.FunctionType == TEXT("HttpAction"))
	{
		OnResult(FConvexResult::MakeError(
			TEXT("HTTP actions are routes, not runnable functions; call them with any HTTP "
				 "client against the deployment's .site URL.")));
		return;
	}

	bool bParsed = false;
	const FString Trimmed = ArgsWireJson.TrimStartAndEnd();
	const FConvexValue Parsed =
		FConvexValue::FromWire(Trimmed.IsEmpty() ? TEXT("{}") : Trimmed, bParsed);
	TMap<FString, FConvexValue> Args;
	if (!bParsed || !Parsed.TryGetObject(Args))
	{
		OnResult(FConvexResult::MakeError(
			TEXT("Args must be a JSON object (Convex wire format, e.g. "
				 "{\"name\": \"value\", \"count\": 3}).")));
		return;
	}

	if (Spec.FunctionType == TEXT("Query"))
	{
		Client->QueryNative(Spec.Identifier, Args, MoveTemp(OnResult));
	}
	else if (Spec.FunctionType == TEXT("Mutation"))
	{
		Client->MutationNative(Spec.Identifier, Args, MoveTemp(OnResult));
	}
	else if (Spec.FunctionType == TEXT("Action"))
	{
		Client->ActionNative(Spec.Identifier, Args, MoveTemp(OnResult));
	}
	else
	{
		OnResult(FConvexResult::MakeError(
			FString::Printf(TEXT("Unknown function type '%s'."), *Spec.FunctionType)));
	}
}

void FConvexAdminSession::NotifyChanged()
{
	OnChanged.Broadcast();
}

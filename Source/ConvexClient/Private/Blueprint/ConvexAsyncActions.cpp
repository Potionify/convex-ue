// Copyright Potionify. Apache-2.0.

#include "Blueprint/ConvexAsyncActions.h"

#include "ConvexClient.h"
#include "ConvexClientModule.h"
#include "ConvexSubscription.h"
#include "ConvexSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

// ===========================================================================
// UConvexAsyncActionBase
// ===========================================================================

UConvexClient* UConvexAsyncActionBase::ResolveClient(UConvexClient* Provided) const
{
	if (Provided)
	{
		return Provided;
	}
	if (UConvexSubsystem* Subsystem = UConvexSubsystem::Get(WorldContextObject.Get()))
	{
		return Subsystem->GetDefaultClient();
	}
	return nullptr;
}

void UConvexAsyncActionBase::RegisterAndKeep()
{
	if (const UObject* Context = WorldContextObject.Get())
	{
		if (const UWorld* World = Context->GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				RegisterWithGameInstance(GameInstance);
				bRegistered = true;
			}
		}
	}
	// Without a game instance to register with, root the node so it survives GC
	// until the operation completes.
	if (!bRegistered)
	{
		AddToRoot();
		bRooted = true;
	}
}

void UConvexAsyncActionBase::FinishAndDestroy()
{
	if (bRooted)
	{
		RemoveFromRoot();
		bRooted = false;
	}
	SetReadyToDestroy();
}

// ===========================================================================
// UConvexCallAction (Query / Mutation / Action)
// ===========================================================================

void UConvexCallAction::Activate()
{
	UConvexClient* Target = ResolveClient(TargetClient.Get());
	if (!Target)
	{
		UE_LOG(LogConvex, Warning, TEXT("Convex async call '%s': no client available."), *Path);
		OnFailure.Broadcast(FConvexResult::MakeError(TEXT("No Convex client available.")));
		FinishAndDestroy();
		return;
	}

	RegisterAndKeep();

	FConvexResultDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UConvexCallAction, HandleResult));

	switch (Op)
	{
	case EOp::Query:
		Target->Query(Path, Args.ToMap(), Delegate);
		break;
	case EOp::Mutation:
		Target->Mutation(Path, Args.ToMap(), Delegate);
		break;
	case EOp::Action:
		Target->Action(Path, Args.ToMap(), Delegate);
		break;
	}
}

void UConvexCallAction::HandleResult(FConvexResult Result)
{
	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result.Value);
	}
	else
	{
		OnFailure.Broadcast(Result);
	}
	FinishAndDestroy();
}

UConvexQueryAction* UConvexQueryAction::ConvexQuery(const UObject* WorldContextObject, UConvexClient* Client,
	const FString& Path, const FConvexArgs& Args)
{
	UConvexQueryAction* Node = NewObject<UConvexQueryAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->TargetClient = Client;
	Node->Path = Path;
	Node->Args = Args;
	Node->Op = EOp::Query;
	return Node;
}

UConvexMutationAction* UConvexMutationAction::ConvexMutation(const UObject* WorldContextObject, UConvexClient* Client,
	const FString& Path, const FConvexArgs& Args)
{
	UConvexMutationAction* Node = NewObject<UConvexMutationAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->TargetClient = Client;
	Node->Path = Path;
	Node->Args = Args;
	Node->Op = EOp::Mutation;
	return Node;
}

UConvexRunActionAction* UConvexRunActionAction::ConvexRunAction(const UObject* WorldContextObject, UConvexClient* Client,
	const FString& Path, const FConvexArgs& Args)
{
	UConvexRunActionAction* Node = NewObject<UConvexRunActionAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->TargetClient = Client;
	Node->Path = Path;
	Node->Args = Args;
	Node->Op = EOp::Action;
	return Node;
}

// ===========================================================================
// UConvexConnectAction
// ===========================================================================

UConvexConnectAction* UConvexConnectAction::ConnectToConvex(const UObject* WorldContextObject, const FString& Url)
{
	UConvexConnectAction* Node = NewObject<UConvexConnectAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->Url = Url;
	return Node;
}

void UConvexConnectAction::Activate()
{
	UConvexSubsystem* Subsystem = UConvexSubsystem::Get(WorldContextObject.Get());
	if (!Subsystem)
	{
		UE_LOG(LogConvex, Warning, TEXT("Connect To Convex: no Convex subsystem (invalid world context)."));
		OnFailed.Broadcast(nullptr);
		FinishAndDestroy();
		return;
	}

	UConvexClient* Target = Url.IsEmpty() ? Subsystem->GetDefaultClient() : Subsystem->CreateClient(Url);
	if (!Target)
	{
		UE_LOG(LogConvex, Warning, TEXT("Connect To Convex: could not obtain a client."));
		OnFailed.Broadcast(nullptr);
		FinishAndDestroy();
		return;
	}

	Client = Target;
	RegisterAndKeep();

	if (Target->GetConnectionState() == EConvexConnectionState::Connected)
	{
		// Already connected: fire on the next tick so the OnConnected pin runs
		// after the node's exec output is wired up.
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				OnConnected.Broadcast(Client.Get());
				FinishAndDestroy();
				return false;
			}));
		return;
	}

	Target->OnConnectionStateChanged.AddDynamic(this, &UConvexConnectAction::HandleStateChanged);
}

void UConvexConnectAction::HandleStateChanged(EConvexConnectionState State)
{
	if (State != EConvexConnectionState::Connected)
	{
		return;
	}
	if (UConvexClient* Target = Client.Get())
	{
		Target->OnConnectionStateChanged.RemoveDynamic(this, &UConvexConnectAction::HandleStateChanged);
	}
	OnConnected.Broadcast(Client.Get());
	FinishAndDestroy();
}

void UConvexConnectAction::FinishAndDestroy()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Super::FinishAndDestroy();
}

// ===========================================================================
// UConvexSubscribeAction
// ===========================================================================

UConvexSubscribeAction* UConvexSubscribeAction::ConvexSubscribe(const UObject* WorldContextObject, UConvexClient* Client,
	const FString& Path, const FConvexArgs& Args)
{
	UConvexSubscribeAction* Node = NewObject<UConvexSubscribeAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->TargetClient = Client;
	Node->Path = Path;
	Node->Args = Args;
	return Node;
}

void UConvexSubscribeAction::Activate()
{
	UConvexClient* Target = ResolveClient(TargetClient.Get());
	if (!Target)
	{
		UE_LOG(LogConvex, Warning, TEXT("Convex Subscribe '%s': no client available."), *Path);
		OnFailed.Broadcast(FConvexResult::MakeError(TEXT("No Convex client available.")));
		FinishAndDestroy();
		return;
	}

	RegisterAndKeep();

	FConvexResultDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UConvexSubscribeAction, HandleUpdate));

	Subscription = Target->Subscribe(Path, Args.ToMap(), Delegate);
	if (!Subscription)
	{
		UE_LOG(LogConvex, Warning, TEXT("Convex Subscribe '%s': subscription failed."), *Path);
		OnFailed.Broadcast(FConvexResult::MakeError(TEXT("Convex subscription failed.")));
		FinishAndDestroy();
		return;
	}

	OnSubscribed.Broadcast(Subscription);
}

void UConvexSubscribeAction::HandleUpdate(FConvexResult Result)
{
	OnUpdate.Broadcast(Result);

	// If the subscription has been torn down (e.g. client shutdown), finish.
	if (!Subscription || !Subscription->IsActive())
	{
		FinishAndDestroy();
	}
}

void UConvexSubscribeAction::Unsubscribe()
{
	if (Subscription)
	{
		Subscription->Unsubscribe();
		Subscription = nullptr;
	}
	FinishAndDestroy();
}

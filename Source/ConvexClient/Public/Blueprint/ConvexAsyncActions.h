// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexArgs.h"
#include "ConvexDelegates.h"
#include "ConvexResult.h"
#include "ConvexValue.h"
#include "Containers/Ticker.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "ConvexAsyncActions.generated.h"

class UConvexClient;
class UConvexSubscription;

// Exec-pin delegate types for the async nodes. Each maps to one output pin.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexClientPin, UConvexClient*, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexValuePin, FConvexValue, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexResultPin, FConvexResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexSubscriptionPin, UConvexSubscription*, Subscription);

/**
 * Shared plumbing for Convex async-action nodes: caches the world context,
 * resolves a client, and keeps the node alive (registered with the game
 * instance, or rooted as a fallback) until it finishes so pending callbacks
 * survive garbage collection and world teardown.
 */
UCLASS(Abstract)
class CONVEXCLIENT_API UConvexAsyncActionBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	/// Return Provided if non-null, otherwise the subsystem's default client.
	UConvexClient* ResolveClient(UConvexClient* Provided) const;

	/// Keep this node alive while the operation is in flight.
	void RegisterAndKeep();

	/// Release the keep-alive and mark the node ready for destruction.
	virtual void FinishAndDestroy();

	TWeakObjectPtr<const UObject> WorldContextObject;

private:
	bool bRegistered = false;
	bool bRooted = false;
};

/**
 * Base for the one-shot query/mutation/action nodes. They differ only in which
 * client entry point they call; the success/failure pins are shared.
 */
UCLASS(Abstract)
class CONVEXCLIENT_API UConvexCallAction : public UConvexAsyncActionBase
{
	GENERATED_BODY()

public:
	/// Fires with the result value when the call succeeds.
	UPROPERTY(BlueprintAssignable)
	FConvexValuePin OnSuccess;

	/// Fires with the full result when the call fails (or no client is available).
	UPROPERTY(BlueprintAssignable)
	FConvexResultPin OnFailure;

	virtual void Activate() override;

protected:
	enum class EOp : uint8
	{
		Query,
		Mutation,
		Action
	};

	UPROPERTY()
	TWeakObjectPtr<UConvexClient> TargetClient;

	FString Path;
	FConvexArgs Args;
	EOp Op = EOp::Query;

	UFUNCTION()
	void HandleResult(FConvexResult Result);
};

/** "Convex Query": one-shot query over the realtime connection. */
UCLASS()
class CONVEXCLIENT_API UConvexQueryAction : public UConvexCallAction
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Convex|Async",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Convex Query", AutoCreateRefTerm = "Args"))
	static UConvexQueryAction* ConvexQuery(const UObject* WorldContextObject, UConvexClient* Client,
		const FString& Path, const FConvexArgs& Args);
};

/** "Convex Mutation": one-shot mutation over the realtime connection. */
UCLASS()
class CONVEXCLIENT_API UConvexMutationAction : public UConvexCallAction
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Convex|Async",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Convex Mutation", AutoCreateRefTerm = "Args"))
	static UConvexMutationAction* ConvexMutation(const UObject* WorldContextObject, UConvexClient* Client,
		const FString& Path, const FConvexArgs& Args);
};

/** "Convex Run Action": one-shot action over the realtime connection. */
UCLASS()
class CONVEXCLIENT_API UConvexRunActionAction : public UConvexCallAction
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Convex|Async",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Convex Run Action", AutoCreateRefTerm = "Args"))
	static UConvexRunActionAction* ConvexRunAction(const UObject* WorldContextObject, UConvexClient* Client,
		const FString& Path, const FConvexArgs& Args);
};

/**
 * "Connect To Convex": ensures a client exists and is connected. With an empty
 * Url it uses the settings-configured default client; otherwise it creates a
 * client for Url.
 */
UCLASS()
class CONVEXCLIENT_API UConvexConnectAction : public UConvexAsyncActionBase
{
	GENERATED_BODY()

public:
	/// Fires (once) when the client reaches the connected state.
	UPROPERTY(BlueprintAssignable)
	FConvexClientPin OnConnected;

	/// Fires when no client could be obtained, the client failed to
	/// initialize (e.g. bad URL), or the timeout elapsed before connecting.
	/// Client is null when none could be obtained.
	UPROPERTY(BlueprintAssignable)
	FConvexClientPin OnFailed;

	UFUNCTION(BlueprintCallable, Category = "Convex|Async",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Connect To Convex", AdvancedDisplay = "TimeoutSeconds"))
	static UConvexConnectAction* ConnectToConvex(const UObject* WorldContextObject, const FString& Url,
		float TimeoutSeconds = 30.0f);

	virtual void Activate() override;

protected:
	virtual void FinishAndDestroy() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UConvexClient> Client;

	FString Url;
	float TimeoutSeconds = 30.0f;
	FTSTicker::FDelegateHandle TickerHandle;
	FTSTicker::FDelegateHandle TimeoutHandle;

	UFUNCTION()
	void HandleStateChanged(EConvexConnectionState State);
};

/**
 * "Convex Subscribe": subscribes to a query. OnSubscribed fires once with the
 * subscription handle; OnUpdate fires on every update; OnFailed fires if no
 * client/subscription could be obtained. Stays alive while subscribed.
 */
UCLASS()
class CONVEXCLIENT_API UConvexSubscribeAction : public UConvexAsyncActionBase
{
	GENERATED_BODY()

public:
	/// Fires once, right after subscribing, with the handle used to unsubscribe.
	UPROPERTY(BlueprintAssignable)
	FConvexSubscriptionPin OnSubscribed;

	/// Fires with each update (including query errors delivered as data).
	UPROPERTY(BlueprintAssignable)
	FConvexResultPin OnUpdate;

	/// Fires if the subscription could not be established.
	UPROPERTY(BlueprintAssignable)
	FConvexResultPin OnFailed;

	UFUNCTION(BlueprintCallable, Category = "Convex|Async",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Convex Subscribe", AutoCreateRefTerm = "Args"))
	static UConvexSubscribeAction* ConvexSubscribe(const UObject* WorldContextObject, UConvexClient* Client,
		const FString& Path, const FConvexArgs& Args);

	/// Unsubscribe and finish the node.
	UFUNCTION(BlueprintCallable, Category = "Convex|Async")
	void Unsubscribe();

	virtual void Activate() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UConvexClient> TargetClient;

	UPROPERTY()
	TObjectPtr<UConvexSubscription> Subscription;

	FString Path;
	FConvexArgs Args;

	UFUNCTION()
	void HandleUpdate(FConvexResult Result);
};

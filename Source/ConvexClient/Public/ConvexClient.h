// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexDelegates.h"
#include "ConvexValue.h"
#include "Containers/Ticker.h"
#include "Templates/PimplPtr.h"
#include "UObject/Object.h"

#include "ConvexClient.generated.h"

class UConvexSubscription;
struct FConvexClientImpl;

/**
 * A realtime Convex client: wraps convex::client (+ a one-shot HTTP client and
 * file-storage transport) and exposes them to native C++ and Blueprint.
 *
 * Not spawnable in Blueprint: obtain one from UConvexSubsystem. All Convex
 * callbacks are delivered on the game thread (pumped mode + an FTSTicker that
 * calls process_events() each tick), so every delegate below fires on the game
 * thread. Every call into convex:: that may throw is wrapped and fails
 * gracefully with a LogConvex error.
 */
UCLASS(BlueprintType, NotBlueprintable)
class CONVEXCLIENT_API UConvexClient : public UObject
{
	GENERATED_BODY()

public:
	/// Build the client for a deployment URL and start connecting. Succeeds at
	/// most once; a failed attempt (bad URL) may be retried with a new URL.
	void Initialize(const FString& DeploymentUrl);

	/// True once Initialize succeeded. A client that is not initialized (never
	/// initialized, or the last attempt failed) performs no operations.
	UFUNCTION(BlueprintPure, Category = "Convex")
	bool IsInitialized() const { return bInitialized; }

	/// True when the last Initialize attempt failed (e.g. malformed URL).
	/// Cleared by the next Initialize call.
	UFUNCTION(BlueprintPure, Category = "Convex")
	bool HasInitializationFailed() const { return bInitializationFailed; }

	/// Tear down the client (joins worker threads, completes pending callbacks
	/// with errors). Idempotent; called automatically on destruction.
	void Shutdown();

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

	// ------------------------------------------------------------------
	// Subscriptions
	// ------------------------------------------------------------------

	/// Subscribe to a query. The returned object exposes OnUpdate; the passed
	/// delegate is also bound to the first/every update.
	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (AutoCreateRefTerm = "Args"))
	UConvexSubscription* Subscribe(const FString& Path, const TMap<FString, FConvexValue>& Args,
		FConvexResultDelegate OnUpdate);

	/// Native subscribe; OnUpdate may be empty.
	UConvexSubscription* SubscribeNative(const FString& Path, const TMap<FString, FConvexValue>& Args,
		FConvexResultNative OnUpdate);

	// ------------------------------------------------------------------
	// One-shot operations over the realtime connection
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (AutoCreateRefTerm = "Args"))
	void Query(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (AutoCreateRefTerm = "Args"))
	void Mutation(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (AutoCreateRefTerm = "Args"))
	void Action(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	void QueryNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);
	void MutationNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);
	void ActionNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);

	// ------------------------------------------------------------------
	// One-shot operations over plain HTTP
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Convex|HTTP", meta = (AutoCreateRefTerm = "Args"))
	void HttpQuery(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	UFUNCTION(BlueprintCallable, Category = "Convex|HTTP", meta = (AutoCreateRefTerm = "Args"))
	void HttpMutation(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	UFUNCTION(BlueprintCallable, Category = "Convex|HTTP", meta = (AutoCreateRefTerm = "Args"))
	void HttpAction(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultDelegate OnResult);

	void HttpQueryNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);
	void HttpMutationNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);
	void HttpActionNative(const FString& Path, const TMap<FString, FConvexValue>& Args, FConvexResultNative OnResult);

	// ------------------------------------------------------------------
	// File storage (pre-signed upload/download URLs; binary safe)
	// ------------------------------------------------------------------

	/// Upload bytes to a generated upload URL; OnDone receives the storage-id
	/// object on success.
	UFUNCTION(BlueprintCallable, Category = "Convex|Files")
	void UploadFile(const FString& UploadUrl, const TArray<uint8>& Data, const FString& ContentType,
		FConvexResultDelegate OnDone);

	void UploadFileNative(const FString& UploadUrl, const TArray<uint8>& Data, const FString& ContentType,
		FConvexResultNative OnDone);

	/// Download raw bytes from a storage URL.
	UFUNCTION(BlueprintCallable, Category = "Convex|Files")
	void DownloadFile(const FString& Url, FConvexDownloadDelegate OnDone);

	void DownloadFileNative(const FString& Url, FConvexDownloadNative OnDone);

	// ------------------------------------------------------------------
	// Auth
	// ------------------------------------------------------------------

	/// Authenticate as an end user with an OIDC JWT. This is the auth path
	/// meant for shipped clients.
	UFUNCTION(BlueprintCallable, Category = "Convex|Auth")
	void SetUserAuth(const FString& Jwt);

	/// Native-only, deliberately NOT Blueprint-callable: deployment admin keys
	/// are secrets. Anything referenced from a Blueprint graph can end up in
	/// cooked assets and packaged client builds. Use only from trusted server
	/// or editor tooling code; never embed an admin key in a shipped game.
	void SetAdminAuth(const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Convex|Auth")
	void ClearAuth();

	// ------------------------------------------------------------------
	// Connection
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex")
	EConvexConnectionState GetConnectionState() const;

	/// Fires on every connection-state transition (game thread).
	UPROPERTY(BlueprintAssignable, Category = "Convex")
	FConvexConnectionStateDelegate OnConnectionStateChanged;

	/// Native mirror of OnConnectionStateChanged.
	FConvexConnectionStateNative OnConnectionStateChangedNative;

	/// Stop rooting a subscription once it has been unsubscribed/released.
	void ForgetSubscription(UConvexSubscription* Subscription);

private:
	bool Tick(float DeltaTime);

	/// Keeps handed-out subscriptions alive until unsubscribed/torn down.
	UPROPERTY()
	TArray<TObjectPtr<UConvexSubscription>> ActiveSubscriptions;

	TPimplPtr<FConvexClientImpl> Impl;
	FTSTicker::FDelegateHandle TickerHandle;
	bool bInitialized = false;
	bool bInitializationFailed = false;
	bool bShutDown = false;
};

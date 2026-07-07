// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexDelegates.h"
#include "Templates/PimplPtr.h"
#include "UObject/Object.h"

#include "ConvexSubscription.generated.h"

class UConvexClient;
struct FConvexSubscriptionHandle;

/**
 * Handle to a live Convex query subscription. Created by UConvexClient; not
 * spawnable in Blueprint. Destroying the object (or calling Unsubscribe)
 * removes the subscriber.
 */
UCLASS(BlueprintType)
class CONVEXCLIENT_API UConvexSubscription : public UObject
{
	GENERATED_BODY()

public:
	/// Fires with the current result immediately and again on every change
	/// (including query errors delivered as data).
	UPROPERTY(BlueprintAssignable, Category = "Convex")
	FConvexUpdateDelegate OnUpdate;

	/// Native multicast mirror of OnUpdate.
	FConvexUpdateNative OnUpdateNative;

	/// Stop receiving updates. Idempotent.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	void Unsubscribe();

	/// True while the subscription is still active.
	UFUNCTION(BlueprintPure, Category = "Convex")
	bool IsActive() const;

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

	// --- Native wiring (used by UConvexClient) -------------------------------

	/// Take ownership of the native, move-only subscription handle. OwningClient
	/// is notified when this subscription is released, so it can stop rooting it.
	void SetHandle(FConvexSubscriptionHandle&& InHandle, UConvexClient* InOwningClient);

	/// Broadcast an update to both the dynamic and native listeners.
	void BroadcastUpdate(const FConvexResult& Result);

private:
	void NotifyReleased();

	TPimplPtr<FConvexSubscriptionHandle> Handle;
	TWeakObjectPtr<UConvexClient> OwningClient;
};

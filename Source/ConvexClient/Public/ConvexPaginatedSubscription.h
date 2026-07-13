// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexResult.h"
#include "ConvexValue.h"
#include "Templates/PimplPtr.h"
#include "UObject/Object.h"

#include "ConvexPaginatedSubscription.generated.h"

class UConvexClient;
struct FConvexPaginatedHandle;

namespace convex
{
	struct paginated_snapshot;
}

/// Where a paginated list stands; mirrors convex::pagination_status (the
/// convex-js usePaginatedQuery statuses, plus Error carried as data).
UENUM(BlueprintType)
enum class EConvexPaginationStatus : uint8
{
	/// The first page has no result yet (fresh subscription, or right after
	/// a reset).
	LoadingFirstPage,
	/// Every requested page is loaded and the server has more items.
	CanLoadMore,
	/// A LoadMore page is in flight.
	LoadingMore,
	/// The server reached the end of the list.
	Exhausted,
	/// A page failed (other than the stale-cursor errors that reset
	/// pagination instead); see FConvexPaginatedSnapshot::Error.
	Error
};

/**
 * Point-in-time view of a paginated list: every loaded item in page order
 * plus the pagination status. Mirrors convex::paginated_snapshot.
 */
USTRUCT(BlueprintType)
struct CONVEXCLIENT_API FConvexPaginatedSnapshot
{
	GENERATED_BODY()

	/// Items from all loaded pages, concatenated in page order. On error,
	/// holds the items from the pages before the failed one.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	TArray<FConvexValue> Results;

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	EConvexPaginationStatus Status = EConvexPaginationStatus::LoadingFirstPage;

	/// True while the first page or a LoadMore page is in flight.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	bool bIsLoading = true;

	/// The failing page's result when Status == Error; a default (empty
	/// error) result otherwise.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FConvexResult Error;

	/// Build from a native snapshot.
	static FConvexPaginatedSnapshot FromNative(const convex::paginated_snapshot& Snapshot);
};

/// Repeated paginated-list update callback (Blueprint).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexPaginatedUpdateDelegate, FConvexPaginatedSnapshot, Snapshot);

/// One-shot bindable snapshot callback (parameter form of the above).
DECLARE_DYNAMIC_DELEGATE_OneParam(FConvexPaginatedSnapshotDelegate, FConvexPaginatedSnapshot, Snapshot);

/// Native multicast mirror.
DECLARE_MULTICAST_DELEGATE_OneParam(FConvexPaginatedUpdateNative, const FConvexPaginatedSnapshot&);

/// Native one-shot binding used by SubscribePaginatedNative.
using FConvexPaginatedUpdateNativeFn = TFunction<void(const FConvexPaginatedSnapshot&)>;

/**
 * A growing, live-updating list backed by a paginated Convex query (one that
 * takes `paginationOpts` and returns a PaginationResult) — the convex-js
 * usePaginatedQuery state machine. Created by UConvexClient; not spawnable in
 * Blueprint.
 *
 * Every loaded page is a live server subscription: items keep updating, and
 * page boundaries stay seam-free across updates and reconnects (query
 * journals). LoadMore appends the next page; changing circumstances the
 * helper cannot patch over (stale cursors, pages past the server read limit)
 * reset the list to a fresh first page automatically.
 *
 * All updates are delivered on the game thread. Destroying the object (or
 * calling Unsubscribe) drops every page subscription.
 */
UCLASS(BlueprintType)
class CONVEXCLIENT_API UConvexPaginatedSubscription : public UObject
{
	GENERATED_BODY()

public:
	/// Fires with a fresh snapshot on every change: page results arriving,
	/// LoadMore starting a page, and resets.
	UPROPERTY(BlueprintAssignable, Category = "Convex")
	FConvexPaginatedUpdateDelegate OnUpdate;

	/// Native multicast mirror of OnUpdate.
	FConvexPaginatedUpdateNative OnUpdateNative;

	/// Request the next page of up to NumItems items. Only acts when the
	/// status is CanLoadMore (a no-op while loading, after exhaustion, and on
	/// error). Returns true when a page load actually started.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	bool LoadMore(int32 NumItems);

	/// Drop every page and start over from a fresh first page (initial page
	/// size).
	UFUNCTION(BlueprintCallable, Category = "Convex")
	void Reset();

	/// Current combined results + status.
	UFUNCTION(BlueprintPure, Category = "Convex")
	FConvexPaginatedSnapshot GetSnapshot() const;

	/// Stop receiving updates and drop every page subscription. Idempotent.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	void Unsubscribe();

	/// True while the paginated subscription is still active.
	UFUNCTION(BlueprintPure, Category = "Convex")
	bool IsActive() const;

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

	// --- Native wiring (used by UConvexClient) -------------------------------

	/// Take ownership of the native, move-only helper. OwningClient is
	/// notified when this subscription is released, so it can stop rooting it.
	void SetHandle(TPimplPtr<FConvexPaginatedHandle>&& InHandle, UConvexClient* InOwningClient);

	/// Broadcast an update to both the dynamic and native listeners.
	void BroadcastUpdate(const FConvexPaginatedSnapshot& Snapshot);

private:
	void NotifyReleased();

	TPimplPtr<FConvexPaginatedHandle> Handle;
	TWeakObjectPtr<UConvexClient> OwningClient;
};

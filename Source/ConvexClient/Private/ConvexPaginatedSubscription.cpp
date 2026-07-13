// Copyright Potionify. Apache-2.0.

#include "ConvexPaginatedSubscription.h"

#include "ConvexClient.h"
#include "ConvexClientModule.h"
#include "ConvexPaginatedHandle.h"

#include <convex/paginated.h>

#include <exception>

FConvexPaginatedSnapshot FConvexPaginatedSnapshot::FromNative(const convex::paginated_snapshot& Snapshot)
{
	FConvexPaginatedSnapshot Out;
	Out.Results.Reserve(static_cast<int32>(Snapshot.results.size()));
	for (const convex::value& Item : Snapshot.results)
	{
		Out.Results.Add(FConvexValue::FromNative(Item));
	}
	switch (Snapshot.status)
	{
	case convex::pagination_status::loading_first_page:
		Out.Status = EConvexPaginationStatus::LoadingFirstPage;
		break;
	case convex::pagination_status::can_load_more:
		Out.Status = EConvexPaginationStatus::CanLoadMore;
		break;
	case convex::pagination_status::loading_more:
		Out.Status = EConvexPaginationStatus::LoadingMore;
		break;
	case convex::pagination_status::exhausted:
		Out.Status = EConvexPaginationStatus::Exhausted;
		break;
	case convex::pagination_status::error:
		Out.Status = EConvexPaginationStatus::Error;
		break;
	}
	Out.bIsLoading = Snapshot.is_loading();
	if (Snapshot.error)
	{
		Out.Error = FConvexResult::FromNative(*Snapshot.error);
	}
	return Out;
}

bool UConvexPaginatedSubscription::LoadMore(int32 NumItems)
{
	if (!Handle || NumItems <= 0)
	{
		return false;
	}
	try
	{
		return Handle->Native.load_more(static_cast<std::size_t>(NumItems));
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UConvexPaginatedSubscription::LoadMore failed: %hs"), Error.what());
		return false;
	}
}

void UConvexPaginatedSubscription::Reset()
{
	if (!Handle)
	{
		return;
	}
	try
	{
		Handle->Native.reset();
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UConvexPaginatedSubscription::Reset failed: %hs"), Error.what());
	}
}

FConvexPaginatedSnapshot UConvexPaginatedSubscription::GetSnapshot() const
{
	if (!Handle)
	{
		return FConvexPaginatedSnapshot();
	}
	try
	{
		return FConvexPaginatedSnapshot::FromNative(Handle->Native.snapshot());
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UConvexPaginatedSubscription::GetSnapshot failed: %hs"), Error.what());
		return FConvexPaginatedSnapshot();
	}
}

void UConvexPaginatedSubscription::Unsubscribe()
{
	if (!Handle)
	{
		return;
	}
	// Releasing the handle drops every page subscription via its destructor.
	Handle.Reset();
	NotifyReleased();
}

bool UConvexPaginatedSubscription::IsActive() const
{
	return Handle.IsValid();
}

void UConvexPaginatedSubscription::BeginDestroy()
{
	Handle.Reset();
	Super::BeginDestroy();
}

void UConvexPaginatedSubscription::SetHandle(TPimplPtr<FConvexPaginatedHandle>&& InHandle,
	UConvexClient* InOwningClient)
{
	Handle = MoveTemp(InHandle);
	OwningClient = InOwningClient;
}

void UConvexPaginatedSubscription::NotifyReleased()
{
	if (UConvexClient* Client = OwningClient.Get())
	{
		Client->ForgetPaginatedSubscription(this);
	}
	OwningClient.Reset();
}

void UConvexPaginatedSubscription::BroadcastUpdate(const FConvexPaginatedSnapshot& Snapshot)
{
	OnUpdate.Broadcast(Snapshot);
	OnUpdateNative.Broadcast(Snapshot);
}

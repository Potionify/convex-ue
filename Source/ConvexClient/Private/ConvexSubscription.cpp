// Copyright Potionify. Apache-2.0.

#include "ConvexSubscription.h"

#include "ConvexClient.h"
#include "ConvexClientModule.h"
#include "ConvexSubscriptionHandle.h"

#include <exception>
#include <utility>

void UConvexSubscription::Unsubscribe()
{
	if (!Handle)
	{
		return;
	}
	try
	{
		Handle->Native.unsubscribe();
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("UConvexSubscription::Unsubscribe failed: %hs"), Error.what());
	}
	NotifyReleased();
}

bool UConvexSubscription::IsActive() const
{
	return Handle && Handle->Native.active();
}

void UConvexSubscription::BeginDestroy()
{
	// Releasing the handle unsubscribes via its RAII destructor.
	Handle.Reset();
	Super::BeginDestroy();
}

void UConvexSubscription::SetHandle(FConvexSubscriptionHandle&& InHandle, UConvexClient* InOwningClient)
{
	Handle = MakePimpl<FConvexSubscriptionHandle>(std::move(InHandle));
	OwningClient = InOwningClient;
}

void UConvexSubscription::NotifyReleased()
{
	if (UConvexClient* Client = OwningClient.Get())
	{
		Client->ForgetSubscription(this);
	}
	OwningClient.Reset();
}

void UConvexSubscription::BroadcastUpdate(const FConvexResult& Result)
{
	OnUpdate.Broadcast(Result);
	OnUpdateNative.Broadcast(Result);
}

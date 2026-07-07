// Copyright Potionify. Apache-2.0.

#pragma once

#include <convex/client.h>

#include <utility>

/// Owns the native, move-only convex::client::subscription. Kept in a private
/// header so both the subscription object and the client can construct one.
/// The RAII handle unsubscribes on destruction and may safely outlive the
/// client (per the convex-cpp contract).
struct FConvexSubscriptionHandle
{
	convex::client::subscription Native;

	FConvexSubscriptionHandle() = default;
	explicit FConvexSubscriptionHandle(convex::client::subscription&& InNative)
		: Native(std::move(InNative))
	{
	}
};

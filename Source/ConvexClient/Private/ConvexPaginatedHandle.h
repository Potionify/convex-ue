// Copyright Potionify. Apache-2.0.

#pragma once

#include <convex/paginated.h>

#include <utility>

/// Owns the native, move-only convex::paginated_query. Kept in a private
/// header so both the subscription object and the client can construct one.
/// Unlike plain subscription handles, the paginated helper must NOT outlive
/// its convex::client (LoadMore/Reset re-subscribe through it), so
/// UConvexClient tears these down in Shutdown().
struct FConvexPaginatedHandle
{
	convex::paginated_query Native;

	explicit FConvexPaginatedHandle(convex::paginated_query&& InNative)
		: Native(std::move(InNative))
	{
	}
};

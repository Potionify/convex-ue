// Copyright Potionify. All Rights Reserved.

// Compile-time proof that the vendored convex-cpp public headers include and
// build cleanly inside the ConvexClient module (which uses UE's shared PCHs
// and full macro set). Nothing here is ever called at runtime.

#include <convex/client.h>
#include <convex/value.h>

namespace
{
	// Instantiate a convex::value to force template/type resolution of the
	// vendored headers. Marked unused; never invoked.
	[[maybe_unused]] void ConvexHeaderSanityCheck()
	{
		convex::value NullValue;
		convex::value IntValue(int64_t{42});
		(void)NullValue;
		(void)IntValue;
	}
}

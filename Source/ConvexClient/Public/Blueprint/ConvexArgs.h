// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexValue.h"

#include "ConvexArgs.generated.h"

/**
 * A Blueprint-friendly argument map for Convex functions: a named set of
 * FConvexValue fields. Built up with the chainable Add*Arg helpers in
 * UConvexBlueprintLibrary and consumed by the query/mutation/action/subscribe
 * nodes.
 */
USTRUCT(BlueprintType)
struct CONVEXCLIENT_API FConvexArgs
{
	GENERATED_BODY()

	/// The argument fields, keyed by name.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	TMap<FString, FConvexValue> Fields;

	/// Access the underlying map (used internally to feed convex:: calls).
	const TMap<FString, FConvexValue>& ToMap() const { return Fields; }
};

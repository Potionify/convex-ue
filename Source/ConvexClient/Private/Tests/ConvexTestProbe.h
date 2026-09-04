// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexResult.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ConvexTestProbe.generated.h"

/// A dynamic-delegate target for the lifetime tests: nothing references it
/// except the client's pending-callback list or a subscription's listeners.
UCLASS()
class UConvexTestProbe : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void OnResult(FConvexResult Result) { ++Calls; }

	/// Collects garbage from inside the callback, then touches the object:
	/// the client must still be holding it at that point.
	UFUNCTION()
	void OnResultCollect(FConvexResult Result)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		++Calls;
	}

	int32 Calls = 0;
};

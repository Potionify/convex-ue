// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ConvexExampleTestLibrary.generated.h"

/**
 * Test-only helpers for the Example project's AngelScript tests.
 *
 * The fork's AngelscriptTest commandlet runs unit tests synchronously and
 * never ticks the engine between statements, so a script test that talks to
 * a live backend has no way to wait for a callback. Pump ticks the core
 * tickers, which is where UConvexClient and the engine WebSockets manager
 * deliver their events, for a bounded time. Script-only; not part of the plugin.
 */
UCLASS()
class UConvexExampleTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/// Tick the core ticker for up to Seconds of wall-clock time, sleeping
	/// briefly between ticks. Returns after the time has passed; callers
	/// poll their own state and loop.
	UFUNCTION(meta = (ScriptCallable))
	static void Pump(float Seconds);
};

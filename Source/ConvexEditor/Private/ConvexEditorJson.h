// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexValue.h"
#include "CoreMinimal.h"

/// JSON helpers for the editor UI. UE's Json module is used only for display
/// formatting; wire encode/decode always goes through the convex-cpp codec
/// (FConvexValue::FromWire/ToWire) so value semantics stay exact.
namespace ConvexEditorJson
{
	/// Pretty-print a Convex wire-JSON document for display. Falls back to the
	/// input on parse failure (never throws, never loses data).
	FString PrettyPrint(const FString& WireJson);

	/// Build a pretty-printed args skeleton from a function's args validator
	/// (validator-JSON as a Convex value): required fields get type-appropriate
	/// defaults, optional fields are omitted. Returns "{}" when the validator
	/// is absent/any/unsupported.
	FString SeedArgsFromValidator(const FConvexValue& ArgsValidator);
}

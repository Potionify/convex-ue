// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

// The vendored convex-cpp value type. Included here (not merely forward
// declared) because FConvexValue holds a TSharedPtr<convex::value> and the
// helper signatures below traffic in convex::value_object.
#include <convex/value.h>

#include "ConvexValue.generated.h"

/// The kind of a Convex value; mirrors convex::value::kind.
UENUM(BlueprintType)
enum class EConvexValueKind : uint8
{
	Null,
	Boolean,
	Int64,
	Float64,
	String,
	Bytes,
	Array,
	Object
};

/**
 * Blueprint-friendly wrapper around an immutable convex::value.
 *
 * The underlying value is held by a shared pointer that is intentionally NOT a
 * UPROPERTY (USTRUCT cannot reflect a TSharedPtr). The struct stays copyable
 * and Blueprint-usable through the static factories and Try* accessors; copies
 * share the same immutable value, which is safe because nothing mutates through
 * this wrapper.
 */
USTRUCT(BlueprintType)
struct CONVEXCLIENT_API FConvexValue
{
	GENERATED_BODY()

	FConvexValue();

	/// Wrap an existing native value (deep-copied into a shared holder).
	static FConvexValue FromNative(const convex::value& InValue);

	/// The wrapped value. Never null after construction (a default is the
	/// Convex `null` value).
	const convex::value& GetNative() const;

	// ------------------------------------------------------------------
	// Factories
	// ------------------------------------------------------------------

	static FConvexValue Null();
	static FConvexValue Bool(bool bValue);
	static FConvexValue Int64(int64 Value);
	static FConvexValue Float(double Value);
	static FConvexValue String(const FString& Value);
	static FConvexValue Bytes(const TArray<uint8>& Value);
	static FConvexValue Array(const TArray<FConvexValue>& Value);
	static FConvexValue Object(const TMap<FString, FConvexValue>& Value);

	// ------------------------------------------------------------------
	// Introspection
	// ------------------------------------------------------------------

	EConvexValueKind GetKind() const;

	bool TryGetBool(bool& OutValue) const;
	bool TryGetInt64(int64& OutValue) const;
	bool TryGetFloat(double& OutValue) const;
	bool TryGetString(FString& OutValue) const;
	bool TryGetBytes(TArray<uint8>& OutValue) const;
	bool TryGetArray(TArray<FConvexValue>& OutValue) const;
	bool TryGetObject(TMap<FString, FConvexValue>& OutFields) const;

	// ------------------------------------------------------------------
	// Wire JSON (delegates to the core codec; never UE's Json module)
	// ------------------------------------------------------------------

	/// Decode Convex wire JSON. bOutSuccess is false on a codec error.
	static FConvexValue FromWire(const FString& Json, bool& bOutSuccess);

	/// Encode to Convex wire JSON. bOutSuccess is false on a codec error.
	FString ToWire(bool& bOutSuccess) const;

private:
	// Non-UPROPERTY on purpose (see struct comment). Auto-detected struct ops
	// handle the non-trivial copy/destruct.
	TSharedPtr<convex::value> Inner;
};

/// Build a convex::value_object argument map from Blueprint values. Keys are
/// UTF-8 encoded.
convex::value_object ConvexMakeArgs(const TMap<FString, FConvexValue>& Args);

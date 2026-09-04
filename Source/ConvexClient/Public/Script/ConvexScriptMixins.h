// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexPaginatedSubscription.h"
#include "ConvexResult.h"
#include "ConvexValue.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ConvexScriptMixins.generated.h"

/**
 * Method-style accessors for the Convex value, result, and snapshot structs
 * in AngelScript.
 *
 * The Hazelight UnrealEngine-Angelscript fork reads the ScriptMixin metadata
 * and binds every static function whose first parameter is one of the listed
 * structs as a method on that struct, so `Result.Value.Field("author").AsString()`
 * reads a nested field in one expression. Stock UE ignores the metadata and
 * these functions are not Blueprint nodes; Blueprint keeps using
 * UConvexBlueprintLibrary, whose out-parameter style suits graphs better.
 *
 * Every accessor returns a value and never fails loudly: a type mismatch or a
 * missing field yields a default (Convex null, an empty container, 0, false,
 * or the fallback passed to an *Or form). Use the Is* predicates or
 * HasField when the difference between "absent" and "present but empty"
 * matters.
 */
UCLASS(meta = (ScriptMixin = "FConvexValue FConvexResult FConvexPaginatedSnapshot"))
class CONVEXCLIENT_API UConvexScriptMixins : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// FConvexValue: kind
	// ------------------------------------------------------------------

	UFUNCTION(meta = (ScriptCallable))
	static EConvexValueKind Kind(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsNull(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsBool(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsInt(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsFloat(const FConvexValue& Value);

	/// True for Int64 and Float64 alike.
	UFUNCTION(meta = (ScriptCallable))
	static bool IsNumber(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsString(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsBytes(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsArray(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsObject(const FConvexValue& Value);

	// ------------------------------------------------------------------
	// FConvexValue: scalar reads
	// ------------------------------------------------------------------
	//
	// The plain As* forms return false, 0, or an empty string on a mismatch.
	// The *Or forms take the fallback as an argument. Stock UHT records
	// default argument values only for Blueprint-callable functions, so the
	// two forms are separate functions rather than one with a default.

	UFUNCTION(meta = (ScriptCallable))
	static bool AsBool(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static bool AsBoolOr(const FConvexValue& Value, bool Default);

	/// Int64 values, and Float64 values that hold a whole number, read as an
	/// integer.
	UFUNCTION(meta = (ScriptCallable))
	static int64 AsInt(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static int64 AsIntOr(const FConvexValue& Value, int64 Default);

	/// Float64 and Int64 values both read as a float.
	UFUNCTION(meta = (ScriptCallable))
	static double AsFloat(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static double AsFloatOr(const FConvexValue& Value, double Default);

	UFUNCTION(meta = (ScriptCallable))
	static FString AsString(const FConvexValue& Value);

	UFUNCTION(meta = (ScriptCallable))
	static FString AsStringOr(const FConvexValue& Value, const FString& Default);

	/// The bytes of a Bytes value; empty for anything else.
	UFUNCTION(meta = (ScriptCallable))
	static TArray<uint8> AsBytes(const FConvexValue& Value);

	/// The elements of an Array value; empty for anything else.
	UFUNCTION(meta = (ScriptCallable))
	static TArray<FConvexValue> AsArray(const FConvexValue& Value);

	/// The fields of an Object value; empty for anything else.
	UFUNCTION(meta = (ScriptCallable))
	static TMap<FString, FConvexValue> AsObject(const FConvexValue& Value);

	// ------------------------------------------------------------------
	// FConvexValue: navigation
	// ------------------------------------------------------------------

	/// Number of elements of an Array value, or of fields of an Object value;
	/// 0 for anything else.
	UFUNCTION(meta = (ScriptCallable))
	static int32 Length(const FConvexValue& Value);

	/// Element Index of an Array value; Convex null when out of range or not
	/// an array.
	UFUNCTION(meta = (ScriptCallable))
	static FConvexValue At(const FConvexValue& Value, int32 Index);

	/// True when Value is an Object that has a field named Name.
	UFUNCTION(meta = (ScriptCallable))
	static bool HasField(const FConvexValue& Value, const FString& Name);

	/// The field named exactly Name of an Object value; Convex null when
	/// absent or not an object. Unlike Field, the name is not split on dots.
	UFUNCTION(meta = (ScriptCallable))
	static FConvexValue Get(const FConvexValue& Value, const FString& Name);

	/// The field at a dotted path such as "player.stats.hp". Each segment
	/// names an object field; a segment that is a whole number indexes an
	/// array. Convex null when any step is missing, and for an empty path or
	/// an empty segment ("a..b").
	UFUNCTION(meta = (ScriptCallable))
	static FConvexValue Field(const FConvexValue& Value, const FString& Path);

	/// The field names of an Object value, sorted; empty for anything else.
	UFUNCTION(meta = (ScriptCallable))
	static TArray<FString> Keys(const FConvexValue& Value);

	/// Convex wire JSON for logging; empty on a codec error.
	UFUNCTION(meta = (ScriptCallable))
	static FString ToJson(const FConvexValue& Value);

	// ------------------------------------------------------------------
	// FConvexResult
	// ------------------------------------------------------------------

	/// True for either error flavor (the opposite of bSuccess).
	UFUNCTION(meta = (ScriptCallable))
	static bool IsError(const FConvexResult& Result);

	/// Result.Value.Field(Path); Convex null on an error result.
	UFUNCTION(meta = (ScriptCallable))
	static FConvexValue ResultField(const FConvexResult& Result, const FString& Path);

	/// One line for logs: the value as JSON on success, the error text
	/// otherwise.
	UFUNCTION(meta = (ScriptCallable))
	static FString Describe(const FConvexResult& Result);

	// ------------------------------------------------------------------
	// FConvexPaginatedSnapshot
	// ------------------------------------------------------------------

	/// Number of loaded items.
	UFUNCTION(meta = (ScriptCallable))
	static int32 Num(const FConvexPaginatedSnapshot& Snapshot);

	/// Loaded item Index; Convex null when out of range.
	UFUNCTION(meta = (ScriptCallable))
	static FConvexValue Item(const FConvexPaginatedSnapshot& Snapshot, int32 Index);

	UFUNCTION(meta = (ScriptCallable))
	static bool CanLoadMore(const FConvexPaginatedSnapshot& Snapshot);

	UFUNCTION(meta = (ScriptCallable))
	static bool IsExhausted(const FConvexPaginatedSnapshot& Snapshot);

	UFUNCTION(meta = (ScriptCallable))
	static bool HasError(const FConvexPaginatedSnapshot& Snapshot);
};

// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexArgs.h"
#include "ConvexResult.h"
#include "ConvexValue.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ConvexBlueprintLibrary.generated.h"

class UConvexClient;
class UConvexSubsystem;

/**
 * Static Blueprint helpers for building and reading Convex values and
 * arguments, converting to/from wire JSON, inspecting results, and reaching the
 * subsystem/default client. Pure data helpers; the actual calls are made
 * through UConvexClient or the async-action nodes.
 */
UCLASS()
class CONVEXCLIENT_API UConvexBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// Value construction
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexNull();

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexBool(bool bValue);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexInt(int64 Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexFloat(double Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexString(const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexBytes(const TArray<uint8>& Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexArray(const TArray<FConvexValue>& Values);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue MakeConvexObject(const TMap<FString, FConvexValue>& Fields);

	// ------------------------------------------------------------------
	// Value reading
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static EConvexValueKind GetValueKind(const FConvexValue& Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static bool GetBoolValue(const FConvexValue& Value, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static int64 GetIntValue(const FConvexValue& Value, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static double GetFloatValue(const FConvexValue& Value, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FString GetStringValue(const FConvexValue& Value, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static TArray<uint8> GetBytesValue(const FConvexValue& Value, bool& bSuccess);

	/// Read an array value. Returns false (and leaves Out empty) if not an array.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static bool GetArrayValue(const FConvexValue& Value, TArray<FConvexValue>& Out);

	/// Read an object value. Returns false (and leaves Out empty) if not an object.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static bool GetObjectValue(const FConvexValue& Value, TMap<FString, FConvexValue>& Out);

	/// Element at Index of an array value; bSuccess is false if out of range or
	/// not an array.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue GetArrayElement(const FConvexValue& Value, int32 Index, bool& bSuccess);

	/// Number of elements in an array value; 0 if not an array.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static int32 GetArrayLength(const FConvexValue& Value);

	/// Field named Name of an object value; bSuccess is false if absent or not
	/// an object.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static FConvexValue GetObjectField(const FConvexValue& Value, const FString& Name, bool& bSuccess);

	/// Keys of an object value. Returns false (and leaves Out empty) if not an
	/// object.
	UFUNCTION(BlueprintPure, Category = "Convex|Value")
	static bool GetObjectKeys(const FConvexValue& Value, TArray<FString>& Out);

	// ------------------------------------------------------------------
	// Arguments (chainable builder; each returns the updated args)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs MakeEmptyArgs();

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddStringArg(FConvexArgs Args, const FString& Name, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddIntArg(FConvexArgs Args, const FString& Name, int64 Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddFloatArg(FConvexArgs Args, const FString& Name, double Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddBoolArg(FConvexArgs Args, const FString& Name, bool Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddNullArg(FConvexArgs Args, const FString& Name);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddBytesArg(FConvexArgs Args, const FString& Name, const TArray<uint8>& Value);

	UFUNCTION(BlueprintPure, Category = "Convex|Args")
	static FConvexArgs AddValueArg(FConvexArgs Args, const FString& Name, const FConvexValue& Value);

	/// Internal: unwrap an FConvexArgs to its field map.
	static const TMap<FString, FConvexValue>& ArgsToMap(const FConvexArgs& Args);

	// ------------------------------------------------------------------
	// Wire JSON (debug helpers; delegate to the core codec)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex|JSON")
	static FString ValueToJsonString(const FConvexValue& Value, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Convex|JSON")
	static FConvexValue JsonStringToValue(const FString& Json, bool& bSuccess);

	// ------------------------------------------------------------------
	// Result inspection
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Convex|Result")
	static bool IsResultSuccess(const FConvexResult& Result);

	UFUNCTION(BlueprintPure, Category = "Convex|Result")
	static bool IsResultAppError(const FConvexResult& Result);

	UFUNCTION(BlueprintPure, Category = "Convex|Result")
	static FConvexValue GetResultValue(const FConvexResult& Result);

	UFUNCTION(BlueprintPure, Category = "Convex|Result")
	static FString GetResultErrorMessage(const FConvexResult& Result);

	UFUNCTION(BlueprintPure, Category = "Convex|Result")
	static FConvexValue GetResultErrorData(const FConvexResult& Result);

	// ------------------------------------------------------------------
	// Access
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (WorldContext = "WorldContextObject"))
	static UConvexSubsystem* GetConvexSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (WorldContext = "WorldContextObject"))
	static UConvexClient* GetDefaultConvexClient(const UObject* WorldContextObject);
};

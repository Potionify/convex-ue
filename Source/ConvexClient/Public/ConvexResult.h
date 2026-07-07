// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexValue.h"

#include <convex/error.h>

#include "ConvexResult.generated.h"

/// Coarse classification of a FConvexResult.
UENUM(BlueprintType)
enum class EConvexResultKind : uint8
{
	/// Ran successfully; Value holds the result.
	Success,
	/// Developer/system error (redacted in production); ErrorMessage only.
	Error,
	/// Application error raised via ConvexError; ErrorMessage + ErrorData.
	AppError
};

/**
 * Blueprint-friendly wrapper around convex::function_result: the outcome of a
 * query, mutation, or action. Function-level failures are carried as data
 * (never thrown), so subscription updates can deliver errors too.
 */
USTRUCT(BlueprintType)
struct CONVEXCLIENT_API FConvexResult
{
	GENERATED_BODY()

	/// True when the call succeeded and Value is populated.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	bool bSuccess = false;

	/// True when this is an application error (ConvexError) with ErrorData.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	bool bIsAppError = false;

	/// Error text for either error flavor; empty on success.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FString ErrorMessage;

	/// Success value; the Convex `null` value when not successful.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FConvexValue Value;

	/// Application-error payload; valid only when bIsAppError.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FConvexValue ErrorData;

	EConvexResultKind GetKind() const;

	/// Build from a native function_result.
	static FConvexResult FromNative(const convex::function_result& Result);

	/// Build a plain (non-app) error result.
	static FConvexResult MakeError(const FString& Message);
};

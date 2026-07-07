// Copyright Potionify. Apache-2.0.

#include "ConvexResult.h"

#include "ConvexClientModule.h"
#include "ConvexUtils.h"

#include <exception>

EConvexResultKind FConvexResult::GetKind() const
{
	if (bSuccess)
	{
		return EConvexResultKind::Success;
	}
	return bIsAppError ? EConvexResultKind::AppError : EConvexResultKind::Error;
}

FConvexResult FConvexResult::FromNative(const convex::function_result& Result)
{
	FConvexResult Out;
	try
	{
		if (Result.ok())
		{
			Out.bSuccess = true;
			Out.Value = FConvexValue::FromNative(Result.get_value());
		}
		else
		{
			Out.bSuccess = false;
			Out.ErrorMessage = ConvexUtils::Utf8ToFString(Result.error_message());
			if (const convex::convex_error* AppError = Result.app_error())
			{
				Out.bIsAppError = true;
				Out.ErrorData = FConvexValue::FromNative(AppError->data);
			}
		}
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("FConvexResult::FromNative failed: %hs"), Error.what());
		Out = MakeError(TEXT("convex: failed to decode result"));
	}
	return Out;
}

FConvexResult FConvexResult::MakeError(const FString& Message)
{
	FConvexResult Out;
	Out.bSuccess = false;
	Out.bIsAppError = false;
	Out.ErrorMessage = Message;
	return Out;
}

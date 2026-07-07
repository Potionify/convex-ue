// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"

#include <string>

// Shared private helpers for the ConvexClient module: UTF-8 <-> FString
// conversion (every FString<->std::string boundary must go through here, as
// Convex values may carry arbitrary Unicode) and game-thread marshalling for
// UE APIs that are game-thread-affine (WebSockets/HTTP).

namespace ConvexUtils
{
	/// Decode a UTF-8 buffer into an FString. Length-aware (never relies on a
	/// terminating null, which binary-adjacent payloads may lack).
	inline FString Utf8ToFString(const char* Data, int32 Len)
	{
		if (Len <= 0 || Data == nullptr)
		{
			return FString();
		}
		// reinterpret_cast bridges std::string's `char` to UE's UTF8CHAR
		// (which may be char8_t in UE 5.8).
		FUTF8ToTCHAR Conv(reinterpret_cast<const UTF8CHAR*>(Data), Len);
		return FString(Conv.Length(), Conv.Get());
	}

	inline FString Utf8ToFString(const std::string& S)
	{
		return Utf8ToFString(S.data(), static_cast<int32>(S.size()));
	}

	/// Encode an FString as a UTF-8 std::string.
	inline std::string FStringToUtf8(const FString& S)
	{
		FTCHARToUTF8 Conv(*S, S.Len());
		return std::string(reinterpret_cast<const char*>(Conv.Get()), Conv.Length());
	}

	/// Run work on the game thread. Executes inline when already on the game
	/// thread; otherwise queues via AsyncTask. UE's WebSockets and HTTP modules
	/// must only be touched on the game thread.
	inline void RunOnGameThread(TUniqueFunction<void()>&& Fn)
	{
		if (IsInGameThread())
		{
			Fn();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
		}
	}
}

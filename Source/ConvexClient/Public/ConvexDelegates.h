// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "ConvexResult.h"

#include "ConvexDelegates.generated.h"

// Single source of truth for the Convex delegate and enum types shared across
// the client, subscription, subsystem, and the (future) Blueprint layer.
// Include order matters: the delegate signatures below reference FConvexResult
// and EConvexConnectionState, so ConvexResult.h is included above and the enum
// is declared before the delegates.

/// Realtime connection state; mirrors convex::connection_state.
UENUM(BlueprintType)
enum class EConvexConnectionState : uint8
{
	Disconnected,
	Connecting,
	Connected
};

/// Which server function kind emitted a log entry; mirrors
/// convex::log_entry::source_kind.
UENUM(BlueprintType)
enum class EConvexLogSource : uint8
{
	Query,
	Mutation,
	Action
};

/// Server-side console output (`console.log` in a query/mutation/action),
/// attributed to the emitting function; mirrors convex::log_entry.
USTRUCT(BlueprintType)
struct FConvexLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	EConvexLogSource Source = EConvexLogSource::Query;

	/// Canonical function path ("messages:send"); empty when unknown.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FString UdfPath;

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	TArray<FString> Lines;
};

/// Point-in-time connection snapshot for UX and telemetry; mirrors
/// convex::connection_info.
USTRUCT(BlueprintType)
struct FConvexConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	EConvexConnectionState State = EConvexConnectionState::Disconnected;

	/// Consecutive failed/broken connection attempts; 0 while healthy.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	int32 Retries = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	int32 InflightMutations = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	int32 InflightActions = 0;

	/// True once every query re-sent by the last reconnect has a result.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	bool bHasSyncedPastLastRestart = false;

	/// Why the previous connection ended ("InitialConnect" before the first).
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	FString LastCloseReason;

	/// Connect attempts made over this client's lifetime.
	UPROPERTY(BlueprintReadOnly, Category = "Convex")
	int32 ConnectionCount = 0;
};

// ----------------------------------------------------------------------------
// Dynamic (Blueprint-visible) delegates
// ----------------------------------------------------------------------------

/// One-shot result callback (query/mutation/action/upload).
DECLARE_DYNAMIC_DELEGATE_OneParam(FConvexResultDelegate, FConvexResult, Result);

/// Repeated subscription update callback.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexUpdateDelegate, FConvexResult, Result);

/// Connection-state change callback.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexConnectionStateDelegate, EConvexConnectionState, State);

/// Server log-line callback (per function execution).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexServerLogDelegate, FConvexLogEntry, Entry);

/// Terminal authentication failure: the server rejected a token that cannot
/// be refreshed further; the client continues unauthenticated until the next
/// SetUserAuth call.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexAuthFailureDelegate, FString, Reason);

/// File-download completion callback.
DECLARE_DYNAMIC_DELEGATE_TwoParams(FConvexDownloadDelegate, bool, bSuccess, const TArray<uint8>&, Data);

// ----------------------------------------------------------------------------
// Native (C++) equivalents. Not UHT-reflected; usable from native code and by
// the Blueprint async-action layer built on top of this module.
// ----------------------------------------------------------------------------

/// Native one-shot result callback.
using FConvexResultNative = TFunction<void(const FConvexResult&)>;

/// Native file-download completion callback.
using FConvexDownloadNative = TFunction<void(bool /*bSuccess*/, const TArray<uint8>& /*Data*/)>;

/// Native multicast subscription update.
DECLARE_MULTICAST_DELEGATE_OneParam(FConvexUpdateNative, const FConvexResult&);

/// Native multicast connection-state change.
DECLARE_MULTICAST_DELEGATE_OneParam(FConvexConnectionStateNative, EConvexConnectionState);

/// Native multicast server log entry.
DECLARE_MULTICAST_DELEGATE_OneParam(FConvexServerLogNative, const FConvexLogEntry&);

/// Native multicast terminal auth failure.
DECLARE_MULTICAST_DELEGATE_OneParam(FConvexAuthFailureNative, const FString&);

/// Fetches a fresh user JWT when the client reconnects (bForceRefresh=true
/// means the previous token may be expired). Return an unset optional to keep
/// the current token. WARNING: called on an internal worker thread while the
/// client's lock is held — it must be fast, thread-safe, and must NOT call
/// back into the client or touch UObjects. Returning a cached token that game
/// code refreshes elsewhere is the intended pattern.
using FConvexAuthRefreshNative = TFunction<TOptional<FString>(bool bForceRefresh)>;

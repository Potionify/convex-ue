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

// ----------------------------------------------------------------------------
// Dynamic (Blueprint-visible) delegates
// ----------------------------------------------------------------------------

/// One-shot result callback (query/mutation/action/upload).
DECLARE_DYNAMIC_DELEGATE_OneParam(FConvexResultDelegate, FConvexResult, Result);

/// Repeated subscription update callback.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexUpdateDelegate, FConvexResult, Result);

/// Connection-state change callback.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConvexConnectionStateDelegate, EConvexConnectionState, State);

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

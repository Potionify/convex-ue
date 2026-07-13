// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

/**
 * Debug tap on the Convex websocket transport: every text frame (both
 * directions, every client in the process) is forwarded to a multicast
 * delegate while enabled. Powers the editor's traffic inspector; costs one
 * boolean check per frame when disabled.
 *
 * All callbacks fire on the game thread (the transport marshals socket work
 * there). Not intended for shipping builds — the editor module is the only
 * expected consumer.
 */
namespace ConvexWireTap
{
	enum class EDirection : uint8
	{
		Outgoing,
		Incoming
	};

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWireFrame,
		EDirection /*Direction*/, const FString& /*Url*/, const FString& /*Text*/);

	/// Bind/unbind listeners here.
	CONVEXCLIENT_API FOnWireFrame& OnWireFrame();

	/// Master switch checked on the frame hot path.
	CONVEXCLIENT_API void SetEnabled(bool bEnabled);
	CONVEXCLIENT_API bool IsEnabled();

	/// Transport-internal: broadcast when enabled. Game thread only.
	CONVEXCLIENT_API void Report(EDirection Direction, const FString& Url, const FString& Text);
}

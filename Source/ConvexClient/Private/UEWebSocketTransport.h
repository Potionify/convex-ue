// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

#include <convex/transport.h>

#include <map>
#include <memory>
#include <string>

/**
 * Adapts UE's WebSockets module to convex::websocket_transport.
 *
 * Threading: the convex client calls connect() from its internal worker thread
 * and destroys connections from that worker thread, while send_text() may be
 * called from arbitrary threads under the client's lock. UE's IWebSocket is
 * game-thread-affine, so every IWebSocket call is marshalled to the game
 * thread. Observer callbacks are forwarded only from IWebSocket delegates
 * (game thread); a dead-flag under a critical section guarantees no observer
 * callback fires once a connection has been destroyed.
 */
class FUEWebSocketTransport : public convex::websocket_transport
{
public:
	virtual ~FUEWebSocketTransport() override = default;

	virtual std::unique_ptr<convex::websocket_connection> connect(
		const std::string& url,
		const std::map<std::string, std::string>& headers,
		convex::websocket_observer& observer) override;
};

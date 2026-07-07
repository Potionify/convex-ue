// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

#include <convex/transport.h>

#include <functional>

/**
 * Adapts UE's HTTP module to convex::http_transport for the one-shot HTTP
 * client and file storage. Requests are created and dispatched on the game
 * thread (FHttpModule is game-thread-affine); on_done is invoked from the
 * completion delegate (game thread), which satisfies the "any thread" contract.
 * Bodies are binary-capable in both directions (file upload/download).
 */
class FUEHttpTransport : public convex::http_transport
{
public:
	virtual ~FUEHttpTransport() override = default;

	virtual void send(convex::http_request request,
		std::function<void(convex::http_response)> on_done) override;
};

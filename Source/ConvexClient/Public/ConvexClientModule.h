// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

CONVEXCLIENT_API DECLARE_LOG_CATEGORY_EXTERN(LogConvex, Log, All);

/**
 * ConvexClient module. The UE-facing client layer that wires convex-cpp's
 * abstract transports to UE's WebSockets/HTTP.
 */
class FConvexClientModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

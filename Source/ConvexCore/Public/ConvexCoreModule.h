// Copyright Potionify. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * ConvexCore module. Compiles the vendored pure-C++ convex-cpp library and
 * exposes its headers under the <convex/...> include path. Holds no UE state.
 */
class FConvexCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

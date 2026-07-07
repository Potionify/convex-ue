// Copyright Potionify. All Rights Reserved.

#include "ConvexClientModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogConvex);

void FConvexClientModule::StartupModule()
{
	UE_LOG(LogConvex, Log, TEXT("ConvexClient module started."));
}

void FConvexClientModule::ShutdownModule()
{
	UE_LOG(LogConvex, Log, TEXT("ConvexClient module shut down."));
}

IMPLEMENT_MODULE(FConvexClientModule, ConvexClient)

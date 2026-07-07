// Copyright Potionify. Apache-2.0.

#include "ConvexClientModule.h"

#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY(LogConvex);

void FConvexClientModule::StartupModule()
{
	// The websocket transport creates IWebSocket instances on the game thread;
	// make sure the module is loaded up front so CreateWebSocket never races a
	// first-time load off the game thread.
	FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));

	UE_LOG(LogConvex, Log, TEXT("ConvexClient module started."));
}

void FConvexClientModule::ShutdownModule()
{
	UE_LOG(LogConvex, Log, TEXT("ConvexClient module shut down."));
}

IMPLEMENT_MODULE(FConvexClientModule, ConvexClient)

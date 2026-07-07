// Copyright Potionify. Apache-2.0.

#include "ConvexSubsystem.h"

#include "ConvexClient.h"
#include "ConvexClientModule.h"
#include "ConvexSettings.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UConvexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UConvexSettings* Settings = UConvexSettings::Get();
	if (Settings && Settings->bAutoConnectDefaultClient && !Settings->DeploymentUrl.IsEmpty())
	{
		GetDefaultClient();
	}
}

void UConvexSubsystem::Deinitialize()
{
	if (DefaultClient)
	{
		DefaultClient->Shutdown();
		DefaultClient = nullptr;
	}
	for (const auto& Pair : NamedClients)
	{
		if (Pair.Value)
		{
			Pair.Value->Shutdown();
		}
	}
	NamedClients.Empty();
	for (const TObjectPtr<UConvexClient>& Client : UnnamedClients)
	{
		if (Client)
		{
			Client->Shutdown();
		}
	}
	UnnamedClients.Empty();

	Super::Deinitialize();
}

UConvexClient* UConvexSubsystem::MakeClient(const FString& Url)
{
	UConvexClient* Client = NewObject<UConvexClient>(this);
	Client->Initialize(Url);
	return Client;
}

UConvexClient* UConvexSubsystem::GetDefaultClient()
{
	if (DefaultClient)
	{
		return DefaultClient;
	}

	const UConvexSettings* Settings = UConvexSettings::Get();
	const FString Url = Settings ? Settings->DeploymentUrl : FString();
	if (Url.IsEmpty())
	{
		UE_LOG(LogConvex, Warning,
			TEXT("GetDefaultClient: UConvexSettings::DeploymentUrl is empty; set it in Project Settings -> Plugins -> Convex."));
		return nullptr;
	}

	DefaultClient = MakeClient(Url);
	return DefaultClient;
}

UConvexClient* UConvexSubsystem::GetOrCreateClient(const FString& Name, const FString& Url)
{
	if (const TObjectPtr<UConvexClient>* Existing = NamedClients.Find(Name))
	{
		return *Existing;
	}
	UConvexClient* Client = MakeClient(Url);
	NamedClients.Add(Name, Client);
	return Client;
}

UConvexClient* UConvexSubsystem::GetClient(const FString& Name) const
{
	if (const TObjectPtr<UConvexClient>* Existing = NamedClients.Find(Name))
	{
		return *Existing;
	}
	return nullptr;
}

UConvexClient* UConvexSubsystem::CreateClient(const FString& Url)
{
	UConvexClient* Client = MakeClient(Url);
	UnnamedClients.Add(Client);
	return Client;
}

UConvexSubsystem* UConvexSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UConvexSubsystem>();
		}
	}
	return nullptr;
}

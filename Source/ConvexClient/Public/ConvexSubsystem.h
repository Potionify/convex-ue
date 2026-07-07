// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ConvexSubsystem.generated.h"

class UConvexClient;

/**
 * Owns Convex clients for the lifetime of a game instance. Provides a lazily
 * created default client (from project settings) plus a named registry for
 * additional deployments. All clients are torn down on Deinitialize.
 */
UCLASS()
class CONVEXCLIENT_API UConvexSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/// The default client, created on first use from UConvexSettings.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	UConvexClient* GetDefaultClient();

	/// Create (or return the existing) named client bound to Url.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	UConvexClient* GetOrCreateClient(const FString& Name, const FString& Url);

	/// Look up a previously created named client; nullptr if absent.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	UConvexClient* GetClient(const FString& Name) const;

	/// Create an unnamed client for Url. The subsystem keeps it alive until
	/// Deinitialize.
	UFUNCTION(BlueprintCallable, Category = "Convex")
	UConvexClient* CreateClient(const FString& Url);

	/// Resolve the subsystem from any world context object.
	UFUNCTION(BlueprintCallable, Category = "Convex", meta = (WorldContext = "WorldContextObject"))
	static UConvexSubsystem* Get(const UObject* WorldContextObject);

private:
	UConvexClient* MakeClient(const FString& Url);

	UPROPERTY()
	TObjectPtr<UConvexClient> DefaultClient;

	UPROPERTY()
	TMap<FString, TObjectPtr<UConvexClient>> NamedClients;

	UPROPERTY()
	TArray<TObjectPtr<UConvexClient>> UnnamedClients;
};

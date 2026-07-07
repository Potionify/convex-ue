// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ConvexSettings.generated.h"

/**
 * Project settings for the Convex plugin (Project Settings -> Plugins ->
 * Convex). Stored in DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Convex"))
class CONVEXCLIENT_API UConvexSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UConvexSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	//~ End UDeveloperSettings interface

	/// Deployment URL, e.g. "https://happy-animal-123.convex.cloud".
	UPROPERTY(Config, EditAnywhere, Category = "Convex")
	FString DeploymentUrl;

	/// When true, the subsystem eagerly creates and connects the default
	/// client on game-instance startup (if DeploymentUrl is set).
	UPROPERTY(Config, EditAnywhere, Category = "Convex")
	bool bAutoConnectDefaultClient = true;

	/// Convenience accessor.
	static const UConvexSettings* Get();
};

// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

#include "ConvexChatGameMode.generated.h"

class UConvexChatWidget;

/**
 * Player controller for the demo level. Spawns the chat widget, puts it
 * on screen and hands the mouse to it.
 */
UCLASS()
class CONVEXEXAMPLE_API AConvexChatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AConvexChatPlayerController();

	/// Widget class to spawn. Defaults to the C++ widget itself.
	UPROPERTY(EditDefaultsOnly, Category = "Convex Chat")
	TSubclassOf<UConvexChatWidget> ChatWidgetClass;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UConvexChatWidget> ChatWidget;
};

/** Game mode that uses AConvexChatPlayerController. Set as the project default. */
UCLASS()
class CONVEXEXAMPLE_API AConvexChatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AConvexChatGameMode();
};

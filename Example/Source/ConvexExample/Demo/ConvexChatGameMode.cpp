// Copyright Potionify. Apache-2.0.

#include "ConvexChatGameMode.h"

#include "Blueprint/UserWidget.h"
#include "ConvexChatWidget.h"

AConvexChatPlayerController::AConvexChatPlayerController()
{
	ChatWidgetClass = UConvexChatWidget::StaticClass();
	bShowMouseCursor = true;
}

void AConvexChatPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (!ChatWidgetClass)
	{
		return;
	}
	ChatWidget = CreateWidget<UConvexChatWidget>(this, ChatWidgetClass);
	if (!ChatWidget)
	{
		return;
	}
	ChatWidget->AddToViewport();
	ChatWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
	ChatWidget->SetDesiredSizeInViewport(FVector2D(560.0f, 420.0f));
	// Position and size setters reset the anchors, so center only afterwards.
	ChatWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
	ChatWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(ChatWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

AConvexChatGameMode::AConvexChatGameMode()
{
	PlayerControllerClass = AConvexChatPlayerController::StaticClass();
}

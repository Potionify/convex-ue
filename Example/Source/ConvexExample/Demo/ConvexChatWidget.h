// Copyright Potionify. Apache-2.0.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ConvexValue.h"
#include "CoreMinimal.h"

#include "ConvexChatWidget.generated.h"

class UButton;
class UConvexSubscription;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

/**
 * Chat panel for the demo level, plain C++ with no Blueprint. On construct
 * it subscribes to messages:list for one channel and repaints the list on
 * every update. Enter or the Send button runs the messages:send mutation.
 */
UCLASS(Blueprintable)
class CONVEXEXAMPLE_API UConvexChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/// Channel to subscribe to and send into.
	UPROPERTY(EditAnywhere, Category = "Convex Chat")
	FString Channel = TEXT("general");

	/// Author written into sent messages.
	UPROPERTY(EditAnywhere, Category = "Convex Chat")
	FString Author = TEXT("unreal");

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> MessageList;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> Input;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SendButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UConvexSubscription> Subscription;

	/// Repaint the list from a messages:list result: an array of objects
	/// with author and body fields.
	void RenderMessages(const FConvexValue& Messages);

	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void Submit();
};

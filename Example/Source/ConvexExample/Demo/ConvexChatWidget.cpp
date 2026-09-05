// Copyright Potionify. Apache-2.0.

#include "ConvexChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "ConvexClient.h"
#include "ConvexResult.h"
#include "ConvexSubscription.h"
#include "ConvexSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor PanelColor(0.035f, 0.04f, 0.055f, 0.96f);
	const FLinearColor InputColor(0.07f, 0.08f, 0.1f, 1.0f);
	const FLinearColor AccentColor(0.886f, 0.655f, 0.231f, 1.0f);
	const FLinearColor AuthorColor(0.55f, 0.58f, 0.66f, 1.0f);
	const FLinearColor BodyColor(0.91f, 0.915f, 0.93f, 1.0f);

	FSlateFontInfo Font(int32 Size, FName Typeface = TEXT("Regular"))
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		return Info;
	}
}

TSharedRef<SWidget> UConvexChatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(18.0f, 14.0f));
		WidgetTree->RootWidget = Panel;

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Panel->SetContent(Column);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("#") + Channel));
		TitleText->SetFont(Font(18, TEXT("Bold")));
		TitleText->SetColorAndOpacity(FSlateColor(AccentColor));
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}

		MessageList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MessageList"));
		MessageList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(MessageList))
		{
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InputRow"));
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
		}

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input"));
		Input->SetHintText(FText::FromString(TEXT("Say something")));
		Input->WidgetStyle.SetFont(Font(15));
		Input->WidgetStyle.SetPadding(FMargin(10.0f, 8.0f));
		Input->WidgetStyle.BackgroundImageNormal.TintColor = FSlateColor(InputColor);
		Input->WidgetStyle.BackgroundImageHovered.TintColor = FSlateColor(InputColor);
		Input->WidgetStyle.BackgroundImageFocused.TintColor = FSlateColor(InputColor);
		Input->WidgetStyle.SetForegroundColor(FSlateColor(BodyColor));
		Input->WidgetStyle.SetFocusedForegroundColor(FSlateColor(BodyColor));
		Input->OnTextCommitted.AddDynamic(this, &UConvexChatWidget::HandleTextCommitted);
		if (UHorizontalBoxSlot* InputSlot = Row->AddChildToHorizontalBox(Input))
		{
			InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			InputSlot->SetVerticalAlignment(VAlign_Fill);
		}

		SendButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SendButton"));
		SendButton->SetBackgroundColor(AccentColor);
		SendButton->OnClicked.AddDynamic(this, &UConvexChatWidget::HandleSendClicked);
		UTextBlock* SendLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SendLabel"));
		SendLabel->SetText(FText::FromString(TEXT("Send")));
		SendLabel->SetFont(Font(14, TEXT("Bold")));
		SendLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.06f, 0.03f)));
		SendButton->SetContent(SendLabel);
		if (UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(SendButton))
		{
			ButtonSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UConvexChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UConvexSubsystem* Subsystem = UConvexSubsystem::Get(this);
	UConvexClient* Client = Subsystem ? Subsystem->GetDefaultClient() : nullptr;
	if (!Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConvexChat: no default Convex client. Set DeploymentUrl in project settings."));
		return;
	}

	// Live query: the callback fires on the game thread on every change.
	TMap<FString, FConvexValue> Args;
	Args.Add(TEXT("channel"), FConvexValue::String(Channel));
	Subscription = Client->SubscribeNative(TEXT("messages:list"), Args,
		[this](const FConvexResult& Result)
		{
			if (Result.bSuccess)
			{
				RenderMessages(Result.Value);
			}
		});
}

void UConvexChatWidget::NativeDestruct()
{
	if (Subscription)
	{
		Subscription->Unsubscribe();
		Subscription = nullptr;
	}
	Super::NativeDestruct();
}

void UConvexChatWidget::RenderMessages(const FConvexValue& Messages)
{
	if (!MessageList || !WidgetTree)
	{
		return;
	}

	MessageList->ClearChildren();

	TArray<FConvexValue> Docs;
	if (!Messages.TryGetArray(Docs))
	{
		return;
	}

	for (const FConvexValue& Doc : Docs)
	{
		TMap<FString, FConvexValue> Fields;
		if (!Doc.TryGetObject(Fields))
		{
			continue;
		}
		FString DocAuthor, DocBody;
		if (const FConvexValue* Found = Fields.Find(TEXT("author")))
		{
			Found->TryGetString(DocAuthor);
		}
		if (const FConvexValue* Found = Fields.Find(TEXT("body")))
		{
			Found->TryGetString(DocBody);
		}

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* AuthorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		AuthorText->SetText(FText::FromString(DocAuthor));
		AuthorText->SetFont(Font(14, TEXT("Bold")));
		AuthorText->SetColorAndOpacity(FSlateColor(AuthorColor));
		if (UHorizontalBoxSlot* AuthorSlot = Line->AddChildToHorizontalBox(AuthorText))
		{
			AuthorSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}

		UTextBlock* BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		BodyText->SetText(FText::FromString(DocBody));
		BodyText->SetFont(Font(14));
		BodyText->SetColorAndOpacity(FSlateColor(BodyColor));
		BodyText->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* BodySlot = Line->AddChildToHorizontalBox(BodyText))
		{
			BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(MessageList->AddChild(Line)))
		{
			LineSlot->SetPadding(FMargin(0.0f, 4.0f));
		}
	}

	MessageList->ScrollToEnd();
}

void UConvexChatWidget::HandleSendClicked()
{
	Submit();
}

void UConvexChatWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Submit();
	}
}

void UConvexChatWidget::Submit()
{
	if (!Input)
	{
		return;
	}
	const FString Body = Input->GetText().ToString().TrimStartAndEnd();
	if (Body.IsEmpty())
	{
		return;
	}
	Input->SetText(FText::GetEmpty());
	Input->SetKeyboardFocus();

	UConvexSubsystem* Subsystem = UConvexSubsystem::Get(this);
	UConvexClient* Client = Subsystem ? Subsystem->GetDefaultClient() : nullptr;
	if (!Client)
	{
		return;
	}
	// The callback fires only after the subscription above already shows the write.
	Client->MutationNative(TEXT("messages:send"),
		{
			{TEXT("channel"), FConvexValue::String(Channel)},
			{TEXT("author"), FConvexValue::String(Author)},
			{TEXT("body"), FConvexValue::String(Body)},
		},
		[](const FConvexResult& Result)
		{
			if (!Result.bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("ConvexChat: send failed: %s"), *Result.ErrorMessage);
			}
		});
}

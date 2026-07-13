// Copyright Potionify. Apache-2.0.

#include "SConvexTrafficPanel.h"

#include "ConvexEditorJson.h"
#include "ConvexWireTap.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	constexpr int32 MaxFrames = 2000;
	constexpr int32 MaxStoredFrameChars = 128 * 1024;

	/// Cheap "type" extraction — frames can be megabytes (Transitions), so no
	/// full JSON parse on the capture path.
	FString PeekType(const FString& Text)
	{
		static const FString Marker(TEXT("\"type\":"));
		int32 Start = Text.Find(Marker, ESearchCase::CaseSensitive);
		if (Start == INDEX_NONE)
		{
			return TEXT("?");
		}
		Start += Marker.Len();
		while (Start < Text.Len() && (Text[Start] == TEXT(' ') || Text[Start] == TEXT('"')))
		{
			++Start;
		}
		int32 End = Start;
		while (End < Text.Len() && Text[End] != TEXT('"'))
		{
			++End;
		}
		return Text.Mid(Start, End - Start);
	}
}

void SConvexTrafficPanel::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 4.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("FilterFrames", "Filter by message type or content..."))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					FilterString = Text.ToString();
					ApplyFilter();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::Format(
						LOCTEXT("FrameCount", "{0} frames (editor session + PIE clients)"),
						AllItems.Num());
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]()
					{ return bPaused ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda(
					[this](ECheckBoxState State) { bPaused = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PauseFrames", "Pause"))
					.Margin(FMargin(4.f, 2.f))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearFrames", "Clear"))
				.OnClicked_Lambda([this]()
				{
					AllItems.Reset();
					ApplyFilter();
					if (DetailBox.IsValid())
					{
						DetailBox->SetText(FText::GetEmpty());
					}
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.6f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(2.f)
				[
					SAssignNew(ListView, SListView<FFrameItem>)
					.ListItemsSource(&FilteredItems)
					.OnGenerateRow(this, &SConvexTrafficPanel::MakeRow)
					.OnSelectionChanged_Lambda([this](FFrameItem Item, ESelectInfo::Type)
					{
						if (DetailBox.IsValid())
						{
							DetailBox->SetText(Item.IsValid()
								? FText::FromString(ConvexEditorJson::PrettyPrint(Item->Text))
								: FText::GetEmpty());
						}
					})
					.SelectionMode(ESelectionMode::Single)
				]
			]

			+ SSplitter::Slot()
			.Value(0.4f)
			[
				SAssignNew(DetailBox, SMultiLineEditableTextBox)
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.IsReadOnly(true)
			]
		]
	];

	// The tap stays enabled exactly as long as a traffic panel exists.
	TapHandle = ConvexWireTap::OnWireFrame().AddLambda(
		[WeakThis = TWeakPtr<SConvexTrafficPanel>(SharedThis(this))](
			ConvexWireTap::EDirection Direction, const FString& Url, const FString& Text)
		{
			if (const TSharedPtr<SConvexTrafficPanel> This = WeakThis.Pin())
			{
				This->OnFrame((int32)Direction, Url, Text);
			}
		});
	ConvexWireTap::SetEnabled(true);
}

SConvexTrafficPanel::~SConvexTrafficPanel()
{
	ConvexWireTap::SetEnabled(false);
	if (TapHandle.IsValid())
	{
		ConvexWireTap::OnWireFrame().Remove(TapHandle);
		TapHandle.Reset();
	}
}

void SConvexTrafficPanel::OnFrame(int32 Direction, const FString& Url, const FString& Text)
{
	if (bPaused)
	{
		return;
	}
	const TSharedPtr<FFrameRow> Row = MakeShared<FFrameRow>();
	Row->Time = FDateTime::Now().ToString(TEXT("%H:%M:%S.%s"));
	Row->bOutgoing = Direction == (int32)ConvexWireTap::EDirection::Outgoing;
	Row->Type = PeekType(Text);
	Row->Bytes = Text.Len();
	Row->Url = Url;
	Row->Text = Text.Len() > MaxStoredFrameChars
		? Text.Left(MaxStoredFrameChars) + TEXT("\n… (truncated)")
		: Text;

	AllItems.Add(Row);
	if (AllItems.Num() > MaxFrames)
	{
		AllItems.RemoveAt(0, AllItems.Num() - MaxFrames);
	}
	ApplyFilter();
}

void SConvexTrafficPanel::ApplyFilter()
{
	FilteredItems.Reset();
	for (const FFrameItem& Item : AllItems)
	{
		if (FilterString.IsEmpty() || Item->Type.Contains(FilterString) ||
			Item->Text.Contains(FilterString))
		{
			FilteredItems.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
		if (FilteredItems.Num() > 0)
		{
			ListView->RequestScrollIntoView(FilteredItems.Last());
		}
	}
}

TSharedRef<ITableRow> SConvexTrafficPanel::MakeRow(FFrameItem Item, const TSharedRef<STableViewBase>& Owner)
{
	const FLinearColor DirectionColor = Item->bOutgoing
		? FLinearColor(0.4f, 0.7f, 1.0f)
		: FLinearColor(0.5f, 0.85f, 0.5f);
	return SNew(STableRow<FFrameItem>, Owner)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.f, 2.f, 8.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Time))
			.Font(FAppStyle::GetFontStyle("MonospacedText"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 2.f, 8.f, 2.f)
		[
			SNew(STextBlock)
			.Text(Item->bOutgoing ? FText::FromString(TEXT("→ out"))
								  : FText::FromString(TEXT("← in")))
			.ColorAndOpacity(FSlateColor(DirectionColor))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 2.f, 8.f, 2.f)
		[
			SNew(SBox)
			.WidthOverride(140.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Type))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 2.f, 8.f, 2.f)
		[
			SNew(SBox)
			.WidthOverride(80.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%d B"), Item->Bytes)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Url))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		]
	];
}

#undef LOCTEXT_NAMESPACE

// Copyright Potionify. Apache-2.0.

#include "SConvexFunctionRunner.h"

#include "ConvexEditorJson.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	FLinearColor TypeColor(const FString& FunctionType)
	{
		if (FunctionType == TEXT("Query")) return FLinearColor(0.25f, 0.65f, 0.35f);
		if (FunctionType == TEXT("Mutation")) return FLinearColor(0.85f, 0.55f, 0.15f);
		if (FunctionType == TEXT("Action")) return FLinearColor(0.3f, 0.55f, 0.9f);
		return FLinearColor(0.55f, 0.55f, 0.55f);  // HttpAction / unknown
	}

	FString TypeAbbrev(const FString& FunctionType)
	{
		if (FunctionType == TEXT("Query")) return TEXT("Q");
		if (FunctionType == TEXT("Mutation")) return TEXT("M");
		if (FunctionType == TEXT("Action")) return TEXT("A");
		return TEXT("H");
	}
}

void SConvexFunctionRunner::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;
	Session->OnChanged.AddSP(SharedThis(this), &SConvexFunctionRunner::RebuildItems);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// Left: filterable function list.
		+ SSplitter::Slot()
		.Value(0.38f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(LOCTEXT("FilterFunctions", "Filter functions..."))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					FilterString = Text.ToString();
					ApplyFilter();
				})
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(4.f, 0.f, 4.f, 4.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(2.f)
				[
					SAssignNew(ListView, SListView<FItem>)
					.ListItemsSource(&FilteredItems)
					.OnGenerateRow(this, &SConvexFunctionRunner::MakeRow)
					.OnSelectionChanged(this, &SConvexFunctionRunner::OnSelectionChanged)
					.SelectionMode(ESelectionMode::Single)
				]
			]
		]

		// Right: selected function, args, run, result.
		+ SSplitter::Slot()
		.Value(0.62f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 8.f, 8.f, 4.f)
			[
				SNew(STextBlock)
				.Text(this, &SConvexFunctionRunner::GetSelectedHeaderText)
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 2.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArgsLabel", "Arguments (Convex wire JSON)"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(8.f, 0.f, 8.f, 4.f)
			[
				SAssignNew(ArgsBox, SMultiLineEditableTextBox)
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.HintText(LOCTEXT("ArgsHint", "{}"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 4.f)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(this, &SConvexFunctionRunner::GetRunButtonText)
				.IsEnabled(this, &SConvexFunctionRunner::CanRun)
				.OnClicked(this, &SConvexFunctionRunner::OnRunClicked)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 4.f, 8.f, 2.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ResultLabel", "Result"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.6f)
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				SAssignNew(ResultBox, SMultiLineEditableTextBox)
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.IsReadOnly(true)
			]
		]
	];

	RebuildItems();
}

void SConvexFunctionRunner::RebuildItems()
{
	const FString PreviousSelection = Selected.IsValid() ? Selected->Identifier : FString();

	AllItems.Reset();
	for (const FConvexFunctionSpec& Spec : Session->GetFunctions())
	{
		AllItems.Add(MakeShared<FConvexFunctionSpec>(Spec));
	}
	ApplyFilter();

	// Keep the selection stable across apiSpec refreshes (live subscription
	// re-fires on every deploy).
	if (!PreviousSelection.IsEmpty())
	{
		for (const FItem& Item : FilteredItems)
		{
			if (Item->Identifier == PreviousSelection)
			{
				ListView->SetSelection(Item, ESelectInfo::Direct);
				Selected = Item;
				return;
			}
		}
	}
}

void SConvexFunctionRunner::ApplyFilter()
{
	FilteredItems.Reset();
	for (const FItem& Item : AllItems)
	{
		if (FilterString.IsEmpty() || Item->Identifier.Contains(FilterString) ||
			Item->FunctionType.Contains(FilterString))
		{
			FilteredItems.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SConvexFunctionRunner::MakeRow(FItem Item, const TSharedRef<STableViewBase>& Owner)
{
	const bool bInternal = Item->Visibility == TEXT("internal");
	return SNew(STableRow<FItem>, Owner)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f, 6.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TypeAbbrev(Item->FunctionType)))
			.ColorAndOpacity(FSlateColor(TypeColor(Item->FunctionType)))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
			.ToolTipText(FText::FromString(Item->FunctionType))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Identifier))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(bInternal
				? FSlateColor::UseSubduedForeground()
				: FSlateColor::UseForeground())
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f)
		[
			SNew(STextBlock)
			.Text(bInternal ? LOCTEXT("Internal", "internal") : FText::GetEmpty())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Font(FAppStyle::GetFontStyle("SmallFont"))
		]
	];
}

void SConvexFunctionRunner::OnSelectionChanged(FItem Item, ESelectInfo::Type)
{
	// Stash the current edit before switching.
	if (Selected.IsValid() && ArgsBox.IsValid())
	{
		ArgsCache.Add(Selected->Identifier, ArgsBox->GetText().ToString());
	}
	Selected = Item;
	if (!ArgsBox.IsValid())
	{
		return;
	}
	if (!Item.IsValid())
	{
		ArgsBox->SetText(FText::GetEmpty());
		return;
	}
	if (const FString* Cached = ArgsCache.Find(Item->Identifier))
	{
		ArgsBox->SetText(FText::FromString(*Cached));
	}
	else
	{
		ArgsBox->SetText(
			FText::FromString(ConvexEditorJson::SeedArgsFromValidator(Item->ArgsValidator)));
	}
}

bool SConvexFunctionRunner::CanRun() const
{
	return !bRunning && Selected.IsValid() && Selected->FunctionType != TEXT("HttpAction") &&
		Session->IsConnected();
}

FText SConvexFunctionRunner::GetRunButtonText() const
{
	if (bRunning)
	{
		return LOCTEXT("Running", "Running...");
	}
	if (Selected.IsValid())
	{
		if (Selected->FunctionType == TEXT("HttpAction"))
		{
			return LOCTEXT("RunHttpNA", "HTTP route (not runnable)");
		}
		return FText::Format(LOCTEXT("RunTyped", "Run {0}"),
			FText::FromString(Selected->FunctionType));
	}
	return LOCTEXT("Run", "Run");
}

FText SConvexFunctionRunner::GetSelectedHeaderText() const
{
	if (!Selected.IsValid())
	{
		return LOCTEXT("NoSelection", "Select a function");
	}
	FString Header = Selected->Identifier + TEXT("  (") + Selected->FunctionType;
	if (Selected->Visibility == TEXT("internal"))
	{
		Header += TEXT(", internal");
	}
	Header += TEXT(")");
	return FText::FromString(Header);
}

FReply SConvexFunctionRunner::OnRunClicked()
{
	if (!CanRun())
	{
		return FReply::Handled();
	}
	bRunning = true;
	const uint64 Run = ++RunCounter;
	ResultBox->SetText(FText::GetEmpty());

	Session->RunFunction(*Selected, ArgsBox->GetText().ToString(),
		[WeakThis = TWeakPtr<SConvexFunctionRunner>(SharedThis(this)), Run](
			const FConvexResult& Result)
		{
			const TSharedPtr<SConvexFunctionRunner> This = WeakThis.Pin();
			if (!This.IsValid() || This->RunCounter != Run)
			{
				return;
			}
			This->bRunning = false;

			FString Text;
			if (Result.bSuccess)
			{
				bool bEncoded = false;
				const FString Wire = Result.Value.ToWire(bEncoded);
				Text = bEncoded ? ConvexEditorJson::PrettyPrint(Wire)
								: TEXT("<failed to encode result>");
			}
			else
			{
				Text = TEXT("Error: ") + Result.ErrorMessage;
				if (Result.bIsAppError)
				{
					bool bEncoded = false;
					const FString Wire = Result.ErrorData.ToWire(bEncoded);
					if (bEncoded)
					{
						Text += TEXT("\n\nConvexError data:\n") +
							ConvexEditorJson::PrettyPrint(Wire);
					}
				}
			}
			This->ResultBox->SetText(FText::FromString(Text));
		});
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

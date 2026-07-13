// Copyright Potionify. Apache-2.0.

#include "SConvexDataBrowser.h"

#include "ConvexClient.h"
#include "ConvexEditorJson.h"
#include "ConvexSubscription.h"
#include "Misc/Base64.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	constexpr int32 PageSize = 25;
	constexpr int32 PreviewMaxChars = 220;

	FString MakePreview(const FString& WireJson)
	{
		FString OneLine = WireJson.Replace(TEXT("\n"), TEXT(" "));
		if (OneLine.Len() > PreviewMaxChars)
		{
			OneLine = OneLine.Left(PreviewMaxChars) + TEXT("…");
		}
		return OneLine;
	}
}

void SConvexDataBrowser::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;
	Session->OnChanged.AddSP(SharedThis(this), &SConvexDataBrowser::OnSessionChanged);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// Left: table list.
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.Padding(2.f)
			[
				SAssignNew(TableList, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&TableItems)
				.OnGenerateRow(this, &SConvexDataBrowser::MakeTableRow)
				.OnSelectionChanged_Lambda(
					[this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectTable(Item); })
				.SelectionMode(ESelectionMode::Single)
			]
		]

		// Right: toolbar, documents, detail.
		+ SSplitter::Slot()
		.Value(0.78f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.f, 4.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SConvexDataBrowser::GetStatusText)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([this]()
						{ return bAscending ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						bAscending = State == ECheckBoxState::Checked;
						ResetPages();
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OrderAsc", "Oldest first"))
						.Margin(FMargin(4.f, 2.f))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoadMore", "Load more"))
					.IsEnabled(this, &SConvexDataBrowser::CanLoadMore)
					.OnClicked(this, &SConvexDataBrowser::OnLoadMoreClicked)
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.62f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					.Padding(2.f)
					[
						SAssignNew(DocList, SListView<FDocItem>)
						.ListItemsSource(&DocItems)
						.OnGenerateRow(this, &SConvexDataBrowser::MakeDocRow)
						.OnSelectionChanged_Lambda([this](FDocItem Item, ESelectInfo::Type)
						{
							if (DetailBox.IsValid())
							{
								DetailBox->SetText(Item.IsValid()
									? FText::FromString(Item->PrettyJson)
									: FText::GetEmpty());
							}
						})
						.SelectionMode(ESelectionMode::Single)
					]
				]

				+ SSplitter::Slot()
				.Value(0.38f)
				[
					SAssignNew(DetailBox, SMultiLineEditableTextBox)
					.Font(FAppStyle::GetFontStyle("MonospacedText"))
					.IsReadOnly(true)
				]
			]
		]
	];

	OnSessionChanged();
}

SConvexDataBrowser::~SConvexDataBrowser()
{
	// ClearPages, not ResetPages: resubscribing here would call SharedThis()
	// on a widget whose shared controller is already gone.
	ClearPages();
	if (RowCountSubscription.IsValid())
	{
		RowCountSubscription->Unsubscribe();
		RowCountSubscription.Reset();
	}
}

void SConvexDataBrowser::OnSessionChanged()
{
	// Rebuild the table list, keeping the selection when it survives.
	const TArray<FString>& Names = Session->GetTableNames();
	bool bChanged = Names.Num() != TableItems.Num();
	if (!bChanged)
	{
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (*TableItems[Index] != Names[Index])
			{
				bChanged = true;
				break;
			}
		}
	}
	if (!bChanged)
	{
		return;
	}

	TableItems.Reset();
	TSharedPtr<FString> Reselect;
	for (const FString& Name : Names)
	{
		TSharedPtr<FString> Item = MakeShared<FString>(Name);
		if (Name == SelectedTable)
		{
			Reselect = Item;
		}
		TableItems.Add(MoveTemp(Item));
	}
	if (TableList.IsValid())
	{
		TableList->RequestListRefresh();
		if (Reselect.IsValid())
		{
			TableList->SetSelection(Reselect, ESelectInfo::Direct);
		}
		else if (TableItems.Num() > 0)
		{
			// First population (or the selected table vanished): show the
			// first table rather than an empty pane.
			TableList->SetSelection(TableItems[0], ESelectInfo::Direct);
			SelectTable(TableItems[0]);
		}
	}
	if (TableItems.Num() == 0 && !SelectedTable.IsEmpty())
	{
		SelectedTable.Reset();
		ResetPages();
	}
}

void SConvexDataBrowser::SelectTable(TSharedPtr<FString> Table)
{
	const FString NewTable = Table.IsValid() ? *Table : FString();
	if (NewTable == SelectedTable)
	{
		return;
	}
	SelectedTable = NewTable;

	// Live row count for the selected table.
	if (RowCountSubscription.IsValid())
	{
		RowCountSubscription->Unsubscribe();
		RowCountSubscription.Reset();
	}
	RowCount = -1;
	UConvexClient* Client = Session->GetClient();
	// tableSize rejects system tables; leave their count unknown.
	if (Client != nullptr && !SelectedTable.IsEmpty() && !SelectedTable.StartsWith(TEXT("_")))
	{
		TMap<FString, FConvexValue> Args;
		Args.Add(TEXT("tableName"), FConvexValue::String(SelectedTable));
		RowCountSubscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
			TEXT("_system/frontend/tableSize"), Args,
			[WeakThis = TWeakPtr<SConvexDataBrowser>(SharedThis(this))](const FConvexResult& Result)
			{
				const TSharedPtr<SConvexDataBrowser> This = WeakThis.Pin();
				if (!This.IsValid())
				{
					return;
				}
				double Count = 0.0;
				This->RowCount =
					(Result.bSuccess && Result.Value.TryGetFloat(Count)) ? (int64)Count : -1;
			}));
	}

	ResetPages();
}

FString SConvexDataBrowser::FiltersArgument() const
{
	if (!bAscending)
	{
		return FString();  // default order (desc): send null
	}
	const FTCHARToUTF8 Utf8(TEXT("{\"clauses\":[],\"order\":\"asc\"}"));
	return FBase64::Encode(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

void SConvexDataBrowser::ClearPages()
{
	++PagesGeneration;
	for (const TSharedPtr<FPage>& Page : Pages)
	{
		if (Page->Subscription.IsValid())
		{
			Page->Subscription->Unsubscribe();
		}
	}
	Pages.Reset();
	DocItems.Reset();
}

void SConvexDataBrowser::ResetPages()
{
	ClearPages();
	RebuildDocItems();
	if (!SelectedTable.IsEmpty() && Session->GetClient() != nullptr)
	{
		SubscribePage(0, FString());
	}
}

void SConvexDataBrowser::SubscribePage(int32 PageIndex, const FString& Cursor)
{
	UConvexClient* Client = Session->GetClient();
	if (Client == nullptr)
	{
		return;
	}
	const TSharedPtr<FPage> Page = MakeShared<FPage>();
	Pages.Add(Page);

	TMap<FString, FConvexValue> PaginationOpts;
	PaginationOpts.Add(TEXT("numItems"), FConvexValue::Float(PageSize));
	PaginationOpts.Add(TEXT("cursor"),
		Cursor.IsEmpty() ? FConvexValue::Null() : FConvexValue::String(Cursor));
	PaginationOpts.Add(TEXT("id"), FConvexValue::Float(PageIndex + 1));

	// System tables (_file_storage, _scheduled_jobs) are only readable via
	// db.system, which paginatedTableDocuments does not use — the CLI's
	// tableData query handles both kinds (no filter support there).
	const bool bSystemTable = SelectedTable.StartsWith(TEXT("_"));

	TMap<FString, FConvexValue> Args;
	Args.Add(TEXT("paginationOpts"), FConvexValue::Object(PaginationOpts));
	Args.Add(TEXT("table"), FConvexValue::String(SelectedTable));
	if (bSystemTable)
	{
		Args.Add(TEXT("order"),
			FConvexValue::String(bAscending ? TEXT("asc") : TEXT("desc")));
	}
	else
	{
		const FString Filters = FiltersArgument();
		Args.Add(TEXT("filters"),
			Filters.IsEmpty() ? FConvexValue::Null() : FConvexValue::String(Filters));
	}

	const uint64 Gen = PagesGeneration;
	Page->Subscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
		bSystemTable ? TEXT("_system/cli/tableData")
					 : TEXT("_system/frontend/paginatedTableDocuments"),
		Args,
		[WeakThis = TWeakPtr<SConvexDataBrowser>(SharedThis(this)), Gen, Page](
			const FConvexResult& Result)
		{
			const TSharedPtr<SConvexDataBrowser> This = WeakThis.Pin();
			if (!This.IsValid() || This->PagesGeneration != Gen)
			{
				return;
			}
			Page->Docs.Reset();
			Page->bLoaded = true;

			TMap<FString, FConvexValue> Fields;
			if (Result.bSuccess && Result.Value.TryGetObject(Fields))
			{
				if (const FConvexValue* IsDone = Fields.Find(TEXT("isDone")))
				{
					bool bDone = false;
					IsDone->TryGetBool(bDone);
					Page->bIsDone = bDone;
				}
				if (const FConvexValue* Continue = Fields.Find(TEXT("continueCursor")))
				{
					Continue->TryGetString(Page->ContinueCursor);
				}
				TArray<FConvexValue> Rows;
				if (const FConvexValue* PageRows = Fields.Find(TEXT("page"));
					PageRows != nullptr && PageRows->TryGetArray(Rows))
				{
					for (const FConvexValue& Row : Rows)
					{
						const TSharedPtr<FDocRow> Doc = MakeShared<FDocRow>();
						TMap<FString, FConvexValue> RowFields;
						if (Row.TryGetObject(RowFields))
						{
							if (const FConvexValue* IdValue = RowFields.Find(TEXT("_id")))
							{
								IdValue->TryGetString(Doc->Id);
							}
							// Filter-validation errors ride inside the page
							// as {error, filter} objects; show them in place.
							if (Doc->Id.IsEmpty())
							{
								if (const FConvexValue* Error = RowFields.Find(TEXT("error")))
								{
									Error->TryGetString(Doc->Id);
									Doc->Id = TEXT("(filter error)");
								}
							}
						}
						bool bEncoded = false;
						const FString Wire = Row.ToWire(bEncoded);
						if (bEncoded)
						{
							Doc->Preview = MakePreview(Wire);
							Doc->PrettyJson = ConvexEditorJson::PrettyPrint(Wire);
						}
						Page->Docs.Add(Doc);
					}
				}
			}
			else if (!Result.bSuccess)
			{
				const TSharedPtr<FDocRow> Doc = MakeShared<FDocRow>();
				Doc->Id = TEXT("(error)");
				Doc->Preview = Result.ErrorMessage;
				Doc->PrettyJson = Result.ErrorMessage;
				Page->Docs.Add(Doc);
				Page->bIsDone = true;
			}
			This->RebuildDocItems();
		}));
}

void SConvexDataBrowser::RebuildDocItems()
{
	DocItems.Reset();
	for (const TSharedPtr<FPage>& Page : Pages)
	{
		DocItems.Append(Page->Docs);
	}
	if (DocList.IsValid())
	{
		DocList->RequestListRefresh();
	}
}

bool SConvexDataBrowser::CanLoadMore() const
{
	if (Pages.Num() == 0)
	{
		return false;
	}
	const TSharedPtr<FPage>& Last = Pages.Last();
	return Last->bLoaded && !Last->bIsDone && !Last->ContinueCursor.IsEmpty();
}

FReply SConvexDataBrowser::OnLoadMoreClicked()
{
	if (CanLoadMore())
	{
		SubscribePage(Pages.Num(), Pages.Last()->ContinueCursor);
	}
	return FReply::Handled();
}

FText SConvexDataBrowser::GetStatusText() const
{
	if (SelectedTable.IsEmpty())
	{
		return LOCTEXT("PickTable", "Select a table");
	}
	FString Status = SelectedTable;
	if (RowCount >= 0)
	{
		Status += FString::Printf(TEXT("  —  %lld rows"), RowCount);
	}
	Status += FString::Printf(TEXT("  (%d loaded)"), DocItems.Num());
	return FText::FromString(Status);
}

TSharedRef<ITableRow> SConvexDataBrowser::MakeTableRow(
	TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FString>>, Owner)
	[
		SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Margin(FMargin(4.f, 3.f))
	];
}

TSharedRef<ITableRow> SConvexDataBrowser::MakeDocRow(
	FDocItem Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<FDocItem>, Owner)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.f, 2.f, 8.f, 2.f)
		[
			SNew(SBox)
			.WidthOverride(230.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Id))
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Preview))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		]
	];
}

#undef LOCTEXT_NAMESPACE

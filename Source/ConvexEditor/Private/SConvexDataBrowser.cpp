// Copyright Potionify. Apache-2.0.

#include "SConvexDataBrowser.h"

#include "ConvexClient.h"
#include "ConvexEditorJson.h"
#include "ConvexSubscription.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/Base64.h"
#include "Misc/MessageDialog.h"
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

	/// Modal JSON editor. Returns unset on cancel.
	TOptional<FString> ShowJsonDialog(const FText& Title, const FString& InitialText)
	{
		TSharedPtr<FString> Result;

		TSharedPtr<SMultiLineEditableTextBox> Box;
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(640.f, 420.f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		Window->SetContent(
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(8.f)
			[
				SAssignNew(Box, SMultiLineEditableTextBox)
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.Text(FText::FromString(InitialText))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("ConvexEditor", "DialogOk", "OK"))
					.OnClicked_Lambda([&Result, &Box, WeakWindow = TWeakPtr<SWindow>(Window)]()
					{
						Result = MakeShared<FString>(Box->GetText().ToString());
						if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(NSLOCTEXT("ConvexEditor", "DialogCancel", "Cancel"))
					.OnClicked_Lambda([WeakWindow = TWeakPtr<SWindow>(Window)]()
					{
						if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]);

		FSlateApplication::Get().AddModalWindow(
			Window, FGlobalTabmanager::Get()->GetRootWindow());
		return Result.IsValid() ? TOptional<FString>(*Result) : TOptional<FString>();
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
				.Padding(4.f, 0.f, 12.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoadMore", "Load more"))
					.IsEnabled(this, &SConvexDataBrowser::CanLoadMore)
					.OnClicked(this, &SConvexDataBrowser::OnLoadMoreClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddDoc", "Add"))
					.ToolTipText(LOCTEXT("AddDocTip",
						"Insert a document (dev deployments only)"))
					.IsEnabled(this, &SConvexDataBrowser::CanEditSelectedTable)
					.OnClicked(this, &SConvexDataBrowser::OnAddClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("EditDoc", "Edit"))
					.ToolTipText(LOCTEXT("EditDocTip",
						"Replace the selected document (dev deployments only)"))
					.IsEnabled_Lambda([this]()
						{ return CanEditSelectedTable() && HasDocSelection(); })
					.OnClicked(this, &SConvexDataBrowser::OnEditClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DeleteDoc", "Delete"))
					.ToolTipText(LOCTEXT("DeleteDocTip",
						"Delete the selected document (dev deployments only)"))
					.IsEnabled_Lambda([this]()
						{ return CanEditSelectedTable() && HasDocSelection(); })
					.OnClicked(this, &SConvexDataBrowser::OnDeleteClicked)
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
	if (PaginatedSubscription.IsValid())
	{
		// Unsubscribe (never resubscribe) so this stays destructor-safe; any
		// update callbacks still queued become no-ops once the helper stops.
		PaginatedSubscription->Unsubscribe();
		PaginatedSubscription.Reset();
	}
	Snapshot = FConvexPaginatedSnapshot();
	DocItems.Reset();
}

void SConvexDataBrowser::ResetPages()
{
	ClearPages();
	RebuildDocItems();

	UConvexClient* Client = Session->GetClient();
	if (SelectedTable.IsEmpty() || Client == nullptr)
	{
		return;
	}

	// System tables (_file_storage, _scheduled_jobs) are only readable via
	// db.system, which paginatedTableDocuments does not use — the CLI's
	// tableData query handles both kinds (no filter support there).
	const bool bSystemTable = SelectedTable.StartsWith(TEXT("_"));

	TMap<FString, FConvexValue> Args;
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

	PaginatedSubscription = TStrongObjectPtr<UConvexPaginatedSubscription>(
		Client->SubscribePaginatedNative(
			bSystemTable ? TEXT("_system/cli/tableData")
						 : TEXT("_system/frontend/paginatedTableDocuments"),
			Args, PageSize,
			[WeakThis = TWeakPtr<SConvexDataBrowser>(SharedThis(this))](
				const FConvexPaginatedSnapshot& InSnapshot)
			{
				const TSharedPtr<SConvexDataBrowser> This = WeakThis.Pin();
				if (!This.IsValid())
				{
					return;
				}
				This->Snapshot = InSnapshot;
				This->RebuildDocItems();
			}));
}

void SConvexDataBrowser::RebuildDocItems()
{
	DocItems.Reset();
	for (const FConvexValue& Row : Snapshot.Results)
	{
		const TSharedPtr<FDocRow> Doc = MakeShared<FDocRow>();
		TMap<FString, FConvexValue> RowFields;
		if (Row.TryGetObject(RowFields))
		{
			if (const FConvexValue* IdValue = RowFields.Find(TEXT("_id")))
			{
				IdValue->TryGetString(Doc->Id);
			}
			// Filter-validation errors ride inside the page as
			// {error, filter} objects; show them in place.
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
		DocItems.Add(Doc);
	}
	// A failed page (other than the stale-cursor errors the helper resets on
	// itself) surfaces as a trailing error row.
	if (Snapshot.Status == EConvexPaginationStatus::Error)
	{
		const TSharedPtr<FDocRow> Doc = MakeShared<FDocRow>();
		Doc->Id = TEXT("(error)");
		Doc->Preview = Snapshot.Error.ErrorMessage;
		Doc->PrettyJson = Snapshot.Error.ErrorMessage;
		DocItems.Add(Doc);
	}
	if (DocList.IsValid())
	{
		DocList->RequestListRefresh();
	}
}

bool SConvexDataBrowser::CanLoadMore() const
{
	return PaginatedSubscription.IsValid() &&
		Snapshot.Status == EConvexPaginationStatus::CanLoadMore;
}

FReply SConvexDataBrowser::OnLoadMoreClicked()
{
	if (PaginatedSubscription.IsValid())
	{
		PaginatedSubscription->LoadMore(PageSize);
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

bool SConvexDataBrowser::CanEditSelectedTable() const
{
	return Session->CanWrite() && Session->IsConnected() && !SelectedTable.IsEmpty() &&
		!SelectedTable.StartsWith(TEXT("_"));
}

bool SConvexDataBrowser::HasDocSelection() const
{
	if (!DocList.IsValid())
	{
		return false;
	}
	TArray<FDocItem> Selection = DocList->GetSelectedItems();
	return Selection.Num() == 1 && Selection[0].IsValid() &&
		Selection[0]->Id.Len() > 0 && !Selection[0]->Id.StartsWith(TEXT("("));
}

FReply SConvexDataBrowser::OnAddClicked()
{
	const TOptional<FString> Edited = ShowJsonDialog(
		FText::Format(LOCTEXT("AddDocTitle", "Add document to '{0}'"),
			FText::FromString(SelectedTable)),
		TEXT("{\n\t\n}"));
	if (!Edited.IsSet())
	{
		return FReply::Handled();
	}
	bool bParsed = false;
	const FConvexValue Document = FConvexValue::FromWire(*Edited, bParsed);
	TMap<FString, FConvexValue> Fields;
	if (!bParsed || !Document.TryGetObject(Fields))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("AddParseError", "The document must be a JSON object."));
		return FReply::Handled();
	}
	TMap<FString, FConvexValue> Args;
	Args.Add(TEXT("table"), FConvexValue::String(SelectedTable));
	Args.Add(TEXT("documents"), FConvexValue::Array({Document}));
	RunSystemMutation(TEXT("_system/frontend/addDocument"), MoveTemp(Args));
	return FReply::Handled();
}

FReply SConvexDataBrowser::OnEditClicked()
{
	if (!HasDocSelection())
	{
		return FReply::Handled();
	}
	const FDocItem Selected = DocList->GetSelectedItems()[0];
	const TOptional<FString> Edited = ShowJsonDialog(
		FText::Format(LOCTEXT("EditDocTitle", "Replace document {0}"),
			FText::FromString(Selected->Id)),
		Selected->PrettyJson);
	if (!Edited.IsSet())
	{
		return FReply::Handled();
	}
	bool bParsed = false;
	const FConvexValue Document = FConvexValue::FromWire(*Edited, bParsed);
	TMap<FString, FConvexValue> Fields;
	if (!bParsed || !Document.TryGetObject(Fields))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("EditParseError", "The document must be a JSON object."));
		return FReply::Handled();
	}
	// System fields are server-managed; replaceDocument wants the new content
	// only (the id is a separate argument).
	Fields.Remove(TEXT("_id"));
	Fields.Remove(TEXT("_creationTime"));

	TMap<FString, FConvexValue> Args;
	Args.Add(TEXT("id"), FConvexValue::String(Selected->Id));
	Args.Add(TEXT("document"), FConvexValue::Object(Fields));
	RunSystemMutation(TEXT("_system/frontend/replaceDocument"), MoveTemp(Args));
	return FReply::Handled();
}

FReply SConvexDataBrowser::OnDeleteClicked()
{
	if (!HasDocSelection())
	{
		return FReply::Handled();
	}
	const FDocItem Selected = DocList->GetSelectedItems()[0];
	const EAppReturnType::Type Confirm = FMessageDialog::Open(EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("DeleteConfirm", "Delete document {0} from '{1}'? This cannot be undone."),
			FText::FromString(Selected->Id), FText::FromString(SelectedTable)));
	if (Confirm != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	TMap<FString, FConvexValue> Reference;
	Reference.Add(TEXT("id"), FConvexValue::String(Selected->Id));
	Reference.Add(TEXT("tableName"), FConvexValue::String(SelectedTable));

	TMap<FString, FConvexValue> Args;
	// deleteDocuments takes explicit id+table pairs; there is deliberately no
	// path here that can touch more than the one selected document.
	Args.Add(TEXT("toDelete"), FConvexValue::Array({FConvexValue::Object(Reference)}));
	RunSystemMutation(TEXT("_system/frontend/deleteDocuments"), MoveTemp(Args));
	return FReply::Handled();
}

void SConvexDataBrowser::RunSystemMutation(const TCHAR* Path, TMap<FString, FConvexValue> Args)
{
	UConvexClient* Client = Session->GetClient();
	if (Client == nullptr)
	{
		return;
	}
	Client->MutationNative(Path, Args,
		[PathString = FString(Path)](const FConvexResult& Result)
		{
			FString Error;
			if (!Result.bSuccess)
			{
				Error = Result.ErrorMessage;
			}
			else
			{
				// These mutations report failures as {success:false, error}.
				TMap<FString, FConvexValue> Payload;
				if (Result.Value.TryGetObject(Payload))
				{
					bool bOk = true;
					if (const FConvexValue* Success = Payload.Find(TEXT("success")))
					{
						Success->TryGetBool(bOk);
					}
					if (!bOk)
					{
						if (const FConvexValue* ErrorValue = Payload.Find(TEXT("error")))
						{
							ErrorValue->TryGetString(Error);
						}
						if (Error.IsEmpty())
						{
							Error = TEXT("The operation was rejected.");
						}
					}
				}
			}
			if (!Error.IsEmpty())
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						NSLOCTEXT("ConvexEditor", "MutationFailed", "{0} failed:\n{1}"),
						FText::FromString(PathString), FText::FromString(Error)));
			}
			// Success needs no handling: the page subscriptions update live.
		});
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

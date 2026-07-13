// Copyright Potionify. Apache-2.0.

#include "SConvexSchemaPanel.h"

#include "ConvexClient.h"
#include "ConvexEditorJson.h"
#include "ConvexSubscription.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	TSharedRef<SWidget> Section(const FText& Title, TSharedRef<SWidget> Content)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Content
			];
	}
}

void SConvexSchemaPanel::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;
	Session->OnChanged.AddSP(SharedThis(this), &SConvexSchemaPanel::OnSessionChanged);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.Padding(2.f)
			[
				SAssignNew(TableList, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&TableItems)
				.OnGenerateRow(this, &SConvexSchemaPanel::MakeTableRow)
				.OnSelectionChanged_Lambda(
					[this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectTable(Item); })
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SSplitter::Slot()
		.Value(0.78f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			.Padding(8.f, 0.f)
			[
				Section(LOCTEXT("DeclaredSchema", "Declared schema"),
					SAssignNew(DeclaredBox, SMultiLineEditableTextBox)
					.Font(FAppStyle::GetFontStyle("MonospacedText"))
					.IsReadOnly(true)
					.AutoWrapText(true))
			]

			+ SScrollBox::Slot()
			.Padding(8.f, 0.f)
			[
				Section(LOCTEXT("LiveIndexes", "Indexes (live)"),
					SAssignNew(IndexesBox, SMultiLineEditableTextBox)
					.Font(FAppStyle::GetFontStyle("MonospacedText"))
					.IsReadOnly(true)
					.AutoWrapText(true))
			]

			+ SScrollBox::Slot()
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InferredShape", "Inferred shape (shapes2)"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("RefreshShapes", "Refresh"))
						.OnClicked_Lambda([this]()
						{
							RefreshShapes(/*bForceFetch=*/true);
							return FReply::Handled();
						})
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ShapeBox, SMultiLineEditableTextBox)
					.Font(FAppStyle::GetFontStyle("MonospacedText"))
					.IsReadOnly(true)
					.AutoWrapText(true)
				]
			]
		]
	];

	OnSessionChanged();
}

SConvexSchemaPanel::~SConvexSchemaPanel()
{
	++Generation;
	if (IndexesSubscription.IsValid())
	{
		IndexesSubscription->Unsubscribe();
		IndexesSubscription.Reset();
	}
}

void SConvexSchemaPanel::OnSessionChanged()
{
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
	if (bChanged)
	{
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
				TableList->SetSelection(TableItems[0], ESelectInfo::Direct);
				SelectTable(TableItems[0]);
			}
		}
	}
	RefreshDeclaredSchema();
}

void SConvexSchemaPanel::SelectTable(TSharedPtr<FString> Table)
{
	const FString NewTable = Table.IsValid() ? *Table : FString();
	if (NewTable == SelectedTable)
	{
		return;
	}
	SelectedTable = NewTable;
	RefreshDeclaredSchema();
	SubscribeIndexes();
	RefreshShapes(/*bForceFetch=*/ShapesJson.IsEmpty());
}

void SConvexSchemaPanel::RefreshDeclaredSchema()
{
	if (!DeclaredBox.IsValid())
	{
		return;
	}
	if (SelectedTable.IsEmpty())
	{
		DeclaredBox->SetText(LOCTEXT("PickTableSchema", "Select a table"));
		return;
	}
	const FString& SchemaJson = Session->GetActiveSchemaJson();
	if (SchemaJson.IsEmpty())
	{
		DeclaredBox->SetText(
			LOCTEXT("NoSchema", "(no declared schema on this deployment)"));
		return;
	}

	// SchemaJson: {tables: [{tableName, indexes, searchIndexes, documentType}]}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemaJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		DeclaredBox->SetText(LOCTEXT("SchemaParseError", "(failed to parse schema JSON)"));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
	if (Root->TryGetArrayField(TEXT("tables"), Tables))
	{
		for (const TSharedPtr<FJsonValue>& TableValue : *Tables)
		{
			const TSharedPtr<FJsonObject>* TableObject = nullptr;
			if (!TableValue->TryGetObject(TableObject))
			{
				continue;
			}
			FString Name;
			(*TableObject)->TryGetStringField(TEXT("tableName"), Name);
			if (Name != SelectedTable)
			{
				continue;
			}
			FString Pretty;
			const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Pretty);
			FJsonSerializer::Serialize(TableObject->ToSharedRef(), Writer);
			DeclaredBox->SetText(FText::FromString(Pretty));
			return;
		}
	}
	DeclaredBox->SetText(FText::Format(
		LOCTEXT("TableNotInSchema", "(table '{0}' is not declared in the schema)"),
		FText::FromString(SelectedTable)));
}

void SConvexSchemaPanel::SubscribeIndexes()
{
	if (IndexesSubscription.IsValid())
	{
		IndexesSubscription->Unsubscribe();
		IndexesSubscription.Reset();
	}
	if (IndexesBox.IsValid())
	{
		IndexesBox->SetText(FText::GetEmpty());
	}
	UConvexClient* Client = Session->GetClient();
	if (Client == nullptr || SelectedTable.IsEmpty())
	{
		return;
	}
	const uint64 Gen = ++Generation;

	// NOTE: the component argument is named tableNamespace and is REQUIRED
	// (null = root); calling it componentId would re-dispatch the query into
	// the component, which lacks the _index system tables.
	TMap<FString, FConvexValue> Args;
	Args.Add(TEXT("tableName"), FConvexValue::String(SelectedTable));
	Args.Add(TEXT("tableNamespace"), FConvexValue::Null());

	IndexesSubscription = TStrongObjectPtr<UConvexSubscription>(Client->SubscribeNative(
		TEXT("_system/frontend/indexes"), Args,
		[WeakThis = TWeakPtr<SConvexSchemaPanel>(SharedThis(this)), Gen](
			const FConvexResult& Result)
		{
			const TSharedPtr<SConvexSchemaPanel> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen || !This->IndexesBox.IsValid())
			{
				return;
			}
			if (!Result.bSuccess)
			{
				This->IndexesBox->SetText(
					FText::FromString(TEXT("Error: ") + Result.ErrorMessage));
				return;
			}
			// [{name, staged?, fields, backfill: {state, stats?}}]
			FString Text;
			TArray<FConvexValue> Indexes;
			if (Result.Value.TryGetArray(Indexes))
			{
				for (const FConvexValue& Index : Indexes)
				{
					TMap<FString, FConvexValue> Fields;
					if (!Index.TryGetObject(Fields))
					{
						continue;
					}
					FString Name, State;
					if (const FConvexValue* NameValue = Fields.Find(TEXT("name")))
					{
						NameValue->TryGetString(Name);
					}
					if (const FConvexValue* Backfill = Fields.Find(TEXT("backfill")))
					{
						TMap<FString, FConvexValue> BackfillFields;
						if (Backfill->TryGetObject(BackfillFields))
						{
							if (const FConvexValue* StateValue =
									BackfillFields.Find(TEXT("state")))
							{
								StateValue->TryGetString(State);
							}
						}
					}
					FString FieldsText;
					if (const FConvexValue* FieldsValue = Fields.Find(TEXT("fields")))
					{
						bool bEncoded = false;
						FieldsText = FieldsValue->ToWire(bEncoded);
					}
					Text += FString::Printf(TEXT("%s  %s  %s\n"), *Name,
						State == TEXT("done") ? TEXT("[ready]")
											  : *FString::Printf(TEXT("[%s]"), *State),
						*FieldsText);
				}
			}
			This->IndexesBox->SetText(FText::FromString(
				Text.IsEmpty() ? TEXT("(no indexes on this table)") : Text));
		}));
}

void SConvexSchemaPanel::RefreshShapes(bool bForceFetch)
{
	if (!ShapeBox.IsValid())
	{
		return;
	}
	if (SelectedTable.IsEmpty())
	{
		ShapeBox->SetText(FText::GetEmpty());
		return;
	}

	const auto ShowSelected = [this]()
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ShapesJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			ShapeBox->SetText(LOCTEXT("ShapesParseError", "(failed to parse shapes)"));
			return;
		}
		const TSharedPtr<FJsonObject>* Shape = nullptr;
		if (Root->TryGetObjectField(SelectedTable, Shape))
		{
			FString Pretty;
			const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Pretty);
			FJsonSerializer::Serialize(Shape->ToSharedRef(), Writer);
			ShapeBox->SetText(FText::FromString(Pretty));
		}
		else
		{
			ShapeBox->SetText(LOCTEXT("NoShape", "(no inferred shape for this table)"));
		}
	};

	if (!bForceFetch && !ShapesJson.IsEmpty())
	{
		ShowSelected();
		return;
	}
	if (bShapesFetchInFlight)
	{
		return;
	}
	bShapesFetchInFlight = true;
	ShapeBox->SetText(LOCTEXT("ShapesLoading", "Loading..."));
	Session->FetchShapes(
		[WeakThis = TWeakPtr<SConvexSchemaPanel>(SharedThis(this)), ShowSelected](
			bool bSuccess, FString Json)
		{
			const TSharedPtr<SConvexSchemaPanel> This = WeakThis.Pin();
			if (!This.IsValid())
			{
				return;
			}
			This->bShapesFetchInFlight = false;
			if (!bSuccess)
			{
				This->ShapeBox->SetText(
					FText::FromString(TEXT("Error fetching shapes: ") + Json));
				return;
			}
			This->ShapesJson = MoveTemp(Json);
			ShowSelected();
		});
}

TSharedRef<ITableRow> SConvexSchemaPanel::MakeTableRow(
	TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FString>>, Owner)
	[
		SNew(STextBlock)
		.Text(FText::FromString(*Item))
		.Margin(FMargin(4.f, 3.f))
	];
}

#undef LOCTEXT_NAMESPACE

// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexAdminSession.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;
class UConvexSubscription;

/**
 * The schema tab: per-table view of the declared schema (getSchemas), live
 * index state incl. backfill progress (the indexes system query), and the
 * inferred shape for schemaless data (GET /api/shapes2).
 */
class SConvexSchemaPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexSchemaPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);
	virtual ~SConvexSchemaPanel() override;

private:
	void OnSessionChanged();
	void SelectTable(TSharedPtr<FString> Table);
	void RefreshDeclaredSchema();
	void SubscribeIndexes();
	void RefreshShapes(bool bForceFetch);
	TSharedRef<ITableRow> MakeTableRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);

	TSharedPtr<FConvexAdminSession> Session;

	TArray<TSharedPtr<FString>> TableItems;
	TSharedPtr<SListView<TSharedPtr<FString>>> TableList;
	FString SelectedTable;

	TSharedPtr<SMultiLineEditableTextBox> DeclaredBox;
	TSharedPtr<SMultiLineEditableTextBox> IndexesBox;
	TSharedPtr<SMultiLineEditableTextBox> ShapeBox;

	TStrongObjectPtr<UConvexSubscription> IndexesSubscription;

	/// Cached /api/shapes2 response (all tables); empty until fetched.
	FString ShapesJson;
	bool bShapesFetchInFlight = false;
	uint64 Generation = 0;
};

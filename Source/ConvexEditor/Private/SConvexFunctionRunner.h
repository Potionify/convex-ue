// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexAdminSession.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;
class SSearchBox;

/**
 * The function-runner panel: a filterable list of every deployed function
 * (from the live apiSpec subscription) and an args-editor/run/result pane.
 */
class SConvexFunctionRunner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexFunctionRunner) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);

private:
	using FItem = TSharedPtr<FConvexFunctionSpec>;

	void RebuildItems();
	void ApplyFilter();
	TSharedRef<ITableRow> MakeRow(FItem Item, const TSharedRef<STableViewBase>& Owner);
	void OnSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo);
	FReply OnRunClicked();
	bool CanRun() const;
	FText GetSelectedHeaderText() const;
	FText GetRunButtonText() const;

	TSharedPtr<FConvexAdminSession> Session;
	TArray<FItem> AllItems;
	TArray<FItem> FilteredItems;
	FString FilterString;

	TSharedPtr<SListView<FItem>> ListView;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SMultiLineEditableTextBox> ArgsBox;
	TSharedPtr<SMultiLineEditableTextBox> ResultBox;

	FItem Selected;
	/// Preserves per-function args edits across selection changes.
	TMap<FString, FString> ArgsCache;
	bool bRunning = false;
	/// Bumped per Run click so a stale callback cannot clobber a newer result.
	uint64 RunCounter = 0;
};

// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexAdminSession.h"
#include "ConvexSubscription.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;

/**
 * The data tab: live table browser. Tables come from getTableMapping; rows
 * from paginatedTableDocuments, one live subscription per loaded page (the
 * dashboard's usePaginatedQuery pattern — the query journal keeps page
 * boundaries stable while data changes underneath). Read-only in Phase 2;
 * Phase 3 adds dev-gated document CRUD on top.
 */
class SConvexDataBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexDataBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);
	virtual ~SConvexDataBrowser() override;

private:
	struct FDocRow
	{
		FString Id;
		FString Preview;     // single-line wire JSON, truncated
		FString PrettyJson;  // full document, pretty-printed
	};
	using FDocItem = TSharedPtr<FDocRow>;

	/// One live page of documents.
	struct FPage
	{
		TStrongObjectPtr<UConvexSubscription> Subscription;
		TArray<FDocItem> Docs;
		FString ContinueCursor;
		bool bIsDone = false;
		bool bLoaded = false;
	};

	void OnSessionChanged();
	void SelectTable(TSharedPtr<FString> Table);
	/// Unsubscribe and drop all pages (safe in the destructor).
	void ClearPages();
	/// ClearPages + start over from page 0 for the current table.
	void ResetPages();
	void SubscribePage(int32 PageIndex, const FString& Cursor);
	void RebuildDocItems();
	FString FiltersArgument() const;

	TSharedRef<ITableRow> MakeTableRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> MakeDocRow(FDocItem Item, const TSharedRef<STableViewBase>& Owner);
	FReply OnLoadMoreClicked();
	bool CanLoadMore() const;
	FText GetStatusText() const;

	// --- Dev-gated CRUD (Phase 3) --------------------------------------
	bool CanEditSelectedTable() const;
	bool HasDocSelection() const;
	FReply OnAddClicked();
	FReply OnEditClicked();
	FReply OnDeleteClicked();
	/// Run a _system/frontend mutation, surfacing transport errors AND the
	/// {success:false, error} payloads those mutations return on failure.
	void RunSystemMutation(const TCHAR* Path, TMap<FString, FConvexValue> Args);

	TSharedPtr<FConvexAdminSession> Session;

	TArray<TSharedPtr<FString>> TableItems;
	TSharedPtr<SListView<TSharedPtr<FString>>> TableList;
	FString SelectedTable;

	TArray<TSharedPtr<FPage>> Pages;
	TArray<FDocItem> DocItems;
	TSharedPtr<SListView<FDocItem>> DocList;
	TSharedPtr<SMultiLineEditableTextBox> DetailBox;

	TStrongObjectPtr<UConvexSubscription> RowCountSubscription;
	int64 RowCount = -1;  // -1 = unknown

	bool bAscending = false;
	/// Bumped on every reset so callbacks from replaced pages are ignored.
	uint64 PagesGeneration = 0;
};

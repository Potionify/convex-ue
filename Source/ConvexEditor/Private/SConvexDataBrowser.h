// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexAdminSession.h"
#include "ConvexPaginatedSubscription.h"
#include "ConvexSubscription.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;

/**
 * The data tab: live table browser. Tables come from getTableMapping; rows
 * from paginatedTableDocuments through UConvexPaginatedSubscription — the
 * same usePaginatedQuery state machine games use (one live subscription per
 * loaded page; query journals keep page boundaries stable while data changes
 * underneath). Includes dev-gated document CRUD.
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

	void OnSessionChanged();
	void SelectTable(TSharedPtr<FString> Table);
	/// Unsubscribe and drop all pages (safe in the destructor: never
	/// resubscribes, so no SharedThis on a dying widget).
	void ClearPages();
	/// ClearPages + start over from a fresh first page for the current table.
	void ResetPages();
	/// Rebuild the flat row list from the latest pagination snapshot.
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

	/// All loaded pages, as one live paginated subscription. Recreated when
	/// the table or sort order changes; the helper itself handles page
	/// chaining, seam stability, and stale-cursor resets.
	TStrongObjectPtr<UConvexPaginatedSubscription> PaginatedSubscription;
	FConvexPaginatedSnapshot Snapshot;

	TArray<FDocItem> DocItems;
	TSharedPtr<SListView<FDocItem>> DocList;
	TSharedPtr<SMultiLineEditableTextBox> DetailBox;

	TStrongObjectPtr<UConvexSubscription> RowCountSubscription;
	int64 RowCount = -1;  // -1 = unknown

	bool bAscending = false;
};

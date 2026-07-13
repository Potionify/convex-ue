// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexAdminSession.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;

/**
 * The traffic tab: a local inspector of every Convex websocket frame in the
 * process — the editor's own admin session AND any PIE game clients — via
 * the ConvexWireTap transport hook. Purely client-side; nothing is sent to
 * the deployment. The tap is enabled only while this panel exists.
 */
class SConvexTrafficPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexTrafficPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);
	virtual ~SConvexTrafficPanel() override;

private:
	struct FFrameRow
	{
		FString Time;      // HH:MM:SS.mmm local
		bool bOutgoing = false;
		FString Type;      // wire message "type" field
		int32 Bytes = 0;
		FString Url;       // connection tag
		FString Text;      // full frame (truncated past a cap)
	};
	using FFrameItem = TSharedPtr<FFrameRow>;

	void OnFrame(int32 Direction, const FString& Url, const FString& Text);
	void ApplyFilter();
	TSharedRef<ITableRow> MakeRow(FFrameItem Item, const TSharedRef<STableViewBase>& Owner);

	TSharedPtr<FConvexAdminSession> Session;

	TArray<FFrameItem> AllItems;
	TArray<FFrameItem> FilteredItems;
	FString FilterString;
	bool bPaused = false;

	TSharedPtr<SListView<FFrameItem>> ListView;
	TSharedPtr<SMultiLineEditableTextBox> DetailBox;
	FDelegateHandle TapHandle;
};

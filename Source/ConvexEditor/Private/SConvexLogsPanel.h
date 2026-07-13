// Copyright Potionify. Apache-2.0.

#pragma once

#include "Containers/Ticker.h"
#include "ConvexAdminSession.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FJsonValue;
class SMultiLineEditableTextBox;

/**
 * The logs tab: tails deployment function logs via the cursor-based
 * long-poll endpoint GET /api/stream_function_logs. The server holds each
 * request up to 60 s (empty keep-alives carry the cursor forward); errors
 * back off exponentially. History before the tab opened is dropped, exactly
 * like the dashboard and `npx convex logs`. The Convex-Client header
 * identifies as a dashboard client so the backend sends structured log
 * lines instead of "[LEVEL] message" strings.
 */
class SConvexLogsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexLogsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);
	virtual ~SConvexLogsPanel() override;

private:
	struct FLogRow
	{
		FString Time;        // HH:MM:SS local
		FString Kind;        // Completion | Progress
		FString UdfType;     // Query/Mutation/Action/HttpAction
		FString Identifier;  // module:function
		FString Summary;     // duration + first line / error
		FString Detail;      // all lines + error, multi-line
		bool bError = false;
	};
	using FLogItem = TSharedPtr<FLogRow>;

	void OnSessionChanged();
	void StartStream();
	void StopStream();
	void Poll();
	void SchedulePoll(float DelaySeconds);
	void HandleResponse(bool bOk, int32 Code, const FString& Body);
	void AppendEntries(const TArray<TSharedPtr<FJsonValue>>& Entries);
	void ApplyFilter();

	TSharedRef<ITableRow> MakeRow(FLogItem Item, const TSharedRef<STableViewBase>& Owner);
	FText GetStatusText() const;

	TSharedPtr<FConvexAdminSession> Session;

	TArray<FLogItem> AllItems;
	TArray<FLogItem> FilteredItems;
	FString FilterString;
	TSharedPtr<SListView<FLogItem>> ListView;
	TSharedPtr<SMultiLineEditableTextBox> DetailBox;

	FString StreamUrl;  // deployment the running stream belongs to
	double Cursor = 0.0;
	bool bFirstPoll = true;
	bool bPaused = false;
	bool bPollInFlight = false;
	int32 FailureCount = 0;
	FString StatusNote;
	FTSTicker::FDelegateHandle RetryTicker;
	/// Bumped on stop/restart so stale poll callbacks are dropped.
	uint64 Generation = 0;
};

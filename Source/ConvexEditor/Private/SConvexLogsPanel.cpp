// Copyright Potionify. Apache-2.0.

#include "SConvexLogsPanel.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	constexpr int32 MaxLogRows = 10000;

	FString FormatTimestamp(double UnixSeconds)
	{
		const FDateTime Utc = FDateTime::FromUnixTimestamp((int64)UnixSeconds);
		return Utc.ToString(TEXT("%H:%M:%S"));
	}

	/// logLines entries are structured objects for dashboard/cli clients,
	/// plain "[LEVEL] message" strings otherwise; accept both.
	void ExtractLogLines(const TSharedPtr<FJsonObject>& Entry, TArray<FString>& OutLines)
	{
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		if (!Entry->TryGetArrayField(TEXT("logLines"), Lines))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Line : *Lines)
		{
			FString Plain;
			if (Line->TryGetString(Plain))
			{
				OutLines.Add(MoveTemp(Plain));
				continue;
			}
			const TSharedPtr<FJsonObject>* Structured = nullptr;
			if (Line->TryGetObject(Structured))
			{
				FString Level = TEXT("LOG");
				(*Structured)->TryGetStringField(TEXT("level"), Level);
				const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
				FString Joined;
				if ((*Structured)->TryGetArrayField(TEXT("messages"), Messages))
				{
					for (const TSharedPtr<FJsonValue>& Message : *Messages)
					{
						FString Text;
						if (Message->TryGetString(Text))
						{
							if (!Joined.IsEmpty())
							{
								Joined += TEXT(" ");
							}
							Joined += Text;
						}
					}
				}
				OutLines.Add(FString::Printf(TEXT("[%s] %s"), *Level, *Joined));
			}
		}
	}
}

void SConvexLogsPanel::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;
	Session->OnChanged.AddSP(SharedThis(this), &SConvexLogsPanel::OnSessionChanged);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 4.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("FilterLogs", "Filter by function or text..."))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					FilterString = Text.ToString();
					ApplyFilter();
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SConvexLogsPanel::GetStatusText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]()
					{ return bPaused ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bPaused = State == ECheckBoxState::Checked;
					if (!bPaused)
					{
						StartStream();
					}
					else
					{
						StopStream();
					}
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PauseLogs", "Pause"))
					.Margin(FMargin(4.f, 2.f))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearLogs", "Clear"))
				.OnClicked_Lambda([this]()
				{
					AllItems.Reset();
					ApplyFilter();
					if (DetailBox.IsValid())
					{
						DetailBox->SetText(FText::GetEmpty());
					}
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(2.f)
				[
					SAssignNew(ListView, SListView<FLogItem>)
					.ListItemsSource(&FilteredItems)
					.OnGenerateRow(this, &SConvexLogsPanel::MakeRow)
					.OnSelectionChanged_Lambda([this](FLogItem Item, ESelectInfo::Type)
					{
						if (DetailBox.IsValid())
						{
							DetailBox->SetText(Item.IsValid() ? FText::FromString(Item->Detail)
															  : FText::GetEmpty());
						}
					})
					.SelectionMode(ESelectionMode::Single)
				]
			]

			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SAssignNew(DetailBox, SMultiLineEditableTextBox)
				.Font(FAppStyle::GetFontStyle("MonospacedText"))
				.IsReadOnly(true)
			]
		]
	];

	OnSessionChanged();
}

SConvexLogsPanel::~SConvexLogsPanel()
{
	StopStream();
}

void SConvexLogsPanel::OnSessionChanged()
{
	const FString Url = Session->GetConfig().IsValid() ? Session->GetConfig().DeploymentUrl
													   : FString();
	if (Url != StreamUrl)
	{
		StopStream();
		StreamUrl = Url;
		Cursor = 0.0;
		bFirstPoll = true;
		if (!bPaused)
		{
			StartStream();
		}
	}
	else if (!Url.IsEmpty() && !bPaused && !bPollInFlight && !RetryTicker.IsValid())
	{
		StartStream();
	}
}

void SConvexLogsPanel::StartStream()
{
	if (StreamUrl.IsEmpty() || bPollInFlight)
	{
		return;
	}
	FailureCount = 0;
	Poll();
}

void SConvexLogsPanel::StopStream()
{
	++Generation;
	bPollInFlight = false;
	if (RetryTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetryTicker);
		RetryTicker.Reset();
	}
}

void SConvexLogsPanel::SchedulePoll(float DelaySeconds)
{
	const uint64 Gen = Generation;
	RetryTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakThis = TWeakPtr<SConvexLogsPanel>(SharedThis(this)), Gen](float) -> bool
		{
			if (const TSharedPtr<SConvexLogsPanel> This = WeakThis.Pin();
				This.IsValid() && This->Generation == Gen)
			{
				This->RetryTicker.Reset();
				This->Poll();
			}
			return false;  // one-shot
		}),
		DelaySeconds);
}

void SConvexLogsPanel::Poll()
{
	if (StreamUrl.IsEmpty() || bPaused || bPollInFlight)
	{
		return;
	}
	bPollInFlight = true;
	const uint64 Gen = Generation;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(
		TEXT("%s/api/stream_function_logs?cursor=%f"), *StreamUrl, Cursor));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Convex ") + Session->GetConfig().AdminKey);
	// Present as a dashboard client: that's what unlocks structured logLines.
	Request->SetHeader(TEXT("Convex-Client"), TEXT("dashboard-0.0.0"));
	// The server holds up to 60 s; leave headroom.
	Request->SetTimeout(90.0f);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis = TWeakPtr<SConvexLogsPanel>(SharedThis(this)), Gen](
			FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
		{
			const TSharedPtr<SConvexLogsPanel> This = WeakThis.Pin();
			if (!This.IsValid() || This->Generation != Gen)
			{
				return;
			}
			This->bPollInFlight = false;
			This->HandleResponse(bOk && Response.IsValid(),
				Response.IsValid() ? Response->GetResponseCode() : 0,
				Response.IsValid() ? Response->GetContentAsString() : FString());
		});
	Request->ProcessRequest();
}

void SConvexLogsPanel::HandleResponse(bool bOk, int32 Code, const FString& Body)
{
	if (!bOk || Code != 200)
	{
		++FailureCount;
		StatusNote = FString::Printf(TEXT("stream error (HTTP %d), retrying"), Code);
		// 0.5 s * 2^n, capped at 10 s — mirrors the dashboard's backoff.
		const float Delay = FMath::Min(0.5f * FMath::Pow(2.f, (float)FailureCount), 10.f);
		SchedulePoll(Delay);
		return;
	}
	FailureCount = 0;
	StatusNote.Reset();

	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
	{
		double NewCursor = Cursor;
		Json->TryGetNumberField(TEXT("newCursor"), NewCursor);
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		const bool bHasEntries = Json->TryGetArrayField(TEXT("entries"), Entries);
		if (bFirstPoll)
		{
			// Drop history: tail from now on (dashboard/CLI behavior).
			bFirstPoll = false;
		}
		else if (bHasEntries && Entries->Num() > 0)
		{
			AppendEntries(*Entries);
		}
		Cursor = NewCursor;
	}
	Poll();  // immediate re-poll; the server's hold provides backpressure
}

void SConvexLogsPanel::AppendEntries(const TArray<TSharedPtr<FJsonValue>>& Entries)
{
	for (const TSharedPtr<FJsonValue>& EntryValue : Entries)
	{
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!EntryValue->TryGetObject(Entry))
		{
			continue;
		}
		const TSharedPtr<FLogRow> Row = MakeShared<FLogRow>();
		(*Entry)->TryGetStringField(TEXT("kind"), Row->Kind);
		(*Entry)->TryGetStringField(TEXT("udfType"), Row->UdfType);
		(*Entry)->TryGetStringField(TEXT("identifier"), Row->Identifier);
		Row->Identifier.ReplaceInline(TEXT(".js:"), TEXT(":"));

		double Timestamp = 0.0;
		(*Entry)->TryGetNumberField(TEXT("timestamp"), Timestamp);
		Row->Time = FormatTimestamp(Timestamp);

		FString Error;
		(*Entry)->TryGetStringField(TEXT("error"), Error);
		Row->bError = !Error.IsEmpty();

		double ExecutionTime = 0.0;
		const bool bHasDuration =
			(*Entry)->TryGetNumberField(TEXT("executionTime"), ExecutionTime);

		TArray<FString> Lines;
		ExtractLogLines(*Entry, Lines);

		FString Summary;
		if (bHasDuration)
		{
			Summary = FString::Printf(TEXT("%.0f ms"), ExecutionTime * 1000.0);
		}
		if (Row->bError)
		{
			Summary += (Summary.IsEmpty() ? TEXT("") : TEXT("  ")) + Error;
		}
		else if (Lines.Num() > 0)
		{
			Summary += (Summary.IsEmpty() ? TEXT("") : TEXT("  ")) + Lines[0];
		}
		Row->Summary = MoveTemp(Summary);

		Row->Detail = FString::Printf(TEXT("%s %s %s\n"), *Row->Time, *Row->UdfType,
			*Row->Identifier);
		for (const FString& Line : Lines)
		{
			Row->Detail += Line + TEXT("\n");
		}
		if (Row->bError)
		{
			Row->Detail += TEXT("\nError: ") + Error;
		}

		AllItems.Add(Row);
	}
	if (AllItems.Num() > MaxLogRows)
	{
		AllItems.RemoveAt(0, AllItems.Num() - MaxLogRows);
	}
	ApplyFilter();
}

void SConvexLogsPanel::ApplyFilter()
{
	FilteredItems.Reset();
	for (const FLogItem& Item : AllItems)
	{
		if (FilterString.IsEmpty() || Item->Identifier.Contains(FilterString) ||
			Item->Summary.Contains(FilterString) || Item->Detail.Contains(FilterString))
		{
			FilteredItems.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
		if (FilteredItems.Num() > 0)
		{
			ListView->RequestScrollIntoView(FilteredItems.Last());
		}
	}
}

TSharedRef<ITableRow> SConvexLogsPanel::MakeRow(FLogItem Item, const TSharedRef<STableViewBase>& Owner)
{
	const FLinearColor Color = Item->bError
		? FLinearColor(0.9f, 0.4f, 0.35f)
		: (Item->Kind == TEXT("Progress") ? FLinearColor(0.6f, 0.6f, 0.6f)
										  : FLinearColor::White);
	return SNew(STableRow<FLogItem>, Owner)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.f, 2.f, 8.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Time))
			.Font(FAppStyle::GetFontStyle("MonospacedText"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 2.f, 8.f, 2.f)
		[
			SNew(SBox)
			.WidthOverride(240.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Identifier))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.ColorAndOpacity(FSlateColor(Color))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Summary))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(Item->bError ? FSlateColor(Color)
										  : FSlateColor::UseSubduedForeground())
		]
	];
}

FText SConvexLogsPanel::GetStatusText() const
{
	if (StreamUrl.IsEmpty())
	{
		return LOCTEXT("LogsNotConfigured", "Not connected");
	}
	if (bPaused)
	{
		return LOCTEXT("LogsPaused", "Paused");
	}
	if (!StatusNote.IsEmpty())
	{
		return FText::FromString(StatusNote);
	}
	return FText::Format(LOCTEXT("LogsTailing", "Tailing ({0} entries)"), AllItems.Num());
}

#undef LOCTEXT_NAMESPACE

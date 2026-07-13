// Copyright Potionify. Apache-2.0.

// Paginated-subscription tests. Convex.Paginated.SnapshotMapping is offline
// (native -> UE snapshot conversion); Convex.Paginated.Live drives a real
// UConvexPaginatedSubscription against the integration backend (page
// chaining, exhaustion, live inserts) and self-skips when it is down.

#if WITH_DEV_AUTOMATION_TESTS

#include "ConvexClient.h"
#include "ConvexPaginatedSubscription.h"
#include "ConvexValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AutomationTest.h"

#include <convex/error.h>
#include <convex/paginated.h>
#include <convex/value.h>

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexPaginatedSnapshotMapping, "Convex.Paginated.SnapshotMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexPaginatedSnapshotMapping::RunTest(const FString& Parameters)
{
	// Success snapshot: items, status, no error.
	convex::paginated_snapshot Native;
	Native.results.emplace_back(convex::value("a"));
	Native.results.emplace_back(convex::value(std::int64_t{42}));
	Native.status = convex::pagination_status::can_load_more;

	FConvexPaginatedSnapshot Snapshot = FConvexPaginatedSnapshot::FromNative(Native);
	TestEqual(TEXT("two results"), Snapshot.Results.Num(), 2);
	FString First;
	TestTrue(TEXT("first result is a string"), Snapshot.Results[0].TryGetString(First));
	TestEqual(TEXT("first result value"), First, FString(TEXT("a")));
	int64 Second = 0;
	TestTrue(TEXT("second result is int64"), Snapshot.Results[1].TryGetInt64(Second));
	TestEqual(TEXT("second result value"), Second, static_cast<int64>(42));
	TestEqual(TEXT("status maps"), Snapshot.Status, EConvexPaginationStatus::CanLoadMore);
	TestFalse(TEXT("not loading"), Snapshot.bIsLoading);
	TestFalse(TEXT("no error"), Snapshot.Error.bSuccess || !Snapshot.Error.ErrorMessage.IsEmpty());

	// Every status maps 1:1 (and is_loading only for the loading pair).
	const TTuple<convex::pagination_status, EConvexPaginationStatus, bool> Cases[] = {
		{convex::pagination_status::loading_first_page, EConvexPaginationStatus::LoadingFirstPage, true},
		{convex::pagination_status::can_load_more, EConvexPaginationStatus::CanLoadMore, false},
		{convex::pagination_status::loading_more, EConvexPaginationStatus::LoadingMore, true},
		{convex::pagination_status::exhausted, EConvexPaginationStatus::Exhausted, false},
		{convex::pagination_status::error, EConvexPaginationStatus::Error, false},
	};
	for (const auto& Case : Cases)
	{
		convex::paginated_snapshot S;
		S.status = Case.Get<0>();
		const FConvexPaginatedSnapshot Mapped = FConvexPaginatedSnapshot::FromNative(S);
		TestEqual(TEXT("status mapping"), Mapped.Status, Case.Get<1>());
		TestEqual(TEXT("is-loading mapping"), Mapped.bIsLoading, Case.Get<2>());
	}

	// Error snapshot carries the failing page's result.
	convex::paginated_snapshot ErrorNative;
	ErrorNative.status = convex::pagination_status::error;
	ErrorNative.error = convex::function_result::error("boom");
	const FConvexPaginatedSnapshot ErrorSnapshot = FConvexPaginatedSnapshot::FromNative(ErrorNative);
	TestEqual(TEXT("error status"), ErrorSnapshot.Status, EConvexPaginationStatus::Error);
	TestFalse(TEXT("error result not success"), ErrorSnapshot.Error.bSuccess);
	TestEqual(TEXT("error message"), ErrorSnapshot.Error.ErrorMessage, FString(TEXT("boom")));

	return true;
}

// ---------------------------------------------------------------------------

namespace
{

FString PaginatedBackendUrl()
{
	FString Url = FPlatformMisc::GetEnvironmentVariable(TEXT("CONVEX_LOCAL_URL"));
	return Url.IsEmpty() ? TEXT("http://127.0.0.1:3210") : Url;
}

struct FConvexPaginatedLiveState
{
	FAutomationTestBase* Test = nullptr;
	FString Url;
	FString Channel;
	TWeakObjectPtr<UConvexClient> Client;
	TWeakObjectPtr<UConvexPaginatedSubscription> Subscription;

	bool bProbeDone = false;
	bool bProbeOk = false;

	int32 Phase = 0;
	double PhaseStartSeconds = 0.0;

	int32 SendsDone = 0;
	FConvexPaginatedSnapshot Latest;
	int32 Updates = 0;

	TArray<FString> Bodies() const
	{
		TArray<FString> Out;
		for (const FConvexValue& Doc : Latest.Results)
		{
			TMap<FString, FConvexValue> Fields;
			FString Body;
			if (Doc.TryGetObject(Fields) && Fields.FindRef(TEXT("body")).TryGetString(Body))
			{
				Out.Add(Body);
			}
		}
		return Out;
	}
};

}  // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConvexPaginatedLiveFlow,
	TSharedPtr<FConvexPaginatedLiveState>, State);

bool FConvexPaginatedLiveFlow::Update()
{
	FAutomationTestBase* Test = State->Test;
	constexpr double PhaseTimeoutSeconds = 30.0;
	const double Now = FPlatformTime::Seconds();
	if (State->PhaseStartSeconds == 0.0)
	{
		State->PhaseStartSeconds = Now;
	}
	if (Now - State->PhaseStartSeconds > PhaseTimeoutSeconds)
	{
		Test->AddError(FString::Printf(TEXT("Convex paginated live test timed out in phase %d"),
			State->Phase));
		if (UConvexClient* Client = State->Client.Get())
		{
			Client->Shutdown();
			Client->RemoveFromRoot();
		}
		return true;
	}

	const auto AdvanceTo = [this, Now](int32 NextPhase) {
		State->Phase = NextPhase;
		State->PhaseStartSeconds = Now;
	};

	if (State->Phase == 0)  // Reachability probe.
	{
		if (!State->bProbeDone)
		{
			return false;
		}
		if (!State->bProbeOk)
		{
			Test->AddWarning(FString::Printf(
				TEXT("Skipping Convex.Paginated.Live: no backend at %s (start it via "
					 "convex-cpp/integration/backend: docker compose up -d)"),
				*State->Url));
			return true;
		}
		UConvexClient* NewClient = NewObject<UConvexClient>();
		NewClient->AddToRoot();
		NewClient->Initialize(State->Url);
		State->Client = NewClient;
		AdvanceTo(1);
		return false;
	}

	UConvexClient* Client = State->Client.Get();
	if (Client == nullptr)
	{
		Test->AddError(TEXT("Convex client was garbage collected mid-test"));
		return true;
	}

	switch (State->Phase)
	{
	case 1:  // Connected: seed five messages (one client => ordered execution).
	{
		if (Client->GetConnectionState() != EConvexConnectionState::Connected)
		{
			return false;
		}
		TSharedPtr<FConvexPaginatedLiveState> S = State;
		for (int32 i = 1; i <= 5; ++i)
		{
			TMap<FString, FConvexValue> Args;
			Args.Add(TEXT("channel"), FConvexValue::String(State->Channel));
			Args.Add(TEXT("author"), FConvexValue::String(TEXT("ue")));
			Args.Add(TEXT("body"), FConvexValue::String(FString::Printf(TEXT("m%d"), i)));
			Client->MutationNative(TEXT("messages:send"), Args,
				[S](const FConvexResult&) { ++S->SendsDone; });
		}
		AdvanceTo(2);
		return false;
	}
	case 2:  // Seeded: subscribe the first page (2 items).
	{
		if (State->SendsDone < 5)
		{
			return false;
		}
		TMap<FString, FConvexValue> Args;
		Args.Add(TEXT("channel"), FConvexValue::String(State->Channel));
		TSharedPtr<FConvexPaginatedLiveState> S = State;
		State->Subscription = Client->SubscribePaginatedNative(
			TEXT("messages:listPaginated"), Args, /*InitialNumItems=*/2,
			[S](const FConvexPaginatedSnapshot& Snapshot) {
				S->Latest = Snapshot;
				++S->Updates;
			});
		if (!State->Subscription.IsValid())
		{
			Test->AddError(TEXT("SubscribePaginatedNative returned null"));
			Client->Shutdown();
			Client->RemoveFromRoot();
			return true;
		}
		AdvanceTo(3);
		return false;
	}
	case 3:  // First page loaded.
	{
		if (State->Latest.Status != EConvexPaginationStatus::CanLoadMore)
		{
			return false;
		}
		Test->TestEqual(TEXT("first page size"), State->Latest.Results.Num(), 2);
		Test->TestEqual(TEXT("first page bodies"), State->Bodies(),
			TArray<FString>({TEXT("m1"), TEXT("m2")}));
		Test->TestTrue(TEXT("LoadMore starts a page"), State->Subscription->LoadMore(2));
		Test->TestFalse(TEXT("LoadMore while loading is a no-op"),
			State->Subscription->LoadMore(2));
		AdvanceTo(4);
		return false;
	}
	case 4:  // Second page loaded; drain the rest.
	{
		if (State->Latest.Status != EConvexPaginationStatus::CanLoadMore ||
			State->Latest.Results.Num() < 4)
		{
			return false;
		}
		Test->TestEqual(TEXT("two pages combined"), State->Latest.Results.Num(), 4);
		Test->TestTrue(TEXT("LoadMore drains"), State->Subscription->LoadMore(10));
		AdvanceTo(5);
		return false;
	}
	case 5:  // Exhausted; then a live insert grows the last page.
	{
		if (State->Latest.Status != EConvexPaginationStatus::Exhausted)
		{
			return false;
		}
		Test->TestEqual(TEXT("all five messages in order"), State->Bodies(),
			TArray<FString>({TEXT("m1"), TEXT("m2"), TEXT("m3"), TEXT("m4"), TEXT("m5")}));
		Test->TestFalse(TEXT("LoadMore after exhaustion is a no-op"),
			State->Subscription->LoadMore(2));

		TMap<FString, FConvexValue> Args;
		Args.Add(TEXT("channel"), FConvexValue::String(State->Channel));
		Args.Add(TEXT("author"), FConvexValue::String(TEXT("ue")));
		Args.Add(TEXT("body"), FConvexValue::String(TEXT("m6")));
		Client->MutationNative(TEXT("messages:send"), Args, nullptr);
		AdvanceTo(6);
		return false;
	}
	case 6:  // The insert lands in the unbounded last page.
	{
		if (State->Latest.Results.Num() < 6)
		{
			return false;
		}
		Test->TestEqual(TEXT("live insert appended"), State->Bodies(),
			TArray<FString>(
				{TEXT("m1"), TEXT("m2"), TEXT("m3"), TEXT("m4"), TEXT("m5"), TEXT("m6")}));
		Test->TestEqual(TEXT("still exhausted"), State->Latest.Status,
			EConvexPaginationStatus::Exhausted);

		if (UConvexPaginatedSubscription* Subscription = State->Subscription.Get())
		{
			Subscription->Unsubscribe();
			Test->TestFalse(TEXT("inactive after Unsubscribe"), Subscription->IsActive());
		}
		Client->Shutdown();
		Client->RemoveFromRoot();
		return true;
	}
	default:
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexPaginatedLiveTest, "Convex.Paginated.Live",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexPaginatedLiveTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FConvexPaginatedLiveState> State = MakeShared<FConvexPaginatedLiveState>();
	State->Test = this;
	State->Url = PaginatedBackendUrl();
	State->Channel = FString::Printf(TEXT("ue-paged-%lld"), FDateTime::UtcNow().GetTicks());

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Probe = FHttpModule::Get().CreateRequest();
	Probe->SetURL(State->Url + TEXT("/version"));
	Probe->SetVerb(TEXT("GET"));
	Probe->SetTimeout(5.0f);
	Probe->OnProcessRequestComplete().BindLambda(
		[State](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk) {
			State->bProbeOk = bOk && Response.IsValid() && Response->GetResponseCode() == 200;
			State->bProbeDone = true;
		});
	Probe->ProcessRequest();

	ADD_LATENT_AUTOMATION_COMMAND(FConvexPaginatedLiveFlow(State));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

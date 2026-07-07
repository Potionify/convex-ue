// Copyright Potionify. Apache-2.0.

// Live end-to-end automation test: drives the full plugin stack (UE
// transports -> convex-cpp sync protocol) against a running Convex backend
// with the integration schema from convex-cpp/integration deployed.
//
// Requires the local backend (convex-cpp/integration/backend: docker compose
// up -d) or CONVEX_LOCAL_URL pointing elsewhere. The test self-skips when the
// backend is unreachable so it never breaks offline automation runs.
//
// Run headless:
//   UnrealEditor-Cmd.exe ConvexExample.uproject
//     -ExecCmds="Automation RunTests Convex.Live; Quit"
//     -unattended -nullrhi -nosplash

#if WITH_DEV_AUTOMATION_TESTS

#include "ConvexClient.h"
#include "ConvexClientModule.h"
#include "ConvexSubscription.h"
#include "ConvexValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AutomationTest.h"

namespace
{

FString LiveBackendUrl()
{
	FString Url = FPlatformMisc::GetEnvironmentVariable(TEXT("CONVEX_LOCAL_URL"));
	return Url.IsEmpty() ? TEXT("http://127.0.0.1:3210") : Url;
}

/** Shared mutable state driven forward by the latent command below. */
struct FConvexLiveState
{
	FAutomationTestBase* Test = nullptr;
	FString Url;
	TWeakObjectPtr<UConvexClient> Client;
	TWeakObjectPtr<UConvexSubscription> Subscription;
	FString CounterName;

	bool bProbeDone = false;
	bool bProbeOk = false;

	int32 Phase = 0;
	double PhaseStartSeconds = 0.0;

	int32 SubscriptionUpdates = 0;
	bool bFirstUpdateWasNull = false;

	bool bMutationDone = false;
	bool bMutationSawOwnWrite = false;
	double MutationValue = 0.0;
	double SubscriptionValueAtMutationDone = -1.0;

	bool bKitchenDone = false;
	bool bKitchenOk = false;
	FString KitchenError;

	bool bHttpDone = false;
	bool bHttpOk = false;
};

bool CheckKitchenSink(const FConvexResult& Result, FString& OutError)
{
	if (!Result.bSuccess)
	{
		OutError = FString::Printf(TEXT("kitchenSink failed: %s"), *Result.ErrorMessage);
		return false;
	}
	TMap<FString, FConvexValue> Sink;
	if (!Result.Value.TryGetObject(Sink))
	{
		OutError = TEXT("kitchenSink result is not an object");
		return false;
	}

	int64 Big = 0;
	if (!Sink.FindRef(TEXT("int64Big")).TryGetInt64(Big) || Big != 9007199254740993LL)
	{
		OutError = FString::Printf(TEXT("int64Big mismatch: %lld"), Big);
		return false;
	}
	int64 Min = 0;
	if (!Sink.FindRef(TEXT("int64Min")).TryGetInt64(Min) || Min != TNumericLimits<int64>::Min())
	{
		OutError = TEXT("int64Min mismatch");
		return false;
	}
	TArray<uint8> Bytes;
	if (!Sink.FindRef(TEXT("bytes")).TryGetBytes(Bytes) || Bytes.Num() != 8 || Bytes[7] != 7)
	{
		OutError = TEXT("bytes mismatch");
		return false;
	}
	TArray<FConvexValue> Specials;
	double Nan = 0.0;
	if (!Sink.FindRef(TEXT("specialFloats")).TryGetArray(Specials) || Specials.Num() != 4 ||
		!Specials[0].TryGetFloat(Nan) || !FMath::IsNaN(Nan))
	{
		OutError = TEXT("specialFloats mismatch");
		return false;
	}
	FString Unicode;
	if (!Sink.FindRef(TEXT("unicode")).TryGetString(Unicode) || !Unicode.Contains(TEXT("caf")))
	{
		OutError = TEXT("unicode mismatch");
		return false;
	}
	return true;
}

}  // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConvexLiveFlow,
	TSharedPtr<FConvexLiveState>, State);

bool FConvexLiveFlow::Update()
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
		Test->AddError(FString::Printf(TEXT("Convex live test timed out in phase %d"), State->Phase));
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

	// Phase 0 runs before a client exists: wait for the reachability probe.
	if (State->Phase == 0)
	{
		if (!State->bProbeDone)
		{
			return false;
		}
		if (!State->bProbeOk)
		{
			Test->AddWarning(FString::Printf(
				TEXT("Skipping Convex.Live.EndToEnd: no backend at %s (start it via "
					 "convex-cpp/integration/backend: docker compose up -d)"),
				*State->Url));
			return true;
		}
		UConvexClient* NewClient = NewObject<UConvexClient>();
		NewClient->AddToRoot();  // no outer world; keep alive across latent ticks
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
	case 1:  // Wait for the websocket to connect, then subscribe.
	{
		if (Client->GetConnectionState() != EConvexConnectionState::Connected)
		{
			return false;
		}
		TMap<FString, FConvexValue> Args;
		Args.Add(TEXT("name"), FConvexValue::String(State->CounterName));
		TSharedPtr<FConvexLiveState> S = State;
		State->Subscription = Client->SubscribeNative(TEXT("counters:get"), Args,
			[S](const FConvexResult& Result) {
				++S->SubscriptionUpdates;
				if (S->SubscriptionUpdates == 1)
				{
					S->bFirstUpdateWasNull =
						Result.bSuccess && Result.Value.GetKind() == EConvexValueKind::Null;
				}
			});
		AdvanceTo(2);
		return false;
	}
	case 2:  // First subscription update (counter does not exist -> null).
	{
		if (State->SubscriptionUpdates < 1)
		{
			return false;
		}
		Test->TestTrue(TEXT("first subscription update is null"), State->bFirstUpdateWasNull);

		TMap<FString, FConvexValue> Args;
		Args.Add(TEXT("name"), FConvexValue::String(State->CounterName));
		TSharedPtr<FConvexLiveState> S = State;
		Client->MutationNative(TEXT("counters:increment"), Args,
			[S](const FConvexResult& Result) {
				S->bMutationDone = true;
				if (Result.bSuccess)
				{
					Result.Value.TryGetFloat(S->MutationValue);
				}
				// Read-your-writes: by the time the mutation resolves, the
				// subscription must already have delivered the new value.
				S->bMutationSawOwnWrite = S->SubscriptionUpdates >= 2;
			});
		AdvanceTo(3);
		return false;
	}
	case 3:  // Mutation resolved with ordering guarantee intact.
	{
		if (!State->bMutationDone)
		{
			return false;
		}
		Test->TestEqual(TEXT("increment returned 1"), State->MutationValue, 1.0);
		Test->TestTrue(TEXT("subscription observed the write before the mutation resolved"),
			State->bMutationSawOwnWrite);

		TSharedPtr<FConvexLiveState> S = State;
		Client->QueryNative(TEXT("values:kitchenSink"), {}, [S](const FConvexResult& Result) {
			S->bKitchenOk = CheckKitchenSink(Result, S->KitchenError);
			S->bKitchenDone = true;
		});
		AdvanceTo(4);
		return false;
	}
	case 4:  // Kitchen sink over the websocket path.
	{
		if (!State->bKitchenDone)
		{
			return false;
		}
		Test->TestTrue(FString::Printf(TEXT("kitchenSink over WS: %s"), *State->KitchenError),
			State->bKitchenOk);

		TSharedPtr<FConvexLiveState> S = State;
		Client->HttpQueryNative(TEXT("values:kitchenSink"), {}, [S](const FConvexResult& Result) {
			FString Error;
			S->bHttpOk = CheckKitchenSink(Result, Error);
			S->bHttpDone = true;
		});
		AdvanceTo(5);
		return false;
	}
	case 5:  // Kitchen sink over the HTTP path, then tear down.
	{
		if (!State->bHttpDone)
		{
			return false;
		}
		Test->TestTrue(TEXT("kitchenSink over HTTP"), State->bHttpOk);

		if (UConvexSubscription* Subscription = State->Subscription.Get())
		{
			Subscription->Unsubscribe();
		}
		Client->Shutdown();
		Client->RemoveFromRoot();
		return true;
	}
	default:
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexLiveEndToEndTest, "Convex.Live.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexLiveEndToEndTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FConvexLiveState> State = MakeShared<FConvexLiveState>();
	State->Test = this;
	State->Url = LiveBackendUrl();
	State->CounterName =
		FString::Printf(TEXT("ue-live-%lld"), FDateTime::UtcNow().GetTicks());

	// Probe the backend asynchronously (UE HTTP completes on the game-thread
	// ticker, so blocking here would deadlock); phase 0 of the latent flow
	// waits for the result and skips the test when the backend is down.
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

	ADD_LATENT_AUTOMATION_COMMAND(FConvexLiveFlow(State));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

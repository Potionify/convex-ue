// Copyright Potionify. Apache-2.0.

// Offline automation tests: value conversion fidelity, result mapping,
// initialization failure/retry, and shutdown completion of HTTP callbacks.
// No backend required — these protect CI from wrapper regressions that the
// live test only catches when a backend happens to be running.

#if WITH_DEV_AUTOMATION_TESTS

#include "ConvexClient.h"
#include "ConvexResult.h"
#include "ConvexSubscription.h"
#include "ConvexTestProbe.h"
#include "ConvexValue.h"
#include "Misc/AutomationTest.h"

#include <convex/error.h>
#include <convex/value.h>

#include <limits>

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineValueRoundTrip, "Convex.Offline.ValueRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineValueRoundTrip::RunTest(const FString& Parameters)
{
	// Every value kind, including the ones a JSON-number representation would
	// destroy: Int64 above 2^53, INT64_MIN, raw bytes, non-ASCII text.
	TArray<uint8> Bytes;
	for (uint8 i = 0; i < 8; ++i) Bytes.Add(i);

	TMap<FString, FConvexValue> Fields;
	Fields.Add(TEXT("nullValue"), FConvexValue::Null());
	Fields.Add(TEXT("boolValue"), FConvexValue::Bool(true));
	Fields.Add(TEXT("int64Big"), FConvexValue::Int64(9007199254740993LL));
	Fields.Add(TEXT("int64Min"), FConvexValue::Int64(std::numeric_limits<int64>::min()));
	Fields.Add(TEXT("floatValue"), FConvexValue::Float(1.5));
	Fields.Add(TEXT("unicode"), FConvexValue::String(FString(TEXT("café 世界"))));
	Fields.Add(TEXT("bytes"), FConvexValue::Bytes(Bytes));
	Fields.Add(TEXT("list"),
		FConvexValue::Array({FConvexValue::Int64(1), FConvexValue::String(TEXT("two"))}));
	const FConvexValue Original = FConvexValue::Object(Fields);

	bool bEncoded = false;
	const FString Wire = Original.ToWire(bEncoded);
	if (!TestTrue(TEXT("ToWire succeeds"), bEncoded)) return true;

	bool bDecoded = false;
	const FConvexValue Decoded = FConvexValue::FromWire(Wire, bDecoded);
	if (!TestTrue(TEXT("FromWire succeeds"), bDecoded)) return true;

	// Canonical encodings (sorted keys) make wire-string equality a full
	// structural comparison, NaN-safe.
	bool bReEncoded = false;
	TestEqual(TEXT("wire round trip is identical"), Decoded.ToWire(bReEncoded), Wire);
	TestTrue(TEXT("re-encode succeeds"), bReEncoded);

	// Spot-check typed accessors on the decoded copy.
	TMap<FString, FConvexValue> Out;
	if (!TestTrue(TEXT("decoded is object"), Decoded.TryGetObject(Out))) return true;

	int64 Big = 0;
	TestTrue(TEXT("int64Big decodes"), Out.FindRef(TEXT("int64Big")).TryGetInt64(Big));
	TestEqual(TEXT("int64Big survives (2^53+1)"), Big, static_cast<int64>(9007199254740993LL));

	int64 Min = 0;
	TestTrue(TEXT("int64Min decodes"), Out.FindRef(TEXT("int64Min")).TryGetInt64(Min));
	TestEqual(TEXT("int64Min survives"), Min, std::numeric_limits<int64>::min());

	TArray<uint8> OutBytes;
	TestTrue(TEXT("bytes decode"), Out.FindRef(TEXT("bytes")).TryGetBytes(OutBytes));
	TestEqual(TEXT("bytes identical"), OutBytes, Bytes);

	FString OutUnicode;
	TestTrue(TEXT("unicode decodes"), Out.FindRef(TEXT("unicode")).TryGetString(OutUnicode));
	TestEqual(TEXT("unicode survives"), OutUnicode, FString(TEXT("café 世界")));

	TestEqual(TEXT("null kind"), Out.FindRef(TEXT("nullValue")).GetKind(), EConvexValueKind::Null);

	// Int64 and Float64 are distinct Convex types.
	bool bWireOk = false;
	TestNotEqual(TEXT("Int64(1) != Float(1.0) on the wire"),
		FConvexValue::Int64(1).ToWire(bWireOk), FConvexValue::Float(1.0).ToWire(bWireOk));
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineResultMapping, "Convex.Offline.ResultMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineResultMapping::RunTest(const FString& Parameters)
{
	// Success.
	const FConvexResult Ok =
		FConvexResult::FromNative(convex::function_result::success(convex::value(std::int64_t{7})));
	TestTrue(TEXT("success maps"), Ok.bSuccess);
	TestFalse(TEXT("success is not app error"), Ok.bIsAppError);
	int64 V = 0;
	TestTrue(TEXT("success value decodes"), Ok.Value.TryGetInt64(V));
	TestEqual(TEXT("success value"), V, static_cast<int64>(7));

	// Plain (developer/system) error.
	const FConvexResult Plain =
		FConvexResult::FromNative(convex::function_result::error(std::string("Server Error")));
	TestFalse(TEXT("plain error not success"), Plain.bSuccess);
	TestFalse(TEXT("plain error not app error"), Plain.bIsAppError);
	TestEqual(TEXT("plain error message"), Plain.ErrorMessage, TEXT("Server Error"));

	// Application error (ConvexError) with data payload.
	convex::value_object Data;
	Data.emplace("code", convex::value("TEST"));
	const FConvexResult App = FConvexResult::FromNative(convex::function_result::error(
		convex::convex_error{"Uncaught ConvexError", convex::value(std::move(Data))}));
	TestFalse(TEXT("app error not success"), App.bSuccess);
	TestTrue(TEXT("app error flagged"), App.bIsAppError);
	TMap<FString, FConvexValue> ErrorFields;
	TestTrue(TEXT("app error data decodes"), App.ErrorData.TryGetObject(ErrorFields));
	FString Code;
	TestTrue(TEXT("app error code present"), ErrorFields.FindRef(TEXT("code")).TryGetString(Code));
	TestEqual(TEXT("app error code"), Code, TEXT("TEST"));
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineInitRetry, "Convex.Offline.InitializeFailureIsRetryable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineInitRetry::RunTest(const FString& Parameters)
{
	UConvexClient* Client = NewObject<UConvexClient>();

	// Malformed URL: must fail loudly (flag), not half-initialize.
	AddExpectedError(TEXT("Initialize failed"), EAutomationExpectedErrorFlags::Contains, 1);
	Client->Initialize(TEXT("not-a-url"));
	TestFalse(TEXT("not initialized after bad URL"), Client->IsInitialized());
	TestTrue(TEXT("failure is flagged"), Client->HasInitializationFailed());
	TestEqual(TEXT("state is disconnected"), Client->GetConnectionState(),
		EConvexConnectionState::Disconnected);

	// Retry with a well-formed URL (nothing needs to be listening).
	Client->Initialize(TEXT("http://127.0.0.1:65001"));
	TestTrue(TEXT("initialized after retry"), Client->IsInitialized());
	TestFalse(TEXT("failure flag cleared"), Client->HasInitializationFailed());

	Client->Shutdown();
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineHttpShutdown, "Convex.Offline.ShutdownCompletesHttpCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineHttpShutdown::RunTest(const FString& Parameters)
{
	UConvexClient* Client = NewObject<UConvexClient>();
	Client->Initialize(TEXT("http://127.0.0.1:65001"));  // valid shape, nothing listening
	if (!TestTrue(TEXT("initialized"), Client->IsInitialized())) return true;

	// Fire an HTTP one-shot and immediately shut down: the callback must run
	// exactly once, with an error (either the transport failure or the
	// shutdown error, whichever wins the race — never zero, never twice).
	TSharedPtr<int32> CallCount = MakeShared<int32>(0);
	TSharedPtr<FConvexResult> Captured = MakeShared<FConvexResult>();
	Client->HttpQueryNative(TEXT("values:kitchenSink"), {},
		[CallCount, Captured](const FConvexResult& Result)
		{
			++(*CallCount);
			*Captured = Result;
		});

	Client->Shutdown();

	TestEqual(TEXT("callback fired exactly once by the time Shutdown returns"), *CallCount, 1);
	TestFalse(TEXT("callback carried an error result"), Captured->bSuccess);
	TestFalse(TEXT("error message is populated"), Captured->ErrorMessage.IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineOneShotRootsTarget, "Convex.Offline.OneShotRootsDelegateTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineOneShotRootsTarget::RunTest(const FString& Parameters)
{
	UConvexClient* Client = NewObject<UConvexClient>();
	Client->AddToRoot();
	Client->Initialize(TEXT("http://127.0.0.1:65001"));  // valid shape, nothing listening
	if (!TestTrue(TEXT("initialized"), Client->IsInitialized())) return true;

	// The delegate target has no other reference. A one-shot call must keep
	// it alive through a garbage collection until the callback has fired.
	TWeakObjectPtr<UConvexTestProbe> Probe = NewObject<UConvexTestProbe>();
	FConvexResultDelegate OnResult;
	OnResult.BindDynamic(Probe.Get(), &UConvexTestProbe::OnResult);
	Client->HttpQuery(TEXT("values:kitchenSink"), {}, OnResult);

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	if (!TestTrue(TEXT("target survives GC while the call is pending"), Probe.IsValid())) return true;

	// Shutdown fails the pending call, which fires the callback and releases
	// the target.
	Client->Shutdown();
	TestEqual(TEXT("callback reached the target"), Probe->Calls, 1);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("target is collectable once the callback fired"), Probe.IsValid());

	Client->RemoveFromRoot();
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineTargetAliveDuringCallback,
	"Convex.Offline.OneShotTargetAliveDuringCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineTargetAliveDuringCallback::RunTest(const FString& Parameters)
{
	UConvexClient* Client = NewObject<UConvexClient>();
	Client->AddToRoot();
	Client->Initialize(TEXT("http://127.0.0.1:65001"));
	if (!TestTrue(TEXT("initialized"), Client->IsInitialized())) return true;

	TWeakObjectPtr<UConvexTestProbe> Probe = NewObject<UConvexTestProbe>();
	FConvexResultDelegate OnResult;
	OnResult.BindDynamic(Probe.Get(), &UConvexTestProbe::OnResultCollect);
	Client->HttpQuery(TEXT("values:kitchenSink"), {}, OnResult);

	// Shutdown fires the callback, which collects garbage mid-flight. The
	// target must survive that collection and finish its own callback.
	Client->Shutdown();
	TestTrue(TEXT("target survived a GC inside its callback"), Probe.IsValid());
	if (Probe.IsValid())
	{
		TestEqual(TEXT("callback ran to completion"), Probe->Calls, 1);
	}
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("target is collectable afterwards"), Probe.IsValid());

	Client->RemoveFromRoot();
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexOfflineSubscriptionListener, "Convex.Offline.SubscriptionAttachListener",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext |
		EAutomationTestFlags::ProductFilter)

bool FConvexOfflineSubscriptionListener::RunTest(const FString& Parameters)
{
	UConvexClient* Client = NewObject<UConvexClient>();
	Client->AddToRoot();
	Client->Initialize(TEXT("http://127.0.0.1:65001"));
	if (!TestTrue(TEXT("initialized"), Client->IsInitialized())) return true;

	// Subscribing needs no connection: the client queues it. An attached
	// listener lives as long as the subscription and no longer.
	UConvexSubscription* Subscription = Client->SubscribeNative(TEXT("counters:get"), {}, nullptr);
	if (!TestNotNull(TEXT("subscription handle"), Subscription)) return true;

	TWeakObjectPtr<UConvexTestProbe> Listener = NewObject<UConvexTestProbe>();
	Subscription->AttachListener(Listener.Get());

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("listener survives GC while subscribed"), Listener.IsValid());

	Subscription->Unsubscribe();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("listener is collectable after Unsubscribe"), Listener.IsValid());

	Client->Shutdown();
	Client->RemoveFromRoot();
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

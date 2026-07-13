// Copyright Potionify. Apache-2.0.

// Automation coverage for the Convex editor tool:
//   Convex.EditorTool.EnvResolution   pure resolver/parser rules (offline)
//   Convex.EditorTool.JsonHelpers     args seeding + pretty printing (offline)
//   Convex.EditorTool.LiveAdminSession  full admin session against the local
//     backend (self-skips when it is down or no admin key is discoverable):
//     connect w/ admin auth, check_admin_key, deploymentState, apiSpec, and
//     the function runner path (query + mutation), plus a Slate smoke build.

#if WITH_DEV_AUTOMATION_TESTS

#include "ConvexAdminSession.h"
#include "ConvexClient.h"
#include "ConvexCodegenRunner.h"
#include "ConvexDeploymentResolver.h"
#include "ConvexEditorJson.h"
#include "ConvexWireTap.h"
#include "Framework/Docking/TabManager.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SConvexConnectionPanel.h"
#include "SConvexDataBrowser.h"
#include "SConvexFunctionRunner.h"
#include "SConvexLogsPanel.h"
#include "SConvexSchemaPanel.h"
#include "SConvexTrafficPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexEnvResolutionTest,
	"Convex.EditorTool.EnvResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConvexEnvResolutionTest::RunTest(const FString&)
{
	using namespace ConvexDeploymentResolver;

	// ---- env-file parsing --------------------------------------------------
	const FString TempFile = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(),
		TEXT("convex-env-test"), TEXT(".env"));
	FFileHelper::SaveStringToFile(
		TEXT("# comment\n"
			 "CONVEX_DEPLOYMENT=dev:tall-forest-42\n"
			 "export CONVEX_URL=\"https://custom.example.com\"\n"
			 "QUOTED='single'\n"
			 "CONVEX_DEPLOYMENT=prod:should-not-win\n"  // first occurrence wins
			 "NOVALUE=\n"
			 "=broken\n"),
		*TempFile);

	TMap<FString, FString> Vars;
	TestTrue(TEXT("parse env file"), ParseEnvFile(TempFile, Vars));
	TestEqual(TEXT("plain value"), Vars.FindRef(TEXT("CONVEX_DEPLOYMENT")),
		FString(TEXT("dev:tall-forest-42")));
	TestEqual(TEXT("export + double quotes"), Vars.FindRef(TEXT("CONVEX_URL")),
		FString(TEXT("https://custom.example.com")));
	TestEqual(TEXT("single quotes"), Vars.FindRef(TEXT("QUOTED")), FString(TEXT("single")));
	IFileManager::Get().Delete(*TempFile);

	// ---- key classification ------------------------------------------------
	TestEqual(TEXT("dev key"), (int32)TypeFromAdminKey(TEXT("dev:tall-forest-42|secret")),
		(int32)EConvexDeploymentType::Dev);
	TestEqual(TEXT("prod key"), (int32)TypeFromAdminKey(TEXT("prod:big-horse-7|secret")),
		(int32)EConvexDeploymentType::Prod);
	TestEqual(TEXT("preview key"), (int32)TypeFromAdminKey(TEXT("preview:team:proj|secret")),
		(int32)EConvexDeploymentType::Preview);
	TestEqual(TEXT("custom key"), (int32)TypeFromAdminKey(TEXT("custom:my-domain|secret")),
		(int32)EConvexDeploymentType::Custom);
	TestEqual(TEXT("legacy key defaults to prod"),
		(int32)TypeFromAdminKey(TEXT("convex-integration|secret")),
		(int32)EConvexDeploymentType::Prod);
	TestEqual(TEXT("project key is unknown"),
		(int32)TypeFromAdminKey(TEXT("project:team:proj|secret")),
		(int32)EConvexDeploymentType::Unknown);

	TestEqual(TEXT("name from dev key"), NameFromAdminKey(TEXT("dev:tall-forest-42|secret")),
		FString(TEXT("tall-forest-42")));
	TestEqual(TEXT("no name in team preview key"),
		NameFromAdminKey(TEXT("preview:team:proj|secret")), FString());
	TestEqual(TEXT("instance name from legacy key"),
		NameFromAdminKey(TEXT("convex-integration|secret")),
		FString(TEXT("convex-integration")));

	// ---- resolution precedence ---------------------------------------------
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_DEPLOY_KEY"), TEXT("dev:tall-forest-42|secret"));
		V.Add(TEXT("CONVEX_SELF_HOSTED_URL"), TEXT("http://selfhosted:3210"));
		V.Add(TEXT("CONVEX_SELF_HOSTED_ADMIN_KEY"), TEXT("x|y"));
		const FConvexDeploymentConfig C = ResolveFromVars(V, TEXT("test"));
		TestTrue(TEXT("deploy key wins over self-hosted"), C.IsValid());
		TestEqual(TEXT("url synthesized from name"), C.DeploymentUrl,
			FString(TEXT("https://tall-forest-42.convex.cloud")));
		TestEqual(TEXT("type dev"), (int32)C.Type, (int32)EConvexDeploymentType::Dev);
	}
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_DEPLOYMENT_TOKEN"), TEXT("dev:aliased-77|secret"));
		const FConvexDeploymentConfig C = ResolveFromVars(V, TEXT("test"));
		TestTrue(TEXT("deployment token alias accepted"), C.IsValid());
		TestEqual(TEXT("alias name"), C.DeploymentName, FString(TEXT("aliased-77")));
	}
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_DEPLOY_KEY"), TEXT("dev:winner|secret"));
		V.Add(TEXT("CONVEX_DEPLOYMENT_TOKEN"), TEXT("dev:loser|secret"));
		TestEqual(TEXT("DEPLOY_KEY beats TOKEN"),
			ResolveFromVars(V, TEXT("test")).DeploymentName, FString(TEXT("winner")));
	}
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_DEPLOY_KEY"), TEXT("dev:name|secret"));
		V.Add(TEXT("CONVEX_URL"), TEXT("https://my-domain.example.com"));
		TestEqual(TEXT("explicit CONVEX_URL wins over synthesis"),
			ResolveFromVars(V, TEXT("test")).DeploymentUrl,
			FString(TEXT("https://my-domain.example.com")));
	}
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_SELF_HOSTED_URL"), TEXT("http://127.0.0.1:3210"));
		V.Add(TEXT("CONVEX_SELF_HOSTED_ADMIN_KEY"), TEXT("carnitas|secret"));
		const FConvexDeploymentConfig C = ResolveFromVars(V, TEXT("test"));
		TestTrue(TEXT("self-hosted pair valid"), C.IsValid());
		TestEqual(TEXT("self-hosted type"), (int32)C.Type,
			(int32)EConvexDeploymentType::SelfHosted);
		TestEqual(TEXT("self-hosted url verbatim"), C.DeploymentUrl,
			FString(TEXT("http://127.0.0.1:3210")));
	}
	{
		TMap<FString, FString> V;
		V.Add(TEXT("CONVEX_DEPLOYMENT"), TEXT("dev:tall-forest-42"));
		const FConvexDeploymentConfig C = ResolveFromVars(V, TEXT("test"));
		TestFalse(TEXT("deployment without key is not connectable"), C.IsValid());
		TestTrue(TEXT("error mentions the deployment"),
			C.Error.Contains(TEXT("dev:tall-forest-42")));
	}
	{
		const FConvexDeploymentConfig C = ResolveFromVars({}, TEXT("test"));
		TestFalse(TEXT("empty vars invalid"), C.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexJsonHelpersTest,
	"Convex.EditorTool.JsonHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConvexJsonHelpersTest::RunTest(const FString&)
{
	// Args seeding: required fields defaulted, optional omitted, unions use
	// their first member, literals their constant.
	bool bOk = false;
	const FConvexValue Validator = FConvexValue::FromWire(
		TEXT(R"({"type":"object","value":{)"
			 R"("name":{"fieldType":{"type":"string"},"optional":false},)"
			 R"("by":{"fieldType":{"type":"number"},"optional":true},)"
			 R"("mode":{"fieldType":{"type":"literal","value":"fast"},"optional":false},)"
			 R"("choice":{"fieldType":{"type":"union","value":[{"type":"boolean"},)"
			 R"({"type":"string"}]},"optional":false}}})"),
		bOk);
	TestTrue(TEXT("validator parsed"), bOk);

	const FString Seed = ConvexEditorJson::SeedArgsFromValidator(Validator);
	TestTrue(TEXT("seed has required string"), Seed.Contains(TEXT("\"name\"")));
	TestFalse(TEXT("seed omits optional"), Seed.Contains(TEXT("\"by\"")));
	TestTrue(TEXT("seed uses literal constant"), Seed.Contains(TEXT("\"fast\"")));
	TestTrue(TEXT("seed uses first union member"), Seed.Contains(TEXT("false")));

	// Seeds must be valid wire JSON (round-trip through the convex codec).
	bool bSeedParses = false;
	FConvexValue::FromWire(Seed, bSeedParses);
	TestTrue(TEXT("seed parses as wire JSON"), bSeedParses);

	// Any/absent validators seed the empty object.
	TestEqual(TEXT("any validator seeds {}"),
		ConvexEditorJson::SeedArgsFromValidator(FConvexValue::Null()), FString(TEXT("{}")));

	// Pretty printing keeps content and survives non-JSON input.
	const FString Pretty = ConvexEditorJson::PrettyPrint(TEXT("{\"a\":[1,2],\"b\":null}"));
	TestTrue(TEXT("pretty keeps fields"), Pretty.Contains(TEXT("\"a\"")));
	TestTrue(TEXT("pretty is multiline"), Pretty.Contains(TEXT("\n")));
	TestEqual(TEXT("scalar passthrough"), ConvexEditorJson::PrettyPrint(TEXT("42")),
		FString(TEXT("42")));
	TestEqual(TEXT("garbage passthrough"), ConvexEditorJson::PrettyPrint(TEXT("not json")),
		FString(TEXT("not json")));
	return true;
}

// ---------------------------------------------------------------- live test

namespace
{

struct FEditorToolLiveState
{
	FAutomationTestBase* Test = nullptr;
	FString Url;
	FString AdminKey;
	TSharedPtr<FConvexAdminSession> Session;

	bool bProbeDone = false;
	bool bProbeOk = false;

	int32 Phase = 0;
	double PhaseStartSeconds = 0.0;

	bool bQueryDone = false;
	FConvexResult QueryResult;
	bool bMutationDone = false;
	FConvexResult MutationResult;

	// CRUD round-trip through the data-browser mutations.
	bool bAddDone = false;
	FConvexResult AddResult;
	bool bDeleteDone = false;
	FConvexResult DeleteResult;
	FString InsertedDocId;

	// Log-stream poll + wire tap.
	bool bLogPollDone = false;
	bool bLogPollOk = false;
	int32 TapFrames = 0;
	FDelegateHandle TapHandle;
};

/// The local backend admin key: CONVEX_LOCAL_ADMIN_KEY, else read from
/// convex-cpp/integration/local.env at its workspace-relative locations.
FString DiscoverLocalAdminKey()
{
	FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("CONVEX_LOCAL_ADMIN_KEY"));
	if (!Key.IsEmpty())
	{
		return Key;
	}
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	for (const FString& Candidate :
		{ProjectDir / TEXT("../../convex-cpp/integration/local.env"),
			ProjectDir / TEXT("../convex-cpp/integration/local.env")})
	{
		TMap<FString, FString> Vars;
		if (ConvexDeploymentResolver::ParseEnvFile(Candidate, Vars))
		{
			if (const FString* Found = Vars.Find(TEXT("CONVEX_LOCAL_ADMIN_KEY")))
			{
				return *Found;
			}
		}
	}
	return FString();
}

}  // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConvexEditorToolLiveFlow,
	TSharedPtr<FEditorToolLiveState>, State);

bool FConvexEditorToolLiveFlow::Update()
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
		Test->AddError(
			FString::Printf(TEXT("EditorTool live test timed out in phase %d"), State->Phase));
		if (State->Session.IsValid())
		{
			State->Session->Disconnect();
		}
		return true;
	}
	const auto AdvanceTo = [this, Now](int32 NextPhase)
	{
		State->Phase = NextPhase;
		State->PhaseStartSeconds = Now;
	};

	switch (State->Phase)
	{
		case 0:  // Reachability probe + admin key discovery.
		{
			if (!State->bProbeDone)
			{
				return false;
			}
			if (!State->bProbeOk)
			{
				Test->AddWarning(FString::Printf(
					TEXT("Skipping Convex.EditorTool.LiveAdminSession: no backend at %s"),
					*State->Url));
				return true;
			}
			State->AdminKey = DiscoverLocalAdminKey();
			if (State->AdminKey.IsEmpty())
			{
				Test->AddWarning(
					TEXT("Skipping Convex.EditorTool.LiveAdminSession: no admin key "
						 "(set CONVEX_LOCAL_ADMIN_KEY or provide convex-cpp/integration/"
						 "local.env)"));
				return true;
			}

			FConvexDeploymentConfig Config;
			Config.DeploymentUrl = State->Url;
			Config.AdminKey = State->AdminKey;
			Config.DeploymentName = TEXT("local-integration");
			Config.Type = EConvexDeploymentType::SelfHosted;
			Config.Source = TEXT("automation test");

			State->Session = MakeShared<FConvexAdminSession>();
			State->Session->ConnectWithConfig(Config);
			AdvanceTo(1);
			return false;
		}

		case 1:  // Connected + key valid + state/specs arrived.
		{
			const FConvexAdminSession& S = *State->Session;
			if (!S.IsConnected() || !S.GetKeyInfo().bChecked ||
				S.GetDeploymentState().IsEmpty() || S.GetFunctions().Num() == 0)
			{
				return false;
			}
			Test->TestTrue(TEXT("admin key accepted"), S.GetKeyInfo().bValid);
			Test->TestTrue(TEXT("key check produced no error"), S.GetKeyInfo().Error.IsEmpty());
			Test->TestEqual(TEXT("deployment running"), S.GetDeploymentState(),
				FString(TEXT("running")));
			Test->TestFalse(TEXT("server version known"), S.GetServerVersion().IsEmpty());

			const TArray<FConvexFunctionSpec>& Functions = S.GetFunctions();
			const auto Has = [&Functions](const TCHAR* Id, const TCHAR* Type)
			{
				return Functions.ContainsByPredicate([&](const FConvexFunctionSpec& F)
					{ return F.Identifier == Id && F.FunctionType == Type; });
			};
			Test->TestTrue(TEXT("apiSpec lists counters:get as query"),
				Has(TEXT("counters:get"), TEXT("Query")));
			Test->TestTrue(TEXT("apiSpec lists counters:increment as mutation"),
				Has(TEXT("counters:increment"), TEXT("Mutation")));

			// Phase 2 session data arrives over the same connection.
			if (S.GetTableNames().Num() == 0 || S.GetActiveSchemaJson().IsEmpty())
			{
				return false;  // keep waiting within the phase timeout
			}
			Test->TestTrue(TEXT("table list has counters"),
				S.GetTableNames().Contains(TEXT("counters")));
			Test->TestTrue(TEXT("table list has messages"),
				S.GetTableNames().Contains(TEXT("messages")));
			Test->TestTrue(TEXT("declared schema mentions counters"),
				S.GetActiveSchemaJson().Contains(TEXT("counters")));
			AdvanceTo(2);
			return false;
		}

		case 2:  // Runner: query.
		{
			const FConvexFunctionSpec* Get = State->Session->GetFunctions().FindByPredicate(
				[](const FConvexFunctionSpec& F) { return F.Identifier == TEXT("counters:get"); });
			if (Get == nullptr)
			{
				Test->AddError(TEXT("counters:get missing from parsed specs"));
				State->Session->Disconnect();
				return true;
			}

			// The seeded args skeleton itself must be runnable for this spec.
			const FString Seed = ConvexEditorJson::SeedArgsFromValidator(Get->ArgsValidator);
			Test->TestTrue(TEXT("counters:get seed contains name"),
				Seed.Contains(TEXT("\"name\"")));

			TSharedPtr<FEditorToolLiveState> S = State;
			State->Session->RunFunction(*Get,
				TEXT("{\"name\": \"editor-tool-live-test\"}"),
				[S](const FConvexResult& Result)
				{
					S->QueryResult = Result;
					S->bQueryDone = true;
				});
			AdvanceTo(3);
			return false;
		}

		case 3:  // Query result, then runner: mutation.
		{
			if (!State->bQueryDone)
			{
				return false;
			}
			Test->TestTrue(TEXT("query ran"), State->QueryResult.bSuccess);

			const FConvexFunctionSpec* Increment = State->Session->GetFunctions().FindByPredicate(
				[](const FConvexFunctionSpec& F)
				{ return F.Identifier == TEXT("counters:increment"); });
			if (Increment == nullptr)
			{
				Test->AddError(TEXT("counters:increment missing from parsed specs"));
				State->Session->Disconnect();
				return true;
			}
			TSharedPtr<FEditorToolLiveState> S = State;
			State->Session->RunFunction(*Increment,
				TEXT("{\"name\": \"editor-tool-live-test\", \"by\": 2}"),
				[S](const FConvexResult& Result)
				{
					S->MutationResult = Result;
					S->bMutationDone = true;
				});
			AdvanceTo(4);
			return false;
		}

		case 4:  // Mutation result, then CRUD add via the system mutation.
		{
			if (!State->bMutationDone)
			{
				return false;
			}
			Test->TestTrue(TEXT("mutation ran"), State->MutationResult.bSuccess);
			double NewValue = 0.0;
			Test->TestTrue(TEXT("mutation returned a number"),
				State->MutationResult.Value.TryGetFloat(NewValue));
			Test->TestTrue(TEXT("counter incremented"), NewValue >= 2.0);

			// Watch the wire while the CRUD cycle runs.
			TSharedPtr<FEditorToolLiveState> S = State;
			ConvexWireTap::SetEnabled(true);
			State->TapHandle = ConvexWireTap::OnWireFrame().AddLambda(
				[S](ConvexWireTap::EDirection, const FString&, const FString&)
				{ ++S->TapFrames; });

			TMap<FString, FConvexValue> Doc;
			Doc.Add(TEXT("channel"), FConvexValue::String(TEXT("editor-crud-test")));
			Doc.Add(TEXT("author"), FConvexValue::String(TEXT("automation")));
			Doc.Add(TEXT("body"), FConvexValue::String(TEXT("inserted by LiveAdminSession")));
			TMap<FString, FConvexValue> Args;
			Args.Add(TEXT("table"), FConvexValue::String(TEXT("messages")));
			Args.Add(TEXT("documents"), FConvexValue::Array({FConvexValue::Object(Doc)}));
			State->Session->GetClient()->MutationNative(
				TEXT("_system/frontend/addDocument"), Args,
				[S](const FConvexResult& Result)
				{
					S->AddResult = Result;
					S->bAddDone = true;
				});
			AdvanceTo(5);
			return false;
		}

		case 5:  // addDocument result -> find the row -> deleteDocuments.
		{
			if (!State->bAddDone)
			{
				return false;
			}
			Test->TestTrue(TEXT("addDocument transport ok"), State->AddResult.bSuccess);
			TMap<FString, FConvexValue> Payload;
			bool bAdded = false;
			if (State->AddResult.Value.TryGetObject(Payload))
			{
				if (const FConvexValue* Success = Payload.Find(TEXT("success")))
				{
					Success->TryGetBool(bAdded);
				}
			}
			Test->TestTrue(TEXT("addDocument reported success"), bAdded);

			// Fetch the inserted row's id via listTableScan (desc: newest first).
			TSharedPtr<FEditorToolLiveState> S = State;
			TMap<FString, FConvexValue> ScanArgs;
			ScanArgs.Add(TEXT("table"), FConvexValue::String(TEXT("messages")));
			ScanArgs.Add(TEXT("limit"), FConvexValue::Float(10));
			State->Session->GetClient()->QueryNative(
				TEXT("_system/frontend/listTableScan"), ScanArgs,
				[S](const FConvexResult& Result)
				{
					TArray<FConvexValue> Rows;
					if (Result.bSuccess && Result.Value.TryGetArray(Rows))
					{
						for (const FConvexValue& Row : Rows)
						{
							TMap<FString, FConvexValue> Fields;
							FString Channel, Id;
							if (Row.TryGetObject(Fields))
							{
								if (const FConvexValue* ChannelValue =
										Fields.Find(TEXT("channel")))
								{
									ChannelValue->TryGetString(Channel);
								}
								if (const FConvexValue* IdValue = Fields.Find(TEXT("_id")))
								{
									IdValue->TryGetString(Id);
								}
							}
							if (Channel == TEXT("editor-crud-test"))
							{
								S->InsertedDocId = Id;
								break;
							}
						}
					}
					if (S->InsertedDocId.IsEmpty())
					{
						S->bDeleteDone = true;  // fail below with a clear message
						S->DeleteResult =
							FConvexResult::MakeError(TEXT("inserted row not found"));
						return;
					}
					TMap<FString, FConvexValue> Reference;
					Reference.Add(TEXT("id"), FConvexValue::String(S->InsertedDocId));
					Reference.Add(TEXT("tableName"), FConvexValue::String(TEXT("messages")));
					TMap<FString, FConvexValue> DeleteArgs;
					DeleteArgs.Add(TEXT("toDelete"),
						FConvexValue::Array({FConvexValue::Object(Reference)}));
					S->Session->GetClient()->MutationNative(
						TEXT("_system/frontend/deleteDocuments"), DeleteArgs,
						[S](const FConvexResult& Result)
						{
							S->DeleteResult = Result;
							S->bDeleteDone = true;
						});
				});
			AdvanceTo(6);
			return false;
		}

		case 6:  // Delete result + one log-stream poll.
		{
			if (!State->bDeleteDone)
			{
				return false;
			}
			Test->TestTrue(TEXT("deleteDocuments succeeded"), State->DeleteResult.bSuccess);
			Test->TestTrue(TEXT("wire tap captured the CRUD traffic"), State->TapFrames > 0);
			ConvexWireTap::SetEnabled(false);
			ConvexWireTap::OnWireFrame().Remove(State->TapHandle);

			TSharedPtr<FEditorToolLiveState> S = State;
			const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Poll =
				FHttpModule::Get().CreateRequest();
			Poll->SetURL(State->Url + TEXT("/api/stream_function_logs?cursor=0"));
			Poll->SetVerb(TEXT("GET"));
			Poll->SetHeader(TEXT("Authorization"), TEXT("Convex ") + State->AdminKey);
			Poll->SetHeader(TEXT("Convex-Client"), TEXT("dashboard-0.0.0"));
			Poll->SetTimeout(15.0f);
			Poll->OnProcessRequestComplete().BindLambda(
				[S](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
				{
					S->bLogPollOk = bOk && Response.IsValid() &&
						Response->GetResponseCode() == 200 &&
						Response->GetContentAsString().Contains(TEXT("newCursor"));
					S->bLogPollDone = true;
				});
			Poll->ProcessRequest();
			AdvanceTo(7);
			return false;
		}

		case 7:  // Log poll result + Slate smoke + teardown.
		{
			if (!State->bLogPollDone)
			{
				return false;
			}
			Test->TestTrue(TEXT("stream_function_logs responds with a cursor"),
				State->bLogPollOk);

			if (FSlateApplication::IsInitialized())
			{
				// Construction exercises every polled attribute path once
				// (the data/schema panels also auto-select the first table
				// and fire their subscriptions).
				const TSharedRef<SWidget> Panel =
					SNew(SConvexConnectionPanel, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Runner =
					SNew(SConvexFunctionRunner, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Data =
					SNew(SConvexDataBrowser, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Schema =
					SNew(SConvexSchemaPanel, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Logs =
					SNew(SConvexLogsPanel, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Traffic =
					SNew(SConvexTrafficPanel, State->Session.ToSharedRef());
				Test->TestTrue(TEXT("widgets constructed"),
					&Panel.Get() != &SNullWidget::NullWidget.Get() &&
						&Runner.Get() != &SNullWidget::NullWidget.Get() &&
						&Data.Get() != &SNullWidget::NullWidget.Get() &&
						&Schema.Get() != &SNullWidget::NullWidget.Get() &&
						&Logs.Get() != &SNullWidget::NullWidget.Get() &&
						&Traffic.Get() != &SNullWidget::NullWidget.Get());
			}

			State->Session->Disconnect();
			State->Session.Reset();
			return true;
		}
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexVendoredCodegenTest,
	"Convex.EditorTool.VendoredCodegen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConvexVendoredCodegenTest::RunTest(const FString&)
{
	// The vendored emission core must produce the same shape of output as the
	// standalone CLI (byte-parity is enforced upstream by that repo's golden
	// tests; this guards the vendored copy compiling and running in UE).
	const FString Spec = TEXT(
		R"([{"identifier":"counters.js:increment","functionType":"Mutation",)"
		R"("visibility":{"kind":"public"},"args":{"type":"object","value":{)"
		R"("name":{"fieldType":{"type":"string"},"optional":false},)"
		R"("by":{"fieldType":{"type":"number"},"optional":true}}},"returns":null},)"
		R"({"identifier":"hidden.js:x","functionType":"Query",)"
		R"("visibility":{"kind":"internal"},"args":null,"returns":null}])");

	ConvexCodegenRunner::FOptions Options;
	Options.SourceLabel = TEXT("automation test");
	TMap<FString, FString> Files;
	const TOptional<FString> Error = ConvexCodegenRunner::Generate(Spec, Options, Files);
	TestFalse(TEXT("generation succeeded"), Error.IsSet());
	if (Error.IsSet())
	{
		AddError(*Error);
		return false;
	}
	TestEqual(TEXT("four files emitted"), Files.Num(), 4);
	const FString* Header = Files.Find(TEXT("ConvexApi.h"));
	if (TestNotNull(TEXT("native header present"), Header))
	{
		TestTrue(TEXT("namespaced wrapper"), Header->Contains(TEXT("namespace Counters")));
		TestTrue(TEXT("optional arg is TOptional"),
			Header->Contains(TEXT("const TOptional<double>& By")));
		TestFalse(TEXT("internal function excluded"), Header->Contains(TEXT("Hidden")));
	}
	const FString* BpHeader = Files.Find(TEXT("ConvexApiBP.h"));
	if (TestNotNull(TEXT("BP header present"), BpHeader))
	{
		TestTrue(TEXT("BP library class"),
			BpHeader->Contains(TEXT("UConvexApiCountersLibrary")));
	}

	// Malformed input must come back as an error, not an exception.
	TMap<FString, FString> Unused;
	TestTrue(TEXT("malformed spec yields error"),
		ConvexCodegenRunner::Generate(TEXT("not json"), Options, Unused).IsSet());
	return true;
}

// ------------------------------------------------------- tab screenshot

namespace
{

struct FTabScreenshotState
{
	FAutomationTestBase* Test = nullptr;
	TWeakPtr<SDockTab> Tab;
	int32 PanelIndex = 0;  // 0 Functions, 1 Data, 2 Schema
	bool bWaiting = false;
	double PhaseStartSeconds = 0.0;
};

const TCHAR* PanelSuffix(int32 PanelIndex)
{
	switch (PanelIndex)
	{
		case 1: return TEXT("Data");
		case 2: return TEXT("Schema");
		case 3: return TEXT("Logs");
		case 4: return TEXT("Traffic");
		default: return TEXT("Functions");
	}
}

}  // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConvexTabScreenshotFlow,
	TSharedPtr<FTabScreenshotState>, State);

bool FConvexTabScreenshotFlow::Update()
{
	const double Now = FPlatformTime::Seconds();

	if (!State->bWaiting)
	{
		// (Re)spawn the tab showing the requested section.
		IConsoleVariable* StartPanel =
			IConsoleManager::Get().FindConsoleVariable(TEXT("Convex.Editor.StartPanel"));
		if (StartPanel != nullptr)
		{
			StartPanel->Set(State->PanelIndex);
		}
		const TSharedPtr<SDockTab> Tab =
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("ConvexDashboard")));
		if (!Tab.IsValid())
		{
			State->Test->AddWarning(TEXT("Convex tab spawner unavailable"));
			return true;
		}
		State->Tab = Tab;
		State->bWaiting = true;
		State->PhaseStartSeconds = Now;
		return false;
	}

	// Give the session time to connect and the section's subscriptions time
	// to deliver (data pages, schema, indexes) so captures show real content.
	const double WaitSeconds = State->PanelIndex == 0 ? 8.0 : 5.0;
	if (Now - State->PhaseStartSeconds < WaitSeconds)
	{
		return false;
	}

	const TSharedPtr<SDockTab> Tab = State->Tab.Pin();
	if (!Tab.IsValid())
	{
		State->Test->AddWarning(TEXT("Convex tab went away before capture"));
		return true;
	}

	TArray<FColor> Pixels;
	FIntVector Size(0, 0, 0);
	// The SDockTab widget itself is just the tab handle; the dashboard UI is
	// its content widget.
	if (!FSlateApplication::Get().TakeScreenshot(Tab->GetContent(), Pixels, Size))
	{
		State->Test->AddWarning(
			TEXT("Slate screenshot unavailable (null RHI?) — skipping captures"));
		Tab->RequestCloseTab();
		return true;
	}

	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
	const FString Path = FPaths::ProjectSavedDir() /
		FString::Printf(TEXT("ConvexTabScreenshot_%s.png"), PanelSuffix(State->PanelIndex));
	if (FFileHelper::SaveArrayToFile(Png, *Path))
	{
		State->Test->AddInfo(FString::Printf(TEXT("Convex tab screenshot: %s (%dx%d)"),
			*FPaths::ConvertRelativePathToFull(Path), Size.X, Size.Y));
	}
	else
	{
		State->Test->AddWarning(TEXT("Failed to write tab screenshot"));
	}
	Tab->RequestCloseTab();

	if (State->PanelIndex >= 4)
	{
		if (IConsoleVariable* StartPanel =
				IConsoleManager::Get().FindConsoleVariable(TEXT("Convex.Editor.StartPanel")))
		{
			StartPanel->Set(0);
		}
		return true;
	}
	++State->PanelIndex;
	State->bWaiting = false;
	return false;
}

/// Invokes the real dockable tab (spawner, session, widgets) and captures it
/// to Saved/ConvexTabScreenshot.png for visual review. Self-skips without a
/// renderer; primarily run locally with rendering enabled.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexTabScreenshotTest,
	"Convex.EditorTool.TabScreenshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConvexTabScreenshotTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddWarning(TEXT("Skipping Convex.EditorTool.TabScreenshot: Slate not initialized"));
		return true;
	}
	const TSharedPtr<FTabScreenshotState> State = MakeShared<FTabScreenshotState>();
	State->Test = this;
	ADD_LATENT_AUTOMATION_COMMAND(FConvexTabScreenshotFlow(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexEditorToolLiveTest,
	"Convex.EditorTool.LiveAdminSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FConvexEditorToolLiveTest::RunTest(const FString&)
{
	const TSharedPtr<FEditorToolLiveState> State = MakeShared<FEditorToolLiveState>();
	State->Test = this;
	State->Url = FPlatformMisc::GetEnvironmentVariable(TEXT("CONVEX_LOCAL_URL"));
	if (State->Url.IsEmpty())
	{
		State->Url = TEXT("http://127.0.0.1:3210");
	}

	// Async reachability probe; the latent flow waits on it (mirrors
	// Convex.Live.EndToEnd so offline runs skip instead of failing).
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Probe =
		FHttpModule::Get().CreateRequest();
	Probe->SetURL(State->Url / TEXT("version"));
	Probe->SetVerb(TEXT("GET"));
	Probe->SetTimeout(5.0f);
	Probe->OnProcessRequestComplete().BindLambda(
		[State](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
		{
			State->bProbeOk = bOk && Response.IsValid() && Response->GetResponseCode() == 200;
			State->bProbeDone = true;
		});
	Probe->ProcessRequest();

	ADD_LATENT_AUTOMATION_COMMAND(FConvexEditorToolLiveFlow(State));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

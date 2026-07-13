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
#include "ConvexDeploymentResolver.h"
#include "ConvexEditorJson.h"
#include "Framework/Docking/TabManager.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SConvexConnectionPanel.h"
#include "SConvexFunctionRunner.h"
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

		case 4:  // Mutation result + Slate smoke + teardown.
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

			if (FSlateApplication::IsInitialized())
			{
				// Construction exercises every polled attribute path once.
				const TSharedRef<SWidget> Panel =
					SNew(SConvexConnectionPanel, State->Session.ToSharedRef());
				const TSharedRef<SWidget> Runner =
					SNew(SConvexFunctionRunner, State->Session.ToSharedRef());
				Test->TestTrue(TEXT("widgets constructed"),
					&Panel.Get() != &SNullWidget::NullWidget.Get() &&
						&Runner.Get() != &SNullWidget::NullWidget.Get());
			}

			State->Session->Disconnect();
			State->Session.Reset();
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------- tab screenshot

namespace
{

struct FTabScreenshotState
{
	FAutomationTestBase* Test = nullptr;
	TWeakPtr<SDockTab> Tab;
	int32 Phase = 0;
	double PhaseStartSeconds = 0.0;
};

}  // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FConvexTabScreenshotFlow,
	TSharedPtr<FTabScreenshotState>, State);

bool FConvexTabScreenshotFlow::Update()
{
	const double Now = FPlatformTime::Seconds();
	if (State->PhaseStartSeconds == 0.0)
	{
		State->PhaseStartSeconds = Now;
	}

	// Give the session a few seconds to connect and populate the function
	// list so the capture shows the real thing, not an empty shell.
	if (State->Phase == 0)
	{
		if (Now - State->PhaseStartSeconds < 6.0)
		{
			return false;
		}
		State->Phase = 1;
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
			TEXT("Slate screenshot unavailable (null RHI?) — skipping capture"));
	}
	else
	{
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("ConvexTabScreenshot.png");
		if (FFileHelper::SaveArrayToFile(Png, *Path))
		{
			State->Test->AddInfo(FString::Printf(TEXT("Convex tab screenshot: %s (%dx%d)"),
				*FPaths::ConvertRelativePathToFull(Path), Size.X, Size.Y));
		}
		else
		{
			State->Test->AddWarning(TEXT("Failed to write ConvexTabScreenshot.png"));
		}
	}
	Tab->RequestCloseTab();
	return true;
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
	const TSharedPtr<SDockTab> Tab =
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("ConvexDashboard")));
	if (!Tab.IsValid())
	{
		AddWarning(TEXT("Skipping Convex.EditorTool.TabScreenshot: tab spawner unavailable "
						"(commandlet run?)"));
		return true;
	}
	const TSharedPtr<FTabScreenshotState> State = MakeShared<FTabScreenshotState>();
	State->Test = this;
	State->Tab = Tab;
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

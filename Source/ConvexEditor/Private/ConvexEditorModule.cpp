// Copyright Potionify. Apache-2.0.

#include "ConvexAdminSession.h"
#include "ConvexEditorSettings.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SConvexConnectionPanel.h"
#include "SConvexFunctionRunner.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	const FName ConvexTabName(TEXT("ConvexDashboard"));
}

/**
 * Editor-only module: registers the dockable Convex tab (Window > Convex) and
 * owns the admin session so connection state survives tab close/reopen.
 */
class FConvexEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(ConvexTabName,
				FOnSpawnTab::CreateRaw(this, &FConvexEditorModule::SpawnConvexTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Convex"))
			.SetTooltipText(LOCTEXT("TabTooltip",
				"Convex deployment dashboard: connection, function runner"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Server"));
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ConvexTabName);
		}
		// UObjects (the admin client) are only safe to touch while the UObject
		// system is still up; during editor exit it may already be gone.
		if (Session.IsValid() && UObjectInitialized())
		{
			Session->Disconnect();
		}
		Session.Reset();
	}

private:
	TSharedRef<SDockTab> SpawnConvexTab(const FSpawnTabArgs&)
	{
		if (!Session.IsValid())
		{
			Session = MakeShared<FConvexAdminSession>();
		}
		if (!Session->IsConnected() && GetDefault<UConvexEditorSettings>()->bAutoConnect)
		{
			Session->RefreshAndConnect();
		}

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SConvexConnectionPanel, Session.ToSharedRef())
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SNew(SConvexFunctionRunner, Session.ToSharedRef())
				]
			];
	}

	TSharedPtr<FConvexAdminSession> Session;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvexEditorModule, ConvexEditor)

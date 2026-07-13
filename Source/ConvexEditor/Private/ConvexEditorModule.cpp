// Copyright Potionify. Apache-2.0.

#include "ConvexAdminSession.h"
#include "ConvexEditorSettings.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SConvexConnectionPanel.h"
#include "SConvexDataBrowser.h"
#include "SConvexFunctionRunner.h"
#include "SConvexLogsPanel.h"
#include "SConvexSchemaPanel.h"
#include "SConvexTrafficPanel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

namespace
{
	const FName ConvexTabName(TEXT("ConvexDashboard"));

	/// Which section a freshly spawned tab shows (0 Functions, 1 Data,
	/// 2 Schema). Exists so automation (the screenshot test) can capture every
	/// section; harmless interactively.
	TAutoConsoleVariable<int32> CVarStartPanel(
		TEXT("Convex.Editor.StartPanel"), 0,
		TEXT("Section index the Convex tab opens on "
			 "(0 Functions, 1 Data, 2 Schema, 3 Logs, 4 Traffic)."));
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

		const int32 StartPanel = FMath::Clamp(CVarStartPanel.GetValueOnGameThread(), 0, 4);
		const TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher);
		Switcher->AddSlot()[SNew(SConvexFunctionRunner, Session.ToSharedRef())];
		Switcher->AddSlot()[SNew(SConvexDataBrowser, Session.ToSharedRef())];
		Switcher->AddSlot()[SNew(SConvexSchemaPanel, Session.ToSharedRef())];
		Switcher->AddSlot()[SNew(SConvexLogsPanel, Session.ToSharedRef())];
		Switcher->AddSlot()[SNew(SConvexTrafficPanel, Session.ToSharedRef())];
		Switcher->SetActiveWidgetIndex(StartPanel);

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
				.AutoHeight()
				.Padding(8.f, 6.f, 8.f, 2.f)
				.HAlign(HAlign_Left)
				[
					SNew(SSegmentedControl<int32>)
					.Value(StartPanel)
					.OnValueChanged_Lambda([WeakSwitcher = TWeakPtr<SWidgetSwitcher>(Switcher)](
						int32 NewIndex)
					{
						if (const TSharedPtr<SWidgetSwitcher> Pinned = WeakSwitcher.Pin())
						{
							Pinned->SetActiveWidgetIndex(NewIndex);
						}
					})
					+ SSegmentedControl<int32>::Slot(0)
					.Text(LOCTEXT("FunctionsTab", "Functions"))
					+ SSegmentedControl<int32>::Slot(1)
					.Text(LOCTEXT("DataTab", "Data"))
					+ SSegmentedControl<int32>::Slot(2)
					.Text(LOCTEXT("SchemaTab", "Schema"))
					+ SSegmentedControl<int32>::Slot(3)
					.Text(LOCTEXT("LogsTab", "Logs"))
					+ SSegmentedControl<int32>::Slot(4)
					.Text(LOCTEXT("TrafficTab", "Traffic"))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					Switcher
				]
			];
	}

	TSharedPtr<FConvexAdminSession> Session;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConvexEditorModule, ConvexEditor)

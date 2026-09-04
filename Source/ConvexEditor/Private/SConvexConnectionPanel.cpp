// Copyright Potionify. Apache-2.0.

#include "SConvexConnectionPanel.h"

#include "ConvexAdminSession.h"
#include "ConvexCodegenRunner.h"
#include "ConvexEditorSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ISettingsModule.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ConvexEditor"

void SConvexConnectionPanel::Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession)
{
	Session = InSession;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(12.f, 8.f))
		[
			SNew(SVerticalBox)

			// Row 1: status dot, state, deployment identity, actions.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(10.f)
					.HeightOverride(10.f)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
						.ColorAndOpacity(this, &SConvexConnectionPanel::GetStateColor)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(STextBlock)
					.Text(this, &SConvexConnectionPanel::GetStateText)
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SConvexConnectionPanel::GetDeploymentText)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Reconnect", "Reconnect"))
					.ToolTipText(LOCTEXT("ReconnectTip",
						"Re-resolve the deployment from env files and reconnect"))
					.OnClicked_Lambda([this]()
					{
						Session->RefreshAndConnect();
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("GenerateApi", "Generate API"))
					.ToolTipText(LOCTEXT("GenerateApiTip",
						"Run convex-ue-codegen against this deployment: emits typed C++ and "
						"Blueprint wrappers for every deployed function into the configured "
						"output directory"))
					.IsEnabled_Lambda([this]() { return Session->GetConfig().IsValid(); })
					.OnClicked(this, &SConvexConnectionPanel::OnGenerateApiClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Settings", "Settings..."))
					.ToolTipText(LOCTEXT("SettingsTip",
						"Open the Convex Editor preferences (env file location)"))
					.OnClicked_Lambda([]()
					{
						if (ISettingsModule* Settings =
								FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
						{
							Settings->ShowViewer(
								TEXT("Editor"), TEXT("Plugins"), TEXT("ConvexEditor"));
						}
						return FReply::Handled();
					})
				]
			]

			// Row 2: admin key validation outcome.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SConvexConnectionPanel::GetKeyInfoText)
				.ColorAndOpacity(this, &SConvexConnectionPanel::GetKeyInfoColor)
				.AutoWrapText(true)
			]

			// Row 3: source / version details, or the resolution error.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(this, &SConvexConnectionPanel::GetDetailText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

			// Row 4: prod warning banner.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.BorderBackgroundColor(FLinearColor(0.55f, 0.28f, 0.05f))
				.Padding(FMargin(8.f, 4.f))
				.Visibility(this, &SConvexConnectionPanel::GetProdWarningVisibility)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ProdWarning",
						"This is not a dev deployment. Mutations and actions run against real "
						"data — double-check before running anything that writes."))
					.AutoWrapText(true)
				]
			]
		]
	];
}

FText SConvexConnectionPanel::GetStateText() const
{
	if (!Session->GetConfig().IsValid())
	{
		return NSLOCTEXT("ConvexEditor", "NotConfigured", "Not configured");
	}
	switch (Session->GetConnectionState())
	{
		case EConvexConnectionState::Connected:
		{
			const FString& State = Session->GetDeploymentState();
			if (!State.IsEmpty() && State != TEXT("running"))
			{
				return FText::Format(
					NSLOCTEXT("ConvexEditor", "ConnectedState", "Connected ({0})"),
					FText::FromString(State));
			}
			return NSLOCTEXT("ConvexEditor", "Connected", "Connected");
		}
		case EConvexConnectionState::Connecting:
			return NSLOCTEXT("ConvexEditor", "Connecting", "Connecting...");
		default:
			return NSLOCTEXT("ConvexEditor", "Disconnected", "Disconnected");
	}
}

FSlateColor SConvexConnectionPanel::GetStateColor() const
{
	if (!Session->GetConfig().IsValid())
	{
		return FLinearColor(0.5f, 0.5f, 0.5f);
	}
	switch (Session->GetConnectionState())
	{
		case EConvexConnectionState::Connected:
			return Session->GetDeploymentState() == TEXT("paused")
				? FLinearColor(0.9f, 0.65f, 0.1f)
				: FLinearColor(0.2f, 0.75f, 0.3f);
		case EConvexConnectionState::Connecting:
			return FLinearColor(0.9f, 0.65f, 0.1f);
		default:
			return FLinearColor(0.85f, 0.25f, 0.2f);
	}
}

FText SConvexConnectionPanel::GetDeploymentText() const
{
	const FConvexDeploymentConfig& Config = Session->GetConfig();
	if (!Config.IsValid())
	{
		return FText::GetEmpty();
	}
	const FString Name =
		Config.DeploymentName.IsEmpty() ? TEXT("(unnamed)") : Config.DeploymentName;
	return FText::FromString(FString::Printf(TEXT("%s  [%s]  %s"), *Name,
		ConvexDeploymentTypeToString(Config.Type), *Config.DeploymentUrl));
}

FText SConvexConnectionPanel::GetKeyInfoText() const
{
	const FConvexDeploymentConfig& Config = Session->GetConfig();
	if (!Config.IsValid())
	{
		return FText::GetEmpty();
	}
	const FConvexAdminKeyInfo& Info = Session->GetKeyInfo();
	if (!Info.bChecked)
	{
		return NSLOCTEXT("ConvexEditor", "KeyChecking", "Validating admin key...");
	}
	if (!Info.Error.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("ConvexEditor", "KeyError", "Admin key check failed: {0}"),
			FText::FromString(Info.Error));
	}
	if (!Info.bValid)
	{
		return NSLOCTEXT("ConvexEditor", "KeyInvalid", "Admin key rejected by the deployment.");
	}
	FString Text = TEXT("Admin key valid");
	if (Info.bIsReadOnly)
	{
		Text += TEXT(" (read-only)");
	}
	if (Info.AllowedOps.Num() > 0)
	{
		Text += TEXT(" — ops: ") + FString::Join(Info.AllowedOps, TEXT(", "));
	}
	return FText::FromString(Text);
}

FSlateColor SConvexConnectionPanel::GetKeyInfoColor() const
{
	const FConvexAdminKeyInfo& Info = Session->GetKeyInfo();
	if (Info.bChecked && (!Info.Error.IsEmpty() || !Info.bValid))
	{
		return FLinearColor(0.9f, 0.4f, 0.35f);
	}
	return FSlateColor::UseSubduedForeground();
}

FText SConvexConnectionPanel::GetDetailText() const
{
	const FConvexDeploymentConfig& Config = Session->GetConfig();
	if (!Config.Error.IsEmpty())
	{
		return FText::FromString(Config.Error);
	}
	FString Detail = TEXT("Credentials: ") + Config.Source;
	if (!Session->GetServerVersion().IsEmpty())
	{
		Detail += TEXT("  |  backend ") + Session->GetServerVersion();
	}
	const int32 NumFunctions = Session->GetFunctions().Num();
	if (NumFunctions > 0)
	{
		Detail += FString::Printf(TEXT("  |  %d functions"), NumFunctions);
	}
	return FText::FromString(Detail);
}

FReply SConvexConnectionPanel::OnGenerateApiClicked()
{
	const auto Notify = [](const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 6.f;
		if (const TSharedPtr<SNotificationItem> Item =
				FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(
				bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	};

	if (bCodegenRunning)
	{
		Notify(TEXT("Codegen is already running."), false);
		return FReply::Handled();
	}

	const FString OutDir = GetDefault<UConvexEditorSettings>()->CodegenOutputDir.Path;
	const FString ScriptOutDir = GetDefault<UConvexEditorSettings>()->ScriptOutputDir.Path;
	if (OutDir.IsEmpty())
	{
		Notify(TEXT("Set the codegen output directory (a folder inside one of your "
					"project's modules) in Editor Preferences > Plugins > Convex Editor."),
			false);
		return FReply::Handled();
	}

	// Generation runs in-process on the vendored emission core — the same
	// core (and the same bytes) as the standalone CLI and the web app.
	const FConvexDeploymentConfig& Config = Session->GetConfig();
	bCodegenRunning = true;
	ConvexCodegenRunner::FetchApiSpec(Config.DeploymentUrl, Config.AdminKey,
		[WeakThis = TWeakPtr<SConvexConnectionPanel>(SharedThis(this)), Notify, OutDir,
			ScriptOutDir, SourceLabel = Config.DeploymentUrl](bool bFetched, FString Body)
		{
			if (const TSharedPtr<SConvexConnectionPanel> This = WeakThis.Pin())
			{
				This->bCodegenRunning = false;
			}
			if (!bFetched)
			{
				Notify(TEXT("apiSpec fetch failed: ") + Body, false);
				return;
			}
			ConvexCodegenRunner::FOptions Options;
			Options.SourceLabel = SourceLabel;
			Options.bEmitScript = !ScriptOutDir.IsEmpty();
			TMap<FString, FString> Files;
			if (const TOptional<FString> Error =
					ConvexCodegenRunner::Generate(Body, Options, Files))
			{
				Notify(TEXT("Codegen failed: ") + *Error, false);
				return;
			}
			if (const TOptional<FString> Error =
					ConvexCodegenRunner::WriteFiles(OutDir, Files, ScriptOutDir))
			{
				Notify(*Error, false);
				return;
			}
			Notify(ScriptOutDir.IsEmpty()
					   ? FString::Printf(TEXT("Convex API generated: %d files into %s"),
							 Files.Num(), *OutDir)
					   : FString::Printf(
							 TEXT("Convex API generated: %d files into %s (script: %s)"),
							 Files.Num(), *OutDir, *ScriptOutDir),
				true);
		});
	return FReply::Handled();
}

EVisibility SConvexConnectionPanel::GetProdWarningVisibility() const
{
	const bool bShow = Session->GetConfig().IsValid() && !Session->IsWriteSafeDeployment() &&
		Session->IsConnected();
	return bShow ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE

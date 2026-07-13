// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FConvexAdminSession;

/**
 * Compact connection header for the Convex tab: deployment identity (name,
 * type badge, URL, credential source), live connection state, admin-key
 * validation outcome, and reconnect/settings actions. All display state is
 * read from the session via polled Slate attributes.
 */
class SConvexConnectionPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SConvexConnectionPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FConvexAdminSession> InSession);

private:
	FText GetStateText() const;
	FSlateColor GetStateColor() const;
	FText GetDeploymentText() const;
	FText GetKeyInfoText() const;
	FSlateColor GetKeyInfoColor() const;
	FText GetDetailText() const;
	EVisibility GetProdWarningVisibility() const;
	FReply OnGenerateApiClicked();

	TSharedPtr<FConvexAdminSession> Session;
	bool bCodegenRunning = false;
};

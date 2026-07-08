// Copyright Potionify. Apache-2.0.

// Editor-only checks that the plugin's UObject types are usable from
// Blueprint the way the docs promise — asked with the exact function the
// Blueprint editor itself uses, so "works in the variable picker" is pinned
// by a test instead of folklore.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "ConvexClient.h"
#include "ConvexSubscription.h"
#include "EdGraphSchema_K2.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvexBlueprintVariableTypeTest, "Convex.Editor.BlueprintVariableTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConvexBlueprintVariableTypeTest::RunTest(const FString& Parameters)
{
	// This is the predicate the My Blueprint variable-type picker and
	// "Promote to Variable" both consult.
	TestTrue(TEXT("UConvexClient is an allowable Blueprint variable type"),
		UEdGraphSchema_K2::IsAllowableBlueprintVariableType(UConvexClient::StaticClass()));
	TestTrue(TEXT("UConvexSubscription is an allowable Blueprint variable type"),
		UEdGraphSchema_K2::IsAllowableBlueprintVariableType(UConvexSubscription::StaticClass()));

	TestTrue(TEXT("UConvexClient carries CLASS_BlueprintType metadata"),
		UConvexClient::StaticClass()->GetBoolMetaData(TEXT("BlueprintType")));
	return true;
}

#endif  // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

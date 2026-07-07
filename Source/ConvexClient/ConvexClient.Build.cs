// Copyright Potionify. All Rights Reserved.

using UnrealBuildTool;

public class ConvexClient : ModuleRules
{
	public ConvexClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// This module includes convex-cpp headers (which throw), so it also
		// needs C++ exceptions and C++20 enabled.
		bEnableExceptions = true;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ConvexCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"WebSockets",
			"HTTP",
		});
	}
}

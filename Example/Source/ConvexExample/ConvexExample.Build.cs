// Copyright Potionify. Apache-2.0.

using UnrealBuildTool;

public class ConvexExample : ModuleRules
{
	public ConvexExample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ConvexClient",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}

// Copyright Potionify. Apache-2.0.

using UnrealBuildTool;

public class ConvexEditor : ModuleRules
{
	public ConvexEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"DeveloperSettings",
			"Settings",
			"HTTP",
			"Json",
			"UnrealEd",
			"Projects",
			// Admin sync client + value types.
			"ConvexClient",
			"ConvexCore",
		});
	}
}

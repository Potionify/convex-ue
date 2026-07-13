// Copyright Potionify. Apache-2.0.

using UnrealBuildTool;

public class ConvexEditor : ModuleRules
{
	public ConvexEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		// The vendored convex-ue-codegen emission core (Private/convex-ue-codegen,
		// synced by Tools/sync-convex-ue-codegen.ps1) is compiled directly into
		// this module so the Generate API button and the ConvexCodegen commandlet
		// work without any external executable. Same constraints as the vendored
		// convex-cpp sources in ConvexClient: no PCHs/unity (keeps UE macros out
		// of the vendored TUs), C++ exceptions (codegen_error), C++20.
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = true;
		CppStandard = CppStandardVersion.Cpp20;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		bWarningsAsErrors = false;

		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "convex-ue-codegen", "include"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "convex-ue-codegen", "third_party"));

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

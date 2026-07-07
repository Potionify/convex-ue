// Copyright Potionify. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ConvexClient : ModuleRules
{
	public ConvexClient(ReadOnlyTargetRules Target) : base(Target)
	{
		// The vendored convex-cpp translation units live under this module
		// (Private/convex-cpp/src, synced by Tools/sync-convex-cpp.ps1) and
		// are compiled directly into it: MSVC only exports DLL symbols
		// annotated with a module-API macro, which the vendored classes
		// (intentionally) lack, so they cannot be resolved across a module
		// (DLL) boundary. ConvexCore only publishes the headers.
		//
		// No PCHs and no unity builds so the vendored TUs never see UE's
		// macro soup, plus relaxed warnings for the vendored third-party
		// (nlohmann/json).
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;

		// convex-cpp throws std::runtime_error subclasses; needs C++ exceptions
		// and C++20 (std::variant, <bit>, concepts-adjacent features).
		bEnableExceptions = true;
		CppStandard = CppStandardVersion.Cpp20;

		// Relax UE's stricter warning-as-error settings for the vendored code.
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		bWarningsAsErrors = false;

		// The vendored .cpp files use "detail/..." (relative to src) and
		// <nlohmann/json.hpp> (relative to third_party).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "convex-cpp", "src"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "convex-cpp", "third_party"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			// Provides the convex-cpp public include path (<convex/...>).
			"ConvexCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"WebSockets",
			"HTTP",
		});
	}
}

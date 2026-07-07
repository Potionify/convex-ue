// Copyright Potionify. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ConvexCore : ModuleRules
{
	public ConvexCore(ReadOnlyTargetRules Target) : base(Target)
	{
		// The vendored convex-cpp library is plain C++20 and is not written
		// against UE's build conventions, so disable PCHs and unity builds
		// and compile every translation unit on its own.
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;

		// convex-cpp throws std::runtime_error subclasses; UE disables C++
		// exceptions by default, so turn them back on for this module.
		bEnableExceptions = true;

		// UE 5.8 defaults to C++20, but pin it explicitly since the vendored
		// sources use <bit>, concepts-adjacent features and std::variant.
		CppStandard = CppStandardVersion.Cpp20;

		// The vendored third-party (nlohmann/json) trips UE's stricter
		// warning-as-error settings; relax them for this module only rather
		// than editing vendored sources.
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		bWarningsAsErrors = false;

		// Public headers: consumers write #include <convex/client.h>.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public", "convex-cpp", "include"));

		// Private includes: the vendored .cpp files use "detail/wire_json.h"
		// (relative to src) and <nlohmann/json.hpp> (relative to third_party).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "convex-cpp", "src"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "convex-cpp", "third_party"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
	}
}

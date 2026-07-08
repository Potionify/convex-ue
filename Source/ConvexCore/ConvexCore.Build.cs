// Copyright Potionify. Apache-2.0.

using System.IO;
using UnrealBuildTool;

public class ConvexCore : ModuleRules
{
	public ConvexCore(ReadOnlyTargetRules Target) : base(Target)
	{
		// This module only publishes the vendored convex-cpp headers. The
		// vendored .cpp files are compiled inside ConvexClient: the library's
		// classes carry no UE *_API export macros, so their symbols cannot
		// cross a DLL/module boundary.
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;

		// The public headers throw (convex::type_error etc.) and need C++20;
		// consumer modules must enable exceptions themselves too.
		bEnableExceptions = true;
		CppStandard = CppStandardVersion.Cpp20;

		// Public headers: consumers write #include <convex/client.h>.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public", "convex-cpp", "include"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
	}
}

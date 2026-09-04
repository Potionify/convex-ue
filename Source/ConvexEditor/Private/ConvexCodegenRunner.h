// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

/**
 * In-process wrapper over the vendored convex-ue-codegen emission core
 * (Private/convex-ue-codegen, synced from the standalone repo). Produces the
 * exact same bytes as the standalone CLI and the web app — one core, three
 * front ends. Used by the Generate API button and the ConvexCodegen
 * commandlet, so the plugin is self-contained.
 */
namespace ConvexCodegenRunner
{
	struct FOptions
	{
		FString Prefix = TEXT("ConvexApi");
		bool bIncludeInternal = false;
		/// When set, additionally emit <Name>.Build.cs + <Name>Module.cpp.
		FString EmitModule;
		/// Also emit <Prefix>.as, AngelScript wrappers for the Hazelight
		/// UnrealEngine-Angelscript fork. They belong in the project's
		/// Script/ folder, so WriteFiles routes them to ScriptOutDir.
		bool bEmitScript = false;
		/// Provenance shown in the generated file headers (deployment URL).
		FString SourceLabel = TEXT("unknown");
	};

	/// Run the emission core on raw apiSpec JSON (the /api/query response
	/// envelope or a bare array). Returns the error message on failure,
	/// unset on success with OutFiles filled (filename -> content).
	TOptional<FString> Generate(const FString& SpecJson, const FOptions& Options,
		TMap<FString, FString>& OutFiles);

	/// Fetch the raw apiSpec JSON from a deployment with admin auth
	/// (POST /api/query _system/cli/modules:apiSpec). OnDone(bSuccess, body
	/// or error message) fires on the game thread.
	void FetchApiSpec(const FString& DeploymentUrl, const FString& AdminKey,
		TFunction<void(bool, FString)> OnDone);

	/// True for generated files that belong in the project's Script/ folder
	/// (the AngelScript wrappers) rather than in the C++ output directory.
	bool IsScriptFile(const FString& FileName);

	/// Write generated files into a directory (created if missing). Script
	/// files go to ScriptOutDir instead when it is set. Returns the error
	/// message on failure, unset on success.
	TOptional<FString> WriteFiles(const FString& OutDir, const TMap<FString, FString>& Files,
		const FString& ScriptOutDir = FString());
}

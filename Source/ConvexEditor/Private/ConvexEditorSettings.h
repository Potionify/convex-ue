// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ConvexEditorSettings.generated.h"

/**
 * Per-user editor settings for the Convex dashboard tab.
 *
 * Deliberately stores only POINTERS to credentials (an env-file path), never
 * credentials themselves: EditorPerProjectUserSettings lives under Saved/ and
 * is not committed, but keys still do not belong in any .ini. Deployment URL
 * and admin key are resolved from the Convex CLI's env-file conventions
 * (.env.local / .env, CONVEX_DEPLOY_KEY et al) at connect time.
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Convex Editor"))
class UConvexEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//~ Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("ConvexEditor"); }
	//~ End UDeveloperSettings interface

	/// Explicit env file to read deployment settings from (takes priority over
	/// auto-discovery). Leave empty to search the project directory and its
	/// parents for .env.local / convex.env.local / .env.
	UPROPERTY(EditAnywhere, Config, Category = "Connection",
		meta = (FilePathFilter = "env files (*.env*)|*.env*"))
	FFilePath EnvFile;

	/// How many parent directories above the project dir to search for env
	/// files when EnvFile is not set (the Convex project often lives beside,
	/// not inside, the UE project).
	UPROPERTY(EditAnywhere, Config, Category = "Connection", meta = (ClampMin = 0, ClampMax = 6))
	int32 EnvSearchParentDepth = 3;

	/// Connect to the deployment automatically when the Convex tab opens.
	UPROPERTY(EditAnywhere, Config, Category = "Connection")
	bool bAutoConnect = true;

	/// Where generated wrapper files land. Must be inside a module of your
	/// project (e.g. Source/MyGame/ConvexApi) so they compile. Generation
	/// runs in-process on the vendored convex-ue-codegen core — no external
	/// tool needed.
	UPROPERTY(EditAnywhere, Config, Category = "Code Generation")
	FDirectoryPath CodegenOutputDir;

	/// Optional. When set, Generate API also writes AngelScript wrappers
	/// (ConvexApi.as) here, for projects on the Hazelight
	/// UnrealEngine-Angelscript fork. Point it at your project's Script
	/// folder; the fork hot-reloads the file, no C++ build needed. Leave
	/// empty on stock Unreal Engine.
	UPROPERTY(EditAnywhere, Config, Category = "Code Generation")
	FDirectoryPath ScriptOutputDir;
};

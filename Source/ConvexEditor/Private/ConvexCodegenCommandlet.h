// Copyright Potionify. Apache-2.0.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "ConvexCodegenCommandlet.generated.h"

/**
 * Headless typed-API generation without opening the editor UI — the
 * self-contained fallback used by Tools/generate-convex-api.bat:
 *
 *   UnrealEditor-Cmd.exe <project.uproject> -run=ConvexCodegen
 *       -Out=<dir> [-Prefix=ConvexApi] [-EmitModule=<Name>]
 *       [-IncludeInternal] [-Url=<url> -DeployKey=<key>] [-EnvFile=<path>]
 *
 * Without -Url/-DeployKey, the deployment resolves exactly like the Convex
 * tab: process env (CONVEX_DEPLOY_KEY et al) > -EnvFile > discovered
 * .env.local / convex.env.local / .env near the project directory.
 * Runs on the vendored emission core — same bytes as the standalone CLI.
 */
UCLASS()
class UConvexCodegenCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};

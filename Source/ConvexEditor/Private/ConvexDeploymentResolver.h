// Copyright Potionify. Apache-2.0.

#pragma once

#include "CoreMinimal.h"

/// Deployment type derived from the deploy-key prefix (never authenticated —
/// a plaintext label the Convex CLI/dashboard also rely on). Legacy keys with
/// no prefix and self-hosted keys are treated as Prod for gating purposes.
enum class EConvexDeploymentType : uint8
{
	Prod,
	Dev,
	Preview,
	Custom,
	SelfHosted,
	Unknown
};

const TCHAR* ConvexDeploymentTypeToString(EConvexDeploymentType Type);

/// A resolved admin connection target.
struct FConvexDeploymentConfig
{
	/// https://<name>.convex.cloud or a self-hosted/custom URL.
	FString DeploymentUrl;

	/// Full deploy key including any type prefix (the backend strips it).
	FString AdminKey;

	/// Deployment name parsed from the key/CONVEX_DEPLOYMENT, if any.
	FString DeploymentName;

	EConvexDeploymentType Type = EConvexDeploymentType::Unknown;

	/// Human-readable provenance for the UI, e.g.
	/// "CONVEX_DEPLOY_KEY (F:/.../convex.env.local)".
	FString Source;

	/// Populated instead of the fields above when resolution failed.
	FString Error;

	bool IsValid() const { return Error.IsEmpty() && !DeploymentUrl.IsEmpty() && !AdminKey.IsEmpty(); }
};

/**
 * Resolves the admin deployment target following the Convex CLI conventions:
 *
 *   precedence  real process env > explicit env file > .env.local > .env
 *   key         CONVEX_DEPLOY_KEY (alias CONVEX_DEPLOYMENT_TOKEN, KEY wins)
 *               else CONVEX_SELF_HOSTED_URL + CONVEX_SELF_HOSTED_ADMIN_KEY
 *   url         CONVEX_URL when present; else synthesized as
 *               https://<name>.convex.cloud from the key's deployment name
 *               (custom domains need an explicit CONVEX_URL)
 *
 * Pure logic apart from file/env reads; no UObjects.
 */
namespace ConvexDeploymentResolver
{
	/// Parse one dotenv-style file into OutVars (KEY=VALUE, # comments,
	/// surrounding quotes stripped). Existing keys in OutVars win (first-wins
	/// layering). Returns false if the file could not be read.
	bool ParseEnvFile(const FString& FilePath, TMap<FString, FString>& OutVars);

	/// deploymentTypeFromAdminKey: prefix before the first ':' (when it comes
	/// before the '|') maps prod/dev/preview/custom; a key without a type
	/// prefix is legacy/self-hosted and treated as Prod.
	EConvexDeploymentType TypeFromAdminKey(const FString& AdminKey);

	/// Deployment name embedded in a key ("dev:tall-forest-42|..." ->
	/// "tall-forest-42"). Empty for preview:/project: team keys and
	/// prefix-less keys whose instance name is not name-shaped.
	FString NameFromAdminKey(const FString& AdminKey);

	/// Resolve from an explicit ordered var map (unit-testable core).
	FConvexDeploymentConfig ResolveFromVars(const TMap<FString, FString>& Vars, const FString& SourceLabel);

	/// Full resolution: process env, then the explicit file (may be empty),
	/// then discovered env files near ProjectDir (walking up ParentDepth
	/// levels, filenames .env.local, convex.env.local, .env).
	FConvexDeploymentConfig Resolve(const FString& ExplicitEnvFile, int32 ParentDepth);
}

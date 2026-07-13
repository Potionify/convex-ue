// Copyright Potionify. Apache-2.0.

#include "ConvexDeploymentResolver.h"

#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// The env vars the Convex CLI understands (utils.ts). CONVEX_URL /
	// CONVEX_SITE_URL are written by `convex dev` next to the deployment.
	const TCHAR* const KnownVars[] = {
		TEXT("CONVEX_DEPLOYMENT"),
		TEXT("CONVEX_DEPLOY_KEY"),
		TEXT("CONVEX_DEPLOYMENT_TOKEN"),
		TEXT("CONVEX_SELF_HOSTED_URL"),
		TEXT("CONVEX_SELF_HOSTED_ADMIN_KEY"),
		TEXT("CONVEX_URL"),
	};

	FString StripQuotes(FString Value)
	{
		Value.TrimStartAndEndInline();
		if (Value.Len() >= 2 &&
			((Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) ||
				(Value.StartsWith(TEXT("'")) && Value.EndsWith(TEXT("'")))))
		{
			Value = Value.Mid(1, Value.Len() - 2);
		}
		return Value;
	}

	/// The part of the key before '|' (the instance identifier incl. any type
	/// prefix); the whole key when there is no '|'.
	FString KeyIdentifierPart(const FString& AdminKey)
	{
		int32 Bar = INDEX_NONE;
		return AdminKey.FindChar(TEXT('|'), Bar) ? AdminKey.Left(Bar) : AdminKey;
	}
}

const TCHAR* ConvexDeploymentTypeToString(EConvexDeploymentType Type)
{
	switch (Type)
	{
		case EConvexDeploymentType::Prod: return TEXT("prod");
		case EConvexDeploymentType::Dev: return TEXT("dev");
		case EConvexDeploymentType::Preview: return TEXT("preview");
		case EConvexDeploymentType::Custom: return TEXT("custom");
		case EConvexDeploymentType::SelfHosted: return TEXT("self-hosted");
		default: return TEXT("unknown");
	}
}

namespace ConvexDeploymentResolver
{

bool ParseEnvFile(const FString& FilePath, TMap<FString, FString>& OutVars)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		return false;
	}

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, /*bCullEmpty=*/true);
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		// dotenv allows an optional "export " prefix.
		if (Line.StartsWith(TEXT("export ")))
		{
			Line.RightChopInline(7);
			Line.TrimStartInline();
		}
		int32 Eq = INDEX_NONE;
		if (!Line.FindChar(TEXT('='), Eq) || Eq == 0)
		{
			continue;
		}
		FString Key = Line.Left(Eq).TrimStartAndEnd();
		FString Value = StripQuotes(Line.Mid(Eq + 1));
		if (!Key.IsEmpty() && !OutVars.Contains(Key))
		{
			OutVars.Add(MoveTemp(Key), MoveTemp(Value));
		}
	}
	return true;
}

EConvexDeploymentType TypeFromAdminKey(const FString& AdminKey)
{
	const FString Identifier = KeyIdentifierPart(AdminKey);
	int32 Colon = INDEX_NONE;
	if (!Identifier.FindChar(TEXT(':'), Colon))
	{
		// Legacy / self-hosted keys carry no prefix; the CLI defaults to prod.
		return EConvexDeploymentType::Prod;
	}
	const FString Prefix = Identifier.Left(Colon);
	if (Prefix == TEXT("prod")) return EConvexDeploymentType::Prod;
	if (Prefix == TEXT("dev")) return EConvexDeploymentType::Dev;
	if (Prefix == TEXT("preview")) return EConvexDeploymentType::Preview;
	if (Prefix == TEXT("custom")) return EConvexDeploymentType::Custom;
	// "project:" keys and unrecognized labels: be conservative.
	return EConvexDeploymentType::Unknown;
}

FString NameFromAdminKey(const FString& AdminKey)
{
	FString Identifier = KeyIdentifierPart(AdminKey);
	if (Identifier == AdminKey)
	{
		return FString();  // no '|': encrypted-only form, no embedded name
	}
	// Strip the type prefix; what remains is the deployment name — except for
	// team-scoped preview:/project: keys ("preview:<team>:<project>"), where
	// two colons remain and there is no concrete deployment name.
	int32 Colon = INDEX_NONE;
	if (Identifier.FindChar(TEXT(':'), Colon))
	{
		Identifier = Identifier.Mid(Colon + 1);
		if (Identifier.Contains(TEXT(":")))
		{
			return FString();
		}
	}
	return Identifier;
}

FConvexDeploymentConfig ResolveFromVars(const TMap<FString, FString>& Vars, const FString& SourceLabel)
{
	FConvexDeploymentConfig Config;

	const FString* DeployKey = Vars.Find(TEXT("CONVEX_DEPLOY_KEY"));
	FString KeyVarName = TEXT("CONVEX_DEPLOY_KEY");
	if (DeployKey == nullptr || DeployKey->IsEmpty())
	{
		// CONVEX_DEPLOYMENT_TOKEN is an accepted alias; DEPLOY_KEY wins.
		DeployKey = Vars.Find(TEXT("CONVEX_DEPLOYMENT_TOKEN"));
		KeyVarName = TEXT("CONVEX_DEPLOYMENT_TOKEN");
	}

	const FString* SelfHostedUrl = Vars.Find(TEXT("CONVEX_SELF_HOSTED_URL"));
	const FString* SelfHostedKey = Vars.Find(TEXT("CONVEX_SELF_HOSTED_ADMIN_KEY"));
	const FString* ExplicitUrl = Vars.Find(TEXT("CONVEX_URL"));

	if (DeployKey != nullptr && !DeployKey->IsEmpty())
	{
		Config.AdminKey = *DeployKey;
		Config.Type = TypeFromAdminKey(*DeployKey);
		Config.DeploymentName = NameFromAdminKey(*DeployKey);
		Config.Source = KeyVarName + TEXT(" (") + SourceLabel + TEXT(")");

		if (ExplicitUrl != nullptr && !ExplicitUrl->IsEmpty())
		{
			Config.DeploymentUrl = *ExplicitUrl;
		}
		else if (!Config.DeploymentName.IsEmpty())
		{
			// Cloud convention. Custom domains must set CONVEX_URL — the CLI
			// resolves them via the platform API, which needs cloud creds we
			// do not have here.
			Config.DeploymentUrl =
				FString::Printf(TEXT("https://%s.convex.cloud"), *Config.DeploymentName);
		}
		else
		{
			Config.Error = TEXT("Deploy key has no embedded deployment name; set CONVEX_URL "
				"to the deployment URL.");
		}
		return Config;
	}

	if (SelfHostedUrl != nullptr && !SelfHostedUrl->IsEmpty() &&
		SelfHostedKey != nullptr && !SelfHostedKey->IsEmpty())
	{
		Config.AdminKey = *SelfHostedKey;
		Config.DeploymentUrl = *SelfHostedUrl;
		Config.Type = EConvexDeploymentType::SelfHosted;
		Config.DeploymentName = NameFromAdminKey(*SelfHostedKey);
		Config.Source = TEXT("CONVEX_SELF_HOSTED_URL (") + SourceLabel + TEXT(")");
		return Config;
	}

	if (const FString* Deployment = Vars.Find(TEXT("CONVEX_DEPLOYMENT")))
	{
		Config.Error = FString::Printf(
			TEXT("Found CONVEX_DEPLOYMENT=%s but no admin credentials. The editor needs "
				 "CONVEX_DEPLOY_KEY (or CONVEX_SELF_HOSTED_URL + CONVEX_SELF_HOSTED_ADMIN_KEY); "
				 "generate a deploy key in the Convex dashboard."),
			**Deployment);
		return Config;
	}

	Config.Error = TEXT("No Convex deployment found. Provide CONVEX_DEPLOY_KEY (or the "
		"self-hosted pair) via the environment or an env file, and point the Convex Editor "
		"settings at it if it is not auto-discovered.");
	return Config;
}

FConvexDeploymentConfig Resolve(const FString& ExplicitEnvFile, int32 ParentDepth)
{
	// Layered first-wins map. Track per-key provenance for the Source label.
	TMap<FString, FString> Vars;
	TMap<FString, FString> Provenance;

	auto Layer = [&Vars, &Provenance](const TMap<FString, FString>& NewVars, const FString& Label)
	{
		for (const TPair<FString, FString>& Pair : NewVars)
		{
			if (!Pair.Value.IsEmpty() && !Vars.Contains(Pair.Key))
			{
				Vars.Add(Pair.Key, Pair.Value);
				Provenance.Add(Pair.Key, Label);
			}
		}
	};

	// 1. Real process environment beats every file (dotenv semantics).
	{
		TMap<FString, FString> EnvVars;
		for (const TCHAR* Name : KnownVars)
		{
			const FString Value = FPlatformMisc::GetEnvironmentVariable(Name);
			if (!Value.IsEmpty())
			{
				EnvVars.Add(Name, Value);
			}
		}
		Layer(EnvVars, TEXT("process environment"));
	}

	// 2. Explicit file from settings.
	if (!ExplicitEnvFile.IsEmpty())
	{
		TMap<FString, FString> FileVars;
		if (ParseEnvFile(ExplicitEnvFile, FileVars))
		{
			Layer(FileVars, FPaths::ConvertRelativePathToFull(ExplicitEnvFile));
		}
		else
		{
			FConvexDeploymentConfig Config;
			Config.Error =
				FString::Printf(TEXT("Could not read env file '%s' (Convex Editor settings)."),
					*ExplicitEnvFile);
			return Config;
		}
	}

	// 3. Discovery: .env.local beats .env at each level; nearer directories
	//    beat parents. convex.env.local is a workspace-convention extra.
	FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	for (int32 Depth = 0; Depth <= ParentDepth && !Dir.IsEmpty(); ++Depth)
	{
		for (const TCHAR* FileName :
			{TEXT(".env.local"), TEXT("convex.env.local"), TEXT(".env")})
		{
			const FString Path = Dir / FileName;
			TMap<FString, FString> FileVars;
			if (ParseEnvFile(Path, FileVars))
			{
				Layer(FileVars, Path);
			}
		}
		const FString Parent = FPaths::GetPath(Dir.TrimChar(TEXT('/')));
		if (Parent == Dir)
		{
			break;
		}
		Dir = Parent;
	}

	// Label the source with wherever the winning credential came from.
	FString SourceLabel = TEXT("unknown");
	for (const TCHAR* CredVar : {TEXT("CONVEX_DEPLOY_KEY"), TEXT("CONVEX_DEPLOYMENT_TOKEN"),
			 TEXT("CONVEX_SELF_HOSTED_ADMIN_KEY")})
	{
		if (const FString* Label = Provenance.Find(CredVar))
		{
			SourceLabel = *Label;
			break;
		}
	}

	return ResolveFromVars(Vars, SourceLabel);
}

}  // namespace ConvexDeploymentResolver

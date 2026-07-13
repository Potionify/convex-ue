// Copyright Potionify. Apache-2.0.

#include "ConvexCodegenCommandlet.h"

#include "ConvexCodegenRunner.h"
#include "ConvexDeploymentResolver.h"
#include "HttpManager.h"
#include "HttpModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogConvexCodegen, Log, All);

int32 UConvexCodegenCommandlet::Main(const FString& Params)
{
	FString OutDir;
	if (!FParse::Value(*Params, TEXT("Out="), OutDir) || OutDir.IsEmpty())
	{
		UE_LOG(LogConvexCodegen, Error,
			TEXT("Usage: -run=ConvexCodegen -Out=<dir> [-Prefix=ConvexApi] "
				 "[-EmitModule=<Name>] [-IncludeInternal] [-Url=<url> -DeployKey=<key>] "
				 "[-EnvFile=<path>]"));
		return 1;
	}

	ConvexCodegenRunner::FOptions Options;
	FParse::Value(*Params, TEXT("Prefix="), Options.Prefix);
	FParse::Value(*Params, TEXT("EmitModule="), Options.EmitModule);
	Options.bIncludeInternal = FParse::Param(*Params, TEXT("IncludeInternal"));

	// Explicit credentials win; otherwise resolve like the Convex tab does.
	FString Url, Key, EnvFile;
	FParse::Value(*Params, TEXT("Url="), Url);
	FParse::Value(*Params, TEXT("DeployKey="), Key);
	FParse::Value(*Params, TEXT("EnvFile="), EnvFile);
	if (Url.IsEmpty() || Key.IsEmpty())
	{
		const FConvexDeploymentConfig Config =
			ConvexDeploymentResolver::Resolve(EnvFile, /*ParentDepth=*/3);
		if (!Config.IsValid())
		{
			UE_LOG(LogConvexCodegen, Error, TEXT("Deployment resolution failed: %s"),
				*Config.Error);
			return 1;
		}
		Url = Config.DeploymentUrl;
		Key = Config.AdminKey;
		UE_LOG(LogConvexCodegen, Display, TEXT("Deployment: %s (%s) via %s"),
			*Config.DeploymentName, ConvexDeploymentTypeToString(Config.Type),
			*Config.Source);
	}
	Options.SourceLabel = Url;

	bool bDone = false;
	bool bFetched = false;
	FString Body;
	ConvexCodegenRunner::FetchApiSpec(Url, Key,
		[&bDone, &bFetched, &Body](bool bOk, FString InBody)
		{
			bFetched = bOk;
			Body = MoveTemp(InBody);
			bDone = true;
		});
	// Commandlets have no engine tick loop; pump HTTP until the request
	// completes (Flush alone can return before delegates ran).
	const double Deadline = FPlatformTime::Seconds() + 60.0;
	while (!bDone && FPlatformTime::Seconds() < Deadline)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.05f);
		FPlatformProcess::Sleep(0.02f);
	}
	if (!bDone || !bFetched)
	{
		UE_LOG(LogConvexCodegen, Error, TEXT("apiSpec fetch failed: %s"),
			bDone ? *Body : TEXT("timed out"));
		return 1;
	}

	TMap<FString, FString> Files;
	if (const TOptional<FString> Error = ConvexCodegenRunner::Generate(Body, Options, Files))
	{
		UE_LOG(LogConvexCodegen, Error, TEXT("Codegen failed: %s"), **Error);
		return 1;
	}
	if (const TOptional<FString> Error = ConvexCodegenRunner::WriteFiles(OutDir, Files))
	{
		UE_LOG(LogConvexCodegen, Error, TEXT("%s"), **Error);
		return 1;
	}

	UE_LOG(LogConvexCodegen, Display, TEXT("Generated %d file(s) into %s"), Files.Num(),
		*OutDir);
	return 0;
}

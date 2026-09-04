// Copyright Potionify. Apache-2.0.

#include "ConvexCodegenRunner.h"

#include "ConvexVersion.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Vendored emission core (pure C++, throws convex_codegen::codegen_error).
#include <convex_codegen/api_spec.h>
#include <convex_codegen/emit.h>

#include <map>
#include <string>

namespace ConvexCodegenRunner
{

TOptional<FString> Generate(const FString& SpecJson, const FOptions& Options,
	TMap<FString, FString>& OutFiles)
{
	OutFiles.Reset();

	convex_codegen::emit_options CoreOptions;
	CoreOptions.prefix = TCHAR_TO_UTF8(*Options.Prefix);
	CoreOptions.include_internal = Options.bIncludeInternal;
	CoreOptions.source_label = TCHAR_TO_UTF8(*Options.SourceLabel);
	if (!Options.EmitModule.IsEmpty())
	{
		CoreOptions.emit_module = TCHAR_TO_UTF8(*Options.EmitModule);
	}
	CoreOptions.emit_script = Options.bEmitScript;

	const FTCHARToUTF8 SpecUtf8(*SpecJson);
	try
	{
		const std::map<std::string, std::string> Files = convex_codegen::emit_all(
			std::string_view(SpecUtf8.Get(), SpecUtf8.Length()), CoreOptions);
		for (const auto& [Name, Content] : Files)
		{
			OutFiles.Add(UTF8_TO_TCHAR(Name.c_str()), UTF8_TO_TCHAR(Content.c_str()));
		}
		return TOptional<FString>();
	}
	catch (const std::exception& Error)
	{
		return FString(UTF8_TO_TCHAR(Error.what()));
	}
}

void FetchApiSpec(const FString& DeploymentUrl, const FString& AdminKey,
	TFunction<void(bool, FString)> OnDone)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();
	Request->SetURL(DeploymentUrl / TEXT("api/query"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Convex ") + AdminKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Convex-Client"), TEXT("unreal-" CONVEX_UE_VERSION));
	Request->SetContentAsString(
		TEXT(R"({"path":"_system/cli/modules:apiSpec","args":{},"format":"json"})"));
	Request->OnProcessRequestComplete().BindLambda(
		[OnDone = MoveTemp(OnDone)](
			FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				OnDone(false, TEXT("Could not reach the deployment."));
				return;
			}
			if (Response->GetResponseCode() != 200)
			{
				OnDone(false, FString::Printf(TEXT("HTTP %d: %s"),
					Response->GetResponseCode(),
					*Response->GetContentAsString().Left(300)));
				return;
			}
			// The body may still be a {"status":"error"} envelope; the core's
			// parser rejects those with a clear message, so pass it through.
			OnDone(true, Response->GetContentAsString());
		});
	Request->ProcessRequest();
}

bool IsScriptFile(const FString& FileName)
{
	return FileName.EndsWith(TEXT(".as"));
}

TOptional<FString> WriteFiles(const FString& OutDir, const TMap<FString, FString>& Files,
	const FString& ScriptOutDir)
{
	if (!IFileManager::Get().MakeDirectory(*OutDir, /*Tree=*/true))
	{
		return FString::Printf(TEXT("Could not create output directory '%s'."), *OutDir);
	}
	if (!ScriptOutDir.IsEmpty() && !IFileManager::Get().MakeDirectory(*ScriptOutDir, /*Tree=*/true))
	{
		return FString::Printf(TEXT("Could not create script output directory '%s'."),
			*ScriptOutDir);
	}
	for (const TPair<FString, FString>& File : Files)
	{
		const bool bScript = IsScriptFile(File.Key) && !ScriptOutDir.IsEmpty();
		const FString Path = (bScript ? ScriptOutDir : OutDir) / File.Key;
		if (!FFileHelper::SaveStringToFile(File.Value, *Path,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return FString::Printf(TEXT("Failed to write '%s'."), *Path);
		}
	}
	return TOptional<FString>();
}

}  // namespace ConvexCodegenRunner

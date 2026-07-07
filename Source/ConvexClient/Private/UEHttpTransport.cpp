// Copyright Potionify. Apache-2.0.

#include "UEHttpTransport.h"

#include "ConvexClientModule.h"
#include "ConvexUtils.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include <memory>
#include <string>
#include <utility>

using ConvexUtils::FStringToUtf8;
using ConvexUtils::RunOnGameThread;
using ConvexUtils::Utf8ToFString;

void FUEHttpTransport::send(convex::http_request request,
	std::function<void(convex::http_response)> on_done)
{
	// Shared holders keep the move-only-ish payloads alive across the game
	// thread hop and the async completion.
	auto Request = std::make_shared<convex::http_request>(std::move(request));
	auto OnDone = std::make_shared<std::function<void(convex::http_response)>>(std::move(on_done));

	RunOnGameThread([Request, OnDone]()
	{
		FHttpModule& Http = FHttpModule::Get();
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = Http.CreateRequest();
		HttpRequest->SetURL(Utf8ToFString(Request->url));
		HttpRequest->SetVerb(Utf8ToFString(Request->method));
		for (const auto& Header : Request->headers)
		{
			HttpRequest->SetHeader(Utf8ToFString(Header.first), Utf8ToFString(Header.second));
		}

		// Body is binary-capable (file uploads); set raw bytes, never a string.
		if (!Request->body.empty())
		{
			TArray<uint8> Content;
			Content.Append(reinterpret_cast<const uint8*>(Request->body.data()),
				static_cast<int32>(Request->body.size()));
			HttpRequest->SetContent(MoveTemp(Content));
		}

		HttpRequest->OnProcessRequestComplete().BindLambda(
			[OnDone](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bSucceeded)
			{
				convex::http_response Result;
				if (bSucceeded && Response.IsValid())
				{
					Result.status = Response->GetResponseCode();
					// Read raw bytes (never GetContentAsString) so binary
					// downloads survive intact.
					const TArray<uint8>& Bytes = Response->GetContent();
					Result.body.assign(reinterpret_cast<const char*>(Bytes.GetData()),
						static_cast<std::size_t>(Bytes.Num()));
				}
				else
				{
					Result.error = "request failed";
				}
				if (OnDone && *OnDone)
				{
					(*OnDone)(std::move(Result));
				}
			});

		if (!HttpRequest->ProcessRequest())
		{
			convex::http_response Result;
			Result.error = "failed to dispatch request";
			if (OnDone && *OnDone)
			{
				(*OnDone)(std::move(Result));
			}
		}
	});
}

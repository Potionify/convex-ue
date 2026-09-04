// Copyright Potionify. Apache-2.0.

#include "ConvexExampleTestLibrary.h"

#include "Containers/BackgroundableTicker.h"
#include "Containers/Ticker.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

void UConvexExampleTestLibrary::Pump(float Seconds)
{
	const double Start = FPlatformTime::Seconds();
	double Last = Start;
	while (true)
	{
		const double Now = FPlatformTime::Seconds();
		// The plugin's transports hop to the game thread through the task
		// graph; the Convex client and the HTTP manager tick on the core
		// ticker; the engine WebSockets manager ticks on the backgroundable
		// one. A commandlet drives none of these on its own.
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		const float Delta = static_cast<float>(Now - Last);
		FTSTicker::GetCoreTicker().Tick(Delta);
		FTSBackgroundableTicker::GetCoreTicker().Tick(Delta);
		Last = Now;
		if (Now - Start >= Seconds)
		{
			return;
		}
		FPlatformProcess::Sleep(0.01f);
	}
}

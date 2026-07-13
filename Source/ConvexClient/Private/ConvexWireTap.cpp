// Copyright Potionify. Apache-2.0.

#include "ConvexWireTap.h"

#include <atomic>

namespace ConvexWireTap
{
	namespace
	{
		std::atomic<bool> GEnabled{false};

		FOnWireFrame& Delegate()
		{
			static FOnWireFrame Instance;
			return Instance;
		}
	}

	FOnWireFrame& OnWireFrame()
	{
		return Delegate();
	}

	void SetEnabled(bool bEnabled)
	{
		GEnabled.store(bEnabled, std::memory_order_relaxed);
	}

	bool IsEnabled()
	{
		return GEnabled.load(std::memory_order_relaxed);
	}

	void Report(EDirection Direction, const FString& Url, const FString& Text)
	{
		if (IsEnabled())
		{
			Delegate().Broadcast(Direction, Url, Text);
		}
	}
}

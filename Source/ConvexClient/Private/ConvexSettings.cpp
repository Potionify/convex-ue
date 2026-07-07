// Copyright Potionify. Apache-2.0.

#include "ConvexSettings.h"

UConvexSettings::UConvexSettings()
{
	CategoryName = TEXT("Plugins");
}

FName UConvexSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

const UConvexSettings* UConvexSettings::Get()
{
	return GetDefault<UConvexSettings>();
}

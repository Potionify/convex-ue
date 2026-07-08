// Copyright Potionify. Apache-2.0.

using UnrealBuildTool;
using System.Collections.Generic;

public class ConvexExampleTarget : TargetRules
{
	public ConvexExampleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ConvexExample");
	}
}

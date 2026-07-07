// Copyright Potionify. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ConvexExampleEditorTarget : TargetRules
{
	public ConvexExampleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ConvexExample");
	}
}

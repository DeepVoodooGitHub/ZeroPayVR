// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ZeroPayVREditorTarget : TargetRules
{
	public ZeroPayVREditorTarget(TargetInfo Target) : base(Target)
	{
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

        Type = TargetType.Editor;
        ExtraModuleNames.AddRange(new string[] { "ZeroPayVR", "VRExpansionPlugin" });
    }

}

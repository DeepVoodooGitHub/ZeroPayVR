// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ZeroPayVR : ModuleRules
{
	public ZeroPayVR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AdvancedSessions", "VRExpansionPlugin" });

        PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
    }
}

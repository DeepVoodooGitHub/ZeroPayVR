// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ZeroPayVR : ModuleRules
{
	public ZeroPayVR(ReadOnlyTargetRules Target) : base(Target)
    {

        //SetupIrisSupport(Target);

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
       // PrivatePCHHeaderFile = "Private/WindowsMixedRealityPrecompiled.h";

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AdvancedSessions", "VRExpansionPlugin" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}

// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ZeroPayVRTarget : TargetRules
{
    public ZeroPayVRTarget(TargetInfo Target) : base(Target)
    {

        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
        ExtraModuleNames.Add("ZeroPayVR");

        bUseLoggingInShipping = true ;

        /*
         * This is our Steam App ID.
         * # Define in both server and client targets
         */
        ProjectDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID= ");

    }
}

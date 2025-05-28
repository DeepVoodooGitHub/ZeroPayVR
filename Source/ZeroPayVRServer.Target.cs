// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class ZeroPayVRServerTarget: TargetRules   // Change this line as shown previously
{
       public ZeroPayVRServerTarget(TargetInfo Target) : base(Target)  // Change this line as shown previously
       {
            Type = TargetType.Server;
            ExtraModuleNames.Add("ZeroPayVR");    // Change this line as shown previously
	        bLegacyPublicIncludePaths = false;
            //bUseLoggingInShipping = true;
            DefaultBuildSettings = BuildSettingsVersion.V5;
            IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

        /*
        * This is our Steam App ID.
        */
        GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=3764610");

        /*
            * This is used on SetProduct(), and should be the same as your Product Name
            * under Dedicated Game Server Information in Steamworks
            */
        GlobalDefinitions.Add("UE_PROJECT_STEAMPRODUCTNAME=\"ZeroPayVR\"");

        /*
            * This is used on SetModDir(), and should be the same as your Product Name
            * under Dedicated Game Server Information in Steamworks
            */
        GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDIR=\"ZeroPayVR\"");

        /*
            * This is what shows up under the game filter in Steam server browsers.
            */
        GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDESC=\"ZeroPay VR\"");
    }
}
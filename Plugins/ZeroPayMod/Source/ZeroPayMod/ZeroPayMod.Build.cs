// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ZeroPayMod : ModuleRules
{
	public ZeroPayMod(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
            }
            );

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "External", "zip"));

        PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				"ZeroPayMod"
                }
            );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "AdvancedSessions",
                "OpenXRExpansionPlugin",
                "VRExpansionPlugin",
                "HTTP",
				"Json",
                "JsonUtilities",
                "PakFile",
                "OnlineSubsystem",
                "OnlineSubsystemUtils",
                "OnlineSubsystemEIK",
				// ... add other public dependencies that you statically link with here ...
			}
            );

		// Editor based builds require some editor dependencies..
		if (Target.bBuildEditor)
		{
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
			    "BlueprintGraph",
            }
            );
		}


        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "GameplayTags",
                "OpenXRExpansionPlugin",
                "VRExpansionPlugin",
                "AssetRegistry",
				"ZeroPayModCore",
				"UMG",
                "AssetRegistry",
				"PakFile",
            }
            );

		// Editor based builds require some editor dependencies..
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(
		    new string[]
			{
                "EditorFramework",
                "UnrealEd",
                "UMGEditor",
                "BlueprintGraph",
                "Blutility",
                "EditorSubsystem",
                "ZeroPayEditorButtonsPlugin",
                "ToolMenus",
                "ToolWidgets",
                }
            );
        }

        DynamicallyLoadedModuleNames.AddRange(
		new string[]
		{
			// ... add any modules that your module loads dynamically here ...
		}
		);

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../../Content"));
    }
}

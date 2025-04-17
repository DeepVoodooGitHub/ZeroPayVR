// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "EditorBuildUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

static const FName ZeroPayEditorButtonsPluginTabName("ZeroPayEditorButtonsPlugin");

#define LOCTEXT_NAMESPACE "FZeroPayEditorButtonsPluginModule"

void FZeroPayEditorButtonsPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FZeroPayEditorButtonsPluginStyle::Initialize();
	FZeroPayEditorButtonsPluginStyle::ReloadTextures();

	FZeroPayEditorButtonsPluginCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().ShowQuest3View,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::ShowQuest3View_Clicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().BakeMap,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::BakeMap_Clicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::RegisterMenus));
}

void FZeroPayEditorButtonsPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FZeroPayEditorButtonsPluginStyle::Shutdown();

	FZeroPayEditorButtonsPluginCommands::Unregister();
}

void FZeroPayEditorButtonsPluginModule::ShowQuest3View_Clicked()
{
	if (GEditor)
	{
		UWorld* World = nullptr;

		// This gives you the PIE world if running, or the editor world if not
		FWorldContext& WorldContext = GEditor->GetEditorWorldContext();
		World = WorldContext.World();
		if (World)
		{
			FName DataLayerAssetPath = FName(TEXT("/Game/ZeroPay/Levels/Debug/DL_ZeroPay_PCVR.DL_ZeroPay_PCVR"));

			FString FullPath = FString::Printf(TEXT("DataLayerAsset'%s'"), *DataLayerAssetPath.ToString());
			UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(StaticLoadObject(UDataLayerAsset::StaticClass(), nullptr, *FullPath));
			if (DataLayerAsset->GetType() == EDataLayerType::Editor)
				DataLayerAsset->SetType(EDataLayerType::Runtime) ;
			else
				DataLayerAsset->SetType(EDataLayerType::Editor);

			FString PackageName = DataLayerAsset->GetOutermost()->GetName();
			UPackage* Package = DataLayerAsset->GetOutermost();

			FAssetRegistryModule::AssetCreated(DataLayerAsset); // Update registry

			Package->MarkPackageDirty(); // Mark the package as modified

			FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			bool bSaved = UPackage::SavePackage(
				Package,
				DataLayerAsset,
				EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
				*FileName,
				GError,
				nullptr,
				false,
				true,
				SAVE_None
			);

			if (bSaved)
			{
				UE_LOG(LogTemp, Log, TEXT("Successfully saved DataLayer: %s"), *PackageName);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to save DataLayer: %s"), *PackageName);
			}

		}
	}
}

void FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("Test"));

}

void FZeroPayEditorButtonsPluginModule::BakeMap_Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("Executing Bake.."));
	ShowTemporaryNotification("ZeroPay - Starting full bake of world (due to UE5 bug).");

	// Check if the Editor is in Play mode
	if (GEditor->PlayWorld)
	{
		ShowTemporaryNotification("ZeroPay - Failed to bake lights, in play mode");
			UE_LOG(LogTemp, Warning, TEXT("Cannot bake while in Play mode."));
		return;
	}

	// Get the current world
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		ShowTemporaryNotification("ZeroPay - Failed to bake lights, no active world");
		UE_LOG(LogTemp, Error, TEXT("No active world found."));
		return;
	}

	// Set the current level to the active level
	ULevel* CurrentLevel = World->GetCurrentLevel();
	if (!CurrentLevel)
	{
		ShowTemporaryNotification("ZeroPay - Failed to bake lights, no level");
		UE_LOG(LogTemp, Error, TEXT("No current level found."));
		return;
	}

	// Build everything
	FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildHierarchicalLOD);
	FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildGeometry);
	FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildAIPaths);
	FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildAllLandscape);

	// Get the current world
	World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("No active world found."));
		return;
	}

	// Get the World Settings
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("World Settings not found."));
		return;
	}

	// Modify the Force No Precomputed Lighting property
	WorldSettings->bForceNoPrecomputedLighting = false ;

	// Mark the settings as dirty to ensure they are saved
	WorldSettings->Modify();

	UE_LOG(LogTemp, Log, TEXT("    + Lighting phase... "));

	// Build Lightning again
	FEditorBuildUtils::EditorBuild(GEditor->GetEditorWorldContext().World(), FBuildOptions::BuildLighting);

}


void FZeroPayEditorButtonsPluginModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().ShowQuest3View, PluginCommands);
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView, PluginCommands);
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().BakeMap, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& ShowQuest3View_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().ShowQuest3View));
				ShowQuest3View_Entry.SetCommandList(PluginCommands);
				FToolMenuEntry& ShowPCVRView_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView));
				ShowPCVRView_Entry.SetCommandList(PluginCommands);
				FToolMenuEntry& BakeMap_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().BakeMap));
				BakeMap_Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FZeroPayEditorButtonsPluginModule::ShowTemporaryNotification(const FString& Message, float Duration)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.bFireAndForget = true;           // Automatically dismiss after duration
	Info.ExpireDuration = Duration;       // Duration before disappearing
	Info.FadeOutDuration = 0.5f;          // Smooth fade out

	// Display the notification in the bottom right corner
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);

	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);  // Optional: Success icon
	}

	UE_LOG(LogTemp, Log, TEXT("Notification displayed: %s"), *Message);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorButtonsPluginModule, ZeroPayEditorButtonsPlugin)
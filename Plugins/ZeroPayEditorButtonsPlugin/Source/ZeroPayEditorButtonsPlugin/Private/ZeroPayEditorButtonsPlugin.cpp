// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"
#include "EditorUtilitySubsystem.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "EditorBuildUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "EngineUtils.h"
#include "Misc/OutputDeviceNull.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/AssetManager.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Windows/WindowsPlatformProcess.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IBlutilityModule.h"

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
		FZeroPayEditorButtonsPluginCommands::Get().BakeLightsOnLevels,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::BakeLightsOnLevels_Clicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::GenerateQuest3ReducedLevel_Clicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().OpenModioWindow,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::OpenModIOWindow_Clicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::RegisterMenus));

	bIsOperationRunning = true;
	ClosurePreventationMessage = "Cannot close window, operation pending";
	/* Create tabs */
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ZeroPay_ModManagement_Tab",
		FOnSpawnTab::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::SpawnModManagementDockableTab))
		.SetDisplayName(FText::FromString("ZeroPay VR Mod Management"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ZeroPay_Quest3Reducer_Tab",
		FOnSpawnTab::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::SpawnQuest3ReducerDockableTab))
		.SetDisplayName(FText::FromString("ZeroPay VR Quest 3 Reducer"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
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

TSharedRef<SDockTab> FZeroPayEditorButtonsPluginModule::SpawnModManagementDockableTab(const FSpawnTabArgs& Args)
{
	FString WidgetPath = TEXT("/ZeroPayEditorPlugin/Blueprints/EUW_ZP_ModioWindow.EUW_ZP_ModioWindow");
	UEditorUtilityWidgetBlueprint* WidgetBP = Cast<UEditorUtilityWidgetBlueprint>(
		StaticLoadObject(UEditorUtilityWidgetBlueprint::StaticClass(), nullptr, *WidgetPath));

	WidgetModManagementInstance = nullptr;

	if (WidgetBP && WidgetBP->GeneratedClass)
	{
		WidgetModManagementInstance = NewObject<UEditorUtilityWidget>(GetTransientPackage(), WidgetBP->GeneratedClass);
		WidgetModManagementInstance->AddToRoot(); // Prevent GC
	}

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnCanCloseTab_Lambda([this]()
			{
				/* During development, changes to the editor utility BP can cause the instance to disappear */
				if (WidgetModManagementInstance == nullptr)
					return true;
				if (!IsValid(WidgetModManagementInstance))
					return true;

				/* Find the function inside the widget to see if we can close the window (i.e. no operation is running) */
				UFunction* Func = WidgetModManagementInstance->FindFunction("IsOperationRunning");
				if (Func)
				{
					struct { bool ReturnValue;  FString Message; } Params;
					WidgetModManagementInstance->ProcessEvent(Func, &Params);
					bIsOperationRunning = Params.ReturnValue;
					ClosurePreventationMessage = Params.Message;
				}

				/* If an operation is running, show a 5 second "hint" as to why we can't close (like uploading..) */
				if (bIsOperationRunning)
				{
					FNotificationInfo Info(FText::FromString(ClosurePreventationMessage));
					Info.FadeInDuration = 0.2f;
					Info.FadeOutDuration = 0.5f;
					Info.ExpireDuration = 5.0f;
					Info.bUseThrobber = false;
					Info.bUseSuccessFailIcons = true;
					Info.bUseLargeFont = false;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}

				if (!bIsOperationRunning)
				{
					TSharedRef<SWidget> SlateWidget = WidgetModManagementInstance->TakeWidget();
					//SlateWidget->UnRegisterActiveTimer(activeWidgetTimer);
				}
				return !bIsOperationRunning;
			});

	if (WidgetModManagementInstance)
	{
		TSharedRef<SWidget> SlateWidget = WidgetModManagementInstance->TakeWidget();
		DockTab->SetContent(SlateWidget);
		SlateWidget->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda( [this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
			{
				WidgetModManagementInstance->Tick(FGeometry(), 0.0f);
				return EActiveTimerReturnType::Continue;
			}));
	}

	return DockTab;
}

TSharedRef<SDockTab> FZeroPayEditorButtonsPluginModule::SpawnQuest3ReducerDockableTab(const FSpawnTabArgs& Args)
{
	FString WidgetPath = TEXT("/ZeroPayEditorPlugin/Blueprints/EUW_ZP_ReducerWindow.EUW_ZP_ReducerWindow");
	UEditorUtilityWidgetBlueprint* WidgetBP = Cast<UEditorUtilityWidgetBlueprint>(
		StaticLoadObject(UEditorUtilityWidgetBlueprint::StaticClass(), nullptr, *WidgetPath));

	WidgetQuest3ReducerInstance = nullptr;

	if (WidgetBP && WidgetBP->GeneratedClass)
	{
		WidgetQuest3ReducerInstance = NewObject<UEditorUtilityWidget>(GetTransientPackage(), WidgetBP->GeneratedClass);
		WidgetQuest3ReducerInstance->AddToRoot(); // Prevent GC
	}

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnCanCloseTab_Lambda([this]()
			{
				/* During development, changes to the editor utility BP can cause the instance to disappear */
				if (WidgetQuest3ReducerInstance == nullptr)
					return true;
				if (!IsValid(WidgetQuest3ReducerInstance))
					return true;

				/* Find the function inside the widget to see if we can close the window (i.e. no operation is running) */
				UFunction* Func = WidgetQuest3ReducerInstance->FindFunction("IsOperationRunning");
				if (Func)
				{
					struct { bool ReturnValue;  FString Message; } Params;
					WidgetQuest3ReducerInstance->ProcessEvent(Func, &Params);
					bIsOperationRunning = Params.ReturnValue;
					ClosurePreventationMessage = Params.Message;
				}

				/* If an operation is running, show a 5 second "hint" as to why we can't close (like uploading..) */
				if (bIsOperationRunning)
				{
					FNotificationInfo Info(FText::FromString(ClosurePreventationMessage));
					Info.FadeInDuration = 0.2f;
					Info.FadeOutDuration = 0.5f;
					Info.ExpireDuration = 5.0f;
					Info.bUseThrobber = false;
					Info.bUseSuccessFailIcons = true;
					Info.bUseLargeFont = false;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}
				return !bIsOperationRunning;
			});

	if (WidgetQuest3ReducerInstance)
	{
		TSharedRef<SWidget> SlateWidget = WidgetQuest3ReducerInstance->TakeWidget();
		DockTab->SetContent(SlateWidget);
	}

	return DockTab;
}

void FZeroPayEditorButtonsPluginModule::BakeLightsOnLevels_Clicked()
{
	PerformLightBake();


}

void FZeroPayEditorButtonsPluginModule::GenerateQuest3ReducedLevel_Clicked()
{
	/* Try and show the window */
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName("ZeroPay_Quest3Reducer_Tab"));

	/* Ensure it's size is maintained */
	if (Tab.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(Tab.ToSharedRef());
		if (ParentWindow.IsValid())
		{
			ParentWindow->Resize(FVector2D(1200, 950));
		}
	}
}


void FZeroPayEditorButtonsPluginModule::ShowQuest3View_Clicked()
{
	/* Get persistent world */
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShowQuest3View_Clicked] failed to find persistent level"));
		return;
	}

	/* Get the path to this level */
	FString PersistentWorldPath = FPackageName::GetLongPackagePath(World->GetOutermost()->GetName());
	/* Strip off "Levels" so we get path to definition file.. */
	if (PersistentWorldPath.EndsWith("/Levels"))
		PersistentWorldPath.LeftChopInline(7);
	/* Add in defition file (we expect) */
	PersistentWorldPath += "/ZeroPayDefinition";

	/* Attempt to open the data asset */
	UZeroPayMod_DefinitionDataAsset* DefinitionDataAsset = LoadObject<UZeroPayMod_DefinitionDataAsset>(nullptr, *PersistentWorldPath);
	if (DefinitionDataAsset == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(FText::FromString("Error, could not find DefinitionDataAsset at path '{0}'"), FText::FromString(PersistentWorldPath)));
		return;
	}

	pcvrLevelLightBake = DefinitionDataAsset->Definition.pcvrlevel.Get();
	if (pcvrLevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find PCVR Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}
	quest3LevelLightBake = DefinitionDataAsset->Definition.quest3level.Get();
	if (quest3LevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find Quest 3 Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}

	SetSpecificSublevelVisible(pcvrLevelLightBake, false);
	SetSpecificSublevelVisible(quest3LevelLightBake, true);

	ShowNotification("Quest 3 sub-level visible", SNotificationItem::ECompletionState::CS_Success);
}

void FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked()
{
	/* Get persistent world */
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShowQuest3View_Clicked] failed to find persistent level"));
		return;
	}

	/* Get the path to this level */
	FString PersistentWorldPath = FPackageName::GetLongPackagePath(World->GetOutermost()->GetName());
	/* Strip off "Levels" so we get path to definition file.. */
	if (PersistentWorldPath.EndsWith("/Levels"))
		PersistentWorldPath.LeftChopInline(7);
	/* Add in defition file (we expect) */
	PersistentWorldPath += "/ZeroPayDefinition";

	/* Attempt to open the data asset */
	UZeroPayMod_DefinitionDataAsset* DefinitionDataAsset = LoadObject<UZeroPayMod_DefinitionDataAsset>(nullptr, *PersistentWorldPath);
	if (DefinitionDataAsset == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(FText::FromString("Error, could not find DefinitionDataAsset at path '{0}'"), FText::FromString(PersistentWorldPath)));
		return;
	}

	pcvrLevelLightBake = DefinitionDataAsset->Definition.pcvrlevel.Get();
	if (pcvrLevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find PCVR Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}
	quest3LevelLightBake = DefinitionDataAsset->Definition.quest3level.Get();
	if (quest3LevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find Quest 3 Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}

	SetSpecificSublevelVisible(pcvrLevelLightBake, true);
	SetSpecificSublevelVisible(quest3LevelLightBake, false);

	ShowNotification("PCVR sub-level visible", SNotificationItem::ECompletionState::CS_Success);
}

TSharedPtr<SWidget> FindWidgetRecursive(TSharedPtr<SWidget> Root, TSharedRef<SWidget> Target)
{
	if (!Root.IsValid())
		return nullptr;

	if (Root == Target)
		return Root;

	if (FChildren* Children = Root->GetChildren())
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> Child = Children->GetChildAt(i);
			TSharedPtr<SWidget> Result = FindWidgetRecursive(Child, Target);
			if (Result.IsValid())
				return Result;
		}
	}

	return nullptr;
}

void FZeroPayEditorButtonsPluginModule::OpenModIOWindow_Clicked()
{
	/* Try and show the window */
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName("ZeroPay_ModManagement_Tab"));

	/* Ensure it's size is maintained */
	if (Tab.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(Tab.ToSharedRef());
		if (ParentWindow.IsValid())
		{
			ParentWindow->Resize(FVector2D(1200, 750));
		}
	}
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
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().BakeLightsOnLevels, PluginCommands);			
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel, PluginCommands);
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().OpenModioWindow, PluginCommands);
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
				FToolMenuEntry& BakeLightsOnLevels_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().BakeLightsOnLevels));
				BakeLightsOnLevels_Entry.SetCommandList(PluginCommands);				
				FToolMenuEntry& GenerateQuest3ReducedLevel_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel));
				GenerateQuest3ReducedLevel_Entry.SetCommandList(PluginCommands);
				FToolMenuEntry& BakeMap_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().OpenModioWindow));
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

TSharedPtr<SWidget> FZeroPayEditorButtonsPluginModule::FindWidgetRecursive(TSharedPtr<SWidget> Root, TSharedRef<SWidget> Target)
{
	if (!Root.IsValid())
		return nullptr;

	if (Root == Target)
		return Root;

	if (FChildren* Children = Root->GetChildren())
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> Child = Children->GetChildAt(i);
			TSharedPtr<SWidget> Result = FindWidgetRecursive(Child, Target);
			if (Result.IsValid())
				return Result;
		}
	}

	return nullptr;
}

/***************************************************************************************************************
*
* Function Library Code 
*
*/

UZeroPayEditorCookPakOperationHandle* UZeroPayEditorButtonsFunctionLibrary::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		return Plugin->CookAndUploadPackages(dataAsset);
	}

	return nullptr;
}


FReducerResults UZeroPayEditorButtonsFunctionLibrary::ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		return Plugin->ReduceLevel(dataAsset, reducerSettings, runtimeSettings);
	}

	return FReducerResults() ;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorButtonsPluginModule, ZeroPayEditorButtonsPlugin)
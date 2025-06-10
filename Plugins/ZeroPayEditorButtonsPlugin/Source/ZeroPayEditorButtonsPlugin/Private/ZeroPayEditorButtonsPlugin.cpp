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
#include "Misc/OutputDeviceNull.h"
#include "Windows/WindowsPlatformProcess.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"

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
		FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::GenerateQuest3ReducedLevel_Clicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked),
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
					return true ;
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
					Info.bUseSuccessFailIcons = true ;
					Info.bUseLargeFont = false;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}
				return !bIsOperationRunning; 
			});

	if (WidgetModManagementInstance)
	{
		TSharedRef<SWidget> SlateWidget = WidgetModManagementInstance->TakeWidget();
		DockTab->SetContent(SlateWidget);
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
			ParentWindow->Resize(FVector2D(1200, 750));
		}
	}
}

void FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked()
{
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
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel, PluginCommands);
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView, PluginCommands);
			Section.AddMenuEntryWithCommandList(FZeroPayEditorButtonsPluginCommands::Get().OpenModioWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& GenerateQuest3ReducedLevel_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().GenerateQuest3ReducedLevel));
				GenerateQuest3ReducedLevel_Entry.SetCommandList(PluginCommands);
				FToolMenuEntry& ShowPCVRView_Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FZeroPayEditorButtonsPluginCommands::Get().ShowPCVRView));
				ShowPCVRView_Entry.SetCommandList(PluginCommands);
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

void FZeroPayEditorButtonsPluginModule::UpdateUIProgressField()
{
	// Post result back to main thread safely
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			/* During development, changes to the editor utility BP can cause the instance to disappear */
			if (WidgetModManagementInstance == nullptr)
				return;
			if (!IsValid(WidgetModManagementInstance))
				return;

			UFunction* Func = WidgetModManagementInstance->FindFunction("UpdateUIProgressField");
			if (!Func)
			{
				UE_LOG(LogTemp, Error, TEXT("Function UpdateUIProgressField not found on %s"), *WidgetModManagementInstance->GetName());
				return;
			}

			// Match the parameter layout: 1 FString
			struct FMyParams
			{
				FString InputString;
			};

			FMyParams Params;
			Params.InputString = LastMessage;

			WidgetModManagementInstance->ProcessEvent(Func, &Params);
		});
}


/***************************************************************************************************************
*
* Function Library Code 
*
*/

UZeroPayEditorOperationHandle* UZeroPayEditorButtonsFunctionLibrary::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		return Plugin->CookAndUploadPackages(dataAsset);
	}

	return nullptr;
}

UZeroPayEditorOperationHandle* UZeroPayEditorButtonsFunctionLibrary::PollUploadStatus()
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		return Plugin->PollUploadStatus();
	}

	return nullptr;
}

void UZeroPayEditorButtonsFunctionLibrary::CancelUploadStatus()
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		Plugin->CancelUploadStatus();
	}

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorButtonsPluginModule, ZeroPayEditorButtonsPlugin)
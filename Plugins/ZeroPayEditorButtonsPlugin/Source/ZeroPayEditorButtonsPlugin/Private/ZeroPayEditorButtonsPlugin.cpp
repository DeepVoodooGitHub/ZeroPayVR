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
		FZeroPayEditorButtonsPluginCommands::Get().ShowQuest3View,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::ShowQuest3View_Clicked),
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
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ZeroPayVRModTab",
		FOnSpawnTab::CreateRaw(this, &FZeroPayEditorButtonsPluginModule::SpawnDockableTab))
		.SetDisplayName(FText::FromString("ZeroPay VR Mod Management"))
		.SetMenuType(ETabSpawnerMenuType::Hidden); // Or Show if you want it in the Window menu
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

TSharedRef<SDockTab> FZeroPayEditorButtonsPluginModule::SpawnDockableTab(const FSpawnTabArgs& Args)
{
	FString WidgetPath = TEXT("/ZeroPayEditorPlugin/Blueprints/EUW_ZP_ModioWindow.EUW_ZP_ModioWindow");
	UEditorUtilityWidgetBlueprint* WidgetBP = Cast<UEditorUtilityWidgetBlueprint>(
		StaticLoadObject(UEditorUtilityWidgetBlueprint::StaticClass(), nullptr, *WidgetPath));

	WidgetInstance = nullptr;

	if (WidgetBP && WidgetBP->GeneratedClass)
	{
		WidgetInstance = NewObject<UEditorUtilityWidget>(GetTransientPackage(), WidgetBP->GeneratedClass);
		WidgetInstance->AddToRoot(); // Prevent GC
	}

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnCanCloseTab_Lambda([this]()
			{
				/* During development, changes to the editor utility BP can cause the instance to disappear */
				if (WidgetInstance == nullptr)
					return true ;
				if (!IsValid(WidgetInstance))
					return true;

				/* Find the function inside the widget to see if we can close the window (i.e. no operation is running) */
				UFunction* Func = WidgetInstance->FindFunction("IsOperationRunning");
				if (Func)
				{
					struct { bool ReturnValue;  FString Message; } Params;
					WidgetInstance->ProcessEvent(Func, &Params);
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

	if (WidgetInstance)
	{
		TSharedRef<SWidget> SlateWidget = WidgetInstance->TakeWidget();
		DockTab->SetContent(SlateWidget);
	}

	return DockTab;
}


void FZeroPayEditorButtonsPluginModule::ShowQuest3View_Clicked()
{
}

void FZeroPayEditorButtonsPluginModule::ShowPCVRView_Clicked()
{
}

void FZeroPayEditorButtonsPluginModule::OpenModIOWindow_Clicked()
{
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName("ZeroPayVRModTab"));

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
			if (WidgetInstance == nullptr)
				return;
			if (!IsValid(WidgetInstance))
				return;

			UFunction* Func = WidgetInstance->FindFunction("UpdateUIProgressField");
			if (!Func)
			{
				UE_LOG(LogTemp, Error, TEXT("Function UpdateUIProgressField not found on %s"), *WidgetInstance->GetName());
				return;
			}

			// Match the parameter layout: 1 FString
			struct FMyParams
			{
				FString InputString;
			};

			FMyParams Params;
			Params.InputString = LastMessage;

			WidgetInstance->ProcessEvent(Func, &Params);
		});
}

/**************************************************** COOKING LOGIC ****************************************************/

UZeroPayEditorOperationHandle* FZeroPayEditorButtonsPluginModule::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	bAbortOperation = false;
	Handle = NewObject<UZeroPayEditorOperationHandle>();

	Async(EAsyncExecution::Thread, [this, dataAsset]()
		{
			/* >>> Pack PCVR <<< */
			if (!CookAndPackWindows(dataAsset))
			{
				bAbortOperation = true;
			}

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this]()
					{
						Handle->OnCompleted.Broadcast(bAbortOperation);
					});
				return;
			}
			/* >>> Pack Quest 3 <<< */
			if (!CookAndPackAndroid(dataAsset))
			{
				bAbortOperation = true;
			}

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this]()
					{
						Handle->OnCompleted.Broadcast(bAbortOperation);
					});
				return;
			}
			/* >>> Pack Linux Server <<< */
			if (!CookAndPackLinuxServer(dataAsset))
			{
				bAbortOperation = true;
			}

			/* All Good! */
			LastMessage = "Cooking and packing stage completed successfully!" ;
			UpdateUIProgressField();
			FPlatformProcess::Sleep(2.0f);

			// Simulate some logic, then notify later
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				Handle->OnCompleted.Broadcast(bAbortOperation);
			});
		});

	return Handle;
}

bool FZeroPayEditorButtonsPluginModule::CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	FString UGCID = dataAsset->Definition.UGCID;
	FString mapName = dataAsset->Definition.persistentlevel.GetAssetName();
	FString neverCookMapName = dataAsset->Definition.quest3level.GetAssetName();

	// All build paths, names, etc
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/Windows/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "Windows.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[1/9] Cooking PCVR/Windows content.. ";
	UpdateUIProgressField();
	FPlatformProcess::Sleep(1.0f);

	/* Cook Platform */
	if (!ExecuteCookShellCmd("Windows", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Windows Cook returned failure code.";
		UpdateUIProgressField();
		return false;
	}

	LastMessage = "[2/9] Generate PCVR/Windows packing list..";
	UpdateUIProgressField();
	FPlatformProcess::Sleep(0.5f);

	/* Build the pak file */
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Found nothing to add to Windows pak list!";
		UpdateUIProgressField();
		return false;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;
		bool foundUFont = AssetFiles[Index].Find(".ufont") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp || foundUFont)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Windows, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			/* Ignore any "game" folders, these will be skipped as the main game containts them and the mod
			   will have a "soft" reference to it which will still load in-game */
			bool bIgnoreGameAsset = false ;
			if ((realignedrelativePakFilePath.StartsWith("../../../VRE/")) || (realignedrelativePakFilePath.StartsWith("../../../ZeroPay/")))
				bIgnoreGameAsset = true;

			if (!bIgnoreGameAsset)
				generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Asset registry */
	FString ProjectAssetRegistryPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectAssetRegistryPath += "Cooked/Windows/" + FString(FApp::GetProjectName()) + "/AssetRegistry.bin";
	FString realignedFilePath = ProjectAssetRegistryPath.Replace(TEXT("\\"), TEXT("/"));
	generatedPakListLine += "\"" + realignedFilePath + "\"   \"../../../AssetRegistry.bin\" \n";

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Could not write to Windows PAK list file.";
		UpdateUIProgressField();
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (PCVR) >>>>>>>>>>>>>> 
	//

	LastMessage = FString::Printf(TEXT("[3/9] Packing PCVR content (%d assets)..."), nTotalFiles);
	UpdateUIProgressField();

	if (!ExecutePakShellCmd("Windows", CookedPakLocation_Windows, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - PAKing failed. The log's have been copied to the 'output window', please view for more information.";
		return false;
	}

	return true;
}

bool FZeroPayEditorButtonsPluginModule::CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	FString UGCID = dataAsset->Definition.UGCID;
	FString mapName = dataAsset->Definition.persistentlevel.GetAssetName();
	FString neverCookMapName = dataAsset->Definition.pcvrlevel.GetAssetName();

	// All build paths, names, etc
	FString ProjectCookedPath_Android = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Android += "Cooked/Android/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Android = "Android.pak";
	FString CookedPakLocation_Android = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Android += "Workshop/" + PakFileName_Android;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[4/9] Cooking Quest3/Android PAK File...";
	FPlatformProcess::Sleep(0.5f);
	UpdateUIProgressField();

	/* Cook Platform */
	if (!ExecuteCookShellCmd("Android", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Android Cook returned failure code.";
		UpdateUIProgressField();
		return false;
	}

	LastMessage = "[5/9] Generate Quest3/Android packing list..";
	UpdateUIProgressField();
	FPlatformProcess::Sleep(0.5f);

	/* Build the pak file */
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Android, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Found nothing to add to Android pak list!";
		UpdateUIProgressField();
		return false;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;
		bool foundUFont = AssetFiles[Index].Find(".ufont") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp || foundUFont)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Android, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			/* Ignore any "game" folders, these will be skipped as the main game containts them and the mod
			   will have a "soft" reference to it which will still load in-game */
			bool bIgnoreGameAsset = false;
			if ((realignedrelativePakFilePath.StartsWith("../../../VRE/")) || (realignedrelativePakFilePath.StartsWith("../../../ZeroPay/")))
				bIgnoreGameAsset = true;

			if (!bIgnoreGameAsset)
				generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Asset registry */
	FString ProjectAssetRegistryPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectAssetRegistryPath += "Cooked/Android/" + FString(FApp::GetProjectName()) + "/AssetRegistry.bin";
	FString realignedFilePath = ProjectAssetRegistryPath.Replace(TEXT("\\"), TEXT("/"));
	generatedPakListLine += "\"" + realignedFilePath + "\"   \"../../../AssetRegistry.bin\" \n";

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Could not write to Android PAK list file.";
		UpdateUIProgressField();
		return false;
	}

	LastMessage = FString::Printf(TEXT("[6/9] Packing Quest3/Android content (%d assets)..."), nTotalFiles);
	UpdateUIProgressField();

	if (!ExecutePakShellCmd("Android", CookedPakLocation_Android, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Android PAKing failed. The log's have been copied to the 'output window', please view for more information.";
		UpdateUIProgressField();
		return false;
	}

	return true;
}

bool FZeroPayEditorButtonsPluginModule::CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	FString UGCID = dataAsset->Definition.UGCID;
	FString mapName = dataAsset->Definition.persistentlevel.GetAssetName();
	FString neverCookMapName = dataAsset->Definition.pcvrlevel.GetAssetName();

	// All build paths, names, etc
	FString ProjectCookedPath_LinuxServer = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_LinuxServer += "Cooked/LinuxServer/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_LinuxServer = "LinuxServer.pak";
	FString CookedPakLocation_LinuxServer = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_LinuxServer += "Workshop/" + PakFileName_LinuxServer;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[7/9] Cooking LinuxServer PAK File...";
	UpdateUIProgressField();
	FPlatformProcess::Sleep(1.05f);

	/* Cook Platform */
	if (!ExecuteCookShellCmd("LinuxServer", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - LinuxServer Cook returned failure code.";
		UpdateUIProgressField();
		return false;
	}

	LastMessage = "[8/9] Generate LinuxServer packing list..";
	UpdateUIProgressField();
	FPlatformProcess::Sleep(0.5f);

	/* Build the pak file */
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_LinuxServer, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Found nothing to add to LinuxServer pak list!";
		UpdateUIProgressField();
		return false;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;
		bool foundUFont = AssetFiles[Index].Find(".ufont") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp || foundUFont)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_LinuxServer, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			/* Ignore any "game" folders, these will be skipped as the main game containts them and the mod
			   will have a "soft" reference to it which will still load in-game */
			bool bIgnoreGameAsset = false;
			if ((realignedrelativePakFilePath.StartsWith("../../../VRE/")) || (realignedrelativePakFilePath.StartsWith("../../../ZeroPay/")))
				bIgnoreGameAsset = true;

			if (!bIgnoreGameAsset)
				generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Asset registry */
	FString ProjectAssetRegistryPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectAssetRegistryPath += "Cooked/LinuxServer/" + FString(FApp::GetProjectName()) + "/AssetRegistry.bin";
	FString realignedFilePath = ProjectAssetRegistryPath.Replace(TEXT("\\"), TEXT("/"));
	generatedPakListLine += "\"" + realignedFilePath + "\"   \"../../../AssetRegistry.bin\" \n";

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - Could not write to LinuxServer PAK list file.";
		UpdateUIProgressField();
		return false;
	}

	LastMessage = FString::Printf(TEXT("[9/9] Packing LinuxServer content (%d assets)..."), nTotalFiles);
	UpdateUIProgressField();

	if (!ExecutePakShellCmd("LinuxServer", CookedPakLocation_LinuxServer, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - LinuxServer PAKing failed. The log's have been copied to the 'output window', please view for more information.";
		UpdateUIProgressField();
		return false;
	}

	return true;
}

/********************************************************************************************************/
/*                                          SUPPORT FUNCTIONS                                           */
/********************************************************************************************************/

bool FZeroPayEditorButtonsPluginModule::ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName)
{
	FString CmdExe = TEXT("cmd.exe");

	FString EditorExePath = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	EditorExePath += "Engine/Binaries/Win64/UnrealEditor.exe";

	FString ProjectPath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

	FString MapPath = TEXT("/Game/ZeroPayMods/UGC" + UGCID + "/Levels/" + MapName);
	FString NeverCookDir = TEXT("/Game/ZeroPayMods/UGC" + UGCID + "/Levels/" + NeverCookMapName);

	// The full quoted command passed to /k (entire command in one quoted string)
	FString CommandToRun = FString::Printf(
		TEXT("\"%s\" \"%s\" -run=cook -targetplatform=%s -SkipCookingEditorOnlyData -versioned -map=%s -NeverCookDir=%s"),
		*EditorExePath,  // e.g. X:/UE5-Rel/Engine/Binaries/Win64/UnrealEditor.exe
		*ProjectPath,    // e.g. I:/GameDev/ZeroPayVR/ZeroPayVR.uproject
		*Platform,
		*MapPath,
		*NeverCookDir
	);

	// Arguments to cmd.exe: /k "full command in quotes"
	FString CmdArgs = FString::Printf(TEXT("/k \"%s\""), *CommandToRun);

	UE_LOG(LogTemp, Display, TEXT("Cook Shell CommandArgs -- %s"), *CmdArgs);

	// Stdio pipe
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	// Now launch the process
	FProcHandle CmdHandle = FPlatformProcess::CreateProc(
		*CmdExe,
		*CmdArgs,
		true,   // bLaunchDetached
		false,  // bLaunchHidden
		false,  // bLaunchReallyHidden
		nullptr,
		0,
		nullptr,
		WritePipe,
		ReadPipe
	);

	if (CmdHandle.IsValid())
	{
		// Wait until the process exits
		FString Output;

		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("             >>>    ZeroPayVR Mod - Cooking information    <<<                  "))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))

		while (FPlatformProcess::IsProcRunning(CmdHandle))
		{
			Output = FPlatformProcess::ReadPipe(ReadPipe);
			UE_LOG(LogTemp, Display, TEXT("%s"), *Output)
			FPlatformProcess::Sleep(0.1f); // allow buffer to fill
		}
		/* Final line */
		FPlatformProcess::Sleep(1.0f); // allow buffer to fill
		Output = FPlatformProcess::ReadPipe(ReadPipe);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Output)

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

		// Optionally, get the return code
		int32 ExecReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(CmdHandle, &ExecReturnCode);
		UE_LOG(LogTemp, Log, TEXT("Process exited with code %d"), ExecReturnCode);

		// Clean up
		FPlatformProcess::CloseProc(CmdHandle);

		if (ExecReturnCode != 0)
			return false;
		else
			return true ;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to launch process."));
		return false;
	}

	return false ;
}



bool FZeroPayEditorButtonsPluginModule::ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath)
{
	FString CmdExe = TEXT("cmd.exe");

	FString EditorExePath = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	EditorExePath += "Engine/Binaries/Win64/UnrealPak.exe";

	FString ProjectPath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

//	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Windows\" -UTF8Output -multiprocess -patchpaddingalign=2048";

	// The full quoted command passed to /k (entire command in one quoted string)
	FString CommandToRun = FString::Printf(
		TEXT("\"%s\" \"%s\" -create=\"%s\" -platform=%s -UTF8Output -multiprocess -patchpaddingalign=2048"),
		*EditorExePath,
		*CookedPakLocation_Windows,  
		*CookedPakListFilePath,    
		*Platform
	);

	// Arguments to cmd.exe: /k "full command in quotes"
	FString CmdArgs = FString::Printf(TEXT("/k \"%s\""), *CommandToRun);

	UE_LOG(LogTemp, Display, TEXT("Pak Shell CommandArgs -- %s"), *CmdArgs);

	// Stdio pipe
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	// Now launch the process
	FProcHandle CmdHandle = FPlatformProcess::CreateProc(
		*CmdExe,
		*CmdArgs,
		true,   // bLaunchDetached
		false,  // bLaunchHidden
		false,  // bLaunchReallyHidden
		nullptr,
		0,
		nullptr,
		WritePipe,
		ReadPipe
	);

	if (CmdHandle.IsValid())
	{
		// Wait until the process exits
		FString Output;

		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("              >>>    ZeroPayVR Mod - PAKing information    <<<                  "))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))

		while (FPlatformProcess::IsProcRunning(CmdHandle))
		{
			Output = FPlatformProcess::ReadPipe(ReadPipe);
			UE_LOG(LogTemp, Display, TEXT("%s"), *Output)
			FPlatformProcess::Sleep(0.1f); // allow buffer to fill
		}
		/* Final line */
		FPlatformProcess::Sleep(1.0f); // allow buffer to fill
		Output = FPlatformProcess::ReadPipe(ReadPipe);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Output)

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

		// Optionally, get the return code
		int32 ExecReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(CmdHandle, &ExecReturnCode);
		UE_LOG(LogTemp, Log, TEXT("Process exited with code %d"), ExecReturnCode);

		// Clean up
		FPlatformProcess::CloseProc(CmdHandle);

		if (ExecReturnCode != 0)
			return false;
		else
			return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to launch process."));
		return false;
	}

	return false;
}



UZeroPayEditorOperationHandle* UZeroPayEditorButtonsFunctionLibrary::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		return Plugin->CookAndUploadPackages(dataAsset);
	}

	return nullptr ;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorButtonsPluginModule, ZeroPayEditorButtonsPlugin)
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

void FZeroPayEditorButtonsPluginModule::UpdateUIProgressField(const FString& Message)
{
	/* During development, changes to the editor utility BP can cause the instance to disappear */
	if (WidgetInstance == nullptr)
		return ;
	if (!IsValid(WidgetInstance))
		return ;

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
	Params.InputString = Message;

	WidgetInstance->ProcessEvent(Func, &Params);
}

/**************************************************** COOKING LOGIC ****************************************************/

bool FZeroPayEditorButtonsPluginModule::CookThings(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	//
	// Build paths
	//

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString PakFileName_Windows = "Windows.pak";
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	UE_LOG(LogTemp, Display, TEXT("UnrealBinary: %s"), *UnrealBinary);
	UE_LOG(LogTemp, Display, TEXT("UnrealPakBinary: %s"), *UnrealPakBinary);
	UE_LOG(LogTemp, Display, TEXT("ProjectFullFilePath: %s"), *ProjectFullFilePath);
	UE_LOG(LogTemp, Display, TEXT("PakFileName_Windows: %s"), *PakFileName_Windows);
	UE_LOG(LogTemp, Display, TEXT("CookedPakListFilePath: %s"), *CookedPakListFilePath);
	//UE_LOG(LogTemp, Display, TEXT("GameInstallationPakPath: %s"), *GameInstallationPakPath);

	// Debug
	//UnrealBinary = "D:\\Program Files (x86)\\UE_4.24\\Engine\\Binaries\\Win64\\UnrealEditor.exe";
	//UnrealPakBinary = "D:\\Program Files (x86)\\UE_4.24\\Engine\\Binaries\\Win64\\UnrealPak.exe";
	//ProjectFullFilePath = "D:\\DVG\\SampleProject_UE24C\\SampleProject_UE24C.uproject";
	//PakFileName_Windows = "UGC" + UGCValue.ToString() + "-Windows.pak";
	//CookedPakLocation_Windows = "D:\\DVG\\SampleProject_UE24C\\Saved\\Cooked\\Workshop\\" + PakFileName_Windows;
	//CookedPakListFilePath = "D:\\DVG\\SampleProject_UE24C\\Saved\\custommap_paklist.txt";
	//GameInstallationPakPath = "D:\\DVG\\KModOutput\\Windows\\ZeroPay\\Content\\Paks\\";

	/* Read the level grabbing tha name */
	GlobalUGCValue = dataAsset->Definition.UGCID ;
	FString mapName = dataAsset->Definition.persistentlevel.GetAssetName();
	FString pcvrName = dataAsset->Definition.pcvrlevel.GetAssetName();

	//
	// >>>>>>>>>>>>>> COOKING >>>>>>>>>>>>>> 
	//

	UpdateUIProgressField("Cooking content game (desktop and server)...");


	// JBH Windows+WindowsServer+LinuxServer+
	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \" \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Android -versioned -map=/Game/CustomContent/" + GlobalUGCValue + "/Maps/" + *mapName + "";
	FString CommandWorkingDirectory = "C:\\";

	FString AdditionalArgs = " -NeverCookDir=/Game/CustomContent/" + GlobalUGCValue + "/Maps/Sublevels/PCVR/" + *pcvrName + "";
	CommandArgs += AdditionalArgs;
#if 0
	if (dataAsset->bCookEverythingEvenIfThatsBads)
	{
		FString AdditionalArgs = " -CookDir=\"";
		AdditionalArgs += *FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + FString("CustomContent/") + UGCValue.ToString();
		AdditionalArgs += "/\"";

		CommandArgs += AdditionalArgs;
	}
#endif
	CommandArgs += "\"";

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Cooking information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess;
	nSuccess = StdOut.Find("Success - 0 error(s)", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess <= 0)
	{
		UE_LOG(LogTemp, Display, TEXT("ERROR RECORDED AS:\n%s"), *StdErr)
			UpdateUIProgressField(">> ERROR >> ERROR > ERROR --- Cook failed. Please look in the 'output window' to see what went wrong") ;
		return false;
	}

	UpdateUIProgressField("Cooked with no errors...");
	FPlatformProcess::Sleep(1.0f);

	//
	// >>>>>>>>>>>>>> PACKING LIST >>>>>>>>>>>>>> 
	//

	//if (PackWindows(mapName))
	return (PackAndroid(mapName));
	//			if (PackWindowsServer(mapName))
		//		return PackLinuxServer(mapName) ;

		//
		// >>>>>>>>>>>>>> COPYING >>>>>>>>>>>>>> 
		//

		//UpdateUIProgressField("Moving to location file system...");

		//FString To = GameInstallationPakPath + PakFileName_Windows;
		//FString From = CookedPakLocation_Windows;
		//bool Result = PlatformFile.CopyFile(*To, *From, EPlatformFileRead::None, EPlatformFileWrite::None);	

	return false;
}

bool FZeroPayEditorButtonsPluginModule::PackWindows(FString mapName)
{

	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/Windows/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "Windows.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Windows+WindowsServer -SkipCookingEditorOnlyData -versioned -map=/Game/CustomContent/" + GlobalUGCValue + "/Maps/" + mapName + "";
	FString CommandWorkingDirectory = "";

	UpdateUIProgressField("Generate packing list..");
	FPlatformProcess::Sleep(0.5f);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
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
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	UpdateUIProgressField(FString::Printf(TEXT("Packing desktop content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Windows\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;

}


bool FZeroPayEditorButtonsPluginModule::PackAndroid(FString mapName)
{

	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/Android/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "Android.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Android -SkipCookingEditorOnlyData -versioned -map=/Game/CustomContent/" + GlobalUGCValue + "/Maps/" + mapName + "";
	FString CommandWorkingDirectory = "";

	UpdateUIProgressField("Generate Android Quest 3 packing list..");
	FPlatformProcess::Sleep(0.5f);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
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
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	UpdateUIProgressField(FString::Printf(TEXT("Packing Android / Quest3 content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Android\" -compress -compressionformat=Zlib";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;

}

bool FZeroPayEditorButtonsPluginModule::PackWindowsServer(FString mapName)
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/WindowsServer/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "WindowsServer.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	FString Command;
	FString CommandArgs;
	FString CommandWorkingDirectory = "";

	UpdateUIProgressField("Generate packing list...");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
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

		if (foundUAsset || foundUBulk || foundUMap || foundUExp)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Windows, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	UpdateUIProgressField(FString::Printf(TEXT("Packing windows server content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Windows\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;
}

//-------

bool FZeroPayEditorButtonsPluginModule::PackLinuxServer(FString mapName)
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Linux = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Linux += "Cooked/LinuxServer/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Linux = "LinuxServer.pak";
	FString CookedPakLocation_Linux = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Linux += "Workshop/" + PakFileName_Linux;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	FString Command;
	FString CommandArgs;
	FString CommandWorkingDirectory = "";

	UpdateUIProgressField("Generate packing list...");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Linux, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
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

		if (foundUAsset || foundUBulk || foundUMap || foundUExp)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Linux, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (LINUX) >>>>>>>>>>>>>> 
	//

	UpdateUIProgressField(FString::Printf(TEXT("Packing linux server content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Linux + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Linux\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing (Linux Server) Information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		UpdateUIProgressField(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;
}


void UZeroPayEditorButtonsFunctionLibrary::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	if (FZeroPayEditorButtonsPluginModule* Plugin = FModuleManager::Get().GetModulePtr<FZeroPayEditorButtonsPluginModule>("ZeroPayEditorButtonsPlugin"))
	{
		Plugin->CookThings(dataAsset);
	}
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorButtonsPluginModule, ZeroPayEditorButtonsPlugin)
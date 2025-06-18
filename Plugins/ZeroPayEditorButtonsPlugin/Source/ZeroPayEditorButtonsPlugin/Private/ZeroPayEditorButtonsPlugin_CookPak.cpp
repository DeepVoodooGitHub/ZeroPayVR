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

/**************************************************** COOKING LOGIC ****************************************************/

UZeroPayEditorCookPakOperationHandle* FZeroPayEditorButtonsPluginModule::CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset)
{
	bAbortOperation = false;
	CookPakHandle = NewObject<UZeroPayEditorCookPakOperationHandle>();

	Async(EAsyncExecution::Thread, [this, dataAsset]()
		{
			/* >>> Pack PCVR <<< */
			if (!CookAndPackWindows(dataAsset))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						CookPakHandle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
			/* >>> Pack Quest 3 <<< */
			if (!CookAndPackAndroid(dataAsset))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						CookPakHandle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
			/* >>> Pack Linux Server <<< */
			if (!CookAndPackLinuxServer(dataAsset))
				bAbortOperation = true;

			/* All Good! */
			if (!bAbortOperation)
				LastMessage = "Cooking and packing stage completed successfully!";

			UpdateModManagementUIProgressField();
			FPlatformProcess::Sleep(1.0f);

			// Simulate some logic, then notify later
			AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
				{
					CookPakHandle->OnCompleted.Broadcast(!bAbortOperation, dataAsset->Definition.UGCID);
				});
		});

	return CookPakHandle;
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
	CookedPakLocation_Windows += "Workshop/Windows/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[1/9] Cooking PCVR/Windows content.. ";
	UpdateModManagementUIProgressField();
	FPlatformProcess::Sleep(1.0f);

	/* Cook Platform */
	if (!ExecuteCookShellCmd("Windows", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR : Cooking of windows failed, see 'Output Log'.";
		UpdateModManagementUIProgressField();
		return false;
	}

	LastMessage = "[2/9] Generate PCVR/Windows packing list..";
	UpdateModManagementUIProgressField();
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
		LastMessage = ">>> ERROR : Windows Pak list was empty, see 'Output Log'.";
		UpdateModManagementUIProgressField();
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
		LastMessage = ">>> ERROR : Could not write Windows Pak list.";
		UpdateModManagementUIProgressField();
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (PCVR) >>>>>>>>>>>>>> 
	//

	LastMessage = FString::Printf(TEXT("[3/9] Packing PCVR content (%d assets)..."), nTotalFiles);
	UpdateModManagementUIProgressField();

	if (!ExecutePakShellCmd("Windows", CookedPakLocation_Windows, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR : Windows Pak failed, see 'output log'";
		UpdateModManagementUIProgressField();
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
	CookedPakLocation_Android += "Workshop/Android/" + PakFileName_Android;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[4/9] Cooking Quest3/Android PAK File...";
	FPlatformProcess::Sleep(0.5f);
	UpdateModManagementUIProgressField();

	/* Cook Platform */
	if (!ExecuteCookShellCmd("Android", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR : Cooking of Android failed, see 'Output Log'.";
		UpdateModManagementUIProgressField();
		return false;
	}

	LastMessage = "[5/9] Generate Quest3/Android packing list..";
	UpdateModManagementUIProgressField();
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
		LastMessage = ">>> ERROR : Android Pak list was empty, see 'Output Log'.";
		UpdateModManagementUIProgressField();
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
		UpdateModManagementUIProgressField();
		return false;
	}

	LastMessage = FString::Printf(TEXT("[6/9] Packing Quest3/Android content (%d assets)..."), nTotalFiles);
	UpdateModManagementUIProgressField();

	if (!ExecutePakShellCmd("Android", CookedPakLocation_Android, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR : Android Pak failed, see 'output log'";
		UpdateModManagementUIProgressField();
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
	CookedPakLocation_LinuxServer += "Workshop/LinuxServer/" + PakFileName_LinuxServer;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";

	LastMessage = "[7/9] Cooking LinuxServer PAK File...";
	UpdateModManagementUIProgressField();
	FPlatformProcess::Sleep(1.05f);

	/* Cook Platform */
	if (!ExecuteCookShellCmd("LinuxServer", UGCID, mapName, neverCookMapName))
	{
		LastMessage = ">>> ERROR : Cooking of LinuxServer failed, see 'Output Log'.";
		UpdateModManagementUIProgressField();
		return false;
	}

	LastMessage = "[8/9] Generate LinuxServer packing list..";
	UpdateModManagementUIProgressField();
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
		LastMessage = ">>> ERROR : LinuxServer Pak list was empty, see 'Output Log'.";
		UpdateModManagementUIProgressField();
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
		LastMessage = ">>> ERROR : LinuxServer Pak failed, see 'output log'";
		UpdateModManagementUIProgressField();
		return false;
	}

	LastMessage = FString::Printf(TEXT("[9/9] Packing LinuxServer content (%d assets)..."), nTotalFiles);
	UpdateModManagementUIProgressField();

	if (!ExecutePakShellCmd("LinuxServer", CookedPakLocation_LinuxServer, CookedPakListFilePath))
	{
		LastMessage = ">>> ERROR >> ERROR > ERROR - LinuxServer PAKing failed. The log's have been copied to the 'output window', please view for more information.";
		UpdateModManagementUIProgressField();
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

	/* This next section is just to setup the pipes for redirecting the stdio to the console, this helps with debugging issues and allows user to paste failures
	  from the output window into discord, etc. */
	FString GuidStr = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FString PipeNameStr = FString::Printf(TEXT("\\\\.\\pipe\\UEPluginStdout_%s"), *GuidStr);
	const wchar_t* PipeNameW = *PipeNameStr;

	/* Pipe setup */
	SECURITY_ATTRIBUTES PipeSecurity = {};
	PipeSecurity.nLength = sizeof(SECURITY_ATTRIBUTES);
	PipeSecurity.bInheritHandle = true;
	PipeSecurity.lpSecurityDescriptor = nullptr;

	/* Create the pipe (read side) */
	HANDLE ReadPipe = CreateNamedPipeW(
		PipeNameW,
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,           // Max instances
		65536,       // Output buffer size
		65536,       // Input buffer size
		0,
		&PipeSecurity
	);

	if (ReadPipe == INVALID_HANDLE_VALUE)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNamedPipe failed: %d"), GetLastError());
		return false ;
	}

	/* Connect write handle(child side) - try a few times.. */
	HANDLE WritePipe = INVALID_HANDLE_VALUE;
	for (int i = 0; i < 10 && WritePipe == INVALID_HANDLE_VALUE; ++i)
	{
		WritePipe = CreateFileW(
			PipeNameW,
			GENERIC_WRITE,
			0,
			&PipeSecurity,
			OPEN_EXISTING,
			0,
			nullptr
		);

		if (WritePipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			FPlatformProcess::Sleep(0.05f);
		}
	}

	if (WritePipe == INVALID_HANDLE_VALUE)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateFile failed: %d"), GetLastError());
		CloseHandle(ReadPipe);
		return false;
	}
	
	/* Launch the cooking */
	void* StdOutPipe = reinterpret_cast<void*>(WritePipe);
	FProcHandle CmdHandle = FPlatformProcess::CreateProc(
		*CmdExe,
		*CmdArgs,
		true,   // bLaunchDetached
		false,  // bLaunchHidden
		false,  // bLaunchReallyHidden
		nullptr,
		0,
		nullptr,		
		StdOutPipe
	);

	if (CmdHandle.IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("             >>>    ZeroPayVR Mod - Cooking information    <<<                  "))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))

		FString Remainder = "" ;
		FDateTime ProcessExitTime;
		bool bProcessExited = false;
		bool bProcessExecutedSuccessfully = false ;

		/* Read the process output, but wait an extra 5 seconds before stopping for late coming stdio (and maybe our success field) */
		while (true)
		{
			// Detect process exit time
			if (!bProcessExited && !FPlatformProcess::IsProcRunning(CmdHandle))
			{
				bProcessExited = true;
				ProcessExitTime = FDateTime::UtcNow();
			}

			// After exit: check if 5 seconds have passed
			if (bProcessExited)
			{
				FTimespan Elapsed = FDateTime::UtcNow() - ProcessExitTime;
				if (Elapsed.GetTotalSeconds() > 5.0)
				{
					break;
				}
			}

			/* Read all lines.. */
			FString Line;
			while (ReadNextLineFromPipe(ReadPipe, Line, Remainder))
			{
				/* Output to UE */
				UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
				/* Scan for success line */
				int32 nFoundPosition = Line.Find("Success - 0 error(s)", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
				if (nFoundPosition > 0)
					bProcessExecutedSuccessfully = true;
			}
			FPlatformProcess::Sleep(0.01f);
		}

		/* Clean pipes */
		CloseHandle(ReadPipe);
		CloseHandle(WritePipe);

		/* Get return code, we don't care UE5 don't use it but maybe in the future */
		int32 ExecReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(CmdHandle, &ExecReturnCode);
		UE_LOG(LogTemp, Log, TEXT("Process exited with code %d"), ExecReturnCode);

		/* Clean up */
		FPlatformProcess::CloseProc(CmdHandle);

		return bProcessExecutedSuccessfully ;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to launch process."));
		return false;
	}

	return false ;
}

bool FZeroPayEditorButtonsPluginModule::ReadNextLineFromPipe(HANDLE PipeHandle, FString& OutLine, FString& Remainder)
{
	const int32 ReadChunkSize = 4096;
	char ReadBuffer[ReadChunkSize];
	DWORD BytesRead = 0;

	// Step 1: Try to complete a line from what's already in Remainder
	int32 NewlineIndex;
	if (Remainder.FindChar('\n', NewlineIndex))
	{
		OutLine = Remainder.Left(NewlineIndex + 1); // Include the newline
		Remainder = Remainder.Mid(NewlineIndex + 1);
		return true;
	}

	// Step 2: Peek to see if there's anything to read
	DWORD BytesAvailable = 0;
	if (!PeekNamedPipe(PipeHandle, nullptr, 0, nullptr, &BytesAvailable, nullptr) || BytesAvailable == 0)
	{
		return false; // No data yet
	}

	// Step 3: Read what's available
	if (ReadFile(PipeHandle, ReadBuffer, FMath::Min(BytesAvailable, (DWORD)(ReadChunkSize - 1)), &BytesRead, nullptr) && BytesRead > 0)
	{
		ReadBuffer[BytesRead] = '\0';

		FString Incoming = ANSI_TO_TCHAR(ReadBuffer);
		Remainder += Incoming;

		// Try again now that we've added more data
		if (Remainder.FindChar('\n', NewlineIndex))
		{
			OutLine = Remainder.Left(NewlineIndex + 1);
			Remainder = Remainder.Mid(NewlineIndex + 1);
			return true;
		}
	}

	return false; // Still no full line
}

bool FZeroPayEditorButtonsPluginModule::ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath)
{
	FString CmdExe = TEXT("cmd.exe");

	FString EditorExePath = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	EditorExePath += "Engine/Binaries/Win64/UnrealPak.exe";

	FString ProjectPath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

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
			FPlatformProcess::Sleep(0.01f); // allow buffer to fill
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

UZeroPayEditorCookPakOperationHandle* FZeroPayEditorButtonsPluginModule::PollUploadStatus()
{
	bAbortOperation = false;
	bPollCompleted = false;
	CookPakHandle = NewObject<UZeroPayEditorCookPakOperationHandle>();

	Async(EAsyncExecution::Thread, [this]()
		{
			bool bHasEnteredUploadState = false ;
			while (!bPollCompleted)
			{
				UModioSubsystem* Subsystem = GEngine->GetEngineSubsystem<UModioSubsystem>();
				if (Subsystem == nullptr)
					return ;
				TOptional<FModioModProgressInfo> result = Subsystem->QueryCurrentModUpdate();

				if (result.IsSet())
				{
					/* Get value of progress*/
					FModioModProgressInfo value = result.GetValue();
					/* Wait until mod.io has said it's "uploading" */
					if ((!bHasEnteredUploadState) && (value.GetCurrentState() == EModioModProgressState::Uploading))
					{
						/* Set flag to detect we are uploading */
						bHasEnteredUploadState = true;
						/* Read init values */
						currentProgress = value.GetCurrentProgress(EModioModProgressState::Uploading);
						totalProgress = value.GetTotalProgress(EModioModProgressState::Uploading);
					}
					/* Grab information */
					if ((value.GetCurrentState() == EModioModProgressState::Uploading))
					{
						/* Difference */
						int64 currentProgress_ = (int64) value.GetCurrentProgress(EModioModProgressState::Uploading) ;
						int64 changeInBytes = currentProgress_ - (int64) currentProgress;

						/* Read new value */
						currentProgress = value.GetCurrentProgress(EModioModProgressState::Uploading);

						/* Submit info to BP callback! */
						AsyncTask(ENamedThreads::GameThread, [this, changeInBytes]()
							{
								CookPakHandle->OnUploadProgress.Broadcast(false, (int64)currentProgress, (int64)totalProgress, FormatDataRateResponse(changeInBytes));
							});
					}
					else
					{
						/* Not uploading? make sure we were prior to killing ourself... */
						if (bHasEnteredUploadState)
							bPollCompleted = true;
					}
				}
				else
				{
					/* Invalid, after upload? we're done.. */
					if (bHasEnteredUploadState)
						bPollCompleted = true;
				}
					
				FPlatformProcess::Sleep(1.0f);
			}

			// Simulate some logic, then notify later
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
					CookPakHandle->OnUploadProgress.Broadcast(true, 0, 0, "Completed");
			});
		});

	return CookPakHandle;
}



FString FZeroPayEditorButtonsPluginModule::FormatDataRateResponse(int64 BytesPerSecond)
{
	const TCHAR* Suffix = TEXT("B/s");
	double Rate = static_cast<double>(BytesPerSecond);

	if (Rate >= 1024.0 * 1024.0 * 1024.0)
	{
		Rate /= (1024.0 * 1024.0 * 1024.0);
		Suffix = TEXT("GB/s");
	}
	else if (Rate >= 1024.0 * 1024.0)
	{
		Rate /= (1024.0 * 1024.0);
		Suffix = TEXT("MB/s");
	}
	else if (Rate >= 1024.0)
	{
		Rate /= 1024.0;
		Suffix = TEXT("KB/s");
	}

	return FString::Printf(TEXT("%.2f %s"), Rate, Suffix);
}

void FZeroPayEditorButtonsPluginModule::CancelUploadStatus()
{
	bAbortOperation = true ;
	bPollCompleted = true ;
}

void FZeroPayEditorButtonsPluginModule::UpdateModManagementUIProgressField()
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

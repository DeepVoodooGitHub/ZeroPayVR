// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ZeroPayMod_DefinitionDataAsset.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ModioSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "ZeroPayEditorButtonsPlugin.generated.h"

class FToolBarBuilder;
class FMenuBuilder;

/**********************************************************************************************************************
*
* Class: UZeroPayEditorOperationHandle
* Description: Used for cook and pak event generation
*
*/

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOperationComplete, bool, bSuccess, FString, UGCID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnUploadProgress, bool, bComplete, int64, currentBytes, int64, totalBytes, FString, dataRate);

UCLASS(Blueprintable)
class UZeroPayEditorOperationHandle : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnOperationComplete OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FOnUploadProgress OnUploadProgress;
};


/**********************************************************************************************************************
*
* Class: UZeroPayEditorOperationHandle
* Description: Used for cook and pak event generation
*
*/

USTRUCT(BlueprintType)
struct FZeroPayEditor_ReducerSettingsStruct
{
	GENERATED_BODY()

	/* The parent PCVR level to use for reduction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	TSoftObjectPtr<UWorld> PCVRLevel;

	/* The quest 3 target level for the output of the reduction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	TSoftObjectPtr<UWorld> Quest3Level;

	/* How much to reduce the triangle count down to (0.3 = 30%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float PercentageTriangles = 0.3f;
};

/**********************************************************************************************************************
*
* Class: UZeroPayEditorOperationHandle
* Description: Provide the UE editor plugin module that provides cooking, reducer, etc. functionality
*
*/

class FZeroPayEditorButtonsPluginModule : public IModuleInterface
{
public:
	/* >>> IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/* >>> OnClick routins triggered by UE buttons on the toolbar */
	void GenerateQuest3ReducedLevel_Clicked();
	void ShowPCVRView_Clicked();
	void OpenModIOWindow_Clicked();

	/* >>> Cooking logic - Called from Function Library */
	UZeroPayEditorOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
	UZeroPayEditorOperationHandle* PollUploadStatus() ;
	void CancelUploadStatus();
private:
	/* >>> Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	UEditorUtilityWidget* WidgetModManagementInstance;
	UEditorUtilityWidget* WidgetQuest3ReducerInstance;
	/* >>> Cooking vars */
	UZeroPayEditorOperationHandle* Handle;
	FString GlobalUGCValue;
	FString LastMessage ;
	bool bAbortOperation ;
	bool bPollCompleted ;
	FModioUnsigned64 currentProgress;
	FModioUnsigned64 totalProgress;

	/* >>> Windows, Menus, Dialogs, etc. */
	TSharedRef<SDockTab> SpawnModManagementDockableTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnQuest3ReducerDockableTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);
	void UpdateUIProgressField() ;
	FString FormatDataRateResponse(int64 BytesPerSecond) ;

	/* >>> Cooking logic */
	bool CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset);

	bool ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName);
	bool ReadNextLineFromPipe(HANDLE PipeHandle, FString& OutLine, FString& Remainder);
	bool ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath);

	/* >>> Reducer Logic */
	bool ReduceLevel() ;
};

/**********************************************************************************************************************
*
* Class: UZeroPayEditorButtonsFunctionLibrary
* Description: Provide an BP callable interface to the plugin's core "cooking and packing" operations
* 
*/

UCLASS()
class UZeroPayEditorButtonsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/* Cooking */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static UZeroPayEditorOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);

	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static UZeroPayEditorOperationHandle* PollUploadStatus();

	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static void CancelUploadStatus();
};
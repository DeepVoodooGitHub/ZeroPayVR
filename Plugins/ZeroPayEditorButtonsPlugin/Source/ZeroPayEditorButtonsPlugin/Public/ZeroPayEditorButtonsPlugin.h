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
class UZeroPayEditorCookPakOperationHandle : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnOperationComplete OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FOnUploadProgress OnUploadProgress;
};

UCLASS(Blueprintable)
class UZeroPayEditorReduceOperationHandle : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnOperationComplete OnCompleted;
};

/**********************************************************************************************************************
*
* Class: UZeroPayEditorOperationHandle
* Description: Used for cook and pak event generation
*
*/

UCLASS(BlueprintType)
class UZeroPayEditor_ReducerSettingsAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	/* How much to reduce the triangle count down to (0.3 = 30%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float PercentageTriangles = 0.3f;

	/* The total triangles found in the original PCVR level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_OriginalTriangleCount = 0;

	/* The total triangles found in the reduced Quest3 level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_ReducedTriangleCount = 0;

	/* The total materials found in the original PCVR level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_OriginalMaterialCount = 0;

	/* The total triangles found in the reduced Quest3 level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_ReducedMaterialCount = 0;

	/* The total unique meshes in original PCVR level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_OriginalMeshCount = 0;

	/* The total unique meshes found in the reduced Quest3 level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 Stats_ReducedMeshCount = 0;

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
	UZeroPayEditorCookPakOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
	UZeroPayEditorCookPakOperationHandle* PollUploadStatus() ;
	void CancelUploadStatus();

	/* >>> Reducer Logic - Called from Function Library */
	UZeroPayEditorReduceOperationHandle* ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings);
private:
	/* >>> Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	UEditorUtilityWidget* WidgetModManagementInstance;
	UEditorUtilityWidget* WidgetQuest3ReducerInstance;
	/* >>> Cooking vars */
	UZeroPayEditorCookPakOperationHandle* CookPakHandle;
	UZeroPayEditorReduceOperationHandle* ReduceHandle;
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
	FString FormatDataRateResponse(int64 BytesPerSecond) ;

	/* >>> Cooking logic */
	bool CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset);

	bool ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName);
	bool ReadNextLineFromPipe(HANDLE PipeHandle, FString& OutLine, FString& Remainder);
	bool ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath);

	/* Cooking support */
	void UpdateModManagementUIProgressField(); 
	void UpdateQuest3ReducerUIProgressField();
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

	/* Start the cooking and packaging of all supported targets */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static UZeroPayEditorCookPakOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);

	/* Returns the status of any upload.. */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static UZeroPayEditorCookPakOperationHandle* PollUploadStatus();

	/* Cancels the upload */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static void CancelUploadStatus();

	/* Reduces a PCVR level using the supplied settings, to a Quest3 level */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static UZeroPayEditorReduceOperationHandle* ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings);

};
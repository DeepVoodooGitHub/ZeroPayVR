// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ZeroPayMod_DefinitionDataAsset.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ModioSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

/**********************************************************************************************************************
*
* Class: UZeroPayEditorOperationHandle
* Description: Used for cook and pak event generation
*
*/

USTRUCT(BlueprintType)
struct FZeroPayEditor_MeshReductionSettings
{
	GENERATED_BODY()

	/* The size of each bounding box cluster in units  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float BoundingClusterSize = 200.0f ;
};

USTRUCT(BlueprintType)
struct FZeroPayEditor_ReductionStatistics
{
	GENERATED_BODY()

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
};

UCLASS(BlueprintType)
class UZeroPayEditor_ReducerSettingsAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MeshReductionSettings MeshReductionSettings ;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_ReductionStatistics Statistics ;

};

USTRUCT(BlueprintType)
struct FReducerRuntimeSettings
{
	GENERATED_BODY()

	/* Show stage 1 visual debugging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bStage1_ShowVisualDebug = true;

	/* How long to show any debug boxes, labels, etc. for stage 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float fStage1_VisualDebugDuration = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bStage1_ShowOutputLogDebug = true;

};

struct FFoundAssetInformation
{
	int32 FoundAssets = 0 ; 
	int32 FoundInstances = 0 ;
};

/**********************************************************************************************************************
*
* FMeshMaterialKey - Provides a mesh and unique material map
*
*/

struct FMeshMaterialKey
{
	UStaticMesh* Mesh = nullptr;
	TArray<UMaterialInterface*> Materials;

	bool operator==(const FMeshMaterialKey& Other) const
	{
		return Mesh == Other.Mesh && Materials == Other.Materials;
	}

	friend uint32 GetTypeHash(const FMeshMaterialKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.Mesh);
		for (UMaterialInterface* Mat : Key.Materials)
		{
			Hash = HashCombine(Hash, GetTypeHash(Mat));
		}
		return Hash;
	}
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
	bool ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);
private:
	/* >>> Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	UEditorUtilityWidget* WidgetModManagementInstance;
	UEditorUtilityWidget* WidgetQuest3ReducerInstance;
	/* >>> Cooking vars */
	UZeroPayEditorCookPakOperationHandle* CookPakHandle;
	FString GlobalUGCValue;
	FString LastMessage ;
	bool bAbortOperation ;
	bool bPollCompleted ;
	FModioUnsigned64 currentProgress;
	FModioUnsigned64 totalProgress;
	/* >>> Reducer Vars */

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

	/* >>> Reducer Logic */
	bool Stage1MergeUniformMeshes(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings) ;

	/* Reducer mesh, world, etc. support */
	FBox GetMaximumVisibleBoundingBox(ULevel* Level) ;
	TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> PartitionActorsIntoBoundingBoxes(const FBox& GlobalBounds, const FVector& ChunkSize, ULevel* Level);

	/* Merge system */
	bool MergeMeshIslands(const TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath, TSoftObjectPtr<UWorld> Quest3World);
	bool MergeMesh(const TArray<UStaticMeshComponent*> SelectedComponents, const FString& PackageName, UWorld* targetQuest3World);
	void PlaceMeshProxyInQuest3Level(TArray<UObject*>& NewAssetsToSync, ULevel* Level) ;

	/* Reducer debug */
	void DrawClusterDebugBoxes(const TMap<UStaticMesh*, TArray<TArray<UStaticMeshComponent*>>>& ClusteredGroups, UWorld* World, float Lifetime);

	/* Reducer support */
	FFoundAssetInformation ScanLevelActorsAndDirectory(ULevel* LevelToScan, const FString& TargetAssetPath) ;
	bool DeleteActorsAndAssets(ULevel* TargetLevel, const FString& AssetFolderPathToDelete) ;
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
	static bool ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);

};
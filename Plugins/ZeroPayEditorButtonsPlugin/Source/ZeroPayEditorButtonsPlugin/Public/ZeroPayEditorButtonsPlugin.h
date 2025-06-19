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
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "GPULightmassModule.h"
#include "GPUlightMassSettings.h"
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
struct FZeroPayEditor_MeshReductionZone
{
	GENERATED_BODY()

	/* Distance from previous zone to this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float ZoneDistance = 2500.0f;

	/* Closer to zero avoids expansion / contraction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float MergeDistance = 0.0f;

	/* Screen size, large is more reduction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float ScreenSize = 1.0f;

	FZeroPayEditor_MeshReductionZone(float InZoneDistance, float InMergeDistance, float InScreenSize)
		: ZoneDistance(InZoneDistance)
		, MergeDistance(InMergeDistance)
		, ScreenSize(InScreenSize)
	{
	}

	FZeroPayEditor_MeshReductionZone() = default;
};

USTRUCT(BlueprintType)
struct FZeroPayEditor_MeshReductionSettings
{
	GENERATED_BODY()

	/* Does not perform the merge, but validates the level, chunk size, meshes per chunk, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bDryRun = false ;

	/* The size of each bounding box chunk in units  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	float BoundingChunkSize = 200.0f ;

	/* Max meshes that can be per chunk, making this too large will kill the merging due to out of memory issues */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	int32 MaxMeshesPerChunk = 1000;

	/* Show (in red debug cube for 1 minute) any chunks that have too many meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bShowBadMaxMeshChunks = true ;

	/* If true, we will use any 'ZeroPayEditor_Reducer_PlayerZone' actors to use higher detail around where players are */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bEnablePlayerZoning = true;

	/* Settings for first zone (from any Player Zones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MeshReductionZone PlayerZone1_Settings ;

	/* Settings for second zone (from any Player Zones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MeshReductionZone PlayerZone2_Settings;

	/* Settings for third zone to affinitiy (from any Player Zones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MeshReductionZone PlayerZone3_Settings;

	FZeroPayEditor_MeshReductionSettings()
		: PlayerZone1_Settings(2500.0f, 10.0f, 1.0f)
		, PlayerZone2_Settings(2500.0f, 10.0f, 1.0f)
		, PlayerZone3_Settings(2500.0f, 10.0f, 1.0f)
	{
	}
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
	float fStage1_VisualDebugDuration = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	bool bStage1_ShowOutputLogDebug = true;

};

struct FFoundAssetInformation
{
	int32 FoundAssets = 0 ; 
	int32 FoundInstances = 0 ;
};


USTRUCT(BlueprintType)
struct FReducerResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	bool bFailed ;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 OriginalTriangleCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 OriginalVertexCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 OriginalMaterialCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 OriginalActorCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 ReducedTriangleCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 ReducedVertexCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 ReducedMaterialCount;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh Stats")
	int32 ReducedActorCount;

	FReducerResults()
	{
		bFailed = true;
		OriginalTriangleCount = 0;
		OriginalVertexCount = 0;
		OriginalMaterialCount = 0;
		OriginalActorCount = 0;
		ReducedTriangleCount = 0;
		ReducedVertexCount = 0;
		ReducedMaterialCount = 0;
		ReducedActorCount = 0;

	};
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
	void ShowQuest3View_Clicked();
	void ShowPCVRView_Clicked();
	void BakeLightsOnLevels_Clicked();
	void GenerateQuest3ReducedLevel_Clicked();
	void OpenModIOWindow_Clicked();

	/* >>> Cooking logic - Called from Function Library */
	UZeroPayEditorCookPakOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
	UZeroPayEditorCookPakOperationHandle* PollUploadStatus() ;
	void CancelUploadStatus();

	/* >>> Reducer Logic - Called from Function Library */
	FReducerResults ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);
private:
	/* >>> Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	/* Window widget instances */
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
	TArray<FBox> PlayerZoneBounds;
	TArray<UStaticMeshComponent*> StaticMeshComponentsToMerge;
	/* >>> Baking Vars */
	UGPULightmassSubsystem* GPULightmassSubsystem;
	FTimerDelegate TimerCallback;
	FTimerHandle TimerHandle;

	/* >>> Windows, Menus, Dialogs, etc. */
	TSharedRef<SDockTab> SpawnModManagementDockableTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnQuest3ReducerDockableTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);
	TSharedPtr<SWidget> FindWidgetRecursive(TSharedPtr<SWidget> Root, TSharedRef<SWidget> Target) ;
	FString FormatDataRateResponse(int64 BytesPerSecond) ;

	/* --->>> Cooking logic <<<--- */

	bool CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset);

	bool ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName);
	bool ReadNextLineFromPipe(HANDLE PipeHandle, FString& OutLine, FString& Remainder);
	bool ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath);

	/* Cooking support */
	void UpdateModManagementUIProgressField(); 

	/* --->>> Reducer Logic <<<--- */

	FReducerResults ReducePCVRLevelForQuest3(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings) ;

	/* Reducer mesh, world, etc. support */
	FBox GetMaximumVisibleBoundingBox(ULevel* Level) ;
	TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> PartitionActorsIntoBoundingBoxes(const FBox& GlobalBounds, const FVector& ChunkSize, ULevel* Level, FReducerResults& returnValue, UZeroPayEditor_ReducerSettingsAsset* reducerSettings);

	/* Merge system */
	bool MergeMeshIslands(const TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath, TSoftObjectPtr<UWorld> Quest3World, FReducerResults& returnValue, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);
	bool MergeMesh(const TArray<UStaticMeshComponent*> SelectedComponents, const FString& PackageName, UWorld* targetQuest3World, FZeroPayEditor_MeshReductionZone* reductionZoneSettings, FReducerResults& returnValue);
	void PlaceMeshProxyInQuest3Level(TArray<UObject*>& NewAssetsToSync, ULevel* Level, FReducerResults& returnValue) ;

	/* Reducer support */
	FFoundAssetInformation ScanLevelActorsAndDirectory(ULevel* LevelToScan, const FString& TargetAssetPath) ;
	bool DeleteActorsAndAssets(ULevel* TargetLevel, const FString& AssetFolderPathToDelete) ;
	float BoxSurfaceDistance(const FBox& A, const FBox& B) ;
	void UpdateQuest3ReducerUIProgressField();

	/* --->>> Light baking logic <<<--- */
	bool bPCVRLevel_OriginalVisibility ;
	bool bQuest3Level_OriginalVisibility ;
	UWorld* persistentLeveLightBake ;
	UWorld* pcvrLevelLightBake ;
	UWorld* quest3LevelLightBake;

	void PerformLightBake() ;

	/* Events */
	void HandlePCVRLightBuildComplete();
	void HandleQuest3LightBuildComplete();

	/* Lightbake support */
	void ShowNotification(FString notification, SNotificationItem::ECompletionState State) ;
	bool IsSubLevelVisibleByPath(UWorld* World, UWorld* SubWorld) ;
	void MergeCollisionFromComponents(const TArray<UStaticMeshComponent*>& Components, UStaticMesh* OutMergedMesh);
	void SetSpecificSublevelVisible(UWorld* SubWorld, bool bVisbility) ;
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
	static FReducerResults ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);

};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ZeroPayMod_DefinitionDataAsset.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/MeshMerging.h"          /* FMaterialProxySettings */
#include "Misc/ScopedSlowTask.h"         /* FScopedSlowTask& parameters */
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "GPULightmassModule.h"
#include "GPUlightMassSettings.h"

#include "ZeroPayEditor_ReducerTypes.h"          /* FReducerResults, FReducerRuntimeSettings, FFoundAssetInformation */
#include "ZeroPayEditor_ReducerSettingsAsset.h"  /* UZeroPayEditor_ReducerSettingsAsset and all settings structs */

#include "ZeroPayEditorButtonsPlugin.generated.h"

class FToolBarBuilder;
class FMenuBuilder;
class AStaticMeshActor;


/**********************************************************************************************************************
*
* Class: UZeroPayEditorCookPakOperationHandle
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
* Class: FZeroPayEditorButtonsPluginModule
* Description: Provides the UE editor plugin module - cooking, reducer, light baking, etc.
*
*/

class FZeroPayEditorButtonsPluginModule : public IModuleInterface
{
public:
	/* >>> IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/* >>> OnClick routines triggered by UE buttons on the toolbar */
	void ShowQuest3View_Clicked();
	void ShowPCVRView_Clicked();
	void BakeLightsOnLevels_Clicked();
	void GenerateQuest3ReducedLevel_Clicked();
	void OpenModIOWindow_Clicked();

	/* >>> Cooking logic - Called from Function Library */
	UZeroPayEditorCookPakOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
	UZeroPayEditorCookPakOperationHandle* PollUploadStatus();
	void CancelUploadStatus();

	/* >>> Reducer Logic - Called from Function Library */
	FReducerResults ReduceLevel(
		UZeroPayMod_DefinitionDataAsset* DataAsset,
		UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
		FReducerRuntimeSettings RuntimeSettings);

	FReducerResults Old_ReducePCVRLevelForQuest3(UZeroPayMod_DefinitionDataAsset* DataAsset,
		UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
		FReducerRuntimeSettings RuntimeSettings);

private:

	/* ================================================================== */
	/* Vars                                                                */
	/* ================================================================== */

	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;

	/* Window widget instances */
	UEditorUtilityWidget* WidgetModManagementInstance;
	UEditorUtilityWidget* WidgetQuest3ReducerInstance;

	/* >>> Cooking vars */
	UZeroPayEditorCookPakOperationHandle* CookPakHandle;
	FString GlobalUGCValue;
	FString LastMessage;
	bool bAbortOperation;
	bool bPollCompleted;

	/* >>> Reducer vars
	 * Cached for the duration of a run and read by the UI for debug drawing.
	 * NOTE: the old StaticMeshComponentsToMerge member has been removed - it was
	 * shared mutable state that made the merge path non-reentrant. The merge
	 * backends build their component arrays locally. */
	TArray<FBox> PlayerZoneBounds;

	/* >>> Baking vars */
	UGPULightmassSubsystem* GPULightmassSubsystem;
	FTimerDelegate TimerCallback;
	FTimerHandle TimerHandle;

	/* ================================================================== */
	/* Windows, Menus, Dialogs, etc.                                       */
	/* ================================================================== */

	TSharedRef<SDockTab> SpawnModManagementDockableTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnQuest3ReducerDockableTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);
	TSharedPtr<SWidget> FindWidgetRecursive(TSharedPtr<SWidget> Root, TSharedRef<SWidget> Target);
	FString FormatDataRateResponse(int64 BytesPerSecond);
	void ShowNotification(FString notification, SNotificationItem::ECompletionState State);

	/* ================================================================== */
	/* --->>> Cooking logic <<<---                                         */
	/* ================================================================== */

	bool CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset);

	bool ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName, const TArray<FString>& AlwaysCookDirs);
	bool ReadNextLineFromPipe(HANDLE PipeHandle, FString& OutLine, FString& Remainder);
	bool ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath);

	void UpdateModManagementUIProgressField();

	/* ================================================================== */
	/* --->>> Reducer logic <<<---                                         */
	/* ================================================================== */

	/* ---- Reducer-only types ---- */

	/* A spatial bucket of candidate components. */
	struct FReducerChunk
	{
		FIntVector Key = FIntVector::ZeroValue;
		FBox Bounds = FBox(ForceInit);
		TArray<UStaticMeshComponent*> Components;
	};

	/* One merge operation: a chunk, optionally split by material set and by
	 * MaxMeshesPerChunk, resolved to a single player zone. */
	struct FReducerMergeJob
	{
		FBox Bounds = FBox(ForceInit);
		TArray<UStaticMeshComponent*> Components;
		int32 ZoneIndex = 0;                                   /* 0 = no zoning, otherwise 1..3 */
		const FZeroPayEditor_MeshReductionZone* Zone = nullptr;
		FString MaterialGroupTag;
	};

	/* ---- Entry point ---- */

	FReducerResults ReducePCVRLevelForQuest3(
		UZeroPayMod_DefinitionDataAsset* DataAsset,
		UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
		FReducerRuntimeSettings RuntimeSettings);

	/* ---- Pipeline ---- */

	/* Single predicate shared by the bounds pass and the partition pass, so the
	 * two cannot disagree about what is in the level. */
	bool ShouldConsiderComponent(
		UStaticMeshComponent* Component,
		const UZeroPayEditor_ReducerSettingsAsset* Settings) const;

	FBox GetMaximumVisibleBoundingBox(
		ULevel* Level,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		bool& bOutUsedReducerZone) const;

	TArray<FReducerChunk> PartitionComponentsIntoChunks(
		const FBox& GlobalBounds,
		const FVector& ChunkSize,
		ULevel* Level,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		FReducerResults& Results) const;

	TArray<FReducerMergeJob> BuildMergeJobs(
		const TArray<FReducerChunk>& Chunks,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		const TArray<FBox>& InPlayerZoneBounds,
		FReducerResults& Results) const;

	bool RunMergeJobs(
		const TArray<FReducerMergeJob>& Jobs,
		const FString& TargetFolderPath,
		UWorld* Quest3World,
		ULevel* Quest3Level,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		const FReducerRuntimeSettings& RuntimeSettings,
		FScopedSlowTask& ParentTask,
		FReducerResults& Results);

	bool ExecuteMergeJob(
		const FReducerMergeJob& Job,
		const FString& PackageName,
		UWorld* Quest3World,
		ULevel* Quest3Level,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		FReducerResults& Results);

	/* ---- Merge backends ---- */

	/* Straight merge. Preserves source UVs, vertex colours, lightmap UVs and
	 * collision. The default for everything except distant zones. */
	bool MergeComponents_Standard(
		const FReducerMergeJob& Job,
		const FString& PackageName,
		UWorld* Quest3World,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		TArray<UObject*>& OutAssets,
		FVector& OutMergedLocation,
		FReducerResults& Results);

	/* Proxy (ProxyLOD) generation. Rebuilds geometry and always bakes an atlas.
	 * Gated behind the per-zone bUseProxyMerge. */
	bool MergeComponents_Proxy(
		const FReducerMergeJob& Job,
		const FString& PackageName,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		TArray<UObject*>& OutAssets,
		FReducerResults& Results);

	/* MATERIAL REDUCTION HOOK. Every material/atlas decision in the reducer
	 * flows through this one function - the material reduction stage should
	 * extend this and nothing else. */
	void ConfigureMaterialProxySettings(
		FMaterialProxySettings& OutSettings,
		const FReducerMergeJob& Job,
		const UZeroPayEditor_ReducerSettingsAsset* Settings) const;

	/* ---- Post-merge ---- */

	void ApplyTriangleReduction(
		UStaticMesh* Mesh,
		const FReducerMergeJob& Job,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		FReducerResults& Results) const;

	void MergeCollisionFromComponents(
		const TArray<UStaticMeshComponent*>& Components,
		UStaticMesh* OutMergedMesh,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		FReducerResults& Results) const;

	AStaticMeshActor* PlaceMergedMeshInQuest3Level(
		UStaticMesh* MergedMesh,
		const FVector& SpawnLocation,
		const FReducerMergeJob& Job,
		ULevel* Level,
		const UZeroPayEditor_ReducerSettingsAsset* Settings,
		FReducerResults& Results) const;

	/* ---- Reducer housekeeping ---- */

	FFoundAssetInformation ScanLevelActorsAndDirectory(
		ULevel* LevelToScan,
		const FString& TargetAssetPath) const;

	bool DeleteActorsAndAssets(
		ULevel* TargetLevel,
		const FString& AssetFolderPathToDelete);

	static float BoxSurfaceDistance(const FBox& A, const FBox& B);

	void UpdateQuest3ReducerUIProgressField();

	/* ================================================================== */
	/* --->>> Light baking logic <<<---                                    */
	/* ================================================================== */

	bool bPCVRLevel_OriginalVisibility;
	bool bQuest3Level_OriginalVisibility;
	UWorld* persistentLeveLightBake;
	UWorld* pcvrLevelLightBake;
	UWorld* quest3LevelLightBake;

	void PerformLightBake();

	/* Events */
	void HandlePCVRLightBuildComplete();
	void HandleQuest3LightBuildComplete();

	/* Lightbake support */
	bool IsSubLevelVisibleByPath(UWorld* World, UWorld* SubWorld);
	void SetSpecificSublevelVisible(UWorld* SubWorld, bool bVisbility);
};


/**********************************************************************************************************************
*
* Class: UZeroPayEditorButtonsFunctionLibrary
* Description: Provides a BP callable interface to the plugin's core operations
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

	/* Reduces a PCVR level using the supplied settings, to a Quest3 level */
	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static FReducerResults ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings);

	UFUNCTION(BlueprintCallable, Category = "ZeroPayMod Editor")
	static AActor* LoadLevelAndFindActorOfClass(TSoftObjectPtr<UWorld> Level, TSubclassOf<AActor> ActorClass, bool& bLevelWasAlreadyLoaded);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Editor")
	static bool UnloadInspectedLevel(TSoftObjectPtr<UWorld> Level);
};

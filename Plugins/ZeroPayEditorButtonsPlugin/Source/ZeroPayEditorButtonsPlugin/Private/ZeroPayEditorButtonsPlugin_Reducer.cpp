// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "IAssetTools.h"
#include "IMeshMergeUtilities.h"
#include "IMeshReductionInterfaces.h"
#include "IMeshReductionManagerModule.h"
#include "MeshMergeModule.h"
#include "MeshMerge/MeshMergingSettings.h"
#include "Engine/MeshMerging.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "DrawDebugHelpers.h"

#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"

#include "Components/InstancedStaticMeshComponent.h"

#include "ZeroPayEditor_Reducer_Zone.h"
#include "ZeroPayEditor_Reducer_PlayerZone.h"

DEFINE_LOG_CATEGORY_STATIC(LogZeroPayReducer, Log, All);


namespace ZeroPayReducer
{
	/* World Outliner folder that generated actors are placed into. Everything in
	 * this folder is considered disposable and is destroyed on re-run. */
	static const TCHAR* GeneratedFolder = TEXT("ReducedAssets");

	/* Actor tag stamped onto generated actors so that a re-run can identify them
	 * even if a user moves them out of the folder. */
	static const FName GeneratedActorTag(TEXT("ZeroPay_Generated"));

	/* Full garbage collections are expensive. Run one every N merge jobs rather
	 * than once per job (the original ran one per island, which on a fine grid
	 * meant thousands of full GCs). */
	static constexpr int32 GarbageCollectEveryNJobs = 4 ;

	/* Component/actor classes that must never contribute to bounds or merging. */
	static bool IsReducerHelperActor(const AActor* Actor)
	{
		return Actor
			&& (Actor->IsA<AZeroPayEditor_Reducer_Zone>()
				|| Actor->IsA<AZeroPayEditor_Reducer_PlayerZone>());
	}

	/* Stable key describing a component's material set, used to group components
	 * that can merge without needing an atlas. Sorted so that ordering
	 * differences between two otherwise identical components do not split them
	 * into separate groups. */
	static FString BuildMaterialGroupTag(const UStaticMeshComponent* Component)
	{
		TArray<FString> Paths;
		const int32 NumMaterials = Component->GetNumMaterials();
		Paths.Reserve(NumMaterials);

		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			const UMaterialInterface* Material = Component->GetMaterial(Index);
			Paths.Add(Material ? Material->GetPathName() : TEXT("None"));
		}

		Paths.Sort();
		return FString::Join(Paths, TEXT("|"));
	}

	static int32 GetComponentTriangleCount(const UStaticMeshComponent* Component)
	{
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
		{
			return 0;
		}
		return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
	}
}


/********************************************************************************************************/
/*                                             ENTRY POINT                                              */
/********************************************************************************************************/

FReducerResults FZeroPayEditorButtonsPluginModule::ReduceLevel(
	UZeroPayMod_DefinitionDataAsset* DataAsset,
	UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
	FReducerRuntimeSettings RuntimeSettings)
{
	return ReducePCVRLevelForQuest3(DataAsset, ReducerSettings, RuntimeSettings);
}


/********************************************************************************************************/
/*                            STAGE 1 - SPATIAL MERGE OF STATIC GEOMETRY                                */
/********************************************************************************************************/

FReducerResults FZeroPayEditorButtonsPluginModule::Old_ReducePCVRLevelForQuest3(
	UZeroPayMod_DefinitionDataAsset* DataAsset,
	UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
	FReducerRuntimeSettings RuntimeSettings)
{
	FReducerResults Results;
	const double StartTime = FPlatformTime::Seconds();

	/* ---------------------------------------------------------------- */
	/* Validate inputs                                                   */
	/* ---------------------------------------------------------------- */

	if (!DataAsset)
	{
		Results.Fail(TEXT("Error S1 - No definition data asset supplied."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	if (!ReducerSettings)
	{
		Results.Fail(TEXT("Error S1 - No reducer settings asset supplied."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	if (DataAsset->Definition.pcvrlevel.IsNull())
	{
		Results.Fail(TEXT("Error S1 - PCVR Level is not defined (in data asset in UGC folder)"));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	if (DataAsset->Definition.quest3level.IsNull())
	{
		Results.Fail(TEXT("Error S1 - Quest3 Level is not defined (in data asset in UGC folder)"));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	/* Resolve the soft pointers ONCE. The original code called .Get() on an
	 * unloaded soft pointer and dereferenced the result, which is a null deref
	 * whenever the level is not already in memory. */
	UWorld* PCVRWorld = DataAsset->Definition.pcvrlevel.LoadSynchronous();
	if (!PCVRWorld || !PCVRWorld->PersistentLevel)
	{
		Results.Fail(TEXT("Error S1 - PCVR level could not be loaded."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	UWorld* Quest3World = DataAsset->Definition.quest3level.LoadSynchronous();
	if (!Quest3World || !Quest3World->PersistentLevel)
	{
		Results.Fail(TEXT("Error S2 - Quest3 level could not be loaded."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	ULevel* PCVRLevel = PCVRWorld->PersistentLevel;
	ULevel* Quest3Level = Quest3World->PersistentLevel;

	/* Spawning requires the target level to be part of the world currently open
	 * in the editor, otherwise SpawnActor has nowhere sensible to put things. */
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld || Quest3Level->OwningWorld != EditorWorld)
	{
		Results.Fail(TEXT("Error S2 - The Quest 3 level is not loaded as a sub-level of the currently open persistent level. Open the persistent level and make the Quest 3 sub-level loaded and visible before running the reducer."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	const bool bDryRun = ReducerSettings->GeneralSettings.bDryRun;
	Results.bDryRun = bDryRun;

	const FString ReducedAssetMeshPath = FString::Printf(
		TEXT("/Game/ZeroPayMods/UGC%s/Levels/ReducedAssets/Meshes"),
		*DataAsset->Definition.UGCID);

	/* ---------------------------------------------------------------- */
	/* Confirm destruction of the previous run                           */
	/* ---------------------------------------------------------------- */

	const FFoundAssetInformation Existing = ScanLevelActorsAndDirectory(Quest3Level, ReducedAssetMeshPath);
	if (!bDryRun && (Existing.FoundAssets > 0 || Existing.FoundInstances > 0) && !RuntimeSettings.bSuppressPrompts)
	{
		const FString DialogMessage = FString::Printf(
			TEXT("Warning!\n\nThere are %d found assets in the Content Browser (merged meshes, materials, etc.) that will be destroyed and recreated.\nThere are %d generated actors in the Quest 3 level that will be destroyed and recreated.\n\nAre you sure? You cannot undo these changes later."),
			Existing.FoundAssets, Existing.FoundInstances);

		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(DialogMessage)) != EAppReturnType::Yes)
		{
			Results.Fail(TEXT("User aborted."));
			LastMessage = Results.FailureReason;
			UpdateQuest3ReducerUIProgressField();
			return Results;
		}
	}

	/* ---------------------------------------------------------------- */
	/* Progress. ONE dialog for the whole run. Nested FScopedSlowTasks   */
	/* feed this automatically and must NOT call MakeDialog themselves.  */
	/* ---------------------------------------------------------------- */

	enum { Work_Cleanup = 1, Work_Bounds = 1, Work_Partition = 1, Work_Jobs = 1, Work_Merge = 6, Work_Save = 1 };
	constexpr float TotalWork = Work_Cleanup + Work_Bounds + Work_Partition + Work_Jobs + Work_Merge + Work_Save;

	FScopedSlowTask SlowTask(TotalWork, FText::FromString(TEXT("Reducing PCVR level for Quest 3...")));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	/* ---------------------------------------------------------------- */
	/* Delete the previous run                                           */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Cleanup, FText::FromString(TEXT("Removing previously generated content...")));

	if (!bDryRun)
	{
		if (!DeleteActorsAndAssets(Quest3Level, ReducedAssetMeshPath))
		{
			Results.Fail(TEXT("Failed to delete the existing generated actors and/or the assets under 'UGC/Levels/ReducedAssets'."));
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Results.FailureReason));
			LastMessage = Results.FailureReason;
			UpdateQuest3ReducerUIProgressField();
			return Results;
		}
	}

	/* ---------------------------------------------------------------- */
	/* Player zones                                                      */
	/* ---------------------------------------------------------------- */

	PlayerZoneBounds.Reset();

	if (ReducerSettings->MeshReductionSettings.bEnablePlayerZoning)
	{
		for (TActorIterator<AZeroPayEditor_Reducer_PlayerZone> It(PCVRWorld); It; ++It)
		{
			if (const AZeroPayEditor_Reducer_PlayerZone* Zone = *It)
			{
				const FBox Bounds = Zone->GetWorldBoundingBox();
				if (Bounds.IsValid)
				{
					PlayerZoneBounds.Add(Bounds);
				}
			}
		}

		if (PlayerZoneBounds.Num() == 0)
		{
			Results.Fail(TEXT("bEnablePlayerZoning is enabled but no AZeroPayEditor_Reducer_PlayerZone actors were found in the PCVR level. Place them in the PCVR sub-level (not the persistent level) and make sure that sub-level is visible."));
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Results.FailureReason));
			LastMessage = Results.FailureReason;
			UpdateQuest3ReducerUIProgressField();
			return Results;
		}
	}

	/* ---------------------------------------------------------------- */
	/* Bounds                                                            */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Bounds, FText::FromString(TEXT("Computing level bounds...")));

	bool bUsedReducerZone = false;
	const FBox MaxBoundingBox = GetMaximumVisibleBoundingBox(PCVRLevel, ReducerSettings, bUsedReducerZone);

	if (!MaxBoundingBox.IsValid)
	{
		Results.Fail(TEXT("No mergeable static mesh components were found in the PCVR level. Check that the sub-level is loaded and visible, and that bMergeOnlyStaticActors is not excluding everything."));
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Results.FailureReason));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	if (bUsedReducerZone)
	{
		/* Notification is raised here, by the caller, rather than inside what
		 * should be a pure geometry query. */
		ShowNotification(
			TEXT("The reduction has been limited to the bounds of the AZeroPayEditor_Reducer_Zone actor found in the PCVR level. Meshes outside this box are ignored."),
			SNotificationItem::ECompletionState::CS_Success);
	}

	if (RuntimeSettings.bStage1_ShowVisualDebug && EditorWorld)
	{
		FlushPersistentDebugLines(EditorWorld);
		DrawDebugBox(EditorWorld, MaxBoundingBox.GetCenter(), MaxBoundingBox.GetExtent(),
			FColor::Blue, false, RuntimeSettings.fStage1_VisualDebugDuration, 0, 5.0f);
	}

	/* ---------------------------------------------------------------- */
	/* Partition                                                         */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Partition, FText::FromString(TEXT("Partitioning level into chunks...")));

	const float RawChunkSize = FMath::Max(1.0f, ReducerSettings->MeshReductionSettings.BoundingChunkSize);
	const FVector ChunkSize(RawChunkSize);

	TArray<FReducerChunk> Chunks = PartitionComponentsIntoChunks(
		MaxBoundingBox, ChunkSize, PCVRLevel, ReducerSettings, Results);

	Results.ChunkCount = Chunks.Num();

	if (Chunks.Num() == 0)
	{
		Results.Fail(TEXT("Partitioning produced no chunks - nothing to merge."));
		LastMessage = Results.FailureReason;
		UpdateQuest3ReducerUIProgressField();
		return Results;
	}

	/* ---------------------------------------------------------------- */
	/* Build merge jobs (zone resolution, material grouping, batching)   */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Jobs, FText::FromString(TEXT("Building merge jobs...")));

	const TArray<FReducerMergeJob> Jobs = BuildMergeJobs(Chunks, ReducerSettings, PlayerZoneBounds, Results);
	Results.MergeJobCount = Jobs.Num();

	UE_LOG(LogZeroPayReducer, Log, TEXT("%d chunks produced %d merge jobs from %d components."),
		Chunks.Num(), Jobs.Num(), Results.OriginalComponentCount);

	/* ---------------------------------------------------------------- */
	/* Merge                                                             */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Merge, FText::FromString(TEXT("Generating merged meshes...")));

	{
		FScopedSlowTask MergeTask(static_cast<float>(Jobs.Num()), FText::FromString(TEXT("Merging...")));
		/* Deliberately no MakeDialog() - this nests into SlowTask. */

		if (!RunMergeJobs(Jobs, ReducedAssetMeshPath, Quest3World, Quest3Level,
			ReducerSettings, RuntimeSettings, MergeTask, Results))
		{
			LastMessage = Results.bCancelled ? TEXT("Cancelled by user.") : Results.FailureReason;
			UpdateQuest3ReducerUIProgressField();
			return Results;
		}
	}

	/* ---------------------------------------------------------------- */
	/* Save                                                              */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Save, FText::FromString(TEXT("Saving...")));

	if (!bDryRun)
	{
		UPackage* LevelPackage = Quest3Level->GetOutermost();
		const bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave(
			{ LevelPackage }, /*bCheckDirty=*/true, /*bPromptToSave=*/false)
			== FEditorFileUtils::EPromptReturnCode::PR_Success;

		if (!bSaved)
		{
			Results.Fail(TEXT("Failed to save the updated Quest 3 sub-level. Please save it manually."));
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Results.FailureReason));
			LastMessage = Results.FailureReason;
			UpdateQuest3ReducerUIProgressField();
			return Results;
		}
	}

	Results.bFailed = false;
	Results.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - StartTime);

	LastMessage = FString::Printf(
		TEXT("%s complete in %.1fs. Actors %d -> %d, triangles %lld -> %lld, material slots %d -> %d.%s"),
		bDryRun ? TEXT("Dry run") : TEXT("Reduction"),
		Results.ElapsedSeconds,
		Results.OriginalActorCount, Results.ReducedActorCount,
		Results.OriginalTriangleCount, Results.ReducedTriangleCount,
		Results.OriginalMaterialSlotCount, Results.ReducedMaterialSlotCount,
		Results.FailedMergeJobCount > 0
			? *FString::Printf(TEXT(" %d job(s) failed - see Output Log."), Results.FailedMergeJobCount)
			: TEXT(""));

	UE_LOG(LogZeroPayReducer, Log, TEXT("%s"), *LastMessage);
	UpdateQuest3ReducerUIProgressField();

	return Results;
}


/********************************************************************************************************/
/*                                       COMPONENT FILTERING                                            */
/********************************************************************************************************/

/*
 * Single predicate shared by the bounds pass and the partition pass. Having one
 * predicate is what stops the two passes from disagreeing about what is in the
 * level, which is how you end up with thousands of empty chunks.
 */
bool FZeroPayEditorButtonsPluginModule::ShouldConsiderComponent(
	UStaticMeshComponent* Component,
	const UZeroPayEditor_ReducerSettingsAsset* Settings) const
{
	if (!IsValid(Component) || !Component->IsRegistered())
	{
		return false;
	}

	if (!Component->GetStaticMesh())
	{
		return false;
	}

	AActor* Owner = Component->GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	/* Editor-only helpers (billboards, arrows, volumes, our own zone actors)
	 * must never contribute to bounds or merging. */
	if (Owner->IsEditorOnly() || Component->IsEditorOnly() || Component->bIsEditorOnly)
	{
		return false;
	}

	if (ZeroPayReducer::IsReducerHelperActor(Owner))
	{
		return false;
	}

	/* Hidden geometry is usually a helper or a disabled variant. Merging it
	 * bakes invisible triangles into the output. */
	if (!Component->IsVisible() || Component->bHiddenInGame)
	{
		return false;
	}

	/* Anything that can move must not be baked into static geometry - doors,
	 * pickups and physics props would silently stop working. */
	if (Settings->MeshReductionSettings.bMergeOnlyStaticActors
		&& Component->Mobility != EComponentMobility::Static)
	{
		return false;
	}

	/* An instanced component is already a single draw call; merging it would
	 * expand every instance into unique geometry. */
	if (Component->IsA<UInstancedStaticMeshComponent>())
	{
		return false;
	}

	return true;
}


/********************************************************************************************************/
/*                                             BOUNDS                                                   */
/********************************************************************************************************/

FBox FZeroPayEditorButtonsPluginModule::GetMaximumVisibleBoundingBox(
	ULevel* Level,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	bool& bOutUsedReducerZone) const
{
	bOutUsedReducerZone = false;

	if (!Level)
	{
		return FBox(ForceInit);
	}

	/* An explicit reducer zone overrides everything. */
	for (AActor* Actor : Level->Actors)
	{
		if (const AZeroPayEditor_Reducer_Zone* ReducerZone = Cast<AZeroPayEditor_Reducer_Zone>(Actor))
		{
			const FBox ZoneBox = ReducerZone->GetWorldBoundingBox();
			if (ZoneBox.IsValid)
			{
				bOutUsedReducerZone = true;
				return ZoneBox;
			}
		}
	}

	/* Otherwise accumulate only the components we would actually merge. The
	 * original accumulated every visible primitive, so a single stray editor
	 * sprite at the edge of the map inflated the global bounds. */
	FBox BoundingBox(ForceInit);

	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Comp);
			if (MeshComponent && ShouldConsiderComponent(MeshComponent, Settings))
			{
				BoundingBox += MeshComponent->Bounds.GetBox();
			}
		}
	}

	return BoundingBox;
}


/********************************************************************************************************/
/*                                           PARTITIONING                                               */
/********************************************************************************************************/

TArray<FZeroPayEditorButtonsPluginModule::FReducerChunk>
FZeroPayEditorButtonsPluginModule::PartitionComponentsIntoChunks(
	const FBox& GlobalBounds,
	const FVector& ChunkSize,
	ULevel* Level,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	FReducerResults& Results) const
{
	TArray<FReducerChunk> Chunks;

	if (!Level || !GlobalBounds.IsValid || ChunkSize.X <= 0.0 || ChunkSize.Y <= 0.0 || ChunkSize.Z <= 0.0)
	{
		return Chunks;
	}

	const FVector Origin = GlobalBounds.Min;

	/* Sparse map keyed by grid coordinate. The original flattened the key into
	 * an int32, which overflows on large levels or small chunk sizes and
	 * silently collapses distant chunks into the same bucket - merging geometry
	 * from opposite ends of the map. A sparse FIntVector key cannot overflow and
	 * removes the need for the dense CountX/Y/Z index space entirely. */
	TMap<FIntVector, int32> KeyToChunkIndex;

	TSet<AActor*> CountedActors;

	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Comp);
			if (!MeshComponent || !ShouldConsiderComponent(MeshComponent, Settings))
			{
				continue;
			}

			/* Chunk by COMPONENT bounds origin, not actor centre. A long wall
			 * actor with several mesh components was previously assigned wholly
			 * to the chunk containing its centre, dragging components tens of
			 * metres away into that chunk. */
			const FVector Centre = MeshComponent->Bounds.Origin;

			/* Clamp rather than reject. A component sitting exactly on
			 * GlobalBounds.Max previously failed the >= test and was silently
			 * dropped from the output level. */
			const FIntVector Key(
				FMath::FloorToInt32((Centre.X - Origin.X) / ChunkSize.X),
				FMath::FloorToInt32((Centre.Y - Origin.Y) / ChunkSize.Y),
				FMath::FloorToInt32((Centre.Z - Origin.Z) / ChunkSize.Z));

			int32* ExistingIndex = KeyToChunkIndex.Find(Key);
			int32 ChunkIndex;

			if (ExistingIndex)
			{
				ChunkIndex = *ExistingIndex;
			}
			else
			{
				const FVector BoxMin = Origin + FVector(Key.X, Key.Y, Key.Z) * ChunkSize;
				FReducerChunk NewChunk;
				NewChunk.Key = Key;
				NewChunk.Bounds = FBox(BoxMin, BoxMin + ChunkSize);
				ChunkIndex = Chunks.Add(MoveTemp(NewChunk));
				KeyToChunkIndex.Add(Key, ChunkIndex);
			}

			Chunks[ChunkIndex].Components.Add(MeshComponent);

			/* Statistics */
			Results.OriginalComponentCount++;
			Results.OriginalMaterialSlotCount += MeshComponent->GetNumMaterials();

			if (const UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
			{
				if (const FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
				{
					if (RenderData->LODResources.Num() > 0)
					{
						const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
						Results.OriginalTriangleCount += LOD0.GetNumTriangles();
						Results.OriginalVertexCount += LOD0.GetNumVertices();
					}
				}
			}

			bool bAlreadyCounted = false;
			CountedActors.Add(Actor, &bAlreadyCounted);
			if (!bAlreadyCounted)
			{
				Results.OriginalActorCount++;
			}
		}
	}

	UE_LOG(LogZeroPayReducer, Log,
		TEXT("Partitioned %d components from %d actors into %d chunks (chunk size %.0f)."),
		Results.OriginalComponentCount, Results.OriginalActorCount, Chunks.Num(), ChunkSize.X);

	return Chunks;
}


/********************************************************************************************************/
/*                                          MERGE JOBS                                                  */
/********************************************************************************************************/

/*
 * Turns chunks into concrete merge jobs by:
 *   1. resolving the player zone for the chunk (ABSOLUTE distances),
 *   2. grouping by material set when bBakeMergedMaterials is off, so that a
 *      merged mesh never needs more material slots than its inputs had,
 *   3. batching oversized groups down to MaxMeshesPerChunk instead of aborting
 *      the whole run the way the original did.
 *
 * When the material reduction stage lands, setting bBakeMergedMaterials skips
 * step 2 entirely and the whole chunk becomes one job with one atlased material.
 */
TArray<FZeroPayEditorButtonsPluginModule::FReducerMergeJob>
FZeroPayEditorButtonsPluginModule::BuildMergeJobs(
	const TArray<FReducerChunk>& Chunks,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	const TArray<FBox>& InPlayerZoneBounds,
	FReducerResults& Results) const
{
	TArray<FReducerMergeJob> Jobs;

	const FZeroPayEditor_MeshReductionSettings& MeshSettings = Settings->MeshReductionSettings;
	const int32 MaxPerJob = FMath::Max(1, MeshSettings.MaxMeshesPerChunk);

	/* Zones in near-to-far order. Distances are ABSOLUTE: zone 1 is
	 * [0, Zone1.Distance), zone 2 is [Zone1.Distance, Zone2.Distance), zone 3
	 * is everything beyond. */
	const FZeroPayEditor_MeshReductionZone* ZonesByDistance[3] = {
		&MeshSettings.PlayerZone1_Settings,
		&MeshSettings.PlayerZone2_Settings,
		&MeshSettings.PlayerZone3_Settings
	};

	if (MeshSettings.bEnablePlayerZoning)
	{
		if (ZonesByDistance[1]->Distance <= ZonesByDistance[0]->Distance
			|| ZonesByDistance[2]->Distance <= ZonesByDistance[1]->Distance)
		{
			Results.Warn(TEXT("Player zone distances are not strictly increasing. Zones are interpreted as absolute radii and must be ordered nearest to furthest."));
		}
	}

	for (const FReducerChunk& Chunk : Chunks)
	{
		if (Chunk.Components.Num() == 0)
		{
			continue;
		}

		/* ---- Zone resolution ---- */

		int32 ZoneIndex = 0;
		const FZeroPayEditor_MeshReductionZone* Zone = nullptr;

		if (MeshSettings.bEnablePlayerZoning && InPlayerZoneBounds.Num() > 0)
		{
			float ClosestDistance = TNumericLimits<float>::Max();
			for (const FBox& ZoneBox : InPlayerZoneBounds)
			{
				ClosestDistance = FMath::Min(ClosestDistance, BoxSurfaceDistance(Chunk.Bounds, ZoneBox));
			}

			if (ClosestDistance < ZonesByDistance[0]->Distance)
			{
				ZoneIndex = 1;
			}
			else if (ClosestDistance < ZonesByDistance[1]->Distance)
			{
				ZoneIndex = 2;
			}
			else
			{
				ZoneIndex = 3;
			}

			Zone = ZonesByDistance[ZoneIndex - 1];
		}

		/* ---- Material grouping ---- */

		TMap<FString, TArray<UStaticMeshComponent*>> Groups;

		if (MeshSettings.bMergeByMaterial && !MeshSettings.bBakeMergedMaterials)
		{
			for (UStaticMeshComponent* Component : Chunk.Components)
			{
				Groups.FindOrAdd(ZeroPayReducer::BuildMaterialGroupTag(Component)).Add(Component);
			}
		}
		else
		{
			/* One group. Either the user wants a single merged result per chunk,
			 * or material baking will collapse the slots for us. */
			Groups.Add(TEXT("All"), Chunk.Components);
		}

		/* ---- Batching ---- */

		for (TPair<FString, TArray<UStaticMeshComponent*>>& Group : Groups)
		{
			TArray<UStaticMeshComponent*>& GroupComponents = Group.Value;

			if (GroupComponents.Num() > MaxPerJob)
			{
				Results.Warn(FString::Printf(
					TEXT("Chunk at %s has %d meshes in one material group (MaxMeshesPerChunk is %d). Splitting into batches. Consider increasing BoundingChunkSize or MaxMeshesPerChunk."),
					*Chunk.Bounds.GetCenter().ToCompactString(), GroupComponents.Num(), MaxPerJob));
			}

			for (int32 Start = 0; Start < GroupComponents.Num(); Start += MaxPerJob)
			{
				const int32 Count = FMath::Min(MaxPerJob, GroupComponents.Num() - Start);

				FReducerMergeJob Job;
				Job.Bounds = Chunk.Bounds;
				Job.ZoneIndex = ZoneIndex;
				Job.Zone = Zone;
				Job.MaterialGroupTag = Group.Key;
				Job.Components.Append(GroupComponents.GetData() + Start, Count);

				Jobs.Add(MoveTemp(Job));
			}
		}
	}

	return Jobs;
}


/********************************************************************************************************/
/*                                          MERGE EXECUTION                                             */
/********************************************************************************************************/

bool FZeroPayEditorButtonsPluginModule::RunMergeJobs(
	const TArray<FReducerMergeJob>& Jobs,
	const FString& TargetFolderPath,
	UWorld* Quest3World,
	ULevel* Quest3Level,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	const FReducerRuntimeSettings& RuntimeSettings,
	FScopedSlowTask& ParentTask,
	FReducerResults& Results)
{
	const bool bDryRun = Settings->GeneralSettings.bDryRun;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	int32 JobIndex = 0;

	for (const FReducerMergeJob& Job : Jobs)
	{
		++JobIndex;

		/* Cancellation. A mis-configured run over tens of thousands of actors
		 * was previously unkillable. */
		if (ParentTask.ShouldCancel())
		{
			Results.bCancelled = true;
			Results.Fail(TEXT("Cancelled by user."));
			return false;
		}

		ParentTask.EnterProgressFrame(1.0f, FText::FromString(
			FString::Printf(TEXT("Merging %d of %d (zone %d, %d meshes)..."),
				JobIndex, Jobs.Num(), Job.ZoneIndex, Job.Components.Num())));

		/* Debug draw */
		if (RuntimeSettings.bStage1_ShowVisualDebug && EditorWorld)
		{
			FColor Color = FColor::White;
			switch (Job.ZoneIndex)
			{
			case 1: Color = FColor::Green;          break;
			case 2: Color = FColor(255, 165, 0);    break;
			case 3: Color = FColor::Red;            break;
			default: Color = FColor::White;         break;
			}

			DrawDebugBox(EditorWorld, Job.Bounds.GetCenter(), Job.Bounds.GetExtent(),
				Color, false, RuntimeSettings.fStage1_VisualDebugDuration, 0, 2.0f);
		}

		if (bDryRun)
		{
			continue;
		}

		const FString PackageName = FString::Printf(TEXT("%s/SM_Merged_%05d"), *TargetFolderPath, JobIndex);

		if (!ExecuteMergeJob(Job, PackageName, Quest3World, Quest3Level, Settings, Results))
		{
			/* A single failed job should not abort the whole level. */
			Results.FailedMergeJobCount++;
		}

		/* Periodic GC rather than one per job. */
		if (GEngine && (JobIndex % ZeroPayReducer::GarbageCollectEveryNJobs) == 0)
		{
			GEngine->ForceGarbageCollection(true);
		}
	}

	if (GEngine && !bDryRun)
	{
		GEngine->ForceGarbageCollection(true);
	}

	return true;
}


bool FZeroPayEditorButtonsPluginModule::ExecuteMergeJob(
	const FReducerMergeJob& Job,
	const FString& PackageName,
	UWorld* Quest3World,
	ULevel* Quest3Level,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	FReducerResults& Results)
{
	TArray<UObject*> CreatedAssets;
	FVector MergedLocation = FVector::ZeroVector;

	const bool bUseProxy = Job.Zone && Job.Zone->bUseProxyMerge;

	const bool bMerged = bUseProxy
		? MergeComponents_Proxy(Job, PackageName, Settings, CreatedAssets, Results)
		: MergeComponents_Standard(Job, PackageName, Quest3World, Settings, CreatedAssets, MergedLocation, Results);

	if (!bMerged || CreatedAssets.Num() == 0)
	{
		Results.Warn(FString::Printf(TEXT("Merge produced no assets for %s."), *PackageName));
		return false;
	}

	/* Register everything that came back, then place every static mesh. The
	 * original used FindItemByClass and placed only the FIRST mesh, silently
	 * dropping any others. */
	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	bool bPlacedAny = false;

	for (UObject* Asset : CreatedAssets)
	{
		if (!IsValid(Asset))
		{
			continue;
		}

		AssetRegistry.AssetCreated(Asset);

		if (UStaticMesh* MergedMesh = Cast<UStaticMesh>(Asset))
		{
			/* Clamp before any rebuild - see MergedLightmapResolution. */
			MergedMesh->SetLightMapResolution(FMath::Clamp(
				Settings->MeshReductionSettings.MergedLightmapResolution, 32, 1024));
			MergedMesh->SetLightMapCoordinateIndex(1);

			/* Optional post-merge simplification, driven by the zone. */
			ApplyTriangleReduction(MergedMesh, Job, Settings, Results);

			if (Settings->MeshReductionSettings.bPreserveCollision)
			{
				MergeCollisionFromComponents(Job.Components, MergedMesh, Settings, Results);
			}

			MergedMesh->Modify();
			MergedMesh->MarkPackageDirty();

			if (PlaceMergedMeshInQuest3Level(MergedMesh, MergedLocation, Job, Quest3Level, Settings, Results))
			{
				bPlacedAny = true;
			}
		}
	}

	return bPlacedAny;
}


/********************************************************************************************************/
/*                                         MERGE BACKENDS                                               */
/********************************************************************************************************/

/*
 * Straight merge. Concatenates geometry, preserves source UVs, vertex colours,
 * lightmap UVs and collision. This is the right default for a PCVR -> Quest
 * pass where the source art is already reasonable.
 */
bool FZeroPayEditorButtonsPluginModule::MergeComponents_Standard(
	const FReducerMergeJob& Job,
	const FString& PackageName,
	UWorld* Quest3World,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	TArray<UObject*>& OutAssets,
	FVector& OutMergedLocation,
	FReducerResults& Results)
{
	const IMeshMergeUtilities& MeshMergeUtilities =
		FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

	/* Local, not a member. The member version made this path non-reentrant. */
	TArray<UPrimitiveComponent*> ComponentsToMerge;
	ComponentsToMerge.Reserve(Job.Components.Num());
	for (UStaticMeshComponent* Component : Job.Components)
	{
		if (IsValid(Component) && Component->GetStaticMesh())
		{
			ComponentsToMerge.Add(Component);
		}
	}

	if (ComponentsToMerge.Num() == 0)
	{
		return false;
	}

	const FZeroPayEditor_MeshReductionSettings& MeshSettings = Settings->MeshReductionSettings;

	FMeshMergingSettings MergeSettings;

	/* Source detail selection. Pulling a cheaper source LOD is the cheapest
	 * possible triangle reduction because it costs nothing at merge time. */
	if (Job.Zone && Job.Zone->SourceLODIndex >= 0)
	{
		MergeSettings.LODSelectionType = EMeshLODSelectionType::SpecificLOD;
		MergeSettings.SpecificLOD = FMath::Clamp(Job.Zone->SourceLODIndex, 0, 7);
	}
	else
	{
		MergeSettings.LODSelectionType = EMeshLODSelectionType::CalculateLOD;
	}

	MergeSettings.bMergePhysicsData = MeshSettings.bPreserveCollision;
	MergeSettings.bBakeVertexDataToMesh = MeshSettings.bPreserveVertexColours;
	MergeSettings.bGenerateLightMapUV = MeshSettings.bPreserveLightmapUVs;
	MergeSettings.bReuseMeshLightmapUVs = MeshSettings.bPreserveLightmapUVs;

	/* NEVER let the engine compute this. It derives the resolution from the
	 * merged bounds, which on a large chunk produces a value far beyond the
	 * uint16 limit in FAllocator2D and asserts inside the UV packer. */
	MergeSettings.bComputedLightMapResolution = false;
	MergeSettings.TargetLightMapResolution = FMath::Clamp(
		MeshSettings.MergedLightmapResolution, 32, 1024);

	/* Pivot at the merged bounds rather than world origin keeps the actor
	 * transform meaningful and gives culling something sane to work with. */
	MergeSettings.bPivotPointAtZero = false;

	/* Distance fields are desktop-only cost on a mobile target. */
	MergeSettings.bAllowDistanceField = false;

	/* Material baking. OFF for now - components are grouped by material set
	 * instead, so the merged mesh needs no more slots than its inputs had.
	 * The material reduction stage flips bBakeMergedMaterials and everything
	 * else it needs lives in ConfigureMaterialProxySettings. */
	MergeSettings.bMergeMaterials = MeshSettings.bBakeMergedMaterials;
	MergeSettings.bMergeEquivalentMaterials = true;

	if (MergeSettings.bMergeMaterials)
	{
		ConfigureMaterialProxySettings(MergeSettings.MaterialSettings, Job, Settings);
	}

	MeshMergeUtilities.MergeComponentsToStaticMesh(
		ComponentsToMerge,
		Quest3World,
		MergeSettings,
		/*InBaseMaterial=*/nullptr,
		/*InOuter=*/nullptr,
		PackageName,
		OutAssets,
		OutMergedLocation,
		/*ScreenSize=*/TNumericLimits<float>::Max(),
		/*bSilent=*/true);

	return OutAssets.Num() > 0;
}


/*
 * Proxy generation. Rebuilds geometry from scratch and always bakes its own
 * atlas, discarding source UVs and vertex colours. Slow. Only appropriate for
 * distant zones, which is why it is gated behind the per-zone bUseProxyMerge.
 */
bool FZeroPayEditorButtonsPluginModule::MergeComponents_Proxy(
	const FReducerMergeJob& Job,
	const FString& PackageName,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	TArray<UObject*>& OutAssets,
	FReducerResults& Results)
{
	const IMeshMergeUtilities& MeshMergeUtilities =
		FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

	/* CreateProxyMesh hands results back through a delegate. ProxyLOD runs
	 * synchronously, but a third-party backend (e.g. Simplygon) may not, and
	 * the original captured a stack local by reference and read it on the very
	 * next line. Verify we are on a synchronous backend before relying on that. */
	IMeshReductionManagerModule& ReductionModule =
		FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");

	IMeshMerging* MergingInterface = ReductionModule.GetMeshMergingInterface();
	if (!MergingInterface)
	{
		Results.Warn(TEXT("No mesh merging (proxy) backend is available. Enable the ProxyLOD plugin or disable bUseProxyMerge on the affected zone."));
		return false;
	}

	TArray<UStaticMeshComponent*> ComponentsToMerge;
	ComponentsToMerge.Reserve(Job.Components.Num());
	for (UStaticMeshComponent* Component : Job.Components)
	{
		if (IsValid(Component) && Component->GetStaticMesh())
		{
			ComponentsToMerge.Add(Component);
		}
	}

	if (ComponentsToMerge.Num() == 0)
	{
		return false;
	}

	FMeshProxySettings ProxySettings;
	ProxySettings.ScreenSize = Job.Zone ? FMath::Max(1, Job.Zone->ProxyScreenSize) : 300;
	ProxySettings.MergeDistance = Job.Zone ? Job.Zone->ProxyMergeDistance : 0.0f;
	ProxySettings.bCreateCollision = false;   /* collision is merged separately, exactly */
	ProxySettings.bRecalculateNormals = true;
	ProxySettings.bAllowDistanceField = false;
	ProxySettings.bGenerateLightmapUVs = Settings->MeshReductionSettings.bPreserveLightmapUVs;
	ProxySettings.bReuseMeshLightmapUVs = false;  /* proxy geometry is new, source UVs do not apply */

	/* Proxy ALWAYS bakes a material, so the atlas budget is not optional here.
	 * This is the same entry point the material reduction stage will use. */
	ConfigureMaterialProxySettings(ProxySettings.MaterialSettings, Job, Settings);

	TArray<UObject*> DelegateAssets;
	bool bDelegateFired = false;

	FCreateProxyDelegate ProxyDelegate;
	ProxyDelegate.BindLambda(
		[&DelegateAssets, &bDelegateFired](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
		{
			DelegateAssets = InAssetsToSync;
			bDelegateFired = true;
		});

	MeshMergeUtilities.CreateProxyMesh(
		ComponentsToMerge,
		ProxySettings,
		/*InOuter=*/nullptr,
		PackageName,
		FGuid::NewGuid(),
		ProxyDelegate,
		/*bAllowAsync=*/false,
		/*ScreenAreaSize=*/1.0f);

	if (!bDelegateFired)
	{
		Results.Warn(FString::Printf(
			TEXT("Proxy generation for %s did not complete synchronously. The active merging backend appears to be asynchronous, which this stage does not support."),
			*PackageName));
		return false;
	}

	OutAssets = MoveTemp(DelegateAssets);
	return OutAssets.Num() > 0;
}


/*
 * ---------------------------------------------------------------------------
 * MATERIAL REDUCTION HOOK
 * ---------------------------------------------------------------------------
 * Every material/atlas decision in the whole reducer flows through this one
 * function. When the material reduction stage is implemented it should extend
 * this and nothing else: add channel selection, per-zone texture budgets,
 * gutter space, sizing type, and whatever texture packing policy is wanted.
 *
 * It is currently only called when baking is actually going to happen -
 * that is, on the proxy path (which always bakes) or when the user has
 * explicitly enabled bBakeMergedMaterials.
 */
void FZeroPayEditorButtonsPluginModule::ConfigureMaterialProxySettings(
	FMaterialProxySettings& OutSettings,
	const FReducerMergeJob& Job,
	const UZeroPayEditor_ReducerSettingsAsset* Settings) const
{
	const FZeroPayEditor_MaterialReductionSettings& MaterialSettings = Settings->MaterialReductionSettings;

	/* Per-zone budget first, falling back to the global atlas resolution. */
	int32 TextureSize = MaterialSettings.AtlasResolution;
	if (Job.Zone && Job.Zone->MergedTextureSize > 0)
	{
		TextureSize = FMath::Min(TextureSize, Job.Zone->MergedTextureSize);
	}

	/* Clamp to a sane mobile range and round to a power of two. */
	TextureSize = FMath::Clamp(FMath::RoundUpToPowerOfTwo(TextureSize), 64, 4096);

	/* Allocate atlas space proportional to each material's world-space area
	 * rather than giving every material an identical tile. Without this a large
	 * wall and a small trim piece get the same texel budget, so the wall blurs. */
	OutSettings.TextureSizingType = ETextureSizingType::TextureSizingType_AutomaticFromTexelDensity;
	OutSettings.TargetTexelDensityPerMeter = 128.0f;   /* raise for sharper, costs memory */
	OutSettings.TextureSize = FIntPoint(TextureSize, TextureSize);   /* still the ceiling */

	/* Target texel density in texels per Unreal unit. This is what actually
	 * drives sharpness; TextureSize becomes an upper bound. */
	OutSettings.MeshMaxScreenSizePercent = 1.0f;
	OutSettings.GutterSpace = 4.0f;

	/* Channels. Quest benefits from writing as few as possible. */
	OutSettings.bNormalMap = true;
	OutSettings.bMetallicMap = false;
	OutSettings.bRoughnessMap = true;
	OutSettings.bSpecularMap = false;
	OutSettings.bEmissiveMap = false;
	OutSettings.bOpacityMap = false;
	OutSettings.bOpacityMaskMap = false;
	OutSettings.bAmbientOcclusionMap = false;

	/* Constant fallbacks for the channels we are not baking. */
	OutSettings.MetallicConstant = 0.0f;
	OutSettings.SpecularConstant = 0.5f;

	OutSettings.GutterSpace = 4.0f;
}


/********************************************************************************************************/
/*                                        POST-MERGE PROCESSING                                         */
/********************************************************************************************************/

void FZeroPayEditorButtonsPluginModule::ApplyTriangleReduction(
	UStaticMesh* Mesh,
	const FReducerMergeJob& Job,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	FReducerResults& Results) const
{
	const FZeroPayEditor_MeshReductionSettings& MeshSettings = Settings->MeshReductionSettings;

	if (!Mesh || !MeshSettings.bReduceTriangleCount)
	{
		return;
	}

	/* Proxy output is already simplified by the voxel remesh; reducing again
	 * just destroys silhouettes. */
	if (Job.Zone && Job.Zone->bUseProxyMerge)
	{
		return;
	}

	const float ReductionPercent = Job.Zone
		? Job.Zone->TriangleReductionPercent
		: MeshSettings.DefaultTriangleReductionPercent;

	if (ReductionPercent <= 0.0f)
	{
		return;
	}

	if (!Mesh->IsSourceModelValid(0))
	{
		return;
	}

	const int32 TriangleCount = Mesh->GetNumTriangles(0);
	if (TriangleCount < MeshSettings.IgnoreMeshesBelowTriangleCount)
	{
		return;
	}

	FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
	SourceModel.ReductionSettings.PercentTriangles =
		FMath::Clamp(1.0f - (ReductionPercent / 100.0f), 0.01f, 1.0f);

	/* NOTE: the engine enum really is spelled "Terimation". */
	SourceModel.ReductionSettings.TerminationCriterion =
		EStaticMeshReductionTerimationCriterion::Triangles;

	Mesh->Build(/*bInSilent=*/true);
	Mesh->PostEditChange();
}


/*
 * Transfers simple collision primitives from the source components into the
 * merged mesh, in merged-mesh space.
 */
void FZeroPayEditorButtonsPluginModule::MergeCollisionFromComponents(
	const TArray<UStaticMeshComponent*>& Components,
	UStaticMesh* OutMergedMesh,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	FReducerResults& Results) const
{
	if (!OutMergedMesh)
	{
		return;
	}

	OutMergedMesh->CreateBodySetup();
	UBodySetup* MergedBodySetup = OutMergedMesh->GetBodySetup();
	if (!MergedBodySetup)
	{
		return;
	}

	/* Complex-as-simple uses the merged render geometry directly, so the simple
	* primitives below would be cooked and then ignored. Skip the whole
	* transfer. */
	if (Settings->PhysicsReductionSettings.bUseComplexAsSimpleCollision)
	{
		MergedBodySetup->Modify();
		MergedBodySetup->AggGeom = FKAggregateGeom();
		MergedBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		MergedBodySetup->InvalidatePhysicsData();
		MergedBodySetup->CreatePhysicsMeshes();
		return;
	}

	MergedBodySetup->Modify();
	MergedBodySetup->AggGeom = FKAggregateGeom();

	bool bAnyComplex = false;
	bool bWarnedNonUniform = false;
	int32 PrimitiveCount = 0;

	const int32 MaxPrimitives = FMath::Max(0, Settings->PhysicsReductionSettings.MaxCollisionPrimitiveCount);

	for (UStaticMeshComponent* Comp : Components)
	{
		if (!IsValid(Comp) || !Comp->GetStaticMesh())
		{
			continue;
		}

		/* Honour the COMPONENT's collision state. The original read only the
		 * mesh's body setup, so a component with collision disabled still
		 * contributed primitives to the merged result. */
		if (Comp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		UBodySetup* SourceBodySetup = Comp->GetStaticMesh()->GetBodySetup();
		if (!SourceBodySetup)
		{
			continue;
		}

		const FKAggregateGeom& AggGeom = SourceBodySetup->AggGeom;
		const FTransform CompToMerged = Comp->GetComponentTransform();
		const FVector Scale = CompToMerged.GetScale3D();

		const bool bUniformScale =
			FMath::IsNearlyEqual(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), 0.01)
			&& FMath::IsNearlyEqual(FMath::Abs(Scale.Y), FMath::Abs(Scale.Z), 0.01);

		if (!bUniformScale && !bWarnedNonUniform
			&& (AggGeom.SphereElems.Num() > 0 || AggGeom.SphylElems.Num() > 0))
		{
			bWarnedNonUniform = true;
			Results.Warn(FString::Printf(
				TEXT("'%s' has non-uniform scale %s and sphere/capsule collision. Radii are approximated using the largest axis, so merged collision will not match the source exactly."),
				*Comp->GetName(), *Scale.ToCompactString()));
		}

		/* Negative scale on any odd number of axes mirrors the geometry. */
		const bool bMirrored = (Scale.X * Scale.Y * Scale.Z) < 0.0;
		const float UniformScale = static_cast<float>(Scale.GetAbs().GetMax());

		/* Spheres */
		for (const FKSphereElem& Sphere : AggGeom.SphereElems)
		{
			FKSphereElem NewSphere = Sphere;
			NewSphere.Center = CompToMerged.TransformPosition(Sphere.Center);
			NewSphere.Radius *= UniformScale;
			MergedBodySetup->AggGeom.SphereElems.Add(NewSphere);
			++PrimitiveCount;
		}

		/* Capsules */
		for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
		{
			FKSphylElem NewSphyl = Sphyl;
			NewSphyl.Center = CompToMerged.TransformPosition(Sphyl.Center);
			NewSphyl.Rotation = (CompToMerged.GetRotation() * Sphyl.Rotation.Quaternion()).Rotator();
			NewSphyl.Radius *= UniformScale;
			NewSphyl.Length *= UniformScale;
			MergedBodySetup->AggGeom.SphylElems.Add(NewSphyl);
			++PrimitiveCount;
		}

		/* Convex hulls */
		for (const FKConvexElem& Convex : AggGeom.ConvexElems)
		{
			FKConvexElem NewConvex;
			NewConvex.VertexData.Reserve(Convex.VertexData.Num());

			for (const FVector& Vertex : Convex.VertexData)
			{
				NewConvex.VertexData.Add(CompToMerged.TransformPosition(Vertex));
			}

			/* A mirrored transform inverts hull winding, which produces convex
			 * bodies with inward-facing normals that objects fall through. */
			if (bMirrored)
			{
				Algo::Reverse(NewConvex.VertexData);
			}

			NewConvex.UpdateElemBox();
			MergedBodySetup->AggGeom.ConvexElems.Add(NewConvex);
			++PrimitiveCount;
		}

		/* Boxes */
		for (const FKBoxElem& Box : AggGeom.BoxElems)
		{
			FKBoxElem NewBox = Box;
			NewBox.Center = CompToMerged.TransformPosition(Box.Center);
			NewBox.Rotation = (CompToMerged.GetRotation() * Box.Rotation.Quaternion()).Rotator();
			NewBox.X *= static_cast<float>(FMath::Abs(Scale.X));
			NewBox.Y *= static_cast<float>(FMath::Abs(Scale.Y));
			NewBox.Z *= static_cast<float>(FMath::Abs(Scale.Z));
			MergedBodySetup->AggGeom.BoxElems.Add(NewBox);
			++PrimitiveCount;
		}

		if (SourceBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
		{
			bAnyComplex = true;
		}
	}

	if (MaxPrimitives > 0 && PrimitiveCount > MaxPrimitives)
	{
		Results.Warn(FString::Printf(
			TEXT("Merged mesh '%s' has %d collision primitives (budget is %d)."),
			*OutMergedMesh->GetName(), PrimitiveCount, MaxPrimitives));
	}

	/* Never propagate complex-as-simple. Per-triangle traces against a large
	 * merged proxy are exactly the cost this tool exists to remove. The
	 * original promoted the whole merged mesh to complex if ANY source used it. */
	MergedBodySetup->CollisionTraceFlag = Settings->PhysicsReductionSettings.bUseSimpleCollisionWherePossible
		? CTF_UseSimpleAsComplex
		: CTF_UseDefault;

	if (bAnyComplex)
	{
		Results.Warn(FString::Printf(
			TEXT("Merged mesh '%s' had source meshes using complex-as-simple collision. Simple collision has been used instead; verify gameplay collision in this area."),
			*OutMergedMesh->GetName()));
	}

	MergedBodySetup->InvalidatePhysicsData();
	MergedBodySetup->CreatePhysicsMeshes();
}


AStaticMeshActor* FZeroPayEditorButtonsPluginModule::PlaceMergedMeshInQuest3Level(
	UStaticMesh* MergedMesh,
	const FVector& SpawnLocation,
	const FReducerMergeJob& Job,
	ULevel* Level,
	const UZeroPayEditor_ReducerSettingsAsset* Settings,
	FReducerResults& Results) const
{
	if (!MergedMesh || !Level)
	{
		return nullptr;
	}

	UWorld* World = Level->OwningWorld;
	if (!World)
	{
		return nullptr;
	}

	Level->Modify();

	FActorSpawnParameters Params;
	Params.OverrideLevel = Level;
	Params.ObjectFlags = RF_Transactional;

	AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(SpawnLocation, FRotator::ZeroRotator, Params);
	if (!MergedActor)
	{
		Results.Warn(FString::Printf(TEXT("Failed to spawn an actor for '%s'."), *MergedMesh->GetName()));
		return nullptr;
	}

	MergedActor->Modify();

	UStaticMeshComponent* MeshComponent = MergedActor->GetStaticMeshComponent();

	/* Mobility must be set before the mesh is assigned or the component will
	 * complain about a movable component owning static lighting. */
	MeshComponent->SetMobility(EComponentMobility::Static);
	MeshComponent->SetStaticMesh(MergedMesh);

	MergedActor->SetActorLabel(MergedMesh->GetName());
	MergedActor->SetFolderPath(FName(ZeroPayReducer::GeneratedFolder));
	MergedActor->Tags.AddUnique(ZeroPayReducer::GeneratedActorTag);

	/* Free wins available at exactly this point. */
	const FZeroPayEditor_VisibilityReductionSettings& Visibility = Settings->VisibilityReductionSettings;

	if (Visibility.bEnableDistanceCulling)
	{
		const float Radius = static_cast<float>(MeshComponent->Bounds.SphereRadius);

		float CullDistance;
		if (Radius >= Visibility.NeverCullObjectSize)
		{
			CullDistance = 0.0f;   /* 0 = never cull */
		}
		else if (Radius >= Visibility.TinyObjectBoundsThreshold * 10.0f)
		{
			CullDistance = Visibility.DefaultLargeObjectCullDistance;
		}
		else if (Radius >= Visibility.TinyObjectBoundsThreshold)
		{
			CullDistance = Visibility.DefaultMediumObjectCullDistance;
		}
		else
		{
			CullDistance = Visibility.DefaultSmallObjectCullDistance;
		}

		if (CullDistance > 0.0f && Job.Zone && Visibility.bUsePlayerZonesForCullDistance)
		{
			CullDistance *= Job.Zone->DistanceMultiplier;
		}

		MeshComponent->SetCullDistance(CullDistance);
	}

	/* Shadow cost on Quest is disproportionate; merged static geometry should
	 * rely on baked lighting. */
	if (Settings->LightingReductionSettings.bDisableDynamicShadowsOnStaticGeometry)
	{
		MeshComponent->bCastDynamicShadow = false;
	}

	/* Merged decor never needs to tick. */
	MergedActor->PrimaryActorTick.bCanEverTick = false;
	MeshComponent->PrimaryComponentTick.bCanEverTick = false;

	World->UpdateCullDistanceVolumes(MergedActor, MeshComponent);

	/* Statistics */
	Results.ReducedActorCount++;
	Results.ReducedMaterialSlotCount += MeshComponent->GetNumMaterials();

	if (const FStaticMeshRenderData* RenderData = MergedMesh->GetRenderData())
	{
		if (RenderData->LODResources.Num() > 0)
		{
			const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
			Results.ReducedTriangleCount += LOD0.GetNumTriangles();
			Results.ReducedVertexCount += LOD0.GetNumVertices();
		}
	}

	return MergedActor;
}


/********************************************************************************************************/
/*                                          HOUSEKEEPING                                                */
/********************************************************************************************************/

FFoundAssetInformation FZeroPayEditorButtonsPluginModule::ScanLevelActorsAndDirectory(
	ULevel* LevelToScan,
	const FString& TargetAssetPath) const
{
	FFoundAssetInformation Result;

	if (!LevelToScan)
	{
		UE_LOG(LogZeroPayReducer, Warning, TEXT("ScanLevelActorsAndDirectory: invalid level."));
		return Result;
	}

	for (AActor* Actor : LevelToScan->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		/* Match either the folder OR the tag, so an actor a user dragged out of
		 * the folder is still recognised as generated content. */
		if (Actor->GetFolderPath().ToString().StartsWith(ZeroPayReducer::GeneratedFolder)
			|| Actor->Tags.Contains(ZeroPayReducer::GeneratedActorTag))
		{
			Result.FoundInstances++;
		}
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetList;
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*TargetAssetPath));
	Filter.bRecursivePaths = true;

	AssetRegistryModule.Get().GetAssets(Filter, AssetList);
	Result.FoundAssets = AssetList.Num();

	return Result;
}


bool FZeroPayEditorButtonsPluginModule::DeleteActorsAndAssets(
	ULevel* TargetLevel,
	const FString& AssetFolderPathToDelete)
{
	if (!TargetLevel)
	{
		UE_LOG(LogZeroPayReducer, Warning, TEXT("DeleteActorsAndAssets: invalid level."));
		return false;
	}

	UWorld* World = TargetLevel->GetWorld();
	if (!World)
	{
		UE_LOG(LogZeroPayReducer, Warning, TEXT("DeleteActorsAndAssets: level has no world."));
		return false;
	}

	/* 1. Destroy previously generated actors */

	TArray<AActor*> ActorsToDestroy;
	for (AActor* Actor : TargetLevel->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		if (Actor->GetFolderPath().ToString().StartsWith(ZeroPayReducer::GeneratedFolder)
			|| Actor->Tags.Contains(ZeroPayReducer::GeneratedActorTag))
		{
			ActorsToDestroy.Add(Actor);
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		World->EditorDestroyActor(Actor, /*bShouldModifyLevel=*/true);
	}

	UE_LOG(LogZeroPayReducer, Display, TEXT("Destroyed %d generated actor(s)."), ActorsToDestroy.Num());

	/* 2. Save the level so the asset deletions below cannot find live
	 *    references from actors we have just removed. */

	UPackage* LevelPackage = TargetLevel->GetOutermost();
	if (!LevelPackage)
	{
		UE_LOG(LogZeroPayReducer, Warning, TEXT("DeleteActorsAndAssets: could not resolve the level package."));
		return false;
	}

	if (LevelPackage->IsDirty())
	{
		const bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave(
			{ LevelPackage }, /*bCheckDirty=*/true, /*bPromptToSave=*/false)
			== FEditorFileUtils::EPromptReturnCode::PR_Success;

		if (!bSaved)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				TEXT("Failed to save the Quest 3 level after removing the previously generated actors. Cannot continue.")));
			return false;
		}
	}

	/* 3. Delete the previously generated assets */

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetsToDelete;
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*AssetFolderPathToDelete));
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = false;

	AssetRegistryModule.Get().GetAssets(Filter, AssetsToDelete);

	if (AssetsToDelete.Num() == 0)
	{
		return true;
	}

	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Reserve(AssetsToDelete.Num());
	for (const FAssetData& Asset : AssetsToDelete)
	{
		if (UObject* LoadedAsset = Asset.GetAsset())
		{
			ObjectsToDelete.Add(LoadedAsset);
		}
	}

	if (ObjectsToDelete.Num() > 0)
	{
		/* ForceDeleteObjects fixes up referencers. DeleteObjectsUnchecked, which
		 * the original used, skips reference handling entirely and leaves null
		 * references behind in any other level that happened to use these
		 * assets. */
		const int32 NumDeleted = ObjectTools::ForceDeleteObjects(ObjectsToDelete, /*ShowConfirmation=*/false);

		UE_LOG(LogZeroPayReducer, Display, TEXT("Deleted %d of %d asset(s) from '%s'."),
			NumDeleted, ObjectsToDelete.Num(), *AssetFolderPathToDelete);

		if (NumDeleted < ObjectsToDelete.Num())
		{
			UE_LOG(LogZeroPayReducer, Warning,
				TEXT("Some generated assets could not be deleted, most likely because another level still references them."));
		}
	}

	return true;
}


float FZeroPayEditorButtonsPluginModule::BoxSurfaceDistance(const FBox& A, const FBox& B)
{
	if (A.Intersect(B))
	{
		return 0.0f;
	}

	FVector Gap(0.0);

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (A.Max[Axis] < B.Min[Axis])
		{
			Gap[Axis] = B.Min[Axis] - A.Max[Axis];
		}
		else if (B.Max[Axis] < A.Min[Axis])
		{
			Gap[Axis] = A.Min[Axis] - B.Max[Axis];
		}
	}

	return static_cast<float>(Gap.Size());
}


void FZeroPayEditorButtonsPluginModule::UpdateQuest3ReducerUIProgressField()
{
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			if (!IsValid(WidgetQuest3ReducerInstance))
			{
				return;
			}

			UFunction* Func = WidgetQuest3ReducerInstance->FindFunction(TEXT("UpdateUIProgressField"));
			if (!Func)
			{
				UE_LOG(LogZeroPayReducer, Error, TEXT("UpdateUIProgressField not found on %s"),
					*WidgetQuest3ReducerInstance->GetName());
				return;
			}

			struct FUpdateParams
			{
				FString InputString;
			};

			FUpdateParams Params;
			Params.InputString = LastMessage;

			WidgetQuest3ReducerInstance->ProcessEvent(Func, &Params);
		});
}

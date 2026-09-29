// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * Replacement for FZeroPayEditorButtonsPluginModule::ReducePCVRLevelForQuest3.
 *
 * DELETE the old ReducePCVRLevelForQuest3 definition from
 * ZeroPayEditorButtonsPlugin.cpp - this file provides it. Everything else in
 * that file (ReduceLevel, ShouldConsiderComponent, PlaceMergedMeshInQuest3Level,
 * ScanLevelActorsAndDirectory, DeleteActorsAndAssets, UpdateQuest3ReducerUIProgressField)
 * is still used and must stay. The legacy merge functions (PartitionComponentsIntoChunks,
 * BuildMergeJobs, RunMergeJobs, ...) are no longer called and can be deleted
 * later together with their header declarations.
 */

#include "ZeroPayEditorButtonsPlugin.h"

#include "ZeroPayEditor_AtlasReducer.h"
#include "ZeroPayEditor_ReducerSettingsAsset.h"
#include "ZeroPayEditor_Reducer_Zone.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "StaticMeshResources.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogZeroPayQuest3Reduce, Log, All);


namespace
{
	void AccumulateSourceStatistics(const UStaticMeshComponent* Component, FReducerResults& Results)
	{
		/* One component per AStaticMeshActor, so actors == components here. */
		Results.OriginalActorCount++;
		Results.OriginalComponentCount++;
		Results.OriginalMaterialSlotCount += Component->GetNumMaterials();

		const UStaticMesh* Mesh = Component->GetStaticMesh();
		const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
		if (RenderData && RenderData->LODResources.Num() > 0)
		{
			const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
			Results.OriginalTriangleCount += LOD0.GetNumTriangles();
			Results.OriginalVertexCount += LOD0.GetNumVertices();
		}
	}

	FColor ColourForAtlas(int32 AtlasIndex)
	{
		return FLinearColor::MakeFromHSV8(static_cast<uint8>((AtlasIndex * 47) % 256), 200, 255).ToFColor(true);
	}
}


FReducerResults FZeroPayEditorButtonsPluginModule::ReducePCVRLevelForQuest3(
	UZeroPayMod_DefinitionDataAsset* DataAsset,
	UZeroPayEditor_ReducerSettingsAsset* ReducerSettings,
	FReducerRuntimeSettings RuntimeSettings)
{
	FReducerResults Results;
	const double StartTime = FPlatformTime::Seconds();

	/* Every early exit goes through here so the UI always gets a message. */
	auto Abort = [this, &Results, &RuntimeSettings](const FString& Reason, bool bShowDialog) -> FReducerResults
	{
		if (!Reason.IsEmpty())
		{
			Results.Fail(Reason);
		}

		if (bShowDialog && !RuntimeSettings.bSuppressPrompts && !Results.FailureReason.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Results.FailureReason));
		}

		LastMessage = Results.bCancelled ? FString(TEXT("Cancelled by user.")) : Results.FailureReason;
		UE_LOG(LogZeroPayQuest3Reduce, Warning, TEXT("%s"), *LastMessage);
		UpdateQuest3ReducerUIProgressField();
		return Results;
	};

	/* ---------------------------------------------------------------- */
	/* Validate inputs (unchanged from the legacy path)                  */
	/* ---------------------------------------------------------------- */

	if (!DataAsset)
	{
		return Abort(TEXT("Error S1 - No definition data asset supplied."), false);
	}

	if (!ReducerSettings)
	{
		return Abort(TEXT("Error S1 - No reducer settings asset supplied."), false);
	}

	if (DataAsset->Definition.pcvrlevel.IsNull())
	{
		return Abort(TEXT("Error S1 - PCVR Level is not defined (in data asset in UGC folder)"), false);
	}

	if (DataAsset->Definition.quest3level.IsNull())
	{
		return Abort(TEXT("Error S1 - Quest3 Level is not defined (in data asset in UGC folder)"), false);
	}

	UWorld* PCVRWorld = DataAsset->Definition.pcvrlevel.LoadSynchronous();
	if (!PCVRWorld || !PCVRWorld->PersistentLevel)
	{
		return Abort(TEXT("Error S1 - PCVR level could not be loaded."), false);
	}

	UWorld* Quest3World = DataAsset->Definition.quest3level.LoadSynchronous();
	if (!Quest3World || !Quest3World->PersistentLevel)
	{
		return Abort(TEXT("Error S2 - Quest3 level could not be loaded."), false);
	}

	ULevel* PCVRLevel = PCVRWorld->PersistentLevel;
	ULevel* Quest3Level = Quest3World->PersistentLevel;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld || Quest3Level->OwningWorld != EditorWorld)
	{
		return Abort(TEXT("Error S2 - The Quest 3 level is not loaded as a sub-level of the currently open persistent level. Open the persistent level and make the Quest 3 sub-level loaded and visible before running the reducer."), false);
	}

	const bool bDryRun = ReducerSettings->GeneralSettings.bDryRun;
	Results.bDryRun = bDryRun;

	/* Everything under ReducedAssets is regenerated on every run. The master
	 * material lives beside it so its shaders are not recompiled each time. */
	const FString UGCLevelsPath = FString::Printf(TEXT("/Game/ZeroPayMods/UGC%s/Levels"), *DataAsset->Definition.UGCID);
	const FString ReducedAssetRoot = UGCLevelsPath / TEXT("ReducedAssets");
	const FString MasterMaterialPackage = UGCLevelsPath / TEXT("ReducerMaster/M_Quest3AtlasMaster");

	/* ---------------------------------------------------------------- */
	/* Confirm destruction of the previous run                           */
	/* ---------------------------------------------------------------- */

	const FFoundAssetInformation Existing = ScanLevelActorsAndDirectory(Quest3Level, ReducedAssetRoot);
	if (!bDryRun && (Existing.FoundAssets > 0 || Existing.FoundInstances > 0) && !RuntimeSettings.bSuppressPrompts)
	{
		const FString DialogMessage = FString::Printf(
			TEXT("Warning!\n\nThere are %d assets under '%s' (chunk meshes, atlases, material instances) that will be destroyed and recreated.\nThere are %d generated actors in the Quest 3 level that will be destroyed and recreated.\n\nAre you sure? You cannot undo these changes later."),
			Existing.FoundAssets, *ReducedAssetRoot, Existing.FoundInstances);

		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(DialogMessage)) != EAppReturnType::Yes)
		{
			return Abort(TEXT("User aborted."), false);
		}
	}

	/* ---------------------------------------------------------------- */
	/* Progress - one dialog; the reducer nests its own tasks into it    */
	/* ---------------------------------------------------------------- */

	enum { Work_Cleanup = 1, Work_Gather = 1, Work_Reduce = 16, Work_Place = 1, Work_Save = 1 };
	constexpr float TotalWork = Work_Cleanup + Work_Gather + Work_Reduce + Work_Place + Work_Save;

	FScopedSlowTask SlowTask(TotalWork, FText::FromString(TEXT("Reducing PCVR level for Quest 3 (atlas)...")));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	/* ---------------------------------------------------------------- */
	/* Delete the previous run                                           */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Cleanup, FText::FromString(TEXT("Removing previously generated content...")));

	if (!bDryRun)
	{
		if (!DeleteActorsAndAssets(Quest3Level, ReducedAssetRoot))
		{
			return Abort(FString::Printf(TEXT("Failed to delete the existing generated actors and/or the assets under '%s'."), *ReducedAssetRoot), true);
		}

		/* Deleted assets must be fully gone before packages with the same
		 * names are created again. */
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	/* ---------------------------------------------------------------- */
	/* Gather                                                            */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Gather, FText::FromString(TEXT("Gathering static meshes...")));

	const FZeroPayEditor_AtlasReductionSettings& AtlasSettings = ReducerSettings->AtlasReductionSettings;

	ZeroPayAtlasReducer::FInput Input;
	Input.Settings = ReducerSettings;
	Input.bDryRun = bDryRun;
	Input.AssetRootPath = ReducedAssetRoot;
	Input.MasterMaterialPackage = MasterMaterialPackage;

	/* An explicit reducer zone limits the reduction to its box. */
	for (AActor* Actor : PCVRLevel->Actors)
	{
		if (const AZeroPayEditor_Reducer_Zone* Zone = Cast<AZeroPayEditor_Reducer_Zone>(Actor))
		{
			const FBox ZoneBox = Zone->GetWorldBoundingBox();
			if (ZoneBox.IsValid)
			{
				Input.ClipBounds = ZoneBox;
				break;
			}
		}
	}

	int32 SkippedByTag = 0;

	for (AActor* Actor : PCVRLevel->Actors)
	{
		/* Plain static mesh actors only. Blueprints, foliage (instanced) and
		 * everything else are left alone. */
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!IsValid(MeshActor))
		{
			continue;
		}

		if (!AtlasSettings.SkipActorTag.IsNone() && MeshActor->ActorHasTag(AtlasSettings.SkipActorTag))
		{
			++SkippedByTag;
			continue;
		}

		UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();

		/* Shared predicate: visibility, editor-only, mobility, reducer helpers. */
		if (!Component || !ShouldConsiderComponent(Component, ReducerSettings))
		{
			continue;
		}

		if (Input.ClipBounds.IsValid && !Input.ClipBounds.Intersect(Component->Bounds.GetBox()))
		{
			continue;
		}

		Input.Components.Add(Component);
		AccumulateSourceStatistics(Component, Results);
	}

	UE_LOG(LogZeroPayQuest3Reduce, Log, TEXT("Gathered %d static mesh components (%d actors skipped by tag '%s')."),
		Input.Components.Num(), SkippedByTag, *AtlasSettings.SkipActorTag.ToString());

	if (Input.Components.Num() == 0)
	{
		return Abort(TEXT("No mergeable static mesh actors were found in the PCVR level. Check that the sub-level is loaded and visible, that the actors are Static, and that they are not tagged with the skip tag."), true);
	}

	if (Input.ClipBounds.IsValid)
	{
		ShowNotification(
			TEXT("The reduction has been limited to the bounds of the AZeroPayEditor_Reducer_Zone actor found in the PCVR level. Triangles outside this box are ignored."),
			SNotificationItem::ECompletionState::CS_Success);
	}

	/* ---------------------------------------------------------------- */
	/* Reduce                                                            */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Reduce, FText::FromString(TEXT("Building chunks and texture atlases...")));

	ZeroPayAtlasReducer::FOutput Output;
	const bool bReduced = ZeroPayAtlasReducer::Run(Input, Results, Output);

	Results.ChunkCount = Output.Chunks.Num();
	Results.MergeJobCount = Output.Chunks.Num();

	if (RuntimeSettings.bStage1_ShowVisualDebug && EditorWorld)
	{
		FlushPersistentDebugLines(EditorWorld);

		for (const ZeroPayAtlasReducer::FChunkOutput& Chunk : Output.Chunks)
		{
			/* Colour = primary atlas, so atlas sharing is visible at a glance.
			 * Thicker boxes reference more than one atlas. */
			const int32 PrimaryAtlas = Chunk.AtlasIndices.Num() > 0 ? Chunk.AtlasIndices[0] : 0;
			DrawDebugBox(EditorWorld, Chunk.Bounds.GetCenter(), Chunk.Bounds.GetExtent(),
				ColourForAtlas(PrimaryAtlas), false, RuntimeSettings.fStage1_VisualDebugDuration, 0,
				Chunk.AtlasIndices.Num() > 1 ? 8.0f : 2.0f);
		}
	}

	if (!bReduced)
	{
		/* Anything created before the failure stays dirty and unsaved; the
		 * next run deletes it. */
		return Abort(FString(), !Results.bCancelled);
	}

	/* ---------------------------------------------------------------- */
	/* Place                                                             */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Place, FText::FromString(TEXT("Placing chunk actors...")));

	if (!bDryRun)
	{
		/* PlaceMergedMeshInQuest3Level handles folder, tag, mobility, culling,
		 * shadows and reduced statistics. No player zone applies here. */
		FReducerMergeJob PlacementJob;
		PlacementJob.ZoneIndex = 0;
		PlacementJob.Zone = nullptr;

		for (const ZeroPayAtlasReducer::FChunkOutput& Chunk : Output.Chunks)
		{
			if (!Chunk.Mesh)
			{
				Results.FailedMergeJobCount++;
				continue;
			}

			PlacementJob.Bounds = Chunk.Bounds;

			if (!PlaceMergedMeshInQuest3Level(Chunk.Mesh, Chunk.Pivot, PlacementJob, Quest3Level, ReducerSettings, Results))
			{
				Results.FailedMergeJobCount++;
			}
		}
	}

	/* ---------------------------------------------------------------- */
	/* Save                                                              */
	/* ---------------------------------------------------------------- */

	SlowTask.EnterProgressFrame(Work_Save, FText::FromString(TEXT("Saving...")));

	if (!bDryRun)
	{
		if (Output.Packages.Num() > 0)
		{
			const bool bAssetsSaved = FEditorFileUtils::PromptForCheckoutAndSave(
				Output.Packages, /*bCheckDirty=*/false, /*bPromptToSave=*/false)
				== FEditorFileUtils::EPromptReturnCode::PR_Success;

			if (!bAssetsSaved)
			{
				Results.Warn(TEXT("Some generated assets could not be saved. Use File > Save All before closing the editor."));
			}
		}

		UPackage* LevelPackage = Quest3Level->GetOutermost();
		const bool bLevelSaved = FEditorFileUtils::PromptForCheckoutAndSave(
			{ LevelPackage }, /*bCheckDirty=*/true, /*bPromptToSave=*/false)
			== FEditorFileUtils::EPromptReturnCode::PR_Success;

		if (!bLevelSaved)
		{
			return Abort(TEXT("Failed to save the updated Quest 3 sub-level. Please save it manually."), true);
		}
	}

	/* ---------------------------------------------------------------- */
	/* Report                                                            */
	/* ---------------------------------------------------------------- */

	Results.bFailed = false;
	Results.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - StartTime);

	LastMessage = FString::Printf(
		TEXT("%s complete in %.1fs. Actors %d -> %d, triangles %lld -> %lld, material slots %d -> %d. %d chunks, %d atlases (%d tiles, %d downscaled).%s%s"),
		bDryRun ? TEXT("Dry run") : TEXT("Reduction"),
		Results.ElapsedSeconds,
		Results.OriginalActorCount, Results.ReducedActorCount,
		static_cast<int64>(Results.OriginalTriangleCount), static_cast<int64>(Results.ReducedTriangleCount),
		Results.OriginalMaterialSlotCount, Results.ReducedMaterialSlotCount,
		Results.ChunkCount, Results.AtlasCount, Results.AtlasTileCount, Results.DownscaledTileCount,
		Results.UnsupportedMaterialCount > 0
			? *FString::Printf(TEXT(" %d material(s) only partly supported - see Output Log."), Results.UnsupportedMaterialCount)
			: TEXT(""),
		Results.FailedMergeJobCount > 0
			? *FString::Printf(TEXT(" %d chunk(s) failed - see Output Log."), Results.FailedMergeJobCount)
			: TEXT(""));

	UE_LOG(LogZeroPayQuest3Reduce, Log, TEXT("%s"), *LastMessage);
	UpdateQuest3ReducerUIProgressField();

	return Results;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"
#include "MeshUtilities.h"
#include "IMeshMergeUtilities.h"
#include "IMeshReductionManagerModule.h"
#include "MeshUtilitiesCommon.h"
#include "AssetRegistry/AssetRegistryModule.h"  
#include "Modules/ModuleManager.h"
#include "OverlappingCorners.h"
#include "Engine/MeshMerging.h"
#include "MeshDescription.h"    
#include "MeshMergeModule.h"
#include "MeshMerge/MeshMergingSettings.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "PackageTools.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshDescription.h"
#include "StaticMeshCompiler.h"

UZeroPayEditorReduceOperationHandle* FZeroPayEditorButtonsPluginModule::ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	/* Find the settings file and validate it's correct */
	bAbortOperation = false;
	ReduceHandle = NewObject<UZeroPayEditorReduceOperationHandle>();

	Async(EAsyncExecution::Thread, [this, dataAsset, reducerSettings, runtimeSettings]()
		{
			/* >>> Stage 1 - Uniform Mesh Merging <<< */
			if (!Stage1MergeUniformMeshes(dataAsset, reducerSettings, runtimeSettings))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						ReduceHandle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
#if 0
			/* >>> Pack Quest 3 <<< */
			if (!CookAndPackAndroid(dataAsset))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						Handle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
			/* >>> Pack Linux Server <<< */
			if (!CookAndPackLinuxServer(dataAsset))
				bAbortOperation = true;

#endif
			/* All Good! */
			if (!bAbortOperation)
				LastMessage = "Reduction operation completed successfully!";

			UpdateQuest3ReducerUIProgressField();
			FPlatformProcess::Sleep(1.0f);

			// Simulate some logic, then notify later
			AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
				{
					ReduceHandle->OnCompleted.Broadcast(!bAbortOperation, dataAsset->Definition.UGCID);
				});
		});

	return ReduceHandle;
}

/********************************************************************************************************/
/*                                   STAGE 1 - MERGE UNIFORMED MESHES					                */
/********************************************************************************************************/


bool FZeroPayEditorButtonsPluginModule::Stage1MergeUniformMeshes(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	bool bSuccess = false;

	/* Handle bad dataAsset */
	if (dataAsset->Definition.pcvrlevel == nullptr)
	{
		LastMessage = "Error S1 - PCVR Level is not defined (in data asset in UGC folder)";
		UpdateQuest3ReducerUIProgressField();
		return false ;
	}

	/* Info */
	LastMessage = "Stage 1 - Recording all statis meshes in PCVR level";
	UpdateQuest3ReducerUIProgressField();

	/* >>> Get all static mesh actors from PCVR level */
	TMap<AActor*, TArray<UStaticMeshComponent*>> MeshContainingActors = GetStaticMeshesInSubLevel(dataAsset->Definition.pcvrlevel.GetAssetName());

	/* Handle no meshes found :/ */
	if (MeshContainingActors.Num() == 0)
	{
		LastMessage = FString::Printf(TEXT("Error S1 - No mesh components found in any actors in '%s'"), *dataAsset->Definition.pcvrlevel.GetAssetName());
		UpdateQuest3ReducerUIProgressField();
		return false;
	}

	/* Info */
	LastMessage = "Stage 1 - Grouping them by mesh asset";
	UpdateQuest3ReducerUIProgressField();

	/* >>> Get all mesh grouping (i.e. which mesh assets copies in which in-level mesh components) */
	auto MeshGroups = GroupMeshComponentsByMesh(MeshContainingActors);

	/* Handle no meshes found :/ */
	if (MeshGroups.Num() == 0)
	{
		LastMessage = FString::Printf(TEXT("Error S1 - Found no mesh groups, but found %d mesh actors"), MeshContainingActors.Num());
		UpdateQuest3ReducerUIProgressField();
		return false;
	}

	/* Info */
	LastMessage = FString::Printf(TEXT("Stage 1 - Grouping by distance (%d units) and material similarity"), reducerSettings->MeshReductionSettings.Stage1_MaxDistance);
	UpdateQuest3ReducerUIProgressField();

	/* >>> Group them by proximity to each other... */
	auto ClusteredGroups = GroupByMeshThenProximity_MaterialAware(MeshGroups, reducerSettings->MeshReductionSettings.Stage1_MaxDistance);

	/* Output information from user */
	if (runtimeSettings.bStage1_ShowOutputLogDebug)
	{
		for (const auto& Pair : ClusteredGroups)
		{
			const FMeshMaterialKey& Key = Pair.Key;
			const auto& ClusterArray = Pair.Value;

			UE_LOG(LogTemp, Log, TEXT("Mesh %s with %d material slots has %d clusters"), *Key.Mesh->GetName(), Key.Materials.Num(), ClusterArray.Num());

			for (int32 i = 0; i < ClusterArray.Num(); ++i)
			{
				const auto& Cluster = ClusterArray[i];
				UE_LOG(LogTemp, Log, TEXT("  Cluster %d: %d components"), i, Cluster.Num());
			}
		}
	}
	
	/* Debug? */
	if (runtimeSettings.bStage1_ShowMergeGroups)
		DrawClusterDebugBoxes(ClusteredGroups, GEditor->GetEditorWorldContext().World(), 20.0f);

	/* Info */
	LastMessage = FString::Printf(TEXT("Stage 1 - Merging (%d Clusters)"), ClusteredGroups.Num() );
	UpdateQuest3ReducerUIProgressField();

	/* >>> Merge */
	FString ReducedAssetMeshPath = FString::Printf(TEXT("/Game/ZeroPayMods/UGC%s/Levels/ReducedAssets/Meshes"), *dataAsset->Definition.UGCID);

	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true); // auto-reset

	AsyncTask(ENamedThreads::GameThread, [this, DoneEvent, &bSuccess, ClusteredGroups, ReducedAssetMeshPath]()
		{	
			bSuccess = MergeMeshIslands(ClusteredGroups, 0.5f, *ReducedAssetMeshPath);
			DoneEvent->Trigger();
		});

	DoneEvent->Wait(); // blocks here until Trigger() is called

	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	LastMessage = FString::Printf(TEXT("Stage 1 - Merging (%d Clusters)"), ClusteredGroups.Num());
	UpdateQuest3ReducerUIProgressField();



	return true;
}


/********************************************************************************************************/
/*                                   MESH, WORLD, ETC. SUPPORT FUNCTIONS                                */
/********************************************************************************************************/

/***************************************************************************************************************
*
* GetStaticMeshesInSubLevel - Get's a list of all static mesh components (and they're parent actors).
*
*/

TMap<AActor*, TArray<UStaticMeshComponent*>> FZeroPayEditorButtonsPluginModule::GetStaticMeshesInSubLevel(const FString& SubLevelName)
{
	TMap<AActor*, TArray<UStaticMeshComponent*>> ActorMeshMap;

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FZeroPayEditorButtonsPluginModule::GetStaticMeshesInSubLevel] Failed, Editor world not found."));
		return ActorMeshMap;
	}

	for (ULevel* Level : EditorWorld->GetLevels())
	{
		if (!Level || !Level->GetOutermost()->GetName().Contains(SubLevelName))
			continue;

		for (AActor* Actor : Level->Actors)
		{
			if (!Actor || !Actor->Tags.Contains(FName("ReduceMe")))
				continue;

			TArray<UStaticMeshComponent*> FoundMeshes;
			Actor->GetComponents<UStaticMeshComponent>(FoundMeshes);

			if (FoundMeshes.Num() > 0)
			{
				ActorMeshMap.Add(Actor, FoundMeshes);
			}
		}
	}

	return ActorMeshMap;
}

/***************************************************************************************************************
*
* GroupMeshComponentsByMesh - Returns a mapping of all mesh components against the StaticMesh asset
*
*/

TMap<UStaticMesh*, TArray<UStaticMeshComponent*>> FZeroPayEditorButtonsPluginModule::GroupMeshComponentsByMesh(const TMap<AActor*, TArray<UStaticMeshComponent*>>& ActorMeshMap)
{
	TMap<UStaticMesh*, TArray<UStaticMeshComponent*>> MeshGroups;

	for (const auto& ActorPair : ActorMeshMap)
	{
		for (UStaticMeshComponent* MeshComp : ActorPair.Value)
		{
			if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
			{
				MeshGroups.FindOrAdd(Mesh).Add(MeshComp);
			}
		}
	}

	return MeshGroups;
}

/***************************************************************************************************************
*
* GroupByMeshThenProximity - Returns a mapping of all mesh components against the StaticMesh asset
*
*/

TMap<FMeshMaterialKey, TArray<TArray<UStaticMeshComponent*>>> FZeroPayEditorButtonsPluginModule::GroupByMeshThenProximity_MaterialAware(const TMap<UStaticMesh*, TArray<UStaticMeshComponent*>>& MeshGroups, float MaxDistance)
{
	TMap<FMeshMaterialKey, TArray<UStaticMeshComponent*>> MaterialGroups;

	// Step 1: Group by mesh + material signature
	for (const auto& MeshGroup : MeshGroups)
	{
		UStaticMesh* Mesh = MeshGroup.Key;
		const TArray<UStaticMeshComponent*>& Components = MeshGroup.Value;

		for (UStaticMeshComponent* Comp : Components)
		{
			if (!Comp) continue;

			FMeshMaterialKey Key;
			Key.Mesh = Mesh;

			const int32 NumSlots = Comp->GetNumMaterials();
			for (int32 i = 0; i < NumSlots; ++i)
			{
				Key.Materials.Add(Comp->GetMaterial(i));
			}

			MaterialGroups.FindOrAdd(Key).Add(Comp);
		}
	}

	// Step 2: Spatial clustering within each mesh/material group
	TMap<FMeshMaterialKey, TArray<TArray<UStaticMeshComponent*>>> OutClusters;

	for (const auto& Group : MaterialGroups)
	{
		const FMeshMaterialKey& Key = Group.Key;
		const TArray<UStaticMeshComponent*>& Components = Group.Value;

		TArray<TArray<UStaticMeshComponent*>> Clusters;

		for (UStaticMeshComponent* MeshComp : Components)
		{
			if (!MeshComp) continue;

			const FVector Center = MeshComp->Bounds.Origin;
			bool bAdded = false;

			for (TArray<UStaticMeshComponent*>& Cluster : Clusters)
			{
				if (Cluster.Num() == 0) continue;

				const FVector RefCenter = Cluster[0]->Bounds.Origin;
				if (FVector::Dist(Center, RefCenter) <= MaxDistance)
				{
					Cluster.Add(MeshComp);
					bAdded = true;
					break;
				}
			}

			if (!bAdded)
			{
				TArray<UStaticMeshComponent*> NewCluster;
				NewCluster.Add(MeshComp);
				Clusters.Add(NewCluster);
			}
		}

		OutClusters.Add(Key, Clusters);
	}

	return OutClusters;
}


/***************************************************************************************************************
*
* MergeMeshIslands - Takes currently grouped (mesh = same, material = same) and merges them with reduction 
*
*/

bool FZeroPayEditorButtonsPluginModule::MergeMeshIslands(const TMap<FMeshMaterialKey, TArray<TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return false ;

	// Load mesh merge + reduction interfaces
	IMeshReduction* MeshReduction = FModuleManager::LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetStaticMeshReductionInterface();

	if (!MeshReduction)
	{
		UE_LOG(LogTemp, Error, TEXT("MeshReductionInterface not available."));
		return false ;
	}

	if (!TargetFolderPath.StartsWith(TEXT("/Game")))
	{
		UE_LOG(LogTemp, Error, TEXT("Target folder path must start with /Game."));
		return false;
	}

	FAssetToolsModule& AssetTools = FAssetToolsModule::GetModule();
	int32 MergeIndex = 0;

	for (const auto& IslandGroup : ClusteredIslands)
	{
		const TArray<TArray<UStaticMeshComponent*>>& Clusters = IslandGroup.Value;

		for (const TArray<UStaticMeshComponent*>& Cluster : Clusters)
		{
			if (Cluster.Num() == 0) continue;

			TArray<UPrimitiveComponent*> ComponentsToMerge;
			for (UStaticMeshComponent* Comp : Cluster)
			{
				if (Comp)
					ComponentsToMerge.Add(Comp);
			}

			FMeshMergingSettings MergeSettings;
			MergeSettings.bMergeMaterials = true;
			MergeSettings.MaterialSettings.TextureSize = FIntPoint(2048, 2048);
			MergeSettings.bBakeVertexDataToMesh = true;
			MergeSettings.bGenerateLightMapUV = true;
			MergeSettings.bPivotPointAtZero = false;

			FString AssetName = FString::Printf(TEXT("Merged_Island_%03d"), MergeIndex++);
			FString PackagePath = TargetFolderPath + TEXT("/") + AssetName;
			FString CleanPackagePath = PackageTools::SanitizePackageName(PackagePath);

			UPackage* Package = CreatePackage(*CleanPackagePath);
			if (!Package)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create package: %s"), *CleanPackagePath);
				continue;
			}

			FVector MergedLocation;
			TArray<UObject*> OutAssets;
			const float ScreenSize = 1.0f;

			IMeshMergeUtilities& MergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

			MergeUtilities.MergeComponentsToStaticMesh(
				ComponentsToMerge,
				World,
				MergeSettings,
				nullptr, // InBaseMaterial
				Package,
				AssetName,
				OutAssets,
				MergedLocation,
				ScreenSize,
				false // bSilent
			);

			if (OutAssets.Num() == 0) continue;

			for (UObject* Asset : OutAssets)
			{
				if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
				{
					// Apply triangle reduction if needed
					if (ReductionPercent < 0.99f)
					{
						FMeshDescription* OriginalDesc = StaticMesh->GetMeshDescription(0);
						if (OriginalDesc)
						{
							// Prepare overlapping corners
							FOverlappingCorners OverlappingCorners;
							FStaticMeshOperations::FindOverlappingCorners(
								OverlappingCorners,
								*OriginalDesc,
								0.0001f
							);

							// Copy mesh for output
							FMeshDescription ReducedDesc;
							ReducedDesc = *OriginalDesc ;

							// Settings
							FMeshReductionSettings Settings;
							Settings.PercentTriangles = ReductionPercent;
							Settings.TerminationCriterion = EStaticMeshReductionTerimationCriterion::Triangles;

							float MaxDeviation = 0.0f;

							// Perform reduction
							MeshReduction->ReduceMeshDescription(
								ReducedDesc,
								MaxDeviation,
								*OriginalDesc,
								OverlappingCorners,
								Settings
							);

							StaticMesh->CreateMeshDescription(0, ReducedDesc) ;
							UStaticMesh::FCommitMeshDescriptionParams CommitParams;
							StaticMesh->CommitMeshDescription(0, CommitParams);
						}
					}

					// Finalize asset: only after all changes are done
					StaticMesh->PostEditChange();
					StaticMesh->MarkPackageDirty();
					FAssetRegistryModule::AssetCreated(StaticMesh);

					FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

					bool bSaved = UPackage::SavePackage(
						Package,
						StaticMesh,
						EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
						*PackageFilename,
						GError,
						nullptr,
						false, // bSaveToMemory
						true   // bForceByteSwapping
					);

				}
			}
		}
	}

	return true;
}


/********************************************************************************************************/
/*                                           DEBUG FUNCTIONS                                            */
/********************************************************************************************************/


void FZeroPayEditorButtonsPluginModule::DrawClusterDebugBoxes(const TMap<FMeshMaterialKey, TArray<TArray<UStaticMeshComponent*>>>& ClusteredGroups, UWorld* World, float Lifetime) 
{
	if (!World) return;

	int32 ClusterIndexGlobal = 0;

	for (const auto& Group : ClusteredGroups)
	{
		const FMeshMaterialKey& Key = Group.Key;
		const TArray<TArray<UStaticMeshComponent*>>& Clusters = Group.Value;

		for (const TArray<UStaticMeshComponent*>& Cluster : Clusters)
		{
			if (Cluster.Num() == 0) continue;

			// Compute combined bounding box of the cluster
			FBox ClusterBox(EForceInit::ForceInit);
			for (UStaticMeshComponent* Comp : Cluster)
			{
				ClusterBox += Comp->Bounds.GetBox();
			}

			// Draw cluster box (blue)
			DrawDebugBox(
				World,
				ClusterBox.GetCenter(),
				ClusterBox.GetExtent(),
				FColor::Blue,
				false,
				Lifetime,
				0,
				2.0f
			);

			// Optional: Label cluster
			DrawDebugString(
				World,
				ClusterBox.GetCenter() + FVector(0, 0, 30),
				FString::Printf(TEXT("Cluster %d\nMesh: %s\nMats: %d"),
					ClusterIndexGlobal++,
					*Key.Mesh->GetName(),
					Key.Materials.Num()),
				nullptr,
				FColor::White,
				Lifetime,
				false,
				1.5f
			);

			// Draw individual mesh component boxes (green)
			for (UStaticMeshComponent* Comp : Cluster)
			{
				if (!Comp) continue;

				const FBox CompBox = Comp->Bounds.GetBox();

				DrawDebugBox(
					World,
					CompBox.GetCenter(),
					CompBox.GetExtent(),
					FColor::Green,
					false,
					Lifetime,
					0,
					0.5f
				);
			}
		}
	}
}
/********************************************************************************************************/
/*                                          SUPPORT FUNCTIONS                                           */
/********************************************************************************************************/

void FZeroPayEditorButtonsPluginModule::UpdateQuest3ReducerUIProgressField()
{
	// Post result back to main thread safely
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			/* During development, changes to the editor utility BP can cause the instance to disappear */
			if (WidgetQuest3ReducerInstance == nullptr)
				return;
			if (!IsValid(WidgetQuest3ReducerInstance))
				return;

			UFunction* Func = WidgetQuest3ReducerInstance->FindFunction("UpdateUIProgressField");
			if (!Func)
			{
				UE_LOG(LogTemp, Error, TEXT("Function UpdateUIProgressField not found on %s"), *WidgetQuest3ReducerInstance->GetName());
				return;
			}

			// Match the parameter layout: 1 FString
			struct FMyParams
			{
				FString InputString;
			};

			FMyParams Params;
			Params.InputString = LastMessage;

			WidgetQuest3ReducerInstance->ProcessEvent(Func, &Params);
		});
}


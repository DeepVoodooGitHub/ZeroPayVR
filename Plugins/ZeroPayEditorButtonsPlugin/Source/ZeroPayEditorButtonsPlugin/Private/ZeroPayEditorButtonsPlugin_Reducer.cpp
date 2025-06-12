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
#include "ContentBrowserModule.h" 
#include "Engine/StaticMeshActor.h"
#include "ObjectTools.h"
#include "UObject/UObjectGlobals.h"
#include "IContentBrowserSingleton.h"

UZeroPayEditorReduceOperationHandle* FZeroPayEditorButtonsPluginModule::ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	/* Find the settings file and validate it's correct */
	bAbortOperation = false;
	ReduceHandle = NewObject<UZeroPayEditorReduceOperationHandle>();

	//Async(EAsyncExecution::Thread, [this, dataAsset, reducerSettings, runtimeSettings]()
//		{
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
#endif

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
	FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Gathering actors...")));
	SlowTask.MakeDialog();

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
	LastMessage = FString::Printf(TEXT("Stage 1 - Grouping by distance (%d units)"), reducerSettings->MeshReductionSettings.Stage1_MaxDistance);
	UpdateQuest3ReducerUIProgressField();

	/* >>> Group them by proximity to each other... */
	//auto ClusteredGroups = GroupByMeshThenProximity_MaterialAware(MeshGroups, reducerSettings->MeshReductionSettings.Stage1_MaxDistance);
	auto ClusteredGroups = GroupByMeshesViaProximity(MeshGroups, reducerSettings->MeshReductionSettings.Stage1_MaxDistance);

	SlowTask.EnterProgressFrame(0.1f);

#if 0
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
#endif
	
	/* Debug? */
	if (runtimeSettings.bStage1_ShowMergeGroups)
		DrawClusterDebugBoxes(ClusteredGroups, GEditor->GetEditorWorldContext().World(), 20.0f);

	/* Info */
	LastMessage = FString::Printf(TEXT("Stage 1 - Merging (%d Clusters)"), ClusteredGroups.Num() );
	UpdateQuest3ReducerUIProgressField();

	/* >>> Merge */
	FString ReducedAssetMeshPath = FString::Printf(TEXT("/Game/ZeroPayMods/UGC%s/Levels/ReducedAssets/Meshes"), *dataAsset->Definition.UGCID); 

	bSuccess = MergeMeshIslands(ClusteredGroups, 0.5f, *ReducedAssetMeshPath);

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
* GroupByMeshesViaProximity - Returns a mapping of all mesh components against the StaticMesh asset
*
*/

TMap<UStaticMesh*, TArray<TArray<UStaticMeshComponent*>>> FZeroPayEditorButtonsPluginModule::GroupByMeshesViaProximity(const TMap<UStaticMesh*, TArray<UStaticMeshComponent*>>& MeshGroups, float MaxDistance)
{
	TMap<UStaticMesh*, TArray<TArray<UStaticMeshComponent*>>> OutClusters;

	for (const auto& MeshGroup : MeshGroups)
	{
		UStaticMesh* Mesh = MeshGroup.Key;
		const TArray<UStaticMeshComponent*>& Components = MeshGroup.Value;

		TArray<TArray<UStaticMeshComponent*>> Clusters;

		for (UStaticMeshComponent* MeshComp : Components)
		{
			if (!MeshComp) continue;

			const FVector Center = MeshComp->Bounds.Origin;
			bool bAdded = false;

			// Check against each existing cluster
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

			// If not added to an existing cluster, start a new one
			if (!bAdded)
			{
				TArray<UStaticMeshComponent*> NewCluster;
				NewCluster.Add(MeshComp);
				Clusters.Add(NewCluster);
			}
		}

		OutClusters.Add(Mesh, Clusters);
	}

	return OutClusters;
}


/***************************************************************************************************************
*
* MergeMeshIslands - Takes currently grouped (mesh = same, material = same) and merges them with reduction 
*
*/

bool FZeroPayEditorButtonsPluginModule::MergeMeshIslands(const TMap<UStaticMesh*, TArray<TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath)
{
	/* Get total operations to keep user happy.. */
	int32 nMergeCount = 0;
	for (const auto& ClusterPair : ClusteredIslands)
	{
		const TArray<TArray<UStaticMeshComponent*>>& IslandGroups = ClusterPair.Value;
		for (const TArray<UStaticMeshComponent*>& IslandGroup : IslandGroups)
		{
			nMergeCount++;
		}
	}

	/* Info */
	FScopedSlowTask SlowTask(nMergeCount, FText::FromString(TEXT("Merging and replacing...")));
	SlowTask.MakeDialog();

	/* Iterate all supplied mesh islands and merge them, placing them in the target folder under unique names*/
	int32 nMergeIndex = 0;
	for (const auto& ClusterPair : ClusteredIslands)
	{
		const TArray<TArray<UStaticMeshComponent*>>& IslandGroups = ClusterPair.Value;

		for (const TArray<UStaticMeshComponent*>& IslandGroup : IslandGroups)
		{
			FString PackageName = FString::Printf(TEXT("%s/Merged_Island_%05d"), *TargetFolderPath, nMergeIndex);
			MergeMesh(IslandGroup, PackageName);
			nMergeIndex++;

			SlowTask.EnterProgressFrame(1);
		}
	}
	return true;
}


bool FZeroPayEditorButtonsPluginModule::MergeMesh(const TArray<UStaticMeshComponent*> SelectedComponents, const FString& PackageName)
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	bool bReplaceSourceActors = false;

	FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Merging actors...")));
	SlowTask.MakeDialog();

	// Extracting static mesh components from the selected mesh components in the dialog
	TArray<UPrimitiveComponent*> ComponentsToMerge;

	for (UStaticMeshComponent* SelectedComponent : SelectedComponents)
	{
		// Determine whether or not this component should be incorporated according the user settings
		if (SelectedComponent->IsValidLowLevel())
		{
			ComponentsToMerge.Add(Cast<UPrimitiveComponent>(SelectedComponent));
		}
	}

	FVector MergedActorLocation;
	TArray<UObject*> AssetsToSync;

	if (ComponentsToMerge.Num())
	{
		UWorld* World = ComponentsToMerge[0]->GetWorld();
		checkf(World != nullptr, TEXT("Invalid World retrieved from Mesh components"));
		const float ScreenAreaSize = TNumericLimits<float>::Max();

		// Setup merge settings
		FMeshMergingSettings SettingsObject;
		SettingsObject.bMergeMaterials = true;
		SettingsObject.bGenerateLightMapUV = true;
		SettingsObject.bComputedLightMapResolution = true;
		SettingsObject.LODSelectionType = EMeshLODSelectionType::AllLODs;
		SettingsObject.bUseLandscapeCulling = false;
		SettingsObject.bBakeVertexDataToMesh = true;
		SettingsObject.bAllowDistanceField = false;

		// If the merge destination package already exists, it is possible that the mesh is already used in a scene somewhere, or its materials or even just its textures.
		// Static primitives uniform buffers could become invalid after the operation completes and lead to memory corruption. To avoid it, we force a global reregister.
		if (FindObject<UObject>(nullptr, *PackageName))
		{
			FGlobalComponentReregisterContext GlobalReregister;
			MeshUtilities.MergeComponentsToStaticMesh(ComponentsToMerge, World, SettingsObject, nullptr, nullptr, PackageName, AssetsToSync, MergedActorLocation, ScreenAreaSize, true);
		}
		else
		{
			MeshUtilities.MergeComponentsToStaticMesh(ComponentsToMerge, World, SettingsObject, nullptr, nullptr, PackageName, AssetsToSync, MergedActorLocation, ScreenAreaSize, true);
		}
	}

	if (AssetsToSync.Num())
	{
		FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		for (UObject* AssetToSync : AssetsToSync)
		{
			// MergeComponentsToStaticMesh() will have outered all assets (material instance, textures) to the static mesh package.
			// Move each of them to their own package, so that they show up in the Content Browser
			if (AssetToSync && !AssetToSync->IsA<UStaticMesh>())
			{
				FString AssetName = AssetToSync->GetName();
				FString AssetPackagePath = FPackageName::GetLongPackagePath(AssetToSync->GetPathName());
				FString AssetPackageName = AssetPackagePath / AssetName;

				UPackage* AssetPackage = CreatePackage(*AssetPackageName);
				check(AssetPackage);
				AssetPackage->FullyLoad();
				AssetPackage->Modify();

				// Replace existing asset by the new one.
				if (UObject* OldAsset = FindObject<UObject>(AssetPackage, *AssetName))
				{
					FName ObjectName = OldAsset->GetFName();
					UObject* Outer = OldAsset->GetOuter();
					OldAsset->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);

					// Consolidate or "Replace" the old object with the new object for any living references.
					bool bShowDeleteConfirmation = false;
					TArray<UObject*> OldDataAssetArray = { OldAsset };
					ObjectTools::ConsolidateObjects(AssetToSync, { OldDataAssetArray }, bShowDeleteConfirmation);
				}

				AssetToSync->Rename(*AssetName, AssetPackage, REN_DontCreateRedirectors);
				AssetToSync->SetFlags(RF_Public | RF_Standalone);
			}

			AssetRegistry.AssetCreated(AssetToSync);
			GEditor->BroadcastObjectReimported(AssetToSync);
		}

		//Also notify the content browser that the new assets exists
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, true);

		// Place new mesh in the world
		if (bReplaceSourceActors)
		{
			UStaticMesh* MergedMesh = nullptr;
			if (AssetsToSync.FindItemByClass(&MergedMesh))
			{
				const FScopedTransaction Transaction(FText::FromString(TEXT("Placing merged actors...")));
				UniqueLevels[0]->Modify();

				UWorld* World = UniqueLevels[0]->OwningWorld;
				FActorSpawnParameters Params;
				Params.OverrideLevel = UniqueLevels[0];
				FRotator MergedActorRotation(ForceInit);

				AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(MergedActorLocation, MergedActorRotation, Params);
				MergedActor->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);
				MergedActor->SetActorLabel(MergedMesh->GetName());
				World->UpdateCullDistanceVolumes(MergedActor, MergedActor->GetStaticMeshComponent());
				GEditor->SelectNone(true, true);
				GEditor->SelectActor(MergedActor, true, true);
				// Remove source actors
				for (AActor* Actor : Actors)
				{
					Actor->Destroy();
				}
			}
		}
	}

	return true;
}

/********************************************************************************************************/
/*                                           DEBUG FUNCTIONS                                            */
/********************************************************************************************************/

void FZeroPayEditorButtonsPluginModule::DrawClusterDebugBoxes(const TMap<UStaticMesh*, TArray<TArray<UStaticMeshComponent*>>>& ClusteredGroups, UWorld* World, float Lifetime)
{
	if (!World) return;

	int32 ClusterIndexGlobal = 0;

	for (const auto& Group : ClusteredGroups)
	{
		UStaticMesh* Mesh = Group.Key;
		const TArray<TArray<UStaticMeshComponent*>>& Clusters = Group.Value;

		for (const TArray<UStaticMeshComponent*>& Cluster : Clusters)
		{
			if (Cluster.Num() == 0) continue;

			// Compute combined bounding box of the cluster
			FBox ClusterBox(EForceInit::ForceInit);
			for (UStaticMeshComponent* Comp : Cluster)
			{
				if (Comp)
				{
					ClusterBox += Comp->Bounds.GetBox();
				}
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
				FString::Printf(TEXT("Cluster %d\nMesh: %s"),
					ClusterIndexGlobal++,
					*Mesh->GetName()),
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


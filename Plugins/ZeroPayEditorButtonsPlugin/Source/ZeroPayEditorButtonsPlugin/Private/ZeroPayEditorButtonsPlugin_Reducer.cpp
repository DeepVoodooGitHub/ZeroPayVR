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
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "FileHelpers.h" 

FReducerResults FZeroPayEditorButtonsPluginModule::ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	return ReducePCVRLevelForQuest3(dataAsset, reducerSettings, runtimeSettings);
}

/********************************************************************************************************/
/*                                   STAGE 1 - MERGE UNIFORMED MESHES					                */
/********************************************************************************************************/


FReducerResults FZeroPayEditorButtonsPluginModule::ReducePCVRLevelForQuest3(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
{
	FReducerResults returnValue;

	FString ReducedAssetMeshPath = FString::Printf(TEXT("/Game/ZeroPayMods/UGC%s/Levels/ReducedAssets/Meshes"), *dataAsset->Definition.UGCID);

	/* Handle bad dataAsset */
	if (dataAsset->Definition.pcvrlevel == nullptr)
	{
		LastMessage = "Error S1 - PCVR Level is not defined (in data asset in UGC folder)";
		UpdateQuest3ReducerUIProgressField();
		return returnValue ;
	}

	/* Validate existing installation */
	FFoundAssetInformation Result = ScanLevelActorsAndDirectory(dataAsset->Definition.quest3level->PersistentLevel, *ReducedAssetMeshPath);
	if ( (Result.FoundAssets > 0) || (Result.FoundInstances > 0) )
	{
		const FString DialogMessage = FString::Printf(TEXT("Warning!\n\nThere are %d found assets in the Context Browser (merged meshes, materials, etc.) that will be destroyed and recreated.\nThere are %d instanced actors in the Quest 3 level that will be destroyed and recreated.\n\nAre you sure? You cannot undo these changes later."), Result.FoundAssets, Result.FoundInstances );
		EAppReturnType::Type DialogResult = FMessageDialog::Open( EAppMsgType::YesNo, FText::FromString(DialogMessage));

		switch (DialogResult)
		{
			case EAppReturnType::Yes:
				break;
			case EAppReturnType::No:
			{
				LastMessage = "User aborted.";
				UpdateQuest3ReducerUIProgressField();
				return returnValue;
				break;
			}
		}
	}

	FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Gathering actors...")));
	SlowTask.MakeDialog();

	/* >>> Delete old things */
	bool bDeletionSuccess = DeleteActorsAndAssets(dataAsset->Definition.quest3level->PersistentLevel, *ReducedAssetMeshPath);
	if (!bDeletionSuccess)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, failed to delete existing Quest 3 level 'ReducedAssets' folder and/or the merged assets located under the 'UGC/Levels/ReducedAssets' folder"));
		return returnValue;
	}

	/* Info */
	SlowTask.EnterProgressFrame(0.5f);

	/* Get PCVR World */
	UWorld* PCVRWorld = dataAsset->Definition.pcvrlevel.Get(); // Convert from soft to hard reference
	if (!PCVRWorld)
	{
		LastMessage = "Error.";
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, PCVR level supplied in dataasset was not valid."));
		return returnValue;
	}

	ULevel* PCVRLevel = PCVRWorld->PersistentLevel;

	/* >>> Get the maximum bounding box for all actors we care to merge */
	FBox MaxBoundingBox = GetMaximumVisibleBoundingBox(PCVRLevel) ;

	if (runtimeSettings.bStage1_ShowVisualDebug)
		DrawDebugBox(GEditor->GetEditorWorldContext().World(), MaxBoundingBox.GetCenter(), MaxBoundingBox.GetExtent(), FColor::Blue, false, runtimeSettings.fStage1_VisualDebugDuration, 0, 5.0f);

	/* Check it's not MASSIVE */
	const FVector Min = MaxBoundingBox.Min;
	const FVector Max = MaxBoundingBox.Max;

	volatile  int32 CountX = FMath::CeilToInt((Max.X - Min.X) / reducerSettings->MeshReductionSettings.BoundingClusterSize);
	volatile  int32 CountY = FMath::CeilToInt((Max.Y - Min.Y) / reducerSettings->MeshReductionSettings.BoundingClusterSize);
	volatile  int32 CountZ = FMath::CeilToInt((Max.Z - Min.Z) / reducerSettings->MeshReductionSettings.BoundingClusterSize);

	int64 OverflowCheckMultiplication = static_cast<int64>(CountX) * static_cast<int64>(CountY);
	OverflowCheckMultiplication *= static_cast<int64>(CountZ);

	UE_LOG(LogTemp, Display, TEXT("++++++++++++++++ OVERFLOW %d "), OverflowCheckMultiplication);

	if (OverflowCheckMultiplication >= 2147483646)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, when dividing your PCVR level into 'BoundingClusterSize' (as per the 'DA_Quest3_Reducer_Settings' dataset in the 'Game/ZeroPayMods/UGCxxxxxxx/Levels' directory) we ran out of memory. Please increase the 'BoundingClusterSize', or exclude actors which are too far away, or more then closer to the center"));
		return returnValue;
	}

	/* >>> Break the world into chunks and returns all meshes in that chunk */

	FVector ChunkSize(reducerSettings->MeshReductionSettings.BoundingClusterSize, reducerSettings->MeshReductionSettings.BoundingClusterSize, reducerSettings->MeshReductionSettings.BoundingClusterSize);
	auto Chunks = PartitionActorsIntoBoundingBoxes(MaxBoundingBox, ChunkSize, PCVRLevel, returnValue);

	if (runtimeSettings.bStage1_ShowVisualDebug)
	{
		for (const auto& Pair : Chunks)
		{
			const FBox& Box = Pair.Key;
			const FVector Center = Box.GetCenter();
			const FVector Extent = Box.GetExtent();

			DrawDebugBox(GEditor->GetEditorWorldContext().World(), Center, Extent, FColor::Green, false, runtimeSettings.fStage1_VisualDebugDuration, 0, 2.0f);
		}
	}

	SlowTask.EnterProgressFrame(0.5f, FText::FromString(TEXT("Generated Quest 3 merged meshes and materials...")));

	/* >>> Merge actors within each cluster */
	bool bSuccess = MergeMeshIslands(Chunks, 0.5f, *ReducedAssetMeshPath, dataAsset->Definition.quest3level, returnValue);

	/* >>> Save the changes */
	UPackage* LevelPackage = dataAsset->Definition.quest3level->PersistentLevel->GetOutermost();
	bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave({ LevelPackage }, /*bCheckDirty=*/true, /*bPromptToSave=*/false) == FEditorFileUtils::EPromptReturnCode::PR_Success;
	if (!bSaved)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, failed to save the updated quest 3 sub-level, please manually save it."));
		return returnValue;
	}

	return returnValue;
}


/********************************************************************************************************/
/*                                   MESH, WORLD, ETC. SUPPORT FUNCTIONS                                */
/********************************************************************************************************/


/***************************************************************************************************************
*
* GetMaximumVisibleBoundingBox - Returns a bounding box of actual visible / usable actors which should be considered for merging
*
*/
FBox FZeroPayEditorButtonsPluginModule::GetMaximumVisibleBoundingBox(ULevel* Level)
{
	if (!Level) return FBox(ForceInit);

	// Class name substrings to skip (can be expanded as needed)
	TArray<FString> IgnoredClassNames = {
		TEXT("BlockingVolume"),
		TEXT("NavMeshBoundsVolume"),
		TEXT("LightmassImportanceVolume"),
		TEXT("PrecomputedVisibilityVolume"),
		TEXT("CullDistanceVolume"),
		TEXT("KillZVolume"),
		TEXT("AudioVolume"),
		TEXT("TriggerVolume"),
		TEXT("PhysicsVolume"),
		TEXT("PostProcessVolume")
	};

	FBox BoundingBox(ForceInit);

	for (AActor* Actor : Level->Actors)
	{
		if (!Actor) continue;

		const FString ClassName = Actor->GetClass()->GetName();

		bool bShouldIgnore = false;
		for (const FString& Ignored : IgnoredClassNames)
		{
			if (ClassName.Contains(Ignored))
			{
				bShouldIgnore = true;
				break;
			}
		}
		if (bShouldIgnore) continue;

		// Add bounds of all visible, registered primitive components
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
			{
				if (Prim->IsRegistered() && Prim->IsVisible())
				{
					BoundingBox += Prim->Bounds.GetBox();
				}
			}
		}
	}

	return BoundingBox;
}



/***************************************************************************************************************
*
* PartitionActorsIntoBoundingBoxes - Create a list of actors based on a larger global bounding box, chunked into the required size
*
*/

TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> FZeroPayEditorButtonsPluginModule::PartitionActorsIntoBoundingBoxes(const FBox& GlobalBounds, const FVector& ChunkSize, ULevel* Level, FReducerResults& returnValue)
{
	TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> Results;

	if (!Level || !ChunkSize.X || !ChunkSize.Y || !ChunkSize.Z)
	{
		return Results;
	}

	// Compute grid extents
	const FVector Min = GlobalBounds.Min;
	const FVector Max = GlobalBounds.Max;

	const int32 CountX = FMath::CeilToInt((Max.X - Min.X) / ChunkSize.X);
	const int32 CountY = FMath::CeilToInt((Max.Y - Min.Y) / ChunkSize.Y);
	const int32 CountZ = FMath::CeilToInt((Max.Z - Min.Z) / ChunkSize.Z);

	// Pre-allocate grid of bounds
	TArray<FBox> GridBoxes;
	GridBoxes.Reserve(CountX * CountY * CountZ);

	for (int32 x = 0; x < CountX; ++x)
	{
		for (int32 y = 0; y < CountY; ++y)
		{
			for (int32 z = 0; z < CountZ; ++z)
			{
				const FVector BoxMin = Min + FVector(x, y, z) * ChunkSize;
				const FVector BoxMax = BoxMin + ChunkSize;
				GridBoxes.Add(FBox(BoxMin, BoxMax));
			}
		}
	}

	// Create output structure matching each FBox to a component array
	Results.SetNum(GridBoxes.Num());
	for (int32 i = 0; i < GridBoxes.Num(); ++i)
	{
		Results[i].Key = GridBoxes[i];
		Results[i].Value = TArray<UStaticMeshComponent*>();
	}

	// Scan actors and assign their components to correct cells
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor) continue;

		const FVector ActorBoundsCenter = Actor->GetComponentsBoundingBox().GetCenter();

		// Determine grid cell index this actor belongs to
		int32 x = FMath::FloorToInt((ActorBoundsCenter.X - Min.X) / ChunkSize.X);
		int32 y = FMath::FloorToInt((ActorBoundsCenter.Y - Min.Y) / ChunkSize.Y);
		int32 z = FMath::FloorToInt((ActorBoundsCenter.Z - Min.Z) / ChunkSize.Z);

		// Clamp to valid range
		if (x < 0 || y < 0 || z < 0 || x >= CountX || y >= CountY || z >= CountZ)
			continue;

		const int32 FlatIndex = x * CountY * CountZ + y * CountZ + z;

		if (!Results.IsValidIndex(FlatIndex)) continue;
		/* Stats */
		returnValue.OriginalActorCount++;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Comp))
			{
				if (MeshComponent->IsRegistered())
				{
					Results[FlatIndex].Value.Add(MeshComponent);

					/* Stats */
					UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
					if (Mesh && Mesh->GetRenderData())
					{
						const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
						returnValue.OriginalTriangleCount += LOD0.GetNumTriangles();
						returnValue.OriginalVertexCount += LOD0.GetNumVertices();
						returnValue.OriginalMaterialCount += Mesh->GetStaticMaterials().Num();
					}
				}
			}
		}
	}

	return Results;
}

/***************************************************************************************************************
*
* MergeMeshIslands - Takes currently grouped (mesh = same, material = same) and merges them with reduction 
*
*/

bool FZeroPayEditorButtonsPluginModule::MergeMeshIslands(const TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath, TSoftObjectPtr<UWorld> Quest3World, FReducerResults& returnValue)
{
	// Count how many merge operations we'll perform
	const int32 nMergeCount = ClusteredIslands.Num();

	// Show progress to the user
	FScopedSlowTask SlowTask(nMergeCount, FText::FromString(TEXT("Merging and replacing...")));
	SlowTask.MakeDialog();

	/* Get PCVR World */
	if (!Quest3World.IsValid())
		return false;

	/* Loop round clusters / islands*/
	int32 nMergeIndex = 0;
	double LastTickTime = FPlatformTime::Seconds();
	for (const TPair<FBox, TArray<UStaticMeshComponent*>>& Pair : ClusteredIslands)
	{
		const TArray<UStaticMeshComponent*>& IslandGroup = Pair.Value;

		/* Update progress */
		nMergeIndex++;
		SlowTask.EnterProgressFrame(1);
		// Throttle UI updates: only tick once per second
		double Now = FPlatformTime::Seconds();
		if (Now - LastTickTime >= 1.0)
		{
			FSlateApplication::Get().Tick();
			LastTickTime = Now;
		}

		if (IslandGroup.Num() == 0)
			continue;

		const FString PackageName = FString::Printf(TEXT("%s/Merged_Island_%05d"), *TargetFolderPath, nMergeIndex);

		MergeMesh(IslandGroup, PackageName, Quest3World.Get(), returnValue);
	}

	return true;
}

bool FZeroPayEditorButtonsPluginModule::MergeMesh(const TArray<UStaticMeshComponent*> SelectedComponents, const FString& PackageName, UWorld* targetQuest3World, FReducerResults& returnValue)
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	TArray<UObject*> AssetsToSync;
	bool bReplaceSourceActors = true;

	FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Merging actors...")));
	SlowTask.MakeDialog();

	// Extracting static mesh components from the selected mesh components in the dialog
	TArray<UStaticMeshComponent*> StaticMeshComponentsToMerge;

	for (UStaticMeshComponent* SelectedComponent : SelectedComponents)
	{
		// Determine whether or not this component should be incorporated according the user settings
		if ( SelectedComponent->IsValidLowLevel() )
		{
			// Should always have an owner, but maybe not.. 
			if (SelectedComponent->GetOwner())
			{
				StaticMeshComponentsToMerge.Add(SelectedComponent);
			}
			
		}
	}

	// Get the module for the mesh merge utilities
	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();

	if (StaticMeshComponentsToMerge.Num())
	{
		FVector ProxyLocation = FVector::ZeroVector;
		TArray<UObject*> NewAssetsToSync;

		FCreateProxyDelegate ProxyDelegate;
		ProxyDelegate.BindLambda(
			[&NewAssetsToSync](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
			{
				//Update the asset registry that a new static mash and material has been created
				if (InAssetsToSync.Num())
				{
					FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					int32 AssetCount = InAssetsToSync.Num();
					for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
					{
						AssetRegistry.AssetCreated(InAssetsToSync[AssetIndex]);
						GEditor->BroadcastObjectReimported(InAssetsToSync[AssetIndex]);
					}

					//Also notify the content browser that the new assets exists
					FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					ContentBrowserModule.Get().SyncBrowserToAssets(InAssetsToSync, true);

					NewAssetsToSync += InAssetsToSync;
				}
			});

		StaticMeshComponentsToMerge.RemoveAll([](UStaticMeshComponent* Val) { return Val->GetStaticMesh() == nullptr; });

		if (StaticMeshComponentsToMerge.Num())
		{
			FGuid JobGuid = FGuid::NewGuid();
			FMeshProxySettings Settings;
			MeshMergeUtilities.CreateProxyMesh(StaticMeshComponentsToMerge, Settings, nullptr, PackageName, JobGuid, ProxyDelegate);
		}

		PlaceMeshProxyInQuest3Level(NewAssetsToSync, targetQuest3World->PersistentLevel, returnValue);
	}

	return true;
}


void FZeroPayEditorButtonsPluginModule::PlaceMeshProxyInQuest3Level(TArray<UObject*>& NewAssetsToSync, ULevel* Level, FReducerResults& returnValue)
{
	UStaticMesh* MergedMesh = nullptr;
	if (NewAssetsToSync.FindItemByClass(&MergedMesh))
	{
		Level->Modify();

		UWorld* World = Level->OwningWorld;
		FActorSpawnParameters Params;
		Params.OverrideLevel = Level;
		FRotator MergedActorRotation(ForceInit);
		// The pivot of the merged mesh is always at the origin
		FVector MergedActorLocation(0, 0, 0);

		AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(MergedActorLocation, MergedActorRotation, Params);
		MergedActor->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);
		MergedActor->SetActorLabel(MergedMesh->GetName());
		MergedActor->SetFolderPath("ReducedAssets"); 
		World->UpdateCullDistanceVolumes(MergedActor, MergedActor->GetStaticMeshComponent());
		//GEditor->SelectNone(true, true);
		//GEditor->SelectActor(MergedActor, true, true);

		/* Stats */
		returnValue.ReducedActorCount++ ;

		if (MergedActor->GetStaticMeshComponent())
		{
			UStaticMesh* Mesh = MergedActor->GetStaticMeshComponent()->GetStaticMesh();
			if (Mesh && Mesh->GetRenderData())
			{
				const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
				returnValue.ReducedTriangleCount += LOD0.GetNumTriangles();
				returnValue.ReducedVertexCount += LOD0.GetNumVertices();
				returnValue.ReducedMaterialCount += Mesh->GetStaticMaterials().Num();
			}
		}

	}
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

FFoundAssetInformation FZeroPayEditorButtonsPluginModule::ScanLevelActorsAndDirectory(ULevel* LevelToScan, const FString& TargetAssetPath)
{
	FFoundAssetInformation Result;

	if (!LevelToScan)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid level pointer."));
		return Result ;
	}

	// === 1. Count actors under "ReducedAssets" folder in level ===
	int32 ReducedFolderActorCount = 0;

	for (AActor* Actor : LevelToScan->Actors)
	{
		if (!Actor) continue;

		// Check folder path (this is the editor-level folder grouping in World Outliner)
		FName FolderPath = Actor->GetFolderPath();

		// Match top-level folder called "ReducedAssets"
		if (FolderPath.ToString().StartsWith(TEXT("ReducedAssets")))
		{
			Result.FoundInstances++;
		}
	}

	// === 2. Count all assets under given content folder ===
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetList;

	FARFilter Filter;
	Filter.PackagePaths.Add(*TargetAssetPath); 
	Filter.bRecursivePaths = true;

	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	Result.FoundAssets = AssetList.Num();

	return Result;
}

bool FZeroPayEditorButtonsPluginModule::DeleteActorsAndAssets(ULevel* TargetLevel, const FString& AssetFolderPathToDelete)
{
	if (!TargetLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid level pointer."));
		return false ;
	}

	UWorld* World = TargetLevel->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Level has no valid world."));
		return false;
	}

	// === 1. Destroy actors under World Outliner folder "ReducedAssets" ===
	TArray<AActor*> ActorsToDestroy;
	for (AActor* Actor : TargetLevel->Actors)
	{
		if (!Actor) continue;

		if (Actor->GetFolderPath().ToString().StartsWith(TEXT("ReducedAssets")))
		{
			ActorsToDestroy.Add(Actor);
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		World->EditorDestroyActor(Actor, true);
	}

	UE_LOG(LogTemp, Display, TEXT("Destroyed %d actor(s) in folder 'ReducedAssets'."), ActorsToDestroy.Num());

	// === 2. Save the level ===
	if (TargetLevel->GetOutermost())
	{
		UPackage* LevelPackage = TargetLevel->GetOutermost();
		bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave({ LevelPackage }, /*bCheckDirty=*/true, /*bPromptToSave=*/false) == FEditorFileUtils::EPromptReturnCode::PR_Success;
		if (!bSaved)
		{
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, failed to save the existing Quest3 level after destroying all 'ReducedAssets' - Cannot continue."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not get level package to save."));
		return false;
	}

	// === 3. Delete all assets under the specified folder ===
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetsToDelete;
	FARFilter Filter;
	Filter.PackagePaths.Add(*AssetFolderPathToDelete);
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = false;

	AssetRegistryModule.Get().GetAssets(Filter, AssetsToDelete);

	if (AssetsToDelete.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("No assets found in path: %s"), *AssetFolderPathToDelete);
		return true ;
	}

	// Gather object references to delete
	TArray<UObject*> ObjectsToDelete;
	for (const FAssetData& Asset : AssetsToDelete)
	{
		UObject* LoadedAsset = Asset.GetAsset();
		if (LoadedAsset)
		{
			ObjectsToDelete.Add(LoadedAsset);
		}
	}

	if (ObjectsToDelete.Num() > 0)
	{
		ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);
	}

	UE_LOG(LogTemp, Display, TEXT("Deleted %d asset(s) from folder '%s'."), ObjectsToDelete.Num(), *AssetFolderPathToDelete);
	return true;
}

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


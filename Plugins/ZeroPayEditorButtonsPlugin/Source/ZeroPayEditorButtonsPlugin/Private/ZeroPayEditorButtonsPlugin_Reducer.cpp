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
#include "ZeroPayEditor_Reducer_Zone.h"
#include "ZeroPayEditor_Reducer_PlayerZone.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "Engine/StaticMesh.h"
#include "Logging/LogMacros.h"

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
	if (!reducerSettings->MeshReductionSettings.bDryRun)
	{
		bool bDeletionSuccess = DeleteActorsAndAssets(dataAsset->Definition.quest3level->PersistentLevel, *ReducedAssetMeshPath);
		if (!bDeletionSuccess)
		{
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, failed to delete existing Quest 3 level 'ReducedAssets' folder and/or the merged assets located under the 'UGC/Levels/ReducedAssets' folder"));
			return returnValue;
		}
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

	/* >>> Break the world into chunks and returns all meshes in that chunk */

	FVector ChunkSize(reducerSettings->MeshReductionSettings.BoundingChunkSize, reducerSettings->MeshReductionSettings.BoundingChunkSize, reducerSettings->MeshReductionSettings.BoundingChunkSize);
	auto Chunks = PartitionActorsIntoBoundingBoxes(MaxBoundingBox, ChunkSize, PCVRLevel, returnValue, reducerSettings);

	/* >>> If we are supporting zoning, where meshes closer to the player get higher quality settings build some quick lookups */
	if (reducerSettings->MeshReductionSettings.bEnablePlayerZoning)
	{
		bool bFoundZoning = false ;
		PlayerZoneBounds.Empty();
		for (TActorIterator<AZeroPayEditor_Reducer_PlayerZone> It(PCVRWorld); It; ++It)
		{
			AZeroPayEditor_Reducer_PlayerZone* Zone = *It;
			if (Zone)
			{
				FBox Bounds = Zone->GetWorldBoundingBox();
				if (Bounds.IsValid)
				{
					PlayerZoneBounds.Add(Bounds);
					bFoundZoning = true;
				}
			}
		}
		if (!bFoundZoning)
		{
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, EnablePlayerZoning is turned on but could not find any AZeroPayEditor_Reducer_PlayerZone actors in PCVR level (make sure it's in the PCVR sub-level and not persistent, and that this layer is visible!)"));
			return returnValue;
		}
	}

	/* Info */
	SlowTask.EnterProgressFrame(0.5f, FText::FromString(TEXT("Generated Quest 3 merged meshes and materials...")));

	/* Remove all debug lines, we draw lots more.. */
	FlushPersistentDebugLines(PCVRWorld);

	/* >>> Merge actors within each cluster */
	bool bSuccess = MergeMeshIslands(Chunks, 0.5f, *ReducedAssetMeshPath, dataAsset->Definition.quest3level, returnValue, reducerSettings, runtimeSettings);

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

		// Check if it's the type you're looking for
		if (AZeroPayEditor_Reducer_Zone* ReducerZone = Cast<AZeroPayEditor_Reducer_Zone>(Actor))
		{
			ShowNotification("The reducation operation has been limied to the 'zone' provided by ZeroPayEditor_Reducer_PlayerZone component inside the PCVR level, meshes outside this bounding box will be ignored.", SNotificationItem::ECompletionState::CS_Success) ;
			return ReducerZone->GetWorldBoundingBox();
		}

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

TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> FZeroPayEditorButtonsPluginModule::PartitionActorsIntoBoundingBoxes(const FBox& GlobalBounds, const FVector& ChunkSize, ULevel* Level, FReducerResults& returnValue, UZeroPayEditor_ReducerSettingsAsset* reducerSettings)
{
	int nDebugCount = 0;
	TArray<TPair<FBox, TArray<UStaticMeshComponent*>>> Results;

	if (!Level || ChunkSize.X <= 0 || ChunkSize.Y <= 0 || ChunkSize.Z <= 0)
	{
		return Results;
	}

	const FVector Min = GlobalBounds.Min;
	const FVector Max = GlobalBounds.Max;

	const int32 CountX = FMath::CeilToInt((Max.X - Min.X) / ChunkSize.X);
	const int32 CountY = FMath::CeilToInt((Max.Y - Min.Y) / ChunkSize.Y);
	const int32 CountZ = FMath::CeilToInt((Max.Z - Min.Z) / ChunkSize.Z);

	// Map FlatIndex → Results array index
	TMap<int32, int32> IndexMap;

	for (AActor* Actor : Level->Actors)
	{
		if (!Actor) continue;

		const FVector Center = Actor->GetComponentsBoundingBox(true, false).GetCenter();

		int32 x = FMath::FloorToInt((Center.X - Min.X) / ChunkSize.X);
		int32 y = FMath::FloorToInt((Center.Y - Min.Y) / ChunkSize.Y);
		int32 z = FMath::FloorToInt((Center.Z - Min.Z) / ChunkSize.Z);

		if (x < 0 || y < 0 || z < 0 || x >= CountX || y >= CountY || z >= CountZ)
			continue;

		const int32 FlatIndex = x * CountY * CountZ + y * CountZ + z;

		/* Validate array is not going to be too large */
		constexpr int32 MaxBytes = MAX_int32; 
		constexpr int32 ApproxBytesPerEntry = 24;
		constexpr int32 MaxSafeEntries = MaxBytes / ApproxBytesPerEntry;

		if (IndexMap.Num() >= MaxSafeEntries)
		{
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, when dividing your PCVR level into 'BoundingChunkSize' (as per the 'DA_Quest3_Reducer_Settings' dataset in the 'Game/ZeroPayMods/UGCxxxxxxx/Levels' directory) we ran out of memory. Please INCREASE the 'BoundingChunkSize', or exclude actors which are too far away, or more then closer to the center"));
			return TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>();
		}

		// Create new entry only if needed
		int32 ResultIndex = -1;
		if (!IndexMap.Contains(FlatIndex))
		{
			const FVector BoxMin = Min + FVector(x, y, z) * ChunkSize;
			const FVector BoxMax = BoxMin + ChunkSize;
			const FBox ChunkBox(BoxMin, BoxMax);

			ResultIndex = Results.Num();
			Results.Add(TPair<FBox, TArray<UStaticMeshComponent*>>(ChunkBox, TArray<UStaticMeshComponent*>()));
			IndexMap.Add(FlatIndex, ResultIndex);
		}
		else
		{
			ResultIndex = IndexMap[FlatIndex];
		}

		// Stats
		returnValue.OriginalActorCount++;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Comp))
			{
				if (MeshComponent->IsRegistered())
				{
					Results[ResultIndex].Value.Add(MeshComponent);

					if (UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
					{
						if (const FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
						{
							const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
							returnValue.OriginalTriangleCount += LOD0.GetNumTriangles();
							returnValue.OriginalVertexCount += LOD0.GetNumVertices();
							returnValue.OriginalMaterialCount += Mesh->GetStaticMaterials().Num();
						}
					}
				}
			}
		}
	}

	int32 nCounter = 0;
	bool bBadMeshChunking = false ;
	for (const TPair<FBox, TArray<UStaticMeshComponent*>>& Pair : Results)
	{
		int32 MaxCount = Pair.Value.Num();
		nCounter++;
		/* Check we aren't adding loads of meshes, i.e. the chunk size is too small */
		if (MaxCount > reducerSettings->MeshReductionSettings.MaxMeshesPerChunk)
		{
			bBadMeshChunking = true;
			UE_LOG(LogTemp, Error, TEXT("PartitionActorsIntoBoundingBoxes failed, following box area had %d meshes in that chunk (%d allowed, as specified in 'DA_Quest3_Reducer_Settings' dataset in the 'Game/ZeroPayMods/UGCxxxxxxx/Levels' directory)"), MaxCount, reducerSettings->MeshReductionSettings.MaxMeshesPerChunk);
			const FBox& Box = Pair.Key;
			UE_LOG(LogTemp, Error, TEXT("     + Box Center: %s | Extent: %s"), *Box.GetCenter().ToString(), *Box.GetExtent().ToString());
			if (reducerSettings->MeshReductionSettings.bShowBadMaxMeshChunks)
			{
				DrawDebugBox(GEditor->GetEditorWorldContext().World(), Box.GetCenter(), Box.GetExtent(), FColor::Red, true, 15.0f, 0, 4.0f);
				
			};
		}
	}

	/* Log */
	UE_LOG(LogTemp, Log, TEXT("PartitionActorsIntoBoundingBoxes scanned %d chunks"), nCounter);

	if (bBadMeshChunking)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, when dividing your PCVR level into 'BoundingChunkSize' (as per the 'DA_Quest3_Reducer_Settings' dataset in the 'Game/ZeroPayMods/UGCxxxxxxx/Levels' directory) we found too many meshes in a single chunk. See 'Output Log' for more details, or enable 'bShowBadMaxMeshChunks' in data asset -OR- DECREASE the 'BoundingChunkSize'"));
		return TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>();
	}

	return Results;
}

/***************************************************************************************************************
*
* MergeMeshIslands - Takes currently grouped (mesh = same, material = same) and merges them with reduction 
*
*/

bool FZeroPayEditorButtonsPluginModule::MergeMeshIslands(const TArray<TPair<FBox, TArray<UStaticMeshComponent*>>>& ClusteredIslands, float ReductionPercent, const FString& TargetFolderPath, TSoftObjectPtr<UWorld> Quest3World, FReducerResults& returnValue, UZeroPayEditor_ReducerSettingsAsset* reducerSettings, FReducerRuntimeSettings runtimeSettings)
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

		FZeroPayEditor_MeshReductionZone* reductionZoneSettings = nullptr;
		int32 ZoneSettingsID = 0;
		if (reducerSettings->MeshReductionSettings.bEnablePlayerZoning)
		{			
			const FBox& ClusterBox = Pair.Key;

			//UE_LOG(LogTemp, Error, TEXT("---- Box Center: %s | Extent: %s"), *PlayerZoneBounds[0].GetCenter().ToString(), *ClusterBox.GetCenter().ToString());

			/* Find the distance from any player zone to this box */
			float ClosestDistance = TNumericLimits<float>::Max();
			for (const FBox& ZoneBox : PlayerZoneBounds)
			{
				float Distance = BoxSurfaceDistance(ClusterBox, ZoneBox);
				if (Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
				}
			}

			//UE_LOG(LogTemp, Display, TEXT("--- ClosestDistance %.2f... "), ClosestDistance);

			/* Pick settings based on distance  */
			if (ClosestDistance < reducerSettings->MeshReductionSettings.PlayerZone1_Settings.ZoneDistance)
			{
				reductionZoneSettings = &reducerSettings->MeshReductionSettings.PlayerZone1_Settings;
				ZoneSettingsID = 1;
			}
			else
				if (ClosestDistance < (reducerSettings->MeshReductionSettings.PlayerZone1_Settings.ZoneDistance + reducerSettings->MeshReductionSettings.PlayerZone2_Settings.ZoneDistance))
				{
					reductionZoneSettings = &reducerSettings->MeshReductionSettings.PlayerZone2_Settings;
					ZoneSettingsID = 2 ;
				}
				else
				{
					reductionZoneSettings = &reducerSettings->MeshReductionSettings.PlayerZone3_Settings;
					ZoneSettingsID = 3;
				}
		}

		/* Show debug? */
		if (runtimeSettings.bStage1_ShowVisualDebug)
		{
			const FBox& Box = Pair.Key;
			const FVector Center = Box.GetCenter();
			const FVector Extent = Box.GetExtent();
			FColor Color = FColor::Green ; 
			switch (ZoneSettingsID)
			{
				case 1:
					Color = FColor::Green;
					break;
				case 2:
					Color = FColor(255, 165, 0); // Orange (not predefined)
					break;
				case 3:
					Color = FColor::Red;
					break;
				default:
					Color = FColor::White; // Fallback color
					break;
			}
			DrawDebugBox(GEditor->GetEditorWorldContext().World(), Center, Extent, Color, false, runtimeSettings.fStage1_VisualDebugDuration, 0, 2.0f);
		}


		const FString PackageName = FString::Printf(TEXT("%s/Merged_Island_%05d"), *TargetFolderPath, nMergeIndex);
		if (!reducerSettings->MeshReductionSettings.bDryRun)
			MergeMesh(IslandGroup, PackageName, Quest3World.Get(), reductionZoneSettings, returnValue);
		else
			returnValue.bFailed = false;
	}

	return true;
}

bool FZeroPayEditorButtonsPluginModule::MergeMesh(const TArray<UStaticMeshComponent*> SelectedComponents, const FString& PackageName, UWorld* targetQuest3World, FZeroPayEditor_MeshReductionZone* reductionZoneSettings, FReducerResults& returnValue)
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	TArray<UObject*> AssetsToSync;
	bool bReplaceSourceActors = true;

	FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Merging actors...")));
	SlowTask.MakeDialog();

	// Extracting static mesh components from the selected mesh components in the dialog
	StaticMeshComponentsToMerge.Empty() ;
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
			[&NewAssetsToSync, this](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
			{
				//Update the asset registry that a new static mash and material has been created
				if (InAssetsToSync.Num())
				{
					FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					int32 AssetCount = InAssetsToSync.Num();
					for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
					{
						/* Register with editor */
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
			Settings.bCreateCollision = false;
			Settings.bGenerateLightmapUVs = true;
			Settings.bComputeLightMapResolution = true;
			Settings.bReuseMeshLightmapUVs = true;
			/* If a zone has been supplied, alter settings to reflect it */
			if (reductionZoneSettings)
			{
				Settings.MergeDistance = reductionZoneSettings->MergeDistance;
				Settings.ScreenSize = reductionZoneSettings->ScreenSize;
			}
			MeshMergeUtilities.CreateProxyMesh(StaticMeshComponentsToMerge, Settings, nullptr, PackageName, JobGuid, ProxyDelegate);
		}

		GEngine->ForceGarbageCollection(true) ;

		int32 AssetCount = NewAssetsToSync.Num() ;
		for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
		{
			/* If static mesh? Merge in collision data at this point */
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(NewAssetsToSync[AssetIndex]))
			{
				MergeCollisionFromComponents(StaticMeshComponentsToMerge, StaticMesh);

				StaticMesh->MarkPackageDirty();
				StaticMesh->Modify();
			}
		}

		PlaceMeshProxyInQuest3Level(NewAssetsToSync, targetQuest3World->PersistentLevel, returnValue);
		NewAssetsToSync.Empty() ;
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


float FZeroPayEditorButtonsPluginModule::BoxSurfaceDistance(const FBox& A, const FBox& B)
{
	if (A.Intersect(B))
	{
		return 0.0f;
	}

	FVector Dist(0);

	// For each axis, compute the gap between the two boxes
	for (int i = 0; i < 3; ++i)
	{
		if (A.Max[i] < B.Min[i])
			Dist[i] = B.Min[i] - A.Max[i];
		else if (B.Max[i] < A.Min[i])
			Dist[i] = A.Min[i] - B.Max[i];
		else
			Dist[i] = 0.0f; // Overlap on this axis
	}

	// Return Euclidean distance
	return Dist.Size();
}

void FZeroPayEditorButtonsPluginModule::MergeCollisionFromComponents(const TArray<UStaticMeshComponent*>& Components, UStaticMesh* OutMergedMesh)
{
	if (!OutMergedMesh)
		return;

	OutMergedMesh->CreateBodySetup();
	UBodySetup* MergedBodySetup = OutMergedMesh->GetBodySetup();
	MergedBodySetup->AggGeom = FKAggregateGeom(); // Clear any existing

	bool bHasSimple = false;
	bool bHasComplex = false;

	for (UStaticMeshComponent* Comp : Components)
	{
		if (!Comp || !Comp->GetStaticMesh())
			continue;

		UBodySetup* SourceBodySetup = Comp->GetStaticMesh()->GetBodySetup();
		if (!SourceBodySetup)
			continue;

		// Sphere Colliders - Add spheres with component transform offsets
		const FKAggregateGeom& AggGeom = SourceBodySetup->AggGeom;
		for (const FKSphereElem& Sphere : SourceBodySetup->AggGeom.SphereElems)
		{
			FTransform CompToMerged = Comp->GetComponentTransform();

			// Transform the center
			FVector NewCenter = CompToMerged.TransformPosition(Sphere.Center);

			// Uniform scaling approximation (avoids distortion from non-uniform scale)
			FVector CompScale = CompToMerged.GetScale3D();
			float UniformScale = CompScale.GetMax(); // Or average: (X+Y+Z)/3.0f

			FKSphereElem NewSphere = Sphere;
			NewSphere.Center = NewCenter;
			NewSphere.Radius *= UniformScale;

			MergedBodySetup->AggGeom.SphereElems.Add(NewSphere);
		}

		// Sphy Colliders - Add with local to mesh space
		for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
		{
			FTransform CompToMerged = Comp->GetComponentTransform();

			// Transform center
			FVector NewCenter = CompToMerged.TransformPosition(Sphyl.Center);

			// Transform rotation
			FQuat NewRotation = CompToMerged.GetRotation() * Sphyl.Rotation.Quaternion();

			// Uniform scale for radius/length
			FVector Scale = CompToMerged.GetScale3D();
			float UniformScale = Scale.GetMax(); // or average

			FKSphylElem NewSphyl = Sphyl;
			NewSphyl.Center = NewCenter;
			NewSphyl.Rotation = NewRotation.Rotator();
			NewSphyl.Radius *= UniformScale;
			NewSphyl.Length *= UniformScale;

			MergedBodySetup->AggGeom.SphylElems.Add(NewSphyl);
		}

		// Convex colliders - Add with local mesh space
		for (const FKConvexElem& Convex : AggGeom.ConvexElems)
		{
			FTransform CompToMerged = Comp->GetComponentTransform();

			FKConvexElem NewConvex;

			// Transform each vertex
			for (const FVector& Vertex : Convex.VertexData)
			{
				FVector TransformedVertex = CompToMerged.TransformPosition(Vertex);
				NewConvex.VertexData.Add(TransformedVertex);
			}
			NewConvex.UpdateElemBox();           

			MergedBodySetup->AggGeom.ConvexElems.Add(NewConvex);
		}

		// Box Colliders - Add boxes but recenter boxes from local to mesh space
		FTransform SourceToMerged = Comp->GetComponentTransform() ;
		for (const FKBoxElem& Box : AggGeom.BoxElems)
		{
			// Convert box center and orientation to merged space
			FVector NewCenter = SourceToMerged.TransformPosition(Box.Center);
			FQuat NewRot = SourceToMerged.GetRotation() * Box.Rotation.Quaternion();

			FKBoxElem NewBox = Box;
			NewBox.Center = NewCenter;
			NewBox.Rotation = NewRot.Rotator(); // UE uses FRotator here

			MergedBodySetup->AggGeom.BoxElems.Add(NewBox);
		}

		// Track what type of collision was used
		ECollisionTraceFlag TraceFlag = SourceBodySetup->CollisionTraceFlag;

		if (TraceFlag == CTF_UseSimpleAsComplex)
			bHasSimple = true;
		else if (TraceFlag == CTF_UseComplexAsSimple)
			bHasComplex = true;
	}

	// We use default, unless ANY of the meshes use something else..
	MergedBodySetup->CollisionTraceFlag = CTF_UseDefault;
	if (bHasSimple && bHasComplex)
	{
		UE_LOG(LogTemp, Warning, TEXT("Merged mesh has mixed collision modes (simple and complex). Using complex."));
		MergedBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}
	else if (bHasComplex)
	{
		MergedBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}
	else if (bHasSimple)
	{
		MergedBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}

	// Regenerate physics data
	MergedBodySetup->InvalidatePhysicsData();
	MergedBodySetup->CreatePhysicsMeshes();
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



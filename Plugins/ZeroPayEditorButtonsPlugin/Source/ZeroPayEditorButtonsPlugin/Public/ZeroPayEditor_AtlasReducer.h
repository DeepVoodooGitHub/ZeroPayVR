#pragma once

#include "CoreMinimal.h"

class UPackage;
class UStaticMesh;
class UStaticMeshComponent;
class UZeroPayEditor_ReducerSettingsAsset;
struct FReducerResults;


/* ============================================================================
 * ZeroPay atlas reducer
 * ----------------------------------------------------------------------------
 * Treats every supplied static mesh component as part of one logical mesh,
 * cuts it into a world-aligned 3D grid by triangle centroid, packs the
 * BaseColor / Normal textures each chunk needs into shared atlases, and builds
 * one static mesh per chunk.
 *
 * It does NOT touch levels or actors: the caller gathers the components and
 * places the resulting meshes (see ReducePCVRLevelForQuest3). Everything it
 * creates is written under FInput::AssetRootPath, except the generated master
 * material which lives at FInput::MasterMaterialPackage and persists between
 * runs.
 *
 * Chunk mesh vertex layout:
 *   UV0 = source texture UVs (tiling multipliers baked in, wrapped in shader)
 *   UV1 = generated lightmap UVs
 *   UV2 = atlas rectangle offset  (0..1 atlas space)
 *   UV3 = atlas rectangle size    (0..1 atlas space)
 * ============================================================================
 */

namespace ZeroPayAtlasReducer
{
	struct FInput
	{
		/* Components to reduce. Already filtered by the caller. */
		TArray<UStaticMeshComponent*> Components;

		/* Optional. Triangles whose centroid lies outside are ignored. */
		FBox ClipBounds = FBox(ForceInit);

		/* e.g. /Game/ZeroPayMods/UGC123/Levels/ReducedAssets
		 * Meshes, atlases and material instances are created in sub-folders.
		 * The caller is expected to wipe this folder before each run. */
		FString AssetRootPath;

		/* e.g. /Game/ZeroPayMods/UGC123/Levels/ReducerMaster/M_Quest3AtlasMaster
		 * Created on first use, kept afterwards. Ignored when
		 * AtlasReductionSettings.MasterMaterialOverride is set. */
		FString MasterMaterialPackage;

		const UZeroPayEditor_ReducerSettingsAsset* Settings = nullptr;

		bool bDryRun = false;
	};

	struct FChunkOutput
	{
		/* Grid coordinate of the chunk */
		FIntVector Key = FIntVector::ZeroValue;

		/* The chunk's grid cell in world space */
		FBox Bounds = FBox(ForceInit);

		/* World location the mesh was built around. Spawn the actor here with
		 * zero rotation and unit scale. */
		FVector Pivot = FVector::ZeroVector;

		int32 TriangleCount = 0;

		/* Atlases referenced by this chunk, primary first */
		TArray<int32> AtlasIndices;

		/* Null for dry runs or if the build failed */
		UStaticMesh* Mesh = nullptr;
	};

	struct FOutput
	{
		/* In Morton (processing) order */
		TArray<FChunkOutput> Chunks;

		/* Every package created or modified - save these */
		TArray<UPackage*> Packages;

		int32 AtlasCount = 0;
	};

	/* Returns false on failure or cancellation (Results.bCancelled is set for
	 * the latter). Warnings are appended to Results either way. */
	bool Run(const FInput& Input, FReducerResults& Results, FOutput& Output);

	/* Parameter names the master material must expose */
	extern const FName ParamName_BaseColorAtlas;
	extern const FName ParamName_NormalAtlas;
	extern const FName ParamName_AtlasSize;
	extern const FName ParamName_Roughness;
	extern const FName ParamName_Metallic;
}

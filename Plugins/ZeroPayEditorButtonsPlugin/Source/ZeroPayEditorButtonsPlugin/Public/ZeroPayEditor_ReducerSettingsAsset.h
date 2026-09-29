#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

#include "ZeroPayEditor_ReducerSettingsAsset.generated.h"

class UMaterialInterface;


/* ============================================================================
 * SCOPE
 * ----------------------------------------------------------------------------
 * ReducePCVRLevelForQuest3 now runs the ATLAS reducer (see
 * ZeroPayEditor_AtlasReducer.h). It reads:
 *
 *   GeneralSettings                  - bDryRun
 *   AtlasReductionSettings           - everything chunk / atlas related
 *   MeshReductionSettings            - bMergeOnlyStaticActors,
 *                                      bPreserveCollision,
 *                                      bPreserveLightmapUVs,
 *                                      MergedLightmapResolution
 *   VisibilityReductionSettings      - applied to spawned chunk actors
 *   LightingReductionSettings        - applied to spawned chunk actors
 *   PhysicsReductionSettings         - applied to chunk collision
 *
 * The remaining MeshReductionSettings fields (BoundingChunkSize,
 * MaxMeshesPerChunk, triangle reduction, material grouping, player zones) and
 * MaterialReductionSettings belong to the legacy merge path and are NOT used
 * by the atlas reducer. They are kept so the legacy functions still compile.
 * ============================================================================
 */


 /* ============================================================================
  * Player Zone Settings (legacy merge path only)
  *
  * Distances are ABSOLUTE radii measured from the nearest
  * AZeroPayEditor_Reducer_PlayerZone actor:
  *     Zone 1 = [0, Zone1.Distance)
  *     Zone 2 = [Zone1.Distance, Zone2.Distance)
  *     Zone 3 = everything beyond
  * Zones must therefore be ordered nearest to furthest.
  * ============================================================================
  */
	
USTRUCT(BlueprintType)
struct FZeroPayEditor_MeshReductionZone
{
	GENERATED_USTRUCT_BODY()

	/* Absolute distance from a player zone at which this zone stops applying */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones", meta = (ClampMin = "0.0"))
	float Distance = 2500.0f;

	/* Percentage of geometry removed from the merged result in this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float TriangleReductionPercent = 10.0f;

	/* Multiplier applied to cull distances for actors generated in this zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones", meta = (ClampMin = "0.0"))
	float DistanceMultiplier = 1.0f;

	/* Use full proxy generation (ProxyLOD) rather than a straight merge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones|Backend")
	bool bUseProxyMerge = false;

	/* Proxy only: target screen size in pixels used to pick source detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones|Backend", meta = (ClampMin = "1", EditCondition = "bUseProxyMerge"))
	int32 ProxyScreenSize = 300;

	/* Proxy only: gaps smaller than this are closed. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones|Backend", meta = (ClampMin = "0.0", EditCondition = "bUseProxyMerge"))
	float ProxyMergeDistance = 0.0f;

	/* Source LOD index pulled from each contributing mesh when merging. -1 lets the engine calculate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones", meta = (ClampMin = "-1", ClampMax = "7"))
	int32 SourceLODIndex = 0;

	/* Atlas resolution for this zone (legacy proxy path only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Player Zones|Materials", meta = (ClampMin = "64"))
	int32 MergedTextureSize = 1024;

	FZeroPayEditor_MeshReductionZone()
	{
	}

	FZeroPayEditor_MeshReductionZone(
		const float InDistance,
		const float InTriangleReductionPercent,
		const float InDistanceMultiplier,
		const bool InUseProxyMerge,
		const int32 InSourceLODIndex,
		const int32 InMergedTextureSize)
		: Distance(InDistance)
		, TriangleReductionPercent(InTriangleReductionPercent)
		, DistanceMultiplier(InDistanceMultiplier)
		, bUseProxyMerge(InUseProxyMerge)
		, SourceLODIndex(InSourceLODIndex)
		, MergedTextureSize(InMergedTextureSize)
	{
	}
};


/* ============================================================================
 * General Settings
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_GeneralReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* Performs analysis/validation only and makes no asset or level changes.
	 * The atlas reducer still computes the full chunk + atlas layout and draws
	 * it when visual debug is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|General")
	bool bDryRun = false;
};


/* ============================================================================
 * Atlas Reduction Settings
 *
 * The PCVR level's static meshes are treated as one logical mesh, cut into a
 * 3D grid of chunks by triangle centroid. Every chunk becomes one static mesh
 * whose materials all point at shared texture atlases (BaseColor + Normal).
 * Tiling is preserved in the shader, so no geometry is split.
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_AtlasReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* Size of each chunk in Unreal units. The grid is world aligned, so chunk
	 * boundaries are stable between runs. Use a very large Z to get columns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Chunking", meta = (ClampMin = "100.0"))
	FVector ChunkSize = FVector(2000.0, 2000.0, 2000.0);

	/* An atlas opened by chunk C may be filled by any chunk within this many
	 * chunks of C on every axis (a cube of (2R+1)^3 chunks). 0 means an atlas
	 * is only ever used by the chunk that opened it. Chunks are processed in
	 * Morton order so spatial neighbours reach an atlas first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Chunking", meta = (ClampMin = "0", ClampMax = "16"))
	int32 AtlasShareRadius = 2;

	/* Width and height of every atlas (rounded up to a power of two). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Textures", meta = (ClampMin = "256", ClampMax = "4096"))
	int32 MaxAtlasSize = 2048;

	/* Size of each source texture inside the atlas, as a percentage of its
	 * original size. 100 = unchanged, 50 = half (1024 -> 512). Results are
	 * rounded to the nearest power of two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Textures", meta = (ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "100.0"))
	float TextureScalePercent = 100.0f;

	/* No texture is ever placed in an atlas smaller than this (power of two).
	 * Also the size used for materials that have no texture at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Textures", meta = (ClampMin = "4", ClampMax = "256"))
	int32 MinTileSize = 32;

	/* Opacity baked into the atlas alpha for translucent materials whose
	 * opacity does NOT come from the BaseColor texture alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Textures", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TranslucentFallbackOpacity = 0.5f;

	/* Actors carrying this tag are left out of the reduction entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Filtering")
	FName SkipActorTag = FName(TEXT("NoQuestMerge"));

	/* Optional. When empty the reducer creates (once) and uses
	 * /Game/ZeroPayMods/UGC<ID>/Levels/ReducerMaster/M_Quest3AtlasMaster.
	 * A custom master must expose the same parameter names - see the notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Material")
	TSoftObjectPtr<UMaterialInterface> MasterMaterialOverride;

	/* Rebuild the generated master material's node graph on the next run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Material")
	bool bRegenerateMasterMaterial = false;

	/* Applied to every generated material instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.8f;

	/* Applied to every generated material instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.0f;

	/* 0 = every chunk uses MeshReductionSettings.MergedLightmapResolution.
	 * Above 0 = lightmap resolution is derived from each chunk's surface area
	 * at roughly this many lightmap texels per metre (clamped 32..1024). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Lighting", meta = (ClampMin = "0.0"))
	float LightmapTexelsPerMeter = 0.0f;

	/* Number of chunk meshes built in parallel per batch. Higher is faster but
	 * holds more mesh data in memory at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Atlas|Advanced", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MeshBuildBatchSize = 16;
};


/* ============================================================================
 * Static Mesh / Geometry Settings
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_MeshReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* LEGACY. The atlas reducer uses AtlasReductionSettings.ChunkSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Chunking", meta = (ClampMin = "1.0"))
	float BoundingChunkSize = 2000.0f;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Chunking", meta = (ClampMin = "1"))
	int32 MaxMeshesPerChunk = 1000;

	/* LEGACY. Mesh reduction is not performed by the atlas reducer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Reduction")
	bool bReduceTriangleCount = true;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Reduction", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DefaultTriangleReductionPercent = 50.0f;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Reduction", meta = (ClampMin = "0"))
	int32 IgnoreMeshesBelowTriangleCount = 500;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Merging")
	bool bMergeByMaterial = true;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Merging")
	bool bBakeMergedMaterials = false;

	/* Only merge components that cannot move. Disabling this will bake doors,
	 * pickups and physics props into static geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Merging")
	bool bMergeOnlyStaticActors = true;

	/* Transfer the source meshes' simple collision onto the chunk meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Preservation")
	bool bPreserveCollision = true;

	/* Generate lightmap UVs (UV1) on the chunk meshes. Required for baked lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Preservation")
	bool bPreserveLightmapUVs = true;

	/* LEGACY. The atlas master material does not use vertex colours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Preservation")
	bool bPreserveVertexColours = true;

	/* Lightmap resolution for generated chunk meshes (unless
	 * AtlasReductionSettings.LightmapTexelsPerMeter is set). Fixed rather than
	 * computed - the engine's computed value scales with merged bounds and will
	 * assert inside the UV packer (FAllocator2D, uint16 width) on large chunks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Preservation", meta = (ClampMin = "32", ClampMax = "1024"))
	int32 MergedLightmapResolution = 256;

	/* LEGACY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Player Zones")
	bool bEnablePlayerZoning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Player Zones")
	FZeroPayEditor_MeshReductionZone PlayerZone1_Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Player Zones")
	FZeroPayEditor_MeshReductionZone PlayerZone2_Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Meshes|Player Zones")
	FZeroPayEditor_MeshReductionZone PlayerZone3_Settings;

	FZeroPayEditor_MeshReductionSettings()
		/*                        Dist    TriRed%  DistMul  Proxy  SrcLOD  TexSize */
		: PlayerZone1_Settings(2500.0f, 0.0f, 1.00f, false, 0, 2048)
		, PlayerZone2_Settings(5000.0f, 25.0f, 0.75f, false, 1, 1024)
		, PlayerZone3_Settings(100000.0f, 60.0f, 0.50f, true, -1, 512)
	{
	}
};


/* ============================================================================
 * Material Settings (legacy merge path only)
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_MaterialReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* LEGACY. The atlas reducer uses AtlasReductionSettings.MaxAtlasSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Materials|Atlasing", meta = (ClampMin = "128"))
	int32 AtlasResolution = 2048;
};


/* ============================================================================
 * Visibility / Culling Settings
 *
 * Applied to generated chunk actors at spawn time.
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_VisibilityReductionSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility")
	bool bEnableDistanceCulling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility|Distance", meta = (ClampMin = "0.0"))
	float DefaultSmallObjectCullDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility|Distance", meta = (ClampMin = "0.0"))
	float DefaultMediumObjectCullDistance = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility|Distance", meta = (ClampMin = "0.0"))
	float DefaultLargeObjectCullDistance = 25000.0f;

	/* Merged meshes at or above this bounds radius are never automatically
	 * culled. Raise this if distant landmarks or skyline geometry pop out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility|Distance", meta = (ClampMin = "0.0"))
	float NeverCullObjectSize = 5000.0f;

	/* Bounds radius below which a merged mesh is treated as small. Ten times
	 * this value is the small/medium boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility|Distance", meta = (ClampMin = "0.0"))
	float TinyObjectBoundsThreshold = 20.0f;

	/* Scale the chosen cull distance by the zone's DistanceMultiplier (legacy path only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Visibility")
	bool bUsePlayerZonesForCullDistance = true;
};


/* ============================================================================
 * Lighting Settings
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_LightingReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* Merged static geometry should rely on baked lighting. Dynamic shadow
	 * cost on Quest is disproportionate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Lighting")
	bool bDisableDynamicShadowsOnStaticGeometry = true;
};


/* ============================================================================
 * Collision / Physics Settings
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FZeroPayEditor_PhysicsReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/* Warning threshold for the number of collision primitives transferred
	 * onto a single merged mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Physics", meta = (ClampMin = "0"))
	int32 MaxCollisionPrimitiveCount = 32;

	/* Force merged meshes to simple collision. Strongly recommended - complex
	 * collision on a large merged proxy means per-triangle traces on mobile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Physics")
	bool bUseSimpleCollisionWherePossible = true;

	/* Use the merged render geometry itself as collision (complex-as-simple).
	 * Takes precedence over bUseSimpleCollisionWherePossible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Physics")
	bool bUseComplexAsSimpleCollision = false;
};


/* ============================================================================
 * Settings Asset
 *
 * NOTE: reduction statistics deliberately do NOT live here. They are returned
 * by value in FReducerResults (see ZeroPayEditor_ReducerTypes.h).
 * ============================================================================
 */

UCLASS(BlueprintType)
class UZeroPayEditor_ReducerSettingsAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_GeneralReductionSettings GeneralSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_AtlasReductionSettings AtlasReductionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MeshReductionSettings MeshReductionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_MaterialReductionSettings MaterialReductionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_VisibilityReductionSettings VisibilityReductionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_LightingReductionSettings LightingReductionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer")
	FZeroPayEditor_PhysicsReductionSettings PhysicsReductionSettings;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditor_AtlasReducer.h"

#include "ZeroPayEditor_ReducerSettingsAsset.h"
#include "ZeroPayEditor_ReducerTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogZeroPayAtlasReducer, Log, All);


namespace ZeroPayAtlasReducer
{
	const FName ParamName_BaseColorAtlas(TEXT("BaseColorAtlas"));
	const FName ParamName_NormalAtlas(TEXT("NormalAtlas"));
	const FName ParamName_AtlasSize(TEXT("AtlasSize"));
	const FName ParamName_Roughness(TEXT("Roughness"));
	const FName ParamName_Metallic(TEXT("Metallic"));

namespace
{
	/********************************************************************************************************/
	/*                                            CONSTANTS                                                 */
	/********************************************************************************************************/

	constexpr int32 UVChannel_Texture = 0;
	constexpr int32 UVChannel_Lightmap = 1;
	constexpr int32 UVChannel_RectOffset = 2;
	constexpr int32 UVChannel_RectScale = 3;
	constexpr int32 NumOutputUVChannels = 4;

	/* Matches the clamp the legacy path uses to stay clear of the FAllocator2D
	 * uint16 assert in the lightmap UV packer. */
	constexpr int32 MaxLightmapResolution = 1024;
	constexpr int32 MinLightmapResolution = 32;

	/* Resized tile pixels are cached while atlases are composed. Flushed when
	 * this is exceeded. */
	constexpr int64 TileCacheBudgetBytes = 1024LL * 1024LL * 1024LL;

	enum class EBlendGroup : uint8
	{
		Opaque = 0,
		Masked = 1,
		Translucent = 2
	};

	/* Variant = blend group * 2 + two-sided. One material instance per atlas
	 * per used variant. */
	constexpr int32 NumVariants = 6;

	int32 MakeVariant(EBlendGroup Group, bool bTwoSided)
	{
		return static_cast<int32>(Group) * 2 + (bTwoSided ? 1 : 0);
	}

	EBlendGroup GetVariantBlendGroup(int32 Variant)
	{
		return static_cast<EBlendGroup>(Variant / 2);
	}

	bool IsVariantTwoSided(int32 Variant)
	{
		return (Variant % 2) != 0;
	}

	const TCHAR* GetVariantName(int32 Variant)
	{
		static const TCHAR* Names[NumVariants] =
		{
			TEXT("Opaque"),      TEXT("Opaque_TwoSided"),
			TEXT("Masked"),      TEXT("Masked_TwoSided"),
			TEXT("Translucent"), TEXT("Translucent_TwoSided")
		};
		return Names[FMath::Clamp(Variant, 0, NumVariants - 1)];
	}

	FHashedMaterialParameterInfo MakeParamInfo(FName Name)
	{
		return FHashedMaterialParameterInfo(FMaterialParameterInfo(Name));
	}

	FString FormatCoordinate(int32 Value)
	{
		return Value < 0 ? FString::Printf(TEXT("n%d"), -Value) : FString::Printf(TEXT("%d"), Value);
	}

	/* 21 bits per axis, interleaved. */
	uint64 SplitBy3(uint32 Value)
	{
		uint64 X = Value & 0x1fffff;
		X = (X | X << 32) & 0x1f00000000ffffULL;
		X = (X | X << 16) & 0x1f0000ff0000ffULL;
		X = (X | X << 8) & 0x100f00f00f00f00fULL;
		X = (X | X << 4) & 0x10c30c30c30c30c3ULL;
		X = (X | X << 2) & 0x1249249249249249ULL;
		return X;
	}

	uint64 MortonCode(const FIntVector& NonNegativeKey)
	{
		return SplitBy3(static_cast<uint32>(NonNegativeKey.X))
			| (SplitBy3(static_cast<uint32>(NonNegativeKey.Y)) << 1)
			| (SplitBy3(static_cast<uint32>(NonNegativeKey.Z)) << 2);
	}


	/********************************************************************************************************/
	/*                                              TYPES                                                   */
	/********************************************************************************************************/

	enum class ETileAlpha : uint8
	{
		/* Alpha comes from the BaseColor texture (also used by opaque materials) */
		FromTexture,
		/* Alpha is overwritten with AlphaValue */
		Constant
	};

	/* Identifies the pixel content of one atlas tile. BaseColor and Normal
	 * share one layout, so a tile is a (BaseColor, Normal) pair. */
	struct FTileKey
	{
		UTexture2D* BaseColor = nullptr;
		UTexture2D* Normal = nullptr;
		FColor BaseColorConstant = FColor::White;   /* used when BaseColor is null */
		ETileAlpha AlphaMode = ETileAlpha::FromTexture;
		uint8 AlphaValue = 255;

		bool HasSameColourContent(const FTileKey& Other) const
		{
			return BaseColor == Other.BaseColor
				&& Normal == Other.Normal
				&& BaseColorConstant == Other.BaseColorConstant;
		}

		friend bool operator==(const FTileKey& A, const FTileKey& B)
		{
			return A.HasSameColourContent(B) && A.AlphaMode == B.AlphaMode && A.AlphaValue == B.AlphaValue;
		}

		friend uint32 GetTypeHash(const FTileKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.BaseColor);
			Hash = HashCombine(Hash, GetTypeHash(Key.Normal));
			Hash = HashCombine(Hash, GetTypeHash(Key.BaseColorConstant));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.AlphaMode)));
			Hash = HashCombine(Hash, GetTypeHash(Key.AlphaValue));
			return Hash;
		}
	};

	/* What the reducer understood about one source material. */
	struct FMaterialInfo
	{
		FTileKey Key;

		/* Opaque materials ignore alpha, so they may share any tile with the
		 * same colour content. */
		bool bOpaque = true;

		/* Desired tile size in pixels (powers of two) */
		FIntPoint TileSize = FIntPoint(32, 32);

		/* Source UV channel written into UV0, and the tiling multiplier baked in */
		int32 UVChannel = 0;
		FVector2f UVScale = FVector2f(1.0f, 1.0f);

		int32 Variant = 0;
	};

	/* Local-space triangle soup for one UStaticMesh (LOD0 source data). */
	struct FCachedMesh
	{
		bool bValid = false;
		int32 NumUVChannels = 1;

		TArray<FVector3f> Positions;          /* per vertex */
		TArray<int32> InstanceToVertex;       /* per vertex instance */
		TArray<FVector3f> Normals;            /* per vertex instance */
		TArray<FVector3f> Tangents;           /* per vertex instance */
		TArray<float> BinormalSigns;          /* per vertex instance */
		TArray<FVector2f> UVs;                /* per vertex instance * NumUVChannels */
		TArray<int32> TriangleInstances;      /* 3 per triangle */
		TArray<int32> TriangleMaterial;       /* per triangle: static material slot */

		int32 NumTriangles() const { return TriangleMaterial.Num(); }

		FVector2f GetUV(int32 Instance, int32 Channel) const
		{
			return UVs[Instance * NumUVChannels + FMath::Clamp(Channel, 0, NumUVChannels - 1)];
		}
	};

	struct FComponentRecord
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		int32 MeshIndex = INDEX_NONE;
		FTransform Transform;
		bool bMirrored = false;

		/* Static material slot -> index into MaterialInfos */
		TArray<int32> SlotMaterialInfo;

		/* The chunk holding most of this component's triangles receives its
		 * simple collision. */
		int32 CollisionChunk = INDEX_NONE;

		int32 GetMaterialInfo(int32 Slot) const
		{
			return SlotMaterialInfo[FMath::Clamp(Slot, 0, SlotMaterialInfo.Num() - 1)];
		}
	};

	struct FTriangleRef
	{
		int32 Component = INDEX_NONE;
		int32 Triangle = INDEX_NONE;
	};

	struct FTileRef
	{
		int32 Atlas = INDEX_NONE;
		int32 Placement = INDEX_NONE;
	};

	struct FChunk
	{
		FIntVector Key = FIntVector::ZeroValue;
		FBox Bounds = FBox(ForceInit);
		uint64 Morton = 0;

		TArray<FTriangleRef> Triangles;
		TSet<int32> MaterialInfos;

		/* Material info index -> where its tile lives */
		TMap<int32, FTileRef> Assignment;
		TArray<int32> AtlasIndices;

		TArray<int32> CollisionComponents;
	};

	/* Power-of-two square blocks inside a power-of-two square atlas. Because
	 * every block sits at a multiple of its own size, a tile never shares a
	 * 2x2 box-filter footprint with its neighbours at any mip it owns, so the
	 * atlas can use ordinary mips without bleeding. */
	struct FBuddyAllocator
	{
		int32 Size = 0;
		TArray<TArray<FIntPoint>> FreeBlocks;   /* index = depth, block size = Size >> depth */

		void Init(int32 InSize)
		{
			Size = InSize;
			FreeBlocks.Reset();
			FreeBlocks.SetNum(static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(InSize))) + 1);
			FreeBlocks[0].Add(FIntPoint::ZeroValue);
		}

		bool Allocate(int32 BlockSize, FIntPoint& OutPosition)
		{
			if (BlockSize <= 0 || BlockSize > Size)
			{
				return false;
			}

			const int32 TargetDepth = static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(Size)))
				- static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(BlockSize)));

			int32 Depth = TargetDepth;
			while (Depth >= 0 && FreeBlocks[Depth].Num() == 0)
			{
				--Depth;
			}

			if (Depth < 0)
			{
				return false;
			}

			FIntPoint Position = FreeBlocks[Depth].Pop(EAllowShrinking::No);

			/* Split down to the requested size, keeping the top-left quadrant */
			while (Depth < TargetDepth)
			{
				++Depth;
				const int32 Half = Size >> Depth;
				FreeBlocks[Depth].Add(Position + FIntPoint(Half, Half));
				FreeBlocks[Depth].Add(Position + FIntPoint(0, Half));
				FreeBlocks[Depth].Add(Position + FIntPoint(Half, 0));
			}

			OutPosition = Position;
			return true;
		}
	};

	struct FTilePlacement
	{
		FTileKey Key;
		FIntPoint Position = FIntPoint::ZeroValue;
		FIntPoint Size = FIntPoint::ZeroValue;
	};

	struct FAtlas
	{
		FIntVector OwnerChunk = FIntVector::ZeroValue;
		int32 Size = 0;
		FBuddyAllocator Allocator;

		TArray<FTilePlacement> Placements;
		TMap<FTileKey, int32> KeyToPlacement;

		/* Bit per variant that some chunk uses with this atlas */
		uint8 UsedVariants = 0;

		UTexture2D* BaseColorTexture = nullptr;
		UTexture2D* NormalTexture = nullptr;
		UMaterialInterface* VariantMaterials[NumVariants] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

		int32 Find(const FTileKey& Key, bool bAnyAlpha) const
		{
			if (const int32* Exact = KeyToPlacement.Find(Key))
			{
				return *Exact;
			}

			if (bAnyAlpha)
			{
				for (int32 Index = 0; Index < Placements.Num(); ++Index)
				{
					if (Placements[Index].Key.HasSameColourContent(Key))
					{
						return Index;
					}
				}
			}

			return INDEX_NONE;
		}

		int32 AddPlacement(const FTileKey& Key, const FIntPoint& Position, const FIntPoint& TileSize)
		{
			FTilePlacement Placement;
			Placement.Key = Key;
			Placement.Position = Position;
			Placement.Size = TileSize;

			const int32 Index = Placements.Add(Placement);
			KeyToPlacement.Add(Key, Index);
			return Index;
		}
	};

	struct FTileCacheKey
	{
		UTexture2D* Texture = nullptr;
		FIntPoint Size = FIntPoint::ZeroValue;
		bool bColor = true;

		friend bool operator==(const FTileCacheKey& A, const FTileCacheKey& B)
		{
			return A.Texture == B.Texture && A.Size == B.Size && A.bColor == B.bColor;
		}

		friend uint32 GetTypeHash(const FTileCacheKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.Texture), GetTypeHash(Key.Size)), GetTypeHash(Key.bColor));
		}
	};


	/********************************************************************************************************/
	/*                                     MATERIAL GRAPH HELPERS                                           */
	/********************************************************************************************************/

	/* Walks through reroute nodes (plain and named). */
	UMaterialExpression* SkipReroutes(UMaterialExpression* Expression)
	{
		for (int32 Guard = 0; Expression && Guard < 32; ++Guard)
		{
			if (UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(Expression))
			{
				Expression = Reroute->Input.Expression;
			}
			else if (UMaterialExpressionNamedRerouteUsage* Usage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
			{
				Expression = Usage->Declaration ? Usage->Declaration->Input.Expression : nullptr;
			}
			else
			{
				break;
			}
		}
		return Expression;
	}

	/* Depth-first search from a material input back to every texture sample
	 * that feeds it, passing through arithmetic nodes (tints, lerps, etc.). */
	void CollectTextureSamples(
		UMaterialExpression* Expression,
		TArray<UMaterialExpressionTextureSample*>& OutSamples,
		bool& bOutHitMaterialFunction,
		TSet<UMaterialExpression*>& Visited,
		int32 Depth = 0)
	{
		Expression = SkipReroutes(Expression);
		if (!Expression || Depth > 64)
		{
			return;
		}

		bool bAlreadyVisited = false;
		Visited.Add(Expression, &bAlreadyVisited);
		if (bAlreadyVisited)
		{
			return;
		}

		if (UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			OutSamples.Add(Sample);
			return;
		}

		if (Expression->IsA<UMaterialExpressionMaterialFunctionCall>())
		{
			bOutHitMaterialFunction = true;
			return;
		}

		for (int32 InputIndex = 0; InputIndex < 64; ++InputIndex)
		{
			FExpressionInput* Input = Expression->GetInput(InputIndex);
			if (!Input)
			{
				break;
			}
			CollectTextureSamples(Input->Expression, OutSamples, bOutHitMaterialFunction, Visited, Depth + 1);
		}
	}

	UMaterialExpressionTextureSample* FindFirstTextureSample(UMaterialExpression* Expression, bool& bOutHitMaterialFunction)
	{
		TArray<UMaterialExpressionTextureSample*> Samples;
		TSet<UMaterialExpression*> Visited;
		CollectTextureSamples(Expression, Samples, bOutHitMaterialFunction, Visited);
		return Samples.Num() > 0 ? Samples[0] : nullptr;
	}

	/* Resolves the texture a sample/object node reads, honouring material
	 * instance parameter overrides. */
	UTexture* ResolveTextureExpression(UMaterialExpression* Expression, UMaterialInterface* Material, int32 Depth = 0)
	{
		Expression = SkipReroutes(Expression);
		if (!Expression || Depth > 8)
		{
			return nullptr;
		}

		/* Covers TextureSampleParameter2D and TextureObjectParameter */
		if (UMaterialExpressionTextureSampleParameter* Parameter = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
		{
			UTexture* Override = nullptr;
			if (Material->GetTextureParameterValue(MakeParamInfo(Parameter->ParameterName), Override) && Override)
			{
				return Override;
			}
			return Parameter->Texture;
		}

		if (UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			if (Sample->TextureObject.Expression)
			{
				return ResolveTextureExpression(Sample->TextureObject.Expression, Material, Depth + 1);
			}
			return Sample->Texture;
		}

		if (UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression))
		{
			return TextureBase->Texture;
		}

		return nullptr;
	}

	bool ResolveConstantColour(UMaterialExpression* Expression, UMaterialInterface* Material, FLinearColor& OutColour)
	{
		Expression = SkipReroutes(Expression);

		if (const UMaterialExpressionConstant3Vector* Constant3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			OutColour = Constant3->Constant;
			return true;
		}
		if (const UMaterialExpressionConstant4Vector* Constant4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
		{
			OutColour = Constant4->Constant;
			return true;
		}
		if (const UMaterialExpressionVectorParameter* VectorParameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			OutColour = VectorParameter->DefaultValue;
			FLinearColor Override;
			if (Material->GetVectorParameterValue(MakeParamInfo(VectorParameter->ParameterName), Override))
			{
				OutColour = Override;
			}
			return true;
		}
		if (const UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			OutColour = FLinearColor(Constant->R, Constant->R, Constant->R, 1.0f);
			return true;
		}
		return false;
	}

	/* Scalar / float2 constant feeding one side of a Multiply. */
	bool ResolveConstantFactor(UMaterialExpression* Expression, UMaterialInterface* Material, float UnconnectedValue, FVector2f& OutFactor)
	{
		Expression = SkipReroutes(Expression);

		if (!Expression)
		{
			OutFactor = FVector2f(UnconnectedValue, UnconnectedValue);
			return true;
		}
		if (const UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			OutFactor = FVector2f(Constant->R, Constant->R);
			return true;
		}
		if (const UMaterialExpressionConstant2Vector* Constant2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
		{
			OutFactor = FVector2f(Constant2->R, Constant2->G);
			return true;
		}
		if (const UMaterialExpressionScalarParameter* ScalarParameter = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			float Value = ScalarParameter->DefaultValue;
			float Override = 0.0f;
			if (Material->GetScalarParameterValue(MakeParamInfo(ScalarParameter->ParameterName), Override))
			{
				Value = Override;
			}
			OutFactor = FVector2f(Value, Value);
			return true;
		}
		return false;
	}

	/* Understands:  TexCoord  |  TexCoord * constant  |  constant * TexCoord
	 * Anything else falls back to UV0 with no tiling, and reports false. */
	bool ResolveUVTransform(
		UMaterialExpressionTextureSample* Sample,
		UMaterialInterface* Material,
		int32& OutChannel,
		FVector2f& OutScale)
	{
		OutChannel = Sample->ConstCoordinate;
		OutScale = FVector2f(1.0f, 1.0f);

		UMaterialExpression* Coordinates = SkipReroutes(Sample->Coordinates.Expression);
		if (!Coordinates)
		{
			return true;
		}

		if (const UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(Coordinates))
		{
			OutChannel = TexCoord->CoordinateIndex;
			OutScale = FVector2f(TexCoord->UTiling, TexCoord->VTiling);
			return true;
		}

		if (UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Coordinates))
		{
			UMaterialExpression* A = SkipReroutes(Multiply->A.Expression);
			UMaterialExpression* B = SkipReroutes(Multiply->B.Expression);

			const UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(A);
			UMaterialExpression* Other = B;
			float OtherConstant = Multiply->ConstB;

			if (!TexCoord)
			{
				TexCoord = Cast<UMaterialExpressionTextureCoordinate>(B);
				Other = A;
				OtherConstant = Multiply->ConstA;
			}

			FVector2f Factor;
			if (TexCoord && ResolveConstantFactor(Other, Material, OtherConstant, Factor))
			{
				OutChannel = TexCoord->CoordinateIndex;
				OutScale = FVector2f(TexCoord->UTiling, TexCoord->VTiling) * Factor;
				return true;
			}
		}

		return false;
	}


	/********************************************************************************************************/
	/*                                        SOURCE MESH CACHE                                             */
	/********************************************************************************************************/

	bool BuildCachedMesh(UStaticMesh* StaticMesh, FCachedMesh& Out, FString& OutError)
	{
		if (!StaticMesh || !StaticMesh->IsSourceModelValid(0))
		{
			OutError = TEXT("no valid LOD0 source model");
			return false;
		}

		const FMeshDescription* Source = StaticMesh->GetMeshDescription(0);
		if (!Source || Source->Triangles().Num() == 0)
		{
			OutError = TEXT("LOD0 has no source mesh description or no triangles");
			return false;
		}

		/* Work on a compacted copy so element IDs are contiguous indices. */
		FMeshDescription Mesh = *Source;
		FElementIDRemappings Remappings;
		Mesh.Compact(Remappings);

		FStaticMeshAttributes Attributes(Mesh);

		/* Mirror what the mesh build does: if the build recomputes normals or
		 * tangents (or the source simply lacks them), compute them here too so
		 * normal maps shade exactly as they did on the source mesh. */
		const FMeshBuildSettings& BuildSettings = StaticMesh->GetSourceModel(0).BuildSettings;
		bool bComputeNormals = BuildSettings.bRecomputeNormals;
		bool bComputeTangents = BuildSettings.bRecomputeTangents;

		{
			const auto Normals = Attributes.GetVertexInstanceNormals();
			const auto Tangents = Attributes.GetVertexInstanceTangents();

			for (const FVertexInstanceID InstanceID : Mesh.VertexInstances().GetElementIDs())
			{
				bComputeNormals |= Normals[InstanceID].IsNearlyZero();
				bComputeTangents |= Tangents[InstanceID].IsNearlyZero();
				if (bComputeNormals && bComputeTangents)
				{
					break;
				}
			}
		}

		if (bComputeNormals || bComputeTangents)
		{
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals(Mesh);

			EComputeNTBsFlags Flags = EComputeNTBsFlags::BlendOverlappingNormals;
			if (bComputeNormals)
			{
				Flags |= EComputeNTBsFlags::Normals;
			}
			if (bComputeTangents)
			{
				Flags |= EComputeNTBsFlags::Tangents;
			}
			if (BuildSettings.bUseMikkTSpace)
			{
				Flags |= EComputeNTBsFlags::UseMikkTSpace;
			}
			if (BuildSettings.bComputeWeightedNormals)
			{
				Flags |= EComputeNTBsFlags::WeightedNTBs;
			}

			FStaticMeshOperations::ComputeTangentsAndNormals(Mesh, Flags);
		}

		const auto Positions = Attributes.GetVertexPositions();
		const auto Normals = Attributes.GetVertexInstanceNormals();
		const auto Tangents = Attributes.GetVertexInstanceTangents();
		const auto BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		const auto UVs = Attributes.GetVertexInstanceUVs();
		const auto SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

		const int32 NumVertices = Mesh.Vertices().Num();
		const int32 NumInstances = Mesh.VertexInstances().Num();
		const int32 NumSourceUVChannels = UVs.GetNumChannels();

		Out.NumUVChannels = FMath::Max(1, NumSourceUVChannels);

		Out.Positions.SetNumUninitialized(NumVertices);
		for (int32 Vertex = 0; Vertex < NumVertices; ++Vertex)
		{
			Out.Positions[Vertex] = Positions[FVertexID(Vertex)];
		}

		Out.InstanceToVertex.SetNumUninitialized(NumInstances);
		Out.Normals.SetNumUninitialized(NumInstances);
		Out.Tangents.SetNumUninitialized(NumInstances);
		Out.BinormalSigns.SetNumUninitialized(NumInstances);
		Out.UVs.SetNumZeroed(NumInstances * Out.NumUVChannels);

		for (int32 Instance = 0; Instance < NumInstances; ++Instance)
		{
			const FVertexInstanceID InstanceID(Instance);
			Out.InstanceToVertex[Instance] = Mesh.GetVertexInstanceVertex(InstanceID).GetValue();
			Out.Normals[Instance] = Normals[InstanceID];
			Out.Tangents[Instance] = Tangents[InstanceID];
			Out.BinormalSigns[Instance] = BinormalSigns[InstanceID];

			for (int32 Channel = 0; Channel < NumSourceUVChannels; ++Channel)
			{
				Out.UVs[Instance * Out.NumUVChannels + Channel] = UVs.Get(InstanceID, Channel);
			}
		}

		/* Polygon group -> static material slot, the same way the engine's
		 * merge helpers resolve it. */
		const int32 NumStaticMaterials = StaticMesh->GetStaticMaterials().Num();
		const FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();

		TArray<int32> GroupToMaterial;
		GroupToMaterial.SetNum(Mesh.PolygonGroups().Num());

		for (int32 Group = 0; Group < GroupToMaterial.Num(); ++Group)
		{
			const FPolygonGroupID GroupID(Group);
			int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(SlotNames[GroupID]);
			if (MaterialIndex == INDEX_NONE)
			{
				MaterialIndex = Group;
			}
			if (SectionInfoMap.IsValidSection(0, Group))
			{
				MaterialIndex = SectionInfoMap.Get(0, Group).MaterialIndex;
			}
			GroupToMaterial[Group] = FMath::Clamp(MaterialIndex, 0, FMath::Max(0, NumStaticMaterials - 1));
		}

		const int32 NumTriangles = Mesh.Triangles().Num();
		Out.TriangleInstances.SetNumUninitialized(NumTriangles * 3);
		Out.TriangleMaterial.SetNumUninitialized(NumTriangles);

		for (int32 Triangle = 0; Triangle < NumTriangles; ++Triangle)
		{
			const FTriangleID TriangleID(Triangle);
			TArrayView<const FVertexInstanceID> Corners = Mesh.GetTriangleVertexInstances(TriangleID);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Out.TriangleInstances[Triangle * 3 + Corner] = Corners[Corner].GetValue();
			}
			Out.TriangleMaterial[Triangle] = GroupToMaterial[Mesh.GetTrianglePolygonGroup(TriangleID).GetValue()];
		}

		Out.bValid = true;
		return true;
	}


	/********************************************************************************************************/
	/*                                          TEXTURE HELPERS                                             */
	/********************************************************************************************************/

	/* Reads mip 0 of the texture source, resizes it (in linear space) and
	 * returns BGRA8 pixels in the atlas' colour space. */
	bool MakeTilePixels(UTexture2D* Texture, const FIntPoint& Size, bool bColor, TArray<FColor>& OutPixels, FString& OutError)
	{
		if (!Texture || !Texture->Source.IsValid())
		{
			OutError = TEXT("texture has no source data");
			return false;
		}

		if (Texture->Source.GetNumBlocks() > 1)
		{
			UE_LOG(LogZeroPayAtlasReducer, Warning, TEXT("'%s' is a UDIM texture; only its first block is used."), *Texture->GetPathName());
		}

		FImage SourceImage;
		if (!Texture->Source.GetMipImage(SourceImage, 0, 0, 0))
		{
			OutError = TEXT("could not read texture source mip 0");
			return false;
		}

		if (ERawImageFormat::GetFormatNeedsGammaSpace(SourceImage.Format))
		{
			SourceImage.GammaSpace = Texture->SRGB
				? (Texture->bUseLegacyGamma ? EGammaSpace::Pow22 : EGammaSpace::sRGB)
				: EGammaSpace::Linear;
		}

		FImage LinearImage;
		SourceImage.CopyTo(LinearImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);

		FImage ResizedImage;
		if (LinearImage.SizeX == Size.X && LinearImage.SizeY == Size.Y)
		{
			ResizedImage = MoveTemp(LinearImage);
		}
		else
		{
			LinearImage.ResizeTo(ResizedImage, Size.X, Size.Y, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
		}

		FImage FinalImage;
		ResizedImage.CopyTo(FinalImage, ERawImageFormat::BGRA8, bColor ? EGammaSpace::sRGB : EGammaSpace::Linear);

		const TArrayView64<FColor> Pixels = FinalImage.AsBGRA8();
		const int32 NumPixels = Size.X * Size.Y;
		if (Pixels.Num() < NumPixels)
		{
			OutError = TEXT("resize produced an unexpected image size");
			return false;
		}

		OutPixels.SetNumUninitialized(NumPixels);
		FMemory::Memcpy(OutPixels.GetData(), Pixels.GetData(), sizeof(FColor) * NumPixels);

		/* The atlas does not carry the flag, so bake the flip in. */
		if (!bColor && Texture->bFlipGreenChannel)
		{
			for (FColor& Pixel : OutPixels)
			{
				Pixel.G = 255 - Pixel.G;
			}
		}

		return true;
	}

	void BlitTile(
		TArray<FColor>& AtlasPixels,
		int32 AtlasSize,
		const FTilePlacement& Placement,
		const TArray<FColor>* TilePixels,
		const FColor& Fill,
		bool bOverrideAlpha,
		uint8 Alpha)
	{
		for (int32 Y = 0; Y < Placement.Size.Y; ++Y)
		{
			FColor* Dest = &AtlasPixels[(Placement.Position.Y + Y) * AtlasSize + Placement.Position.X];

			if (TilePixels)
			{
				FMemory::Memcpy(Dest, &(*TilePixels)[Y * Placement.Size.X], sizeof(FColor) * Placement.Size.X);
			}
			else
			{
				for (int32 X = 0; X < Placement.Size.X; ++X)
				{
					Dest[X] = Fill;
				}
			}

			if (bOverrideAlpha)
			{
				for (int32 X = 0; X < Placement.Size.X; ++X)
				{
					Dest[X].A = Alpha;
				}
			}
		}
	}


	/********************************************************************************************************/
	/*                                        MASTER MATERIAL                                               */
	/********************************************************************************************************/

	/* Shared prelude for the three Custom nodes. Gradients are taken from the
	 * UNWRAPPED UV so frac() never produces a derivative spike at the seam,
	 * then clamped so the sampler never selects a mip smaller than the tile. */
	const TCHAR* ShaderPrelude = TEXT(R"(
float2 Gx = ddx(UV) * RectScale;
float2 Gy = ddy(UV) * RectScale;
float MaxLen = max(min(RectScale.x, RectScale.y), 1e-6);
float Len = max(max(length(Gx), length(Gy)), 1e-8);
float GradScale = min(1.0, MaxLen / Len);
)");

	/* Wraps the tiling UV into the tile, insetting by half a texel at the mip
	 * being sampled so bilinear filtering never reaches a neighbour. */
	const TCHAR* ShaderAtlasUV = TEXT(R"(
float TileTexels = MaxLen * AtlasSize;
float Texels = clamp(Len * GradScale * AtlasSize, 1.0, TileTexels);
float MipTexels = min(pow(2.0, ceil(log2(Texels))), TileTexels);
float2 Inset = (0.5 * MipTexels / AtlasSize).xx;
float2 Local = clamp(frac(UV) * RectScale, Inset, max(RectScale - Inset, Inset));
return RectOffset + Local;
)");

	const TCHAR* ShaderDDX = TEXT("return Gx * GradScale;");
	const TCHAR* ShaderDDY = TEXT("return Gy * GradScale;");

	bool BuildMasterMaterialGraph(UMaterial* Material, FString& OutError)
	{
		UTexture* DefaultColour = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		UTexture* DefaultNormal = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
		if (!DefaultColour || !DefaultNormal)
		{
			OutError = TEXT("engine default textures could not be loaded");
			return false;
		}

		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

		Material->Modify();
		Material->BlendMode = BLEND_Opaque;
		Material->TwoSided = false;
		Material->SetShadingModel(MSM_DefaultLit);
		Material->bUsedWithStaticLighting = true;
		/* Tiling UVs and log2/pow in the wrap code need full precision on mobile */
		Material->FloatPrecisionMode = EMaterialFloatPrecisionMode::MFPM_Full;

		auto Create = [Material](UClass* Class, int32 X, int32 Y)
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
		};

		auto MakeTexCoord = [&Create](int32 Index, int32 Y)
		{
			UMaterialExpressionTextureCoordinate* Node = Cast<UMaterialExpressionTextureCoordinate>(
				Create(UMaterialExpressionTextureCoordinate::StaticClass(), -1500, Y));
			Node->CoordinateIndex = Index;
			return Node;
		};

		auto MakeScalar = [&Create](FName Name, float Default, int32 X, int32 Y)
		{
			UMaterialExpressionScalarParameter* Node = Cast<UMaterialExpressionScalarParameter>(
				Create(UMaterialExpressionScalarParameter::StaticClass(), X, Y));
			Node->ParameterName = Name;
			Node->DefaultValue = Default;
			return Node;
		};

		UMaterialExpressionTextureCoordinate* TexCoordUV = MakeTexCoord(UVChannel_Texture, -300);
		UMaterialExpressionTextureCoordinate* TexCoordOffset = MakeTexCoord(UVChannel_RectOffset, -150);
		UMaterialExpressionTextureCoordinate* TexCoordScale = MakeTexCoord(UVChannel_RectScale, 0);
		UMaterialExpressionScalarParameter* AtlasSize = MakeScalar(ParamName_AtlasSize, 2048.0f, -1500, 150);

		auto MakeCustom = [&](const TCHAR* Description, const FString& Code, int32 Y)
		{
			UMaterialExpressionCustom* Node = Cast<UMaterialExpressionCustom>(
				Create(UMaterialExpressionCustom::StaticClass(), -1100, Y));
			Node->Description = Description;
			Node->Code = Code;
			Node->OutputType = ECustomMaterialOutputType::CMOT_Float2;
			Node->Inputs.Reset();

			auto AddInput = [Node](const TCHAR* Name, UMaterialExpression* Source)
			{
				FCustomInput CustomInput;
				CustomInput.InputName = FName(Name);
				CustomInput.Input.Connect(0, Source);
				Node->Inputs.Add(CustomInput);
			};

			AddInput(TEXT("UV"), TexCoordUV);
			AddInput(TEXT("RectOffset"), TexCoordOffset);
			AddInput(TEXT("RectScale"), TexCoordScale);
			AddInput(TEXT("AtlasSize"), AtlasSize);
			return Node;
		};

		UMaterialExpressionCustom* AtlasUV = MakeCustom(TEXT("AtlasUV"), FString(ShaderPrelude) + ShaderAtlasUV, -300);
		UMaterialExpressionCustom* AtlasDDX = MakeCustom(TEXT("AtlasDDX"), FString(ShaderPrelude) + ShaderDDX, -50);
		UMaterialExpressionCustom* AtlasDDY = MakeCustom(TEXT("AtlasDDY"), FString(ShaderPrelude) + ShaderDDY, 200);

		auto MakeSample = [&](FName Name, EMaterialSamplerType SamplerType, UTexture* Default, int32 Y)
		{
			UMaterialExpressionTextureSampleParameter2D* Node = Cast<UMaterialExpressionTextureSampleParameter2D>(
				Create(UMaterialExpressionTextureSampleParameter2D::StaticClass(), -700, Y));
			Node->ParameterName = Name;
			Node->SamplerType = SamplerType;
			Node->Texture = Default;
			Node->MipValueMode = TMVM_Derivative;
			Node->Coordinates.Connect(0, AtlasUV);
			Node->CoordinatesDX.Connect(0, AtlasDDX);
			Node->CoordinatesDY.Connect(0, AtlasDDY);
			return Node;
		};

		UMaterialExpressionTextureSampleParameter2D* BaseColour = MakeSample(ParamName_BaseColorAtlas, SAMPLERTYPE_Color, DefaultColour, -300);
		UMaterialExpressionTextureSampleParameter2D* Normal = MakeSample(ParamName_NormalAtlas, SAMPLERTYPE_Normal, DefaultNormal, 50);

		UMaterialExpressionScalarParameter* Roughness = MakeScalar(ParamName_Roughness, 0.8f, -700, 350);
		UMaterialExpressionScalarParameter* Metallic = MakeScalar(ParamName_Metallic, 0.0f, -700, 450);

		/* TextureSample outputs: 0 = RGB, 1 = R, 2 = G, 3 = B, 4 = A, 5 = RGBA */
		UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
		EditorData->BaseColor.Connect(0, BaseColour);
		EditorData->Opacity.Connect(4, BaseColour);
		EditorData->OpacityMask.Connect(4, BaseColour);
		EditorData->Normal.Connect(0, Normal);
		EditorData->Roughness.Connect(0, Roughness);
		EditorData->Metallic.Connect(0, Metallic);

		UMaterialEditingLibrary::RecompileMaterial(Material);
		return true;
	}


	/********************************************************************************************************/
	/*                                           IMPLEMENTATION                                             */
	/********************************************************************************************************/

	class FReducer
	{
	public:

		FReducer(const FInput& InInput, FReducerResults& InResults, FOutput& InOutput)
			: Input(InInput)
			, Settings(InInput.Settings)
			, Results(InResults)
			, Output(InOutput)
		{
			const FZeroPayEditor_AtlasReductionSettings& Atlas = Settings->AtlasReductionSettings;

			ChunkSize = FVector(
				FMath::Max(100.0, Atlas.ChunkSize.X),
				FMath::Max(100.0, Atlas.ChunkSize.Y),
				FMath::Max(100.0, Atlas.ChunkSize.Z));

			MaxAtlasSize = FMath::Clamp(static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1, Atlas.MaxAtlasSize)))), 256, 4096);
			MinTileSize = FMath::Clamp(static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1, Atlas.MinTileSize)))), 4, MaxAtlasSize);
			ShareRadius = FMath::Max(0, Atlas.AtlasShareRadius);
			TextureScale = FMath::Clamp(Atlas.TextureScalePercent, 1.0f, 100.0f) / 100.0;
			TranslucentAlpha = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Atlas.TranslucentFallbackOpacity * 255.0f), 0, 255));

			MeshFolder = Input.AssetRootPath / TEXT("Meshes");
			TextureFolder = Input.AssetRootPath / TEXT("Atlases");
			MaterialFolder = Input.AssetRootPath / TEXT("Materials");
		}

		bool Run()
		{
			enum
			{
				Work_Prepare = 2,
				Work_Partition = 2,
				Work_Layout = 1,
				Work_Master = 1,
				Work_Textures = 4,
				Work_Materials = 1,
				Work_Meshes = 9
			};
			constexpr float TotalWork = Work_Prepare + Work_Partition + Work_Layout + Work_Master
				+ Work_Textures + Work_Materials + Work_Meshes;

			FScopedSlowTask Task(TotalWork, FText::FromString(TEXT("Atlas reducer")));
			/* No MakeDialog - nests into the caller's dialog. */

			Task.EnterProgressFrame(Work_Prepare, FText::FromString(TEXT("Reading source meshes and materials...")));
			if (!PrepareComponents())
			{
				return false;
			}

			Task.EnterProgressFrame(Work_Partition, FText::FromString(TEXT("Assigning triangles to chunks...")));
			if (!PartitionTriangles())
			{
				return false;
			}

			if (Chunks.Num() == 0)
			{
				Results.Fail(TEXT("No triangles were assigned to any chunk - nothing to reduce."));
				return false;
			}

			Task.EnterProgressFrame(Work_Layout, FText::FromString(TEXT("Packing texture atlases...")));
			if (!LayoutAtlases())
			{
				return false;
			}

			PublishLayout();

			if (Input.bDryRun)
			{
				return true;
			}

			Task.EnterProgressFrame(Work_Master, FText::FromString(TEXT("Preparing master material...")));
			if (!EnsureMasterMaterial())
			{
				return false;
			}

			Task.EnterProgressFrame(Work_Textures, FText::FromString(TEXT("Composing atlas textures...")));
			if (!BuildAtlasTextures())
			{
				return false;
			}

			Task.EnterProgressFrame(Work_Materials, FText::FromString(TEXT("Creating material instances...")));
			CreateMaterialInstances();

			Task.EnterProgressFrame(Work_Meshes, FText::FromString(TEXT("Building chunk meshes...")));
			return BuildChunkMeshes();
		}

	private:

		/* ---------------------------------------------------------------- */
		/* Helpers                                                           */
		/* ---------------------------------------------------------------- */

		bool CheckCancel(const FScopedSlowTask& Task)
		{
			if (Task.ShouldCancel())
			{
				Results.bCancelled = true;
				Results.Fail(TEXT("Cancelled by user."));
				return true;
			}
			return false;
		}

		UPackage* CreateAssetPackage(const FString& Folder, const FString& AssetName)
		{
			UPackage* Package = CreatePackage(*(Folder / AssetName));
			Package->FullyLoad();
			Output.Packages.AddUnique(Package);
			return Package;
		}

		void FinishAsset(UObject* Asset)
		{
			Asset->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(Asset);
		}

		int32 ScaleDimension(int32 SourceDimension) const
		{
			const double Scaled = FMath::Max(1.0, SourceDimension * TextureScale);
			const int32 Up = static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::CeilToInt(Scaled))));
			const int32 Down = FMath::Max(1, Up >> 1);
			const int32 Nearest = (Scaled - Down) < (Up - Scaled) ? Down : Up;
			return FMath::Clamp(Nearest, MinTileSize, MaxAtlasSize);
		}

		FIntPoint GetScaledTextureSize(UTexture2D* Texture) const
		{
			if (!Texture || !Texture->Source.IsValid())
			{
				return FIntPoint(MinTileSize, MinTileSize);
			}
			return FIntPoint(
				ScaleDimension(Texture->Source.GetSizeX()),
				ScaleDimension(Texture->Source.GetSizeY()));
		}

		/* ---------------------------------------------------------------- */
		/* Material analysis                                                 */
		/* ---------------------------------------------------------------- */

		int32 GetMaterialInfoIndex(UMaterialInterface* Material)
		{
			if (const int32* Existing = MaterialLookup.Find(Material))
			{
				return *Existing;
			}

			const int32 Index = MaterialInfos.Add(AnalyseMaterial(Material));
			MaterialLookup.Add(Material, Index);
			return Index;
		}

		FMaterialInfo AnalyseMaterial(UMaterialInterface* Material)
		{
			FMaterialInfo Info;
			Info.TileSize = FIntPoint(MinTileSize, MinTileSize);

			if (!Material)
			{
				Info.Variant = MakeVariant(EBlendGroup::Opaque, false);
				return Info;
			}

			TArray<FString> Problems;

			/* ---- Blend mode / sidedness (instance overrides honoured) ---- */

			EBlendGroup Group = EBlendGroup::Opaque;
			const EBlendMode BlendMode = Material->GetBlendMode();
			switch (BlendMode)
			{
			case BLEND_Opaque:
				Group = EBlendGroup::Opaque;
				break;
			case BLEND_Masked:
				Group = EBlendGroup::Masked;
				break;
			case BLEND_Translucent:
				Group = EBlendGroup::Translucent;
				break;
			default:
				Group = EBlendGroup::Translucent;
				Problems.Add(FString::Printf(TEXT("blend mode %d is not supported and is treated as Translucent"), static_cast<int32>(BlendMode)));
				break;
			}

			Info.Variant = MakeVariant(Group, Material->IsTwoSided());
			Info.bOpaque = (Group == EBlendGroup::Opaque);

			/* ---- Locate the relevant inputs on the base material ---- */

			UMaterial* BaseMaterial = Material->GetMaterial();
			UMaterialEditorOnlyData* EditorData = BaseMaterial ? BaseMaterial->GetEditorOnlyData() : nullptr;
			if (!EditorData)
			{
				Problems.Add(TEXT("base material editor data is unavailable; white is used"));
				ReportMaterialProblems(Material, Problems);
				return Info;
			}

			FExpressionInput* BaseColorInput = &EditorData->BaseColor;
			FExpressionInput* NormalInput = &EditorData->Normal;
			FExpressionInput* OpacityInput = &EditorData->Opacity;
			FExpressionInput* OpacityMaskInput = &EditorData->OpacityMask;

			if (BaseMaterial->bUseMaterialAttributes)
			{
				UMaterialExpressionMakeMaterialAttributes* Make = Cast<UMaterialExpressionMakeMaterialAttributes>(
					SkipReroutes(EditorData->MaterialAttributes.Expression));
				if (Make)
				{
					BaseColorInput = &Make->BaseColor;
					NormalInput = &Make->Normal;
					OpacityInput = &Make->Opacity;
					OpacityMaskInput = &Make->OpacityMask;
				}
				else
				{
					Problems.Add(TEXT("uses material attributes without a Make Material Attributes node; white is used"));
					BaseColorInput = NormalInput = OpacityInput = OpacityMaskInput = nullptr;
				}
			}

			/* ---- Textures ---- */

			bool bHitFunction = false;
			UMaterialExpressionTextureSample* BaseSample = BaseColorInput ? FindFirstTextureSample(BaseColorInput->Expression, bHitFunction) : nullptr;
			UMaterialExpressionTextureSample* NormalSample = NormalInput ? FindFirstTextureSample(NormalInput->Expression, bHitFunction) : nullptr;

			if (bHitFunction && (!BaseSample || !NormalSample))
			{
				Problems.Add(TEXT("reads textures through a Material Function, which is not followed"));
			}

			auto AsTexture2D = [&Problems, Material](UTexture* Texture, const TCHAR* Slot) -> UTexture2D*
			{
				if (!Texture)
				{
					return nullptr;
				}
				UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
				if (!Texture2D)
				{
					Problems.Add(FString::Printf(TEXT("%s texture '%s' is not a Texture2D and is ignored"), Slot, *Texture->GetName()));
				}
				return Texture2D;
			};

			Info.Key.BaseColor = AsTexture2D(ResolveTextureExpression(BaseSample, Material), TEXT("BaseColor"));
			Info.Key.Normal = AsTexture2D(ResolveTextureExpression(NormalSample, Material), TEXT("Normal"));

			if (!Info.Key.BaseColor && BaseColorInput && BaseColorInput->Expression)
			{
				FLinearColor Constant;
				if (ResolveConstantColour(BaseColorInput->Expression, Material, Constant))
				{
					Info.Key.BaseColorConstant = Constant.ToFColor(/*bSRGB=*/true);
				}
				else
				{
					Problems.Add(TEXT("BaseColor is not driven by a texture or a constant; white is used"));
				}
			}

			if (Info.Key.BaseColor)
			{
				UsedTextures.Add(Info.Key.BaseColor);
			}
			if (Info.Key.Normal)
			{
				UsedTextures.Add(Info.Key.Normal);
			}

			/* ---- UV channel / tiling ---- */

			if (UMaterialExpressionTextureSample* UVSource = BaseSample ? BaseSample : NormalSample)
			{
				if (!ResolveUVTransform(UVSource, Material, Info.UVChannel, Info.UVScale))
				{
					Problems.Add(TEXT("texture coordinates use an unsupported node graph; UV0 without tiling is assumed"));
				}

				if (BaseSample && NormalSample)
				{
					int32 NormalChannel = 0;
					FVector2f NormalScale;
					if (ResolveUVTransform(NormalSample, Material, NormalChannel, NormalScale)
						&& (NormalChannel != Info.UVChannel || !NormalScale.Equals(Info.UVScale, 1e-4f)))
					{
						Problems.Add(TEXT("BaseColor and Normal use different UVs or tiling; the Normal map will use the BaseColor mapping"));
					}
				}
			}

			/* ---- Opacity ---- */

			if (Group != EBlendGroup::Opaque)
			{
				FExpressionInput* AlphaInput = (Group == EBlendGroup::Masked) ? OpacityMaskInput : OpacityInput;
				bool bAlphaFromBaseColour = false;

				if (AlphaInput && AlphaInput->Expression && Info.Key.BaseColor)
				{
					TArray<UMaterialExpressionTextureSample*> AlphaSamples;
					TSet<UMaterialExpression*> Visited;
					bool bIgnored = false;
					CollectTextureSamples(AlphaInput->Expression, AlphaSamples, bIgnored, Visited);

					for (UMaterialExpressionTextureSample* AlphaSample : AlphaSamples)
					{
						if (ResolveTextureExpression(AlphaSample, Material) == Info.Key.BaseColor)
						{
							bAlphaFromBaseColour = true;
							break;
						}
					}
				}

				if (!bAlphaFromBaseColour)
				{
					Info.Key.AlphaMode = ETileAlpha::Constant;
					Info.Key.AlphaValue = (Group == EBlendGroup::Translucent) ? TranslucentAlpha : 255;
				}
			}

			/* ---- Tile size: large enough for both textures ---- */

			const FIntPoint BaseSize = GetScaledTextureSize(Info.Key.BaseColor);
			const FIntPoint NormalSize = GetScaledTextureSize(Info.Key.Normal);
			Info.TileSize = FIntPoint(FMath::Max(BaseSize.X, NormalSize.X), FMath::Max(BaseSize.Y, NormalSize.Y));

			ReportMaterialProblems(Material, Problems);
			return Info;
		}

		void ReportMaterialProblems(UMaterialInterface* Material, const TArray<FString>& Problems)
		{
			if (Problems.Num() == 0)
			{
				return;
			}

			Results.UnsupportedMaterialCount++;
			Results.Warn(FString::Printf(TEXT("Material '%s': %s."),
				*Material->GetPathName(), *FString::Join(Problems, TEXT("; "))));
		}

		/* ---------------------------------------------------------------- */
		/* Gather                                                            */
		/* ---------------------------------------------------------------- */

		int32 GetCachedMeshIndex(UStaticMesh* StaticMesh)
		{
			if (const int32* Existing = MeshLookup.Find(StaticMesh))
			{
				return *Existing;
			}

			const int32 Index = Meshes.AddDefaulted();
			FString Error;
			if (!BuildCachedMesh(StaticMesh, Meshes[Index], Error))
			{
				Results.Warn(FString::Printf(TEXT("Static mesh '%s' was skipped: %s."), *GetPathNameSafe(StaticMesh), *Error));
			}

			MeshLookup.Add(StaticMesh, Index);
			return Index;
		}

		bool PrepareComponents()
		{
			FScopedSlowTask Task(static_cast<float>(Input.Components.Num()));

			for (UStaticMeshComponent* Component : Input.Components)
			{
				if (CheckCancel(Task))
				{
					return false;
				}
				Task.EnterProgressFrame(1.0f);

				if (!IsValid(Component) || !Component->GetStaticMesh())
				{
					continue;
				}

				UStaticMesh* StaticMesh = Component->GetStaticMesh();
				const int32 MeshIndex = GetCachedMeshIndex(StaticMesh);
				if (!Meshes[MeshIndex].bValid)
				{
					continue;
				}

				FComponentRecord Record;
				Record.Component = Component;
				Record.MeshIndex = MeshIndex;
				Record.Transform = Component->GetComponentTransform();
				Record.bMirrored = Record.Transform.GetDeterminant() < 0.0;

				const int32 NumSlots = FMath::Max(1, StaticMesh->GetStaticMaterials().Num());
				Record.SlotMaterialInfo.SetNum(NumSlots);
				for (int32 Slot = 0; Slot < NumSlots; ++Slot)
				{
					Record.SlotMaterialInfo[Slot] = GetMaterialInfoIndex(Component->GetMaterial(Slot));
				}

				Components.Add(MoveTemp(Record));
			}

			UE_LOG(LogZeroPayAtlasReducer, Log, TEXT("Prepared %d components, %d unique meshes, %d unique materials."),
				Components.Num(), Meshes.Num(), MaterialInfos.Num());

			return true;
		}

		/* ---------------------------------------------------------------- */
		/* Partition                                                         */
		/* ---------------------------------------------------------------- */

		int32 FindOrAddChunk(const FIntVector& Key)
		{
			if (const int32* Existing = ChunkLookup.Find(Key))
			{
				return *Existing;
			}

			FChunk Chunk;
			Chunk.Key = Key;
			const FVector Min = FVector(Key.X, Key.Y, Key.Z) * ChunkSize;
			Chunk.Bounds = FBox(Min, Min + ChunkSize);

			const int32 Index = Chunks.Add(MoveTemp(Chunk));
			ChunkLookup.Add(Key, Index);
			return Index;
		}

		bool PartitionTriangles()
		{
			const bool bClip = Input.ClipBounds.IsValid != 0;

			FScopedSlowTask Task(static_cast<float>(Components.Num()));

			TArray<FVector> WorldPositions;
			TMap<int32, int32> TrianglesPerChunk;

			for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
			{
				if (CheckCancel(Task))
				{
					return false;
				}
				Task.EnterProgressFrame(1.0f);

				FComponentRecord& Record = Components[ComponentIndex];
				const FCachedMesh& Mesh = Meshes[Record.MeshIndex];

				WorldPositions.SetNumUninitialized(Mesh.Positions.Num());
				for (int32 Vertex = 0; Vertex < Mesh.Positions.Num(); ++Vertex)
				{
					WorldPositions[Vertex] = Record.Transform.TransformPosition(FVector(Mesh.Positions[Vertex]));
				}

				TrianglesPerChunk.Reset();

				for (int32 Triangle = 0; Triangle < Mesh.NumTriangles(); ++Triangle)
				{
					const FVector& P0 = WorldPositions[Mesh.InstanceToVertex[Mesh.TriangleInstances[Triangle * 3]]];
					const FVector& P1 = WorldPositions[Mesh.InstanceToVertex[Mesh.TriangleInstances[Triangle * 3 + 1]]];
					const FVector& P2 = WorldPositions[Mesh.InstanceToVertex[Mesh.TriangleInstances[Triangle * 3 + 2]]];
					const FVector Centroid = (P0 + P1 + P2) / 3.0;

					if (bClip && !Input.ClipBounds.IsInsideOrOn(Centroid))
					{
						continue;
					}

					const FIntVector Key(
						FMath::FloorToInt32(Centroid.X / ChunkSize.X),
						FMath::FloorToInt32(Centroid.Y / ChunkSize.Y),
						FMath::FloorToInt32(Centroid.Z / ChunkSize.Z));

					const int32 ChunkIndex = FindOrAddChunk(Key);
					FChunk& Chunk = Chunks[ChunkIndex];

					FTriangleRef Ref;
					Ref.Component = ComponentIndex;
					Ref.Triangle = Triangle;
					Chunk.Triangles.Add(Ref);
					Chunk.MaterialInfos.Add(Record.GetMaterialInfo(Mesh.TriangleMaterial[Triangle]));

					TrianglesPerChunk.FindOrAdd(ChunkIndex)++;
				}

				int32 BestCount = 0;
				for (const TPair<int32, int32>& Pair : TrianglesPerChunk)
				{
					if (Pair.Value > BestCount)
					{
						BestCount = Pair.Value;
						Record.CollisionChunk = Pair.Key;
					}
				}

				if (Record.CollisionChunk != INDEX_NONE)
				{
					Chunks[Record.CollisionChunk].CollisionComponents.Add(ComponentIndex);
				}
			}

			UE_LOG(LogZeroPayAtlasReducer, Log, TEXT("Partitioned into %d chunks (chunk size %s)."),
				Chunks.Num(), *ChunkSize.ToCompactString());

			return true;
		}

		/* ---------------------------------------------------------------- */
		/* Atlas layout                                                      */
		/* ---------------------------------------------------------------- */

		int32 CreateAtlas(const FIntVector& OwnerChunk)
		{
			const int32 Index = Atlases.AddDefaulted();
			FAtlas& Atlas = Atlases[Index];
			Atlas.OwnerChunk = OwnerChunk;
			Atlas.Size = MaxAtlasSize;
			Atlas.Allocator.Init(MaxAtlasSize);
			return Index;
		}

		bool LayoutAtlases()
		{
			FIntVector MinKey = Chunks[0].Key;
			for (const FChunk& Chunk : Chunks)
			{
				MinKey.X = FMath::Min(MinKey.X, Chunk.Key.X);
				MinKey.Y = FMath::Min(MinKey.Y, Chunk.Key.Y);
				MinKey.Z = FMath::Min(MinKey.Z, Chunk.Key.Z);
			}

			ChunkOrder.Reset(Chunks.Num());
			for (int32 Index = 0; Index < Chunks.Num(); ++Index)
			{
				Chunks[Index].Morton = MortonCode(Chunks[Index].Key - MinKey);
				ChunkOrder.Add(Index);
			}

			ChunkOrder.Sort([this](int32 A, int32 B) { return Chunks[A].Morton < Chunks[B].Morton; });

			FScopedSlowTask Task(static_cast<float>(ChunkOrder.Num()));
			for (int32 ChunkIndex : ChunkOrder)
			{
				if (CheckCancel(Task))
				{
					return false;
				}
				Task.EnterProgressFrame(1.0f);
				AssignChunk(Chunks[ChunkIndex]);
			}

			int32 TileCount = 0;
			for (const FAtlas& Atlas : Atlases)
			{
				TileCount += Atlas.Placements.Num();
			}

			Results.AtlasCount = Atlases.Num();
			Results.AtlasTileCount = TileCount;
			Results.UniqueSourceTextureCount = UsedTextures.Num();
			Output.AtlasCount = Atlases.Num();

			UE_LOG(LogZeroPayAtlasReducer, Log, TEXT("%d chunks packed into %d atlases (%d tiles, %d unique source textures)."),
				Chunks.Num(), Atlases.Num(), TileCount, UsedTextures.Num());

			return true;
		}

		/*
		 * 1. Reuse any tile already present in a nearby atlas (no copying).
		 * 2. Fill nearby atlases with the remaining tiles until they are full.
		 * 3. Put whatever is left into a NEW atlas owned by this chunk,
		 *    downscaling if it cannot fit an empty atlas.
		 */
		void AssignChunk(FChunk& Chunk)
		{
			struct FRequest
			{
				FTileKey Key;
				FIntPoint Size = FIntPoint::ZeroValue;
				bool bAnyAlpha = true;
				TArray<int32, TInlineAllocator<4>> Users;
				FTileRef Tile;

				int32 BlockSize() const { return FMath::Max(Size.X, Size.Y); }
			};

			TArray<FRequest> Requests;
			{
				TMap<FTileKey, int32> RequestLookup;
				for (int32 InfoIndex : Chunk.MaterialInfos)
				{
					const FMaterialInfo& Info = MaterialInfos[InfoIndex];

					int32 RequestIndex;
					if (const int32* Found = RequestLookup.Find(Info.Key))
					{
						RequestIndex = *Found;
					}
					else
					{
						RequestIndex = Requests.AddDefaulted();
						Requests[RequestIndex].Key = Info.Key;
						RequestLookup.Add(Info.Key, RequestIndex);
					}

					FRequest& Request = Requests[RequestIndex];
					Request.Size.X = FMath::Max(Request.Size.X, Info.TileSize.X);
					Request.Size.Y = FMath::Max(Request.Size.Y, Info.TileSize.Y);
					Request.bAnyAlpha &= Info.bOpaque;
					Request.Users.Add(InfoIndex);
				}
			}

			/* ---- Candidate atlases within the share radius ---- */

			struct FCandidate
			{
				int32 Atlas = INDEX_NONE;
				int32 Hits = 0;
			};

			TArray<FCandidate> Candidates;
			for (int32 AtlasIndex = 0; AtlasIndex < Atlases.Num(); ++AtlasIndex)
			{
				const FIntVector Delta = Atlases[AtlasIndex].OwnerChunk - Chunk.Key;
				if (FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z)) > ShareRadius)
				{
					continue;
				}

				FCandidate Candidate;
				Candidate.Atlas = AtlasIndex;
				for (const FRequest& Request : Requests)
				{
					if (Atlases[AtlasIndex].Find(Request.Key, Request.bAnyAlpha) != INDEX_NONE)
					{
						++Candidate.Hits;
					}
				}
				Candidates.Add(Candidate);
			}

			/* Most shared tiles first (fewest sections), then newest */
			Candidates.Sort([](const FCandidate& A, const FCandidate& B)
			{
				return A.Hits != B.Hits ? A.Hits > B.Hits : A.Atlas > B.Atlas;
			});

			/* ---- 1. Reuse ---- */

			for (FRequest& Request : Requests)
			{
				for (const FCandidate& Candidate : Candidates)
				{
					const int32 Placement = Atlases[Candidate.Atlas].Find(Request.Key, Request.bAnyAlpha);
					if (Placement != INDEX_NONE)
					{
						Request.Tile.Atlas = Candidate.Atlas;
						Request.Tile.Placement = Placement;
						break;
					}
				}
			}

			TArray<int32> Pending;
			for (int32 Index = 0; Index < Requests.Num(); ++Index)
			{
				if (Requests[Index].Tile.Atlas == INDEX_NONE)
				{
					Pending.Add(Index);
				}
			}

			auto SortPending = [&Requests](TArray<int32>& List)
			{
				List.Sort([&Requests](int32 A, int32 B) { return Requests[A].BlockSize() > Requests[B].BlockSize(); });
			};

			/* ---- 2. Fill nearby atlases ---- */

			SortPending(Pending);
			for (const FCandidate& Candidate : Candidates)
			{
				if (Pending.Num() == 0)
				{
					break;
				}

				FAtlas& Atlas = Atlases[Candidate.Atlas];
				for (int32 Cursor = 0; Cursor < Pending.Num();)
				{
					FRequest& Request = Requests[Pending[Cursor]];
					FIntPoint Position;
					if (Atlas.Allocator.Allocate(Request.BlockSize(), Position))
					{
						Request.Tile.Atlas = Candidate.Atlas;
						Request.Tile.Placement = Atlas.AddPlacement(Request.Key, Position, Request.Size);
						Pending.RemoveAt(Cursor, EAllowShrinking::No);
					}
					else
					{
						++Cursor;
					}
				}
			}

			/* ---- 3. New atlas (with downscaling) ---- */

			TSet<int32> DownscaledRequests;
			int32 NewAtlasCount = 0;
			const int64 Capacity = static_cast<int64>(MaxAtlasSize) * MaxAtlasSize;

			while (Pending.Num() > 0)
			{
				const int32 NewAtlasIndex = CreateAtlas(Chunk.Key);
				++NewAtlasCount;

				/* Power-of-two blocks sorted largest first pack an empty buddy
				 * atlas perfectly, so a plain area test is exact here. */
				for (;;)
				{
					SortPending(Pending);

					int64 Area = 0;
					for (int32 RequestIndex : Pending)
					{
						const int64 Block = Requests[RequestIndex].BlockSize();
						Area += Block * Block;
					}

					if (Area <= Capacity)
					{
						break;
					}

					FRequest& Largest = Requests[Pending[0]];
					const int32 Block = Largest.BlockSize();
					if (Block <= MinTileSize)
					{
						/* Everything is already minimum size - spill into another atlas */
						break;
					}

					if (Largest.Size.X == Block)
					{
						Largest.Size.X = FMath::Max(MinTileSize, Block / 2);
					}
					if (Largest.Size.Y == Block)
					{
						Largest.Size.Y = FMath::Max(MinTileSize, Block / 2);
					}
					DownscaledRequests.Add(Pending[0]);
				}

				FAtlas& NewAtlas = Atlases[NewAtlasIndex];
				TArray<int32> Remaining;

				for (int32 RequestIndex : Pending)
				{
					FRequest& Request = Requests[RequestIndex];
					FIntPoint Position;
					if (NewAtlas.Allocator.Allocate(Request.BlockSize(), Position))
					{
						Request.Tile.Atlas = NewAtlasIndex;
						Request.Tile.Placement = NewAtlas.AddPlacement(Request.Key, Position, Request.Size);
					}
					else
					{
						Remaining.Add(RequestIndex);
					}
				}

				if (Remaining.Num() == Pending.Num())
				{
					/* Cannot happen with sane settings; guard against looping forever */
					Results.Warn(FString::Printf(TEXT("Chunk %s: %d texture(s) could not be placed in any atlas."),
						*Chunk.Key.ToString(), Remaining.Num()));
					break;
				}

				Pending = MoveTemp(Remaining);
			}

			if (DownscaledRequests.Num() > 0)
			{
				Results.DownscaledTileCount += DownscaledRequests.Num();
				Results.Warn(FString::Printf(
					TEXT("Chunk %s: %d texture(s) were downscaled so the chunk's new textures fit a %dx%d atlas. Lower TextureScalePercent, reduce ChunkSize or raise MaxAtlasSize to avoid this."),
					*Chunk.Key.ToString(), DownscaledRequests.Num(), MaxAtlasSize, MaxAtlasSize));
			}

			if (NewAtlasCount > 1)
			{
				Results.Warn(FString::Printf(
					TEXT("Chunk %s needed %d new atlases even at MinTileSize. Reduce ChunkSize."),
					*Chunk.Key.ToString(), NewAtlasCount));
			}

			/* ---- Record ---- */

			for (const FRequest& Request : Requests)
			{
				if (Request.Tile.Atlas == INDEX_NONE)
				{
					continue;
				}

				FAtlas& Atlas = Atlases[Request.Tile.Atlas];
				for (int32 InfoIndex : Request.Users)
				{
					Chunk.Assignment.Add(InfoIndex, Request.Tile);
					Atlas.UsedVariants |= static_cast<uint8>(1 << MaterialInfos[InfoIndex].Variant);
				}
				Chunk.AtlasIndices.AddUnique(Request.Tile.Atlas);
			}
		}

		void PublishLayout()
		{
			Output.Chunks.Reset(ChunkOrder.Num());
			OutputToChunk.Reset(ChunkOrder.Num());

			for (int32 ChunkIndex : ChunkOrder)
			{
				const FChunk& Chunk = Chunks[ChunkIndex];

				FChunkOutput& ChunkOutput = Output.Chunks.AddDefaulted_GetRef();
				ChunkOutput.Key = Chunk.Key;
				ChunkOutput.Bounds = Chunk.Bounds;
				ChunkOutput.Pivot = Chunk.Bounds.GetCenter();
				ChunkOutput.TriangleCount = Chunk.Triangles.Num();
				ChunkOutput.AtlasIndices = Chunk.AtlasIndices;

				OutputToChunk.Add(ChunkIndex);
			}
		}

		/* ---------------------------------------------------------------- */
		/* Master material                                                   */
		/* ---------------------------------------------------------------- */

		bool EnsureMasterMaterial()
		{
			const FZeroPayEditor_AtlasReductionSettings& AtlasSettings = Settings->AtlasReductionSettings;

			if (!AtlasSettings.MasterMaterialOverride.IsNull())
			{
				MasterMaterial = AtlasSettings.MasterMaterialOverride.LoadSynchronous();
				if (!MasterMaterial)
				{
					Results.Fail(FString::Printf(TEXT("MasterMaterialOverride '%s' could not be loaded."),
						*AtlasSettings.MasterMaterialOverride.ToString()));
					return false;
				}
				return true;
			}

			const FString PackageName = Input.MasterMaterialPackage;
			const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;

			UMaterial* Material = FindObject<UMaterial>(nullptr, *ObjectPath);
			if (!Material && FPackageName::DoesPackageExist(PackageName))
			{
				Material = LoadObject<UMaterial>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			}

			if (Material && !AtlasSettings.bRegenerateMasterMaterial)
			{
				MasterMaterial = Material;
				return true;
			}

			const bool bCreated = (Material == nullptr);
			UPackage* Package = nullptr;

			if (bCreated)
			{
				Package = CreatePackage(*PackageName);
				Package->FullyLoad();
				Material = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			}
			else
			{
				Package = Material->GetOutermost();
			}

			Output.Packages.AddUnique(Package);

			FString Error;
			if (!BuildMasterMaterialGraph(Material, Error))
			{
				Results.Fail(FString::Printf(TEXT("Could not build the master material '%s': %s"), *ObjectPath, *Error));
				return false;
			}

			if (bCreated)
			{
				FAssetRegistryModule::AssetCreated(Material);
			}
			Material->MarkPackageDirty();

			UE_LOG(LogZeroPayAtlasReducer, Log, TEXT("%s master material '%s'."),
				bCreated ? TEXT("Created") : TEXT("Regenerated"), *ObjectPath);

			MasterMaterial = Material;
			return true;
		}

		/* ---------------------------------------------------------------- */
		/* Atlas textures                                                    */
		/* ---------------------------------------------------------------- */

		/* Pointer is only valid until the next call. */
		const TArray<FColor>* GetTexturePixels(UTexture2D* Texture, const FIntPoint& Size, bool bColor)
		{
			if (!Texture || FailedTextures.Contains(Texture))
			{
				return nullptr;
			}

			FTileCacheKey Key;
			Key.Texture = Texture;
			Key.Size = Size;
			Key.bColor = bColor;

			if (const TArray<FColor>* Cached = TileCache.Find(Key))
			{
				return Cached;
			}

			TArray<FColor> Pixels;
			FString Error;
			if (!MakeTilePixels(Texture, Size, bColor, Pixels, Error))
			{
				FailedTextures.Add(Texture);
				Results.Warn(FString::Printf(TEXT("Texture '%s' could not be read (%s); a flat default is used."),
					*Texture->GetPathName(), *Error));
				return nullptr;
			}

			const int64 Bytes = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
			if (TileCacheBytes + Bytes > TileCacheBudgetBytes)
			{
				TileCache.Reset();
				TileCacheBytes = 0;
			}
			TileCacheBytes += Bytes;

			return &TileCache.Add(Key, MoveTemp(Pixels));
		}

		UTexture2D* CreateAtlasTexture(const FString& AssetName, const TArray<FColor>& Pixels, int32 Size, bool bNormal)
		{
			UPackage* Package = CreateAssetPackage(TextureFolder, AssetName);
			UTexture2D* Texture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			if (!Texture)
			{
				return nullptr;
			}

			Texture->Source.Init(Size, Size, /*NumSlices=*/1, /*NumMips=*/1, TSF_BGRA8,
				static_cast<const uint8*>(static_cast<const void*>(Pixels.GetData())));

			Texture->SRGB = !bNormal;
			Texture->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
			Texture->CompressionNoAlpha = bNormal;
			Texture->LODGroup = bNormal ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World;

			/* A 2x2 box filter keeps power-of-two aligned tiles isolated at every mip */
			Texture->MipGenSettings = TMGS_SimpleAverage;

			/* Wrapping is done in the shader. Clamp stops the atlas edges bleeding. */
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;

			Texture->PostEditChange();
			FinishAsset(Texture);
			return Texture;
		}

		bool BuildAtlasTextures()
		{
			static const FColor FlatNormal(128, 128, 255, 255);

			FScopedSlowTask Task(static_cast<float>(Atlases.Num()));
			TArray<UTexture*> CreatedTextures;

			for (int32 AtlasIndex = 0; AtlasIndex < Atlases.Num(); ++AtlasIndex)
			{
				if (CheckCancel(Task))
				{
					return false;
				}
				Task.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(TEXT("Atlas %d of %d"), AtlasIndex + 1, Atlases.Num())));

				FAtlas& Atlas = Atlases[AtlasIndex];
				const int32 Size = Atlas.Size;

				TArray<FColor> BasePixels;
				BasePixels.Init(FColor(128, 128, 128, 255), Size * Size);

				TArray<FColor> NormalPixels;
				NormalPixels.Init(FlatNormal, Size * Size);

				for (const FTilePlacement& Placement : Atlas.Placements)
				{
					const bool bOverrideAlpha = Placement.Key.AlphaMode == ETileAlpha::Constant;

					/* Fetch and blit one at a time - cache pointers do not survive the next fetch */
					const TArray<FColor>* BaseTile = GetTexturePixels(Placement.Key.BaseColor, Placement.Size, /*bColor=*/true);
					const FColor BaseFill = Placement.Key.BaseColor ? FColor::White : Placement.Key.BaseColorConstant;
					BlitTile(BasePixels, Size, Placement, BaseTile, BaseFill, bOverrideAlpha, Placement.Key.AlphaValue);

					const TArray<FColor>* NormalTile = GetTexturePixels(Placement.Key.Normal, Placement.Size, /*bColor=*/false);
					BlitTile(NormalPixels, Size, Placement, NormalTile, FlatNormal, false, 255);
				}

				Atlas.BaseColorTexture = CreateAtlasTexture(
					FString::Printf(TEXT("T_Atlas_%03d_BaseColor"), AtlasIndex), BasePixels, Size, /*bNormal=*/false);
				Atlas.NormalTexture = CreateAtlasTexture(
					FString::Printf(TEXT("T_Atlas_%03d_Normal"), AtlasIndex), NormalPixels, Size, /*bNormal=*/true);

				if (!Atlas.BaseColorTexture || !Atlas.NormalTexture)
				{
					Results.Fail(FString::Printf(TEXT("Failed to create the textures for atlas %d."), AtlasIndex));
					return false;
				}

				CreatedTextures.Add(Atlas.BaseColorTexture);
				CreatedTextures.Add(Atlas.NormalTexture);
			}

			TileCache.Empty();
			TileCacheBytes = 0;

			/* Textures compile asynchronously; make sure they are done before
			 * anything is saved. */
			FTextureCompilingManager::Get().FinishCompilation(CreatedTextures);
			return true;
		}

		/* ---------------------------------------------------------------- */
		/* Material instances                                                */
		/* ---------------------------------------------------------------- */

		void CreateMaterialInstances()
		{
			const FZeroPayEditor_AtlasReductionSettings& AtlasSettings = Settings->AtlasReductionSettings;

			for (int32 AtlasIndex = 0; AtlasIndex < Atlases.Num(); ++AtlasIndex)
			{
				FAtlas& Atlas = Atlases[AtlasIndex];

				for (int32 Variant = 0; Variant < NumVariants; ++Variant)
				{
					if ((Atlas.UsedVariants & (1 << Variant)) == 0)
					{
						continue;
					}

					const FString AssetName = FString::Printf(TEXT("MI_Atlas_%03d_%s"), AtlasIndex, GetVariantName(Variant));
					UPackage* Package = CreateAssetPackage(MaterialFolder, AssetName);

					UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
						Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

					Instance->SetParentEditorOnly(MasterMaterial);
					Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParamName_BaseColorAtlas), Atlas.BaseColorTexture);
					Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParamName_NormalAtlas), Atlas.NormalTexture);
					Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamName_AtlasSize), static_cast<float>(Atlas.Size));
					Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamName_Roughness), AtlasSettings.Roughness);
					Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParamName_Metallic), AtlasSettings.Metallic);

					/* Only override what differs from the (opaque, one-sided)
					 * master so the plain variant shares the master's shaders. */
					FMaterialInstanceBasePropertyOverrides& Overrides = Instance->BasePropertyOverrides;
					const EBlendGroup Group = GetVariantBlendGroup(Variant);
					if (Group != EBlendGroup::Opaque)
					{
						Overrides.bOverride_BlendMode = true;
						Overrides.BlendMode = (Group == EBlendGroup::Masked) ? BLEND_Masked : BLEND_Translucent;
					}
					if (IsVariantTwoSided(Variant))
					{
						Overrides.bOverride_TwoSided = true;
						Overrides.TwoSided = true;
					}

					Instance->PostEditChange();
					FinishAsset(Instance);

					Atlas.VariantMaterials[Variant] = Instance;
				}
			}
		}

		/* ---------------------------------------------------------------- */
		/* Chunk meshes                                                      */
		/* ---------------------------------------------------------------- */

		int32 ComputeLightmapResolution(double SurfaceAreaCm2) const
		{
			const float TexelsPerMeter = Settings->AtlasReductionSettings.LightmapTexelsPerMeter;
			int32 Resolution = Settings->MeshReductionSettings.MergedLightmapResolution;

			if (TexelsPerMeter > 0.0f)
			{
				/* Assume ~50% of the lightmap is usable after chart packing */
				const double AreaM2 = SurfaceAreaCm2 / 10000.0;
				Resolution = FMath::CeilToInt(FMath::Sqrt(AreaM2 / 0.5) * TexelsPerMeter);
			}

			Resolution = static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1, Resolution))));
			return FMath::Clamp(Resolution, MinLightmapResolution, MaxLightmapResolution);
		}

		UStaticMesh* CreateChunkMesh(const FChunk& Chunk, const FVector& Pivot)
		{
			FMeshDescription MeshDescription;
			FStaticMeshAttributes Attributes(MeshDescription);
			Attributes.Register();

			auto Positions = Attributes.GetVertexPositions();
			auto Normals = Attributes.GetVertexInstanceNormals();
			auto Tangents = Attributes.GetVertexInstanceTangents();
			auto BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
			auto UVs = Attributes.GetVertexInstanceUVs();
			auto SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

			UVs.SetNumChannels(NumOutputUVChannels);

			const int32 MaxCorners = Chunk.Triangles.Num() * 3;
			MeshDescription.ReserveNewVertices(MaxCorners);
			MeshDescription.ReserveNewVertexInstances(MaxCorners);
			MeshDescription.ReserveNewTriangles(Chunk.Triangles.Num());
			MeshDescription.ReserveNewPolygons(Chunk.Triangles.Num());
			MeshDescription.ReserveNewEdges(MaxCorners);

			/* One section per (atlas, variant) */
			TMap<int32, FPolygonGroupID> SectionLookup;
			TArray<int32> SectionOrder;

			/* Per-component remaps keep the source topology (welded vertices),
			 * which the lightmap UV generator relies on. */
			int32 CurrentComponent = INDEX_NONE;
			const FComponentRecord* Record = nullptr;
			const FCachedMesh* Mesh = nullptr;
			TArray<int32> VertexRemap;
			TArray<int32> InstanceRemap;
			TArray<int32> InstanceRemapInfo;
			FQuat Rotation = FQuat::Identity;
			FVector Scale = FVector::OneVector;
			FVector InverseScale = FVector::OneVector;

			double SurfaceArea = 0.0;

			for (const FTriangleRef& Ref : Chunk.Triangles)
			{
				if (Ref.Component != CurrentComponent)
				{
					CurrentComponent = Ref.Component;
					Record = &Components[CurrentComponent];
					Mesh = &Meshes[Record->MeshIndex];

					VertexRemap.Init(INDEX_NONE, Mesh->Positions.Num());
					InstanceRemap.Init(INDEX_NONE, Mesh->InstanceToVertex.Num());
					InstanceRemapInfo.Init(INDEX_NONE, Mesh->InstanceToVertex.Num());

					Rotation = Record->Transform.GetRotation();
					Scale = Record->Transform.GetScale3D();
					InverseScale = FTransform::GetSafeScaleReciprocal(Scale);
				}

				const int32 InfoIndex = Record->GetMaterialInfo(Mesh->TriangleMaterial[Ref.Triangle]);
				const FTileRef* Tile = Chunk.Assignment.Find(InfoIndex);
				if (!Tile)
				{
					continue;
				}

				const FMaterialInfo& Info = MaterialInfos[InfoIndex];
				const FAtlas& Atlas = Atlases[Tile->Atlas];
				const FTilePlacement& Placement = Atlas.Placements[Tile->Placement];

				const float InvAtlasSize = 1.0f / static_cast<float>(Atlas.Size);
				const FVector2f RectOffset(Placement.Position.X * InvAtlasSize, Placement.Position.Y * InvAtlasSize);
				const FVector2f RectScale(Placement.Size.X * InvAtlasSize, Placement.Size.Y * InvAtlasSize);

				/* Section */
				const int32 SectionKey = Tile->Atlas * NumVariants + Info.Variant;
				FPolygonGroupID GroupID;
				if (const FPolygonGroupID* Existing = SectionLookup.Find(SectionKey))
				{
					GroupID = *Existing;
				}
				else
				{
					GroupID = MeshDescription.CreatePolygonGroup();
					SlotNames[GroupID] = MakeSlotName(Tile->Atlas, Info.Variant);
					SectionLookup.Add(SectionKey, GroupID);
					SectionOrder.Add(SectionKey);
				}

				/* Mirrored transforms flip winding */
				static const int32 NormalOrder[3] = { 0, 1, 2 };
				static const int32 MirroredOrder[3] = { 0, 2, 1 };
				const int32* Order = Record->bMirrored ? MirroredOrder : NormalOrder;

				FVertexInstanceID Corners[3];

				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 SourceInstance = Mesh->TriangleInstances[Ref.Triangle * 3 + Order[Corner]];

					if (InstanceRemap[SourceInstance] != INDEX_NONE && InstanceRemapInfo[SourceInstance] == InfoIndex)
					{
						Corners[Corner] = FVertexInstanceID(InstanceRemap[SourceInstance]);
						continue;
					}

					const int32 SourceVertex = Mesh->InstanceToVertex[SourceInstance];
					int32& NewVertex = VertexRemap[SourceVertex];
					if (NewVertex == INDEX_NONE)
					{
						const FVertexID VertexID = MeshDescription.CreateVertex();
						const FVector World = Record->Transform.TransformPosition(FVector(Mesh->Positions[SourceVertex]));
						Positions[VertexID] = FVector3f(World - Pivot);
						NewVertex = VertexID.GetValue();
					}

					const FVertexInstanceID InstanceID = MeshDescription.CreateVertexInstance(FVertexID(NewVertex));

					const FVector SourceNormal(Mesh->Normals[SourceInstance]);
					const FVector SourceTangent(Mesh->Tangents[SourceInstance]);

					Normals[InstanceID] = FVector3f(Rotation.RotateVector(SourceNormal * InverseScale).GetSafeNormal());
					Tangents[InstanceID] = FVector3f(Rotation.RotateVector(SourceTangent * Scale).GetSafeNormal());
					BinormalSigns[InstanceID] = Mesh->BinormalSigns[SourceInstance] * (Record->bMirrored ? -1.0f : 1.0f);

					UVs.Set(InstanceID, UVChannel_Texture, Mesh->GetUV(SourceInstance, Info.UVChannel) * Info.UVScale);
					UVs.Set(InstanceID, UVChannel_Lightmap, FVector2f::ZeroVector);
					UVs.Set(InstanceID, UVChannel_RectOffset, RectOffset);
					UVs.Set(InstanceID, UVChannel_RectScale, RectScale);

					InstanceRemap[SourceInstance] = InstanceID.GetValue();
					InstanceRemapInfo[SourceInstance] = InfoIndex;
					Corners[Corner] = InstanceID;
				}

				MeshDescription.CreateTriangle(GroupID, MakeArrayView(Corners, 3));

				const FVector P0(Positions[MeshDescription.GetVertexInstanceVertex(Corners[0])]);
				const FVector P1(Positions[MeshDescription.GetVertexInstanceVertex(Corners[1])]);
				const FVector P2(Positions[MeshDescription.GetVertexInstanceVertex(Corners[2])]);
				SurfaceArea += 0.5 * FVector::CrossProduct(P1 - P0, P2 - P0).Size();
			}

			if (MeshDescription.Triangles().Num() == 0)
			{
				return nullptr;
			}

			/* ---- Asset ---- */

			const FString AssetName = FString::Printf(TEXT("SM_Chunk_%s_%s_%s"),
				*FormatCoordinate(Chunk.Key.X), *FormatCoordinate(Chunk.Key.Y), *FormatCoordinate(Chunk.Key.Z));

			UPackage* Package = CreateAssetPackage(MeshFolder, AssetName);
			UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

			const int32 LightmapResolution = ComputeLightmapResolution(SurfaceArea);

			StaticMesh->SetNumSourceModels(1);
			FMeshBuildSettings& Build = StaticMesh->GetSourceModel(0).BuildSettings;
			Build.bRecomputeNormals = false;
			Build.bRecomputeTangents = false;
			Build.bUseMikkTSpace = true;
			Build.bRemoveDegenerates = true;
			Build.bUseFullPrecisionUVs = true;   /* tiling UVs and atlas rects need it */
			Build.bUseHighPrecisionTangentBasis = false;
			Build.bGenerateLightmapUVs = Settings->MeshReductionSettings.bPreserveLightmapUVs;
			Build.SrcLightmapIndex = UVChannel_Texture;
			Build.DstLightmapIndex = UVChannel_Lightmap;
			Build.MinLightmapResolution = LightmapResolution;
			Build.DistanceFieldResolutionScale = 0.0f;

			for (int32 SectionKey : SectionOrder)
			{
				const int32 AtlasIndex = SectionKey / NumVariants;
				const int32 Variant = SectionKey % NumVariants;
				const FName SlotName = MakeSlotName(AtlasIndex, Variant);
				StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Atlases[AtlasIndex].VariantMaterials[Variant], SlotName, SlotName));
			}

			FMeshDescription* TargetDescription = StaticMesh->CreateMeshDescription(0);
			*TargetDescription = MoveTemp(MeshDescription);
			StaticMesh->CommitMeshDescription(0);

			StaticMesh->SetLightMapCoordinateIndex(UVChannel_Lightmap);
			StaticMesh->SetLightMapResolution(LightmapResolution);

			return StaticMesh;
		}

		static FName MakeSlotName(int32 AtlasIndex, int32 Variant)
		{
			return FName(*FString::Printf(TEXT("Atlas%03d_%s"), AtlasIndex, GetVariantName(Variant)));
		}

		bool BuildChunkMeshes()
		{
			const int32 BatchSize = FMath::Max(1, Settings->AtlasReductionSettings.MeshBuildBatchSize);

			FScopedSlowTask Task(static_cast<float>(Output.Chunks.Num()));

			TArray<UStaticMesh*> Batch;
			TArray<int32> BatchOutputs;

			auto FlushBatch = [&]()
			{
				if (Batch.Num() == 0)
				{
					return;
				}

				UStaticMesh::BatchBuild(Batch, /*bInSilent=*/true);
				FStaticMeshCompilingManager::Get().FinishCompilation(Batch);

				for (int32 Index = 0; Index < Batch.Num(); ++Index)
				{
					UStaticMesh* StaticMesh = Batch[Index];
					FChunkOutput& ChunkOutput = Output.Chunks[BatchOutputs[Index]];
					const FChunk& Chunk = Chunks[OutputToChunk[BatchOutputs[Index]]];

					if (Settings->MeshReductionSettings.bPreserveCollision)
					{
						ApplyCollision(StaticMesh, Chunk, ChunkOutput.Pivot);
					}

					FinishAsset(StaticMesh);
					ChunkOutput.Mesh = StaticMesh;
				}

				Batch.Reset();
				BatchOutputs.Reset();
			};

			for (int32 OutputIndex = 0; OutputIndex < Output.Chunks.Num(); ++OutputIndex)
			{
				if (CheckCancel(Task))
				{
					return false;
				}

				FChunkOutput& ChunkOutput = Output.Chunks[OutputIndex];
				Task.EnterProgressFrame(1.0f, FText::FromString(FString::Printf(
					TEXT("Chunk %d of %d (%d triangles)"), OutputIndex + 1, Output.Chunks.Num(), ChunkOutput.TriangleCount)));

				UStaticMesh* StaticMesh = CreateChunkMesh(Chunks[OutputToChunk[OutputIndex]], ChunkOutput.Pivot);
				if (!StaticMesh)
				{
					Results.Warn(FString::Printf(TEXT("Chunk %s produced no geometry."), *ChunkOutput.Key.ToString()));
					continue;
				}

				Batch.Add(StaticMesh);
				BatchOutputs.Add(OutputIndex);

				if (Batch.Num() >= BatchSize)
				{
					FlushBatch();
				}
			}

			FlushBatch();
			return true;
		}

		/* ---------------------------------------------------------------- */
		/* Collision                                                         */
		/* ---------------------------------------------------------------- */

		/* Port of MergeCollisionFromComponents, but expressed relative to the
		 * chunk pivot (the actor is spawned there, not at the world origin). */
		void ApplyCollision(UStaticMesh* StaticMesh, const FChunk& Chunk, const FVector& Pivot)
		{
			const FZeroPayEditor_PhysicsReductionSettings& Physics = Settings->PhysicsReductionSettings;

			StaticMesh->CreateBodySetup();
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
			if (!BodySetup)
			{
				return;
			}

			BodySetup->Modify();
			BodySetup->AggGeom = FKAggregateGeom();

			if (Physics.bUseComplexAsSimpleCollision)
			{
				BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();
				return;
			}

			int32 PrimitiveCount = 0;
			bool bAnyComplex = false;
			bool bWarnedNonUniform = false;

			for (int32 ComponentIndex : Chunk.CollisionComponents)
			{
				const FComponentRecord& Record = Components[ComponentIndex];
				UStaticMeshComponent* Component = Record.Component.Get();

				if (!Component || !Component->GetStaticMesh()
					|| Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
				{
					continue;
				}

				const UBodySetup* SourceBodySetup = Component->GetStaticMesh()->GetBodySetup();
				if (!SourceBodySetup)
				{
					continue;
				}

				const FKAggregateGeom& Source = SourceBodySetup->AggGeom;

				FTransform ToChunk = Record.Transform;
				ToChunk.AddToTranslation(-Pivot);

				const FVector Scale = ToChunk.GetScale3D();
				const FQuat Rotation = ToChunk.GetRotation();
				const float MaxScale = static_cast<float>(Scale.GetAbs().GetMax());

				const bool bUniformScale =
					FMath::IsNearlyEqual(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), 0.01)
					&& FMath::IsNearlyEqual(FMath::Abs(Scale.Y), FMath::Abs(Scale.Z), 0.01);

				if (!bUniformScale && !bWarnedNonUniform
					&& (Source.SphereElems.Num() > 0 || Source.SphylElems.Num() > 0))
				{
					bWarnedNonUniform = true;
					Results.Warn(FString::Printf(
						TEXT("'%s' has non-uniform scale %s and sphere/capsule collision. Radii use the largest axis."),
						*Component->GetOwner()->GetActorLabel(), *Scale.ToCompactString()));
				}

				for (const FKSphereElem& Sphere : Source.SphereElems)
				{
					FKSphereElem NewSphere = Sphere;
					NewSphere.Center = ToChunk.TransformPosition(Sphere.Center);
					NewSphere.Radius *= MaxScale;
					BodySetup->AggGeom.SphereElems.Add(NewSphere);
					++PrimitiveCount;
				}

				for (const FKSphylElem& Sphyl : Source.SphylElems)
				{
					FKSphylElem NewSphyl = Sphyl;
					NewSphyl.Center = ToChunk.TransformPosition(Sphyl.Center);
					NewSphyl.Rotation = (Rotation * Sphyl.Rotation.Quaternion()).Rotator();
					NewSphyl.Radius *= MaxScale;
					NewSphyl.Length *= MaxScale;
					BodySetup->AggGeom.SphylElems.Add(NewSphyl);
					++PrimitiveCount;
				}

				for (const FKConvexElem& Convex : Source.ConvexElems)
				{
					FKConvexElem NewConvex;
					NewConvex.VertexData.Reserve(Convex.VertexData.Num());
					for (const FVector& Vertex : Convex.VertexData)
					{
						NewConvex.VertexData.Add(ToChunk.TransformPosition(Vertex));
					}
					NewConvex.UpdateElemBox();
					BodySetup->AggGeom.ConvexElems.Add(NewConvex);
					++PrimitiveCount;
				}

				for (const FKBoxElem& Box : Source.BoxElems)
				{
					FKBoxElem NewBox = Box;
					NewBox.Center = ToChunk.TransformPosition(Box.Center);
					NewBox.Rotation = (Rotation * Box.Rotation.Quaternion()).Rotator();
					NewBox.X *= static_cast<float>(FMath::Abs(Scale.X));
					NewBox.Y *= static_cast<float>(FMath::Abs(Scale.Y));
					NewBox.Z *= static_cast<float>(FMath::Abs(Scale.Z));
					BodySetup->AggGeom.BoxElems.Add(NewBox);
					++PrimitiveCount;
				}

				if (SourceBodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
				{
					bAnyComplex = true;
				}
			}

			const int32 MaxPrimitives = FMath::Max(0, Physics.MaxCollisionPrimitiveCount);
			if (MaxPrimitives > 0 && PrimitiveCount > MaxPrimitives)
			{
				Results.Warn(FString::Printf(TEXT("Chunk mesh '%s' has %d collision primitives (budget is %d)."),
					*StaticMesh->GetName(), PrimitiveCount, MaxPrimitives));
			}

			if (bAnyComplex)
			{
				Results.Warn(FString::Printf(
					TEXT("Chunk mesh '%s' contains source meshes that used complex-as-simple collision; they have no simple collision in the output. Verify gameplay collision in this area."),
					*StaticMesh->GetName()));
			}

			BodySetup->CollisionTraceFlag = Physics.bUseSimpleCollisionWherePossible ? CTF_UseSimpleAsComplex : CTF_UseDefault;
			BodySetup->InvalidatePhysicsData();
			BodySetup->CreatePhysicsMeshes();
		}

	private:

		const FInput& Input;
		const UZeroPayEditor_ReducerSettingsAsset* Settings = nullptr;
		FReducerResults& Results;
		FOutput& Output;

		FVector ChunkSize = FVector(2000.0);
		int32 MaxAtlasSize = 2048;
		int32 MinTileSize = 32;
		int32 ShareRadius = 2;
		double TextureScale = 1.0;
		uint8 TranslucentAlpha = 128;

		FString MeshFolder;
		FString TextureFolder;
		FString MaterialFolder;

		TArray<FCachedMesh> Meshes;
		TMap<UStaticMesh*, int32> MeshLookup;

		TArray<FMaterialInfo> MaterialInfos;
		TMap<UMaterialInterface*, int32> MaterialLookup;
		TSet<UTexture2D*> UsedTextures;

		TArray<FComponentRecord> Components;

		TArray<FChunk> Chunks;
		TMap<FIntVector, int32> ChunkLookup;
		TArray<int32> ChunkOrder;
		TArray<int32> OutputToChunk;

		TArray<FAtlas> Atlases;

		UMaterialInterface* MasterMaterial = nullptr;

		TMap<FTileCacheKey, TArray<FColor>> TileCache;
		int64 TileCacheBytes = 0;
		TSet<UTexture2D*> FailedTextures;
	};

} // anonymous namespace


	/********************************************************************************************************/
	/*                                            ENTRY POINT                                               */
	/********************************************************************************************************/

	bool Run(const FInput& Input, FReducerResults& Results, FOutput& Output)
	{
		if (!Input.Settings)
		{
			Results.Fail(TEXT("Atlas reducer: no settings asset supplied."));
			return false;
		}

		if (Input.AssetRootPath.IsEmpty() || Input.MasterMaterialPackage.IsEmpty())
		{
			Results.Fail(TEXT("Atlas reducer: output paths are not set."));
			return false;
		}

		if (Input.Components.Num() == 0)
		{
			Results.Fail(TEXT("Atlas reducer: no static mesh components to reduce."));
			return false;
		}

		FReducer Reducer(Input, Results, Output);
		return Reducer.Run();
	}
}

// Ginger Ninja Gaming Ltd

#pragma once

#include "CoreMinimal.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/DataAsset.h"
#include "Engine/UserDefinedStruct.h"
#include "ZeroPayGunFX_r1.generated.h"


UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayGunFX_DefinitionDataAsset_r1 : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Physics Materials **/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* DefaultPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* FleshBodyPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* DirtPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* ConcretePhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* AsphaltPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* MetalPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* WoodPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* GlassPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* FabricPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* WaterPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* SnowPhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Materials")
	UPhysicalMaterial* VehiclePhysicalMaterial;

	/** Impact Sounds **/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* DefaultSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* FleshBodySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* DirtSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* ConcreteSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* AsphaltSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* MetalSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* WoodSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* GlassSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* FabricSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* WaterSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* SnowSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* VehicleSound;

	/** Impact Particles **/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* DefaultParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* FleshBodyParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* DirtParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* ConcreteParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* AsphaltParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* MetalParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* WoodParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* GlassParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* FabricParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* WaterParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* SnowParticle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Particle")
	UNiagaraSystem* VehicleParticle;

	/** Impact Decal Textures (Decal “stamps” / masks / albedo etc.) **/


	/** Impact Decal Materials (Deferred decal materials, etc.) **/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> DefaultDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> FleshBodyDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> DirtDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> ConcreteDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> AsphaltDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> MetalDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> WoodDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> GlassDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> FabricDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> WaterDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> SnowDecals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Decals")
	TArray<UMaterialInterface*> VehicleDecals;

private:
	/** Helper: pick a random decal from Preferred; fallback to DefaultDecals; may return nullptr. */
	UMaterialInterface* PickRandomDecal(const TArray<UMaterialInterface*>& Preferred) const
	{
		const TArray<UMaterialInterface*>& Source = (Preferred.Num() > 0) ? Preferred : DefaultDecals;
		if (Source.Num() == 0) return nullptr;

		const int32 Index = FMath::RandRange(0, Source.Num() - 1);
		return Source.IsValidIndex(Index) ? Source[Index] : nullptr;
	}

public:

	/** Returns the impact SoundCue for the given PhysicalMaterial (falls back to DefaultSound). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Impact")
	USoundCue* GetSoundCue(const UPhysicalMaterial* InPhysicalMaterial) const
	{
		// Treat null as default.
		if (!InPhysicalMaterial)
		{
			return DefaultSound;
		}

		// If you want "DefaultPhysicalMaterial" to map explicitly, check it first.
		if (DefaultPhysicalMaterial && InPhysicalMaterial == DefaultPhysicalMaterial)
		{
			return DefaultSound;
		}

		if (FleshBodyPhysicalMaterial && InPhysicalMaterial == FleshBodyPhysicalMaterial)
		{
			return FleshBodySound ? FleshBodySound : DefaultSound;
		}

		if (DirtPhysicalMaterial && InPhysicalMaterial == DirtPhysicalMaterial)
		{
			return DirtSound ? DirtSound : DefaultSound;
		}

		if (ConcretePhysicalMaterial && InPhysicalMaterial == ConcretePhysicalMaterial)
		{
			return ConcreteSound ? ConcreteSound : DefaultSound;
		}

		if (AsphaltPhysicalMaterial && InPhysicalMaterial == AsphaltPhysicalMaterial)
		{
			return AsphaltSound ? AsphaltSound : DefaultSound;
		}

		if (MetalPhysicalMaterial && InPhysicalMaterial == MetalPhysicalMaterial)
		{
			return MetalSound ? MetalSound : DefaultSound;
		}

		if (WoodPhysicalMaterial && InPhysicalMaterial == WoodPhysicalMaterial)
		{
			return WoodSound ? WoodSound : DefaultSound;
		}

		if (GlassPhysicalMaterial && InPhysicalMaterial == GlassPhysicalMaterial)
		{
			return GlassSound ? GlassSound : DefaultSound;
		}

		if (FabricPhysicalMaterial && InPhysicalMaterial == FabricPhysicalMaterial)
		{
			return FabricSound ? FabricSound : DefaultSound;
		}

		if (WaterPhysicalMaterial && InPhysicalMaterial == WaterPhysicalMaterial)
		{
			return WaterSound ? WaterSound : DefaultSound;
		}

		if (SnowPhysicalMaterial && InPhysicalMaterial == SnowPhysicalMaterial)
		{
			return SnowSound ? SnowSound : DefaultSound;
		}

		if (VehiclePhysicalMaterial && InPhysicalMaterial == VehiclePhysicalMaterial)
		{
			return VehicleSound ? VehicleSound : DefaultSound;
		}

		// Unknown material -> default.
		return DefaultSound;
	};

	/** Returns the impact NiagaraSystem for the given PhysicalMaterial (falls back to DefaultParticle). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Impact")
	UNiagaraSystem* GetNiagaraSystem(const UPhysicalMaterial* InPhysicalMaterial) const
	{
		if (!InPhysicalMaterial)
		{
			return DefaultParticle;
		}

		if (DefaultPhysicalMaterial && InPhysicalMaterial == DefaultPhysicalMaterial)
		{
			return DefaultParticle;
		}

		if (FleshBodyPhysicalMaterial && InPhysicalMaterial == FleshBodyPhysicalMaterial)
		{
			return FleshBodyParticle ? FleshBodyParticle : DefaultParticle;
		}

		if (DirtPhysicalMaterial && InPhysicalMaterial == DirtPhysicalMaterial)
		{
			return DirtParticle ? DirtParticle : DefaultParticle;
		}

		if (ConcretePhysicalMaterial && InPhysicalMaterial == ConcretePhysicalMaterial)
		{
			return ConcreteParticle ? ConcreteParticle : DefaultParticle;
		}

		if (AsphaltPhysicalMaterial && InPhysicalMaterial == AsphaltPhysicalMaterial)
		{
			return AsphaltParticle ? AsphaltParticle : DefaultParticle;
		}

		if (MetalPhysicalMaterial && InPhysicalMaterial == MetalPhysicalMaterial)
		{
			return MetalParticle ? MetalParticle : DefaultParticle;
		}

		if (WoodPhysicalMaterial && InPhysicalMaterial == WoodPhysicalMaterial)
		{
			return WoodParticle ? WoodParticle : DefaultParticle;
		}

		if (GlassPhysicalMaterial && InPhysicalMaterial == GlassPhysicalMaterial)
		{
			return GlassParticle ? GlassParticle : DefaultParticle;
		}

		if (FabricPhysicalMaterial && InPhysicalMaterial == FabricPhysicalMaterial)
		{
			return FabricParticle ? FabricParticle : DefaultParticle;
		}

		if (WaterPhysicalMaterial && InPhysicalMaterial == WaterPhysicalMaterial)
		{
			return WaterParticle ? WaterParticle : DefaultParticle;
		}

		if (SnowPhysicalMaterial && InPhysicalMaterial == SnowPhysicalMaterial)
		{
			return SnowParticle ? SnowParticle : DefaultParticle;
		}

		if (VehiclePhysicalMaterial && InPhysicalMaterial == VehiclePhysicalMaterial)
		{
			return VehicleParticle ? VehicleParticle : DefaultParticle;
		}

		return DefaultParticle;
	};


	/** Returns a random decal material for the given PhysicalMaterial (falls back to DefaultDecals). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Impact")
	UMaterialInterface* GetDecalMaterial(const UPhysicalMaterial* InPhysicalMaterial) const
	{
		if (!InPhysicalMaterial)
		{
			return PickRandomDecal(DefaultDecals);
		}

		if (DefaultPhysicalMaterial && InPhysicalMaterial == DefaultPhysicalMaterial)
		{
			return PickRandomDecal(DefaultDecals);
		}

		if (FleshBodyPhysicalMaterial && InPhysicalMaterial == FleshBodyPhysicalMaterial) return PickRandomDecal(FleshBodyDecals);
		if (DirtPhysicalMaterial && InPhysicalMaterial == DirtPhysicalMaterial) return PickRandomDecal(DirtDecals);
		if (ConcretePhysicalMaterial && InPhysicalMaterial == ConcretePhysicalMaterial) return PickRandomDecal(ConcreteDecals);
		if (AsphaltPhysicalMaterial && InPhysicalMaterial == AsphaltPhysicalMaterial) return PickRandomDecal(AsphaltDecals);
		if (MetalPhysicalMaterial && InPhysicalMaterial == MetalPhysicalMaterial) return PickRandomDecal(MetalDecals);
		if (WoodPhysicalMaterial && InPhysicalMaterial == WoodPhysicalMaterial) return PickRandomDecal(WoodDecals);
		if (GlassPhysicalMaterial && InPhysicalMaterial == GlassPhysicalMaterial) return PickRandomDecal(GlassDecals);
		if (FabricPhysicalMaterial && InPhysicalMaterial == FabricPhysicalMaterial) return PickRandomDecal(FabricDecals);
		if (WaterPhysicalMaterial && InPhysicalMaterial == WaterPhysicalMaterial) return PickRandomDecal(WaterDecals);
		if (SnowPhysicalMaterial && InPhysicalMaterial == SnowPhysicalMaterial) return PickRandomDecal(SnowDecals);
		if (VehiclePhysicalMaterial && InPhysicalMaterial == VehiclePhysicalMaterial) return PickRandomDecal(VehicleDecals);

		return PickRandomDecal(DefaultDecals);
	}
};

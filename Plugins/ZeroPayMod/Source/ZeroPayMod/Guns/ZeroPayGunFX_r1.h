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
	USoundCue* DefaultSound ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* FleshBodySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* DirtSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
	USoundCue* ConcreteSound ;

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
};

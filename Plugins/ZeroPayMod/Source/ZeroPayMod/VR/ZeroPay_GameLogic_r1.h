// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZeroPayMod_DefinitionDataAsset.h"
#include "ZeroPay_GameLogic_r1.generated.h"

UCLASS()
class ZEROPAYMOD_API AZeroPay_GameLogic_r1 : public AActor
{
	GENERATED_BODY()
	
public:		
	AZeroPay_GameLogic_r1();

	// The mod's definition file (required)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Mods")
	UZeroPayMod_DefinitionDataAsset* ModDefinition;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	
};

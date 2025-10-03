// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "ZeroPay_BodySocket_r1.generated.h"

UENUM(BlueprintType)
enum EZeroPay_BodySocket_Location
{
	LeftLeg,
	RightLeg,
	Waist,
	Chest,
	LeftArm,
	RightArm,
	Back
};


/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_BodySocket_r1 : public USphereComponent
{
	GENERATED_BODY()	
};

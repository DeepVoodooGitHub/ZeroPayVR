// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "ZeroPay_AmmoPocket_r1.generated.h"

UENUM(BlueprintType)
enum class EZeroPayAmmoPocketLocation : uint8
{
	LeftPocket  UMETA(DisplayName = "Left Leg Pocket"),
	RighPockett UMETA(DisplayName = "Right Leg Pocket")
};

#define ECC_AmmoPocket ECC_WorldDynamic

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_AmmoPocket_r1 : public UCapsuleComponent
{
	GENERATED_BODY()	
public:
	UZeroPay_AmmoPocket_r1();

};
// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ZeroPayGunBase_AnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API UZeroPayGunBase_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Gun Anim Instance")
	float indexFingerCurl = 0.0f ;	
};


UCLASS()
class ZEROPAYMOD_API UZeroPayGunBase_Revolver_AnimInstance : public UZeroPayGunBase_AnimInstance
{
	GENERATED_BODY()

public:

	// Flag to "hold open" the barrel of the revolver
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Gun Anim Revolver Instance")
	bool HoldOpen = false;
};

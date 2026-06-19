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

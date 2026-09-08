#pragma once

#include "CoreMinimal.h"
#include "ZeroPay_GunRegistration_Struct.generated.h"

USTRUCT(BlueprintType)
struct FZeroPayGunRegistrationStruct_r1
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FString WeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FString DisplayName;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	//FZPRadialMenuSettings RadialMenuSettings;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	//TSubclassOf<ZeroPayGunbase_r1> GunActor;
};
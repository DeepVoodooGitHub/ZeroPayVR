// (c) Ginger Ninja Games Ltd

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPayMod.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "ZeroPay_DebugConsoleComponent.h"
#include "ZeroPay_DebugSupport.generated.h"

static bool bClientOutputLogsToDisk = false;

UCLASS()
class ZEROPAYMOD_API UZeroPay_DebugSupport : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:	

};

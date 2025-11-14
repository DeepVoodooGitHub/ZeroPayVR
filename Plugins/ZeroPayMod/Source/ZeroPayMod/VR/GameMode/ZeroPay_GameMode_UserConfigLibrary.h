// ZeroPay_GameMode_UserConfigLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPay_GameMode_ConfigOption.h"
#include "ZeroPay_GameMode_UserConfigLibrary.generated.h"

UCLASS()
class ZEROPAYMOD_API UZeroPay_GameMode_UserConfigLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "ZeroPay|GameMode|User Config")
    static bool ApplySingleOptionToObject(UObject* Target,const FZeroPay_GameMode_ConfigOption& Option );

    UFUNCTION(BlueprintCallable, Category = "ZeroPay|GameMode|User Config")
    static int32 ApplyOptionsToObject(UObject* Target,const TArray<FZeroPay_GameMode_ConfigOption>& Options
    );
};

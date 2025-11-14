// ZeroPay_GameMode_ConfigOption.h

#pragma once

#include "CoreMinimal.h"
#include "ZeroPay_GameMode_ConfigOption.generated.h"

USTRUCT(BlueprintType)
struct FZeroPay_GameMode_ConfigOption
{
    GENERATED_BODY()

public:

    // Name of variable on the GameMode (must match BP variable name)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName VariableName;

    // "Bool" or "Int32" or empty
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Type;

    // Value as string   ("1","0","true","42")
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Value;
};

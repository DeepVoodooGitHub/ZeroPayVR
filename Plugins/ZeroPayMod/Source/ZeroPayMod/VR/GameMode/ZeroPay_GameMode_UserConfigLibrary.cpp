// ZeroPay_GameMode_UserConfigLibrary.cpp

#include "ZeroPay_GameMode_UserConfigLibrary.h"
#include "UObject/UnrealType.h"

bool UZeroPay_GameMode_UserConfigLibrary::ApplySingleOptionToObject(
    UObject* Target,
    const FZeroPay_GameMode_ConfigOption& Option
)
{
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplySingleOptionToObject: Target null"));
        return false;
    }

    if (Option.VariableName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplySingleOptionToObject: VariableName is None"));
        return false;
    }

    FProperty* Prop = Target->GetClass()->FindPropertyByName(Option.VariableName);
    if (!Prop)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Property '%s' not found on %s"),
            *Option.VariableName.ToString(),
            *Target->GetName());
        return false;
    }

    const FString TypeLower = Option.Type.ToLower();

    // Bool
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
    {
        if (TypeLower.IsEmpty() || TypeLower == TEXT("bool"))
        {
            const bool bValue =
                Option.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
                Option.Value == TEXT("1");

            BoolProp->SetPropertyValue_InContainer(Target, bValue);
            return true;
        }
    }

    // Int32
    if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
    {
        if (TypeLower.IsEmpty() || TypeLower == TEXT("int32") || TypeLower == TEXT("int"))
        {
            const int32 IntValue = FCString::Atoi(*Option.Value);
            IntProp->SetPropertyValue_InContainer(Target, IntValue);
            return true;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("Unsupported property type for '%s' on %s"),
        *Option.VariableName.ToString(),
        *Target->GetName());

    return false;
}

int32 UZeroPay_GameMode_UserConfigLibrary::ApplyOptionsToObject(
    UObject* Target,
    const TArray<FZeroPay_GameMode_ConfigOption>& Options
)
{
    int32 Applied = 0;

    for (const FZeroPay_GameMode_ConfigOption& Option : Options)
    {
        if (ApplySingleOptionToObject(Target, Option))
        {
            ++Applied;
        }
    }

    return Applied;
}

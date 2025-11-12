#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZeroPay_ModEditModFunctionLib.generated.h"

UENUM(BlueprintType)
enum class EEditModResult : uint8
{
    Success,
    Failure
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditModResult, EEditModResult, Result, const FString&, Message);

UCLASS()
class ZEROPAYMOD_API UEditModAsync : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOnEditModResult OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FOnEditModResult OnFailure;

    /** Edit an existing mod */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
    static UEditModAsync* SubmitModChanges(const FString& InAccessToken, int64 InModId, const FString& InName, const FString& InSummary, const FString& InDescription, const TArray<FString>& InTags, const TArray<FString>& InMetaKeys, const TArray<FString>& InMetaValues);

    virtual void Activate() override;

private:
    FString AccessToken;
    int64 ModId = 0;
    FString Name;
    FString Summary;
    FString Description;
    TArray<FString> Tags;
    TArray<FString> MetaKeys;
    TArray<FString> MetaValues;
};

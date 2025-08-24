#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZeroPay_ModNewModSubmissionFunctionLib.generated.h"

UENUM(BlueprintType)
enum class ENewModSubmitResult : uint8
{
    Success,
    Failure
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModSubmitResult, ENewModSubmitResult, Result, const FString&, Message);

/**
 * Async action to submit a new mod with logo to mod.io
 */
UCLASS()
class ZEROPAYMOD_API UAddModWithLogoAsync : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOnModSubmitResult OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FOnModSubmitResult OnFailure;

    /** Call this node in Blueprints */
    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "ZeroPay Mod Upload")
    static UAddModWithLogoAsync* AddModWithLogo(const FString& AccessToken, const FString& LogoFilePath, const FString& Name, const FString& Summary);

    virtual void Activate() override;

private:
    FString AccessToken;
    FString LogoFilePath;
    FString Name;
    FString Summary;
};

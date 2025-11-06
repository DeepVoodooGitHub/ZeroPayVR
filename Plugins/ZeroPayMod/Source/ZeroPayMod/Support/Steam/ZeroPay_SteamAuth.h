#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZeroPay_SteamAuth.generated.h"

// Output delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeroPayOnSteamAuthSuccess, const FString&, AuthToken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeroPayOnSteamAuthFailure, const FString&, ErrorMessage);

/**
 * Static async Blueprint node that wraps EIK's GetPlatformAuthToken.
 * Starts off GT (to avoid first-call hitches), then runs EIK on GT and forwards results.
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_SteamAuth : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "ZeroPay|Auth")
	FZeroPayOnSteamAuthSuccess OnSuccess;

	UPROPERTY(BlueprintAssignable, Category = "ZeroPay|Auth")
	FZeroPayOnSteamAuthFailure OnFailure;

	// Call this in BP: makes a proper async node with success/failure pins.
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "ZeroPay|Auth")
	static UZeroPay_SteamAuth* GetSteamAuthToken();

	virtual void Activate() override;

private:
	// EIK forwards
	UFUNCTION() void HandleEIKSuccess(const FString& Token);
	UFUNCTION() void HandleEIKFailure(const FString& Error);

	void Cleanup();
};

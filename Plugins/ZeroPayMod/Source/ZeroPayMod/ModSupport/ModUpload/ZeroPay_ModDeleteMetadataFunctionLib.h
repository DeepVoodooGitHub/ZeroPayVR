#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZeroPay_ModDeleteMetadataFunctionLib.generated.h"

UENUM(BlueprintType)
enum class EDeleteMetadataResult : uint8
{
	Success,
	Failure
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeleteMetadataResult, EDeleteMetadataResult, Result, const FString&, Message);

/**
 * Deletes metadata KVP entries on mod.io for a given mod.
 * Pass entries as "key" (delete all values for key) or "key:value" (delete that specific pair).
 */
UCLASS()
class ZEROPAYMOD_API UDeleteModMetadataAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnDeleteMetadataResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnDeleteMetadataResult OnFailure;

	/**
	 * Delete metadata KVP entries.
	 * @param InAccessToken   User Bearer token with permission to manage the mod.
	 * @param InModId         Target mod ID.
	 * @param InMetadataEntries  Each entry is "key" or "key:value". At least one required by the API.
	 * @param bUseGlobalHost  If true uses https://api.mod.io/v1/games/{GameId}/..., otherwise uses game-scoped host https://g-{GameId}.modapi.io/v1/games/{GameId}/...
	 */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload", meta = (BlueprintInternalUseOnly = "true"))
	static UDeleteModMetadataAsync* DeleteModMetadata(const FString& InAccessToken, int64 InModId, const TArray<FString>& InMetadataEntries) ;

	// UBlueprintAsyncActionBase
	virtual void Activate() override;

private:
	FString AccessToken;
	int64   ModId = 0;
	TArray<FString> MetadataEntries;
	bool    bGlobalHost = false;
};

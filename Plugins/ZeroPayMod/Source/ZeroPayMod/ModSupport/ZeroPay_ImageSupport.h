#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Texture2D.h"
#include "ZeroPay_ImageSupport.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextureLoadSuccess, UTexture2D*, Texture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextureLoadFailure, FString, Error);

/**
 * Async BP node: Loads a PNG file from disk and returns a UTexture2D
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_ImageLoader : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnTextureLoadSuccess OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnTextureLoadFailure OnFailure;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "Zero Pay Image Support")
	static UZeroPay_ImageLoader* LoadPNGTextureAsync(const FString& FilePath);

	virtual void Activate() override;

private:
	FString FilePath;

	void HandleLoad();
	void CreateTexture(const TArray<uint8>& RawData, int32 Width, int32 Height);
};

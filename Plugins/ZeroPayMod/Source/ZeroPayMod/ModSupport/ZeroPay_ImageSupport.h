#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Texture2D.h"
#include "ZeroPay_ImageSupport.generated.h"

UENUM(BlueprintType)
enum class EZeroPay_ImageStorage : uint8
{
	Cache      UMETA(DisplayName = "Cache (Saved/Cache/ModLogos)"),
	Directory  UMETA(DisplayName = "Directory (Saved/Mods/<DirectoryName>)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextureLoadSuccess, UTexture2D*, Texture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextureLoadFailure, FString, Error);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadPngSuccess, const FString&, FullPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadPngFailure, const FString&, Error);

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

	UFUNCTION(BlueprintCallable, Category = "Zero Pay Image Support")
	static bool IsLogoInCache(int64 ModID, float& OutAgeDays, FString& OutFullPath);

	virtual void Activate() override;

private:
	FString FilePath;

	void HandleLoad();
	void CreateTexture(const TArray<uint8>& RawData, int32 Width, int32 Height);
};

UCLASS()
class ZEROPAYMOD_API UZeroPay_DownloadPNGAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Start an async PNG download+save. FileName can be "1824" or "logo" (".png" is auto-added if missing).
	 *  For Directory mode, DirectoryName is required.
	 */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Mods", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UZeroPay_DownloadPNGAsync* DownloadPng(
		UObject* WorldContextObject,
		const FString& Url,
		EZeroPay_ImageStorage StorageMode,
		const FString& FileName,
		const FString& DirectoryName /* used only when StorageMode == Directory */
	);

	/** Fired with the absolute path of the saved file */
	UPROPERTY(BlueprintAssignable)
	FOnDownloadPngSuccess OnSuccess;

	/** Fired with a readable error message */
	UPROPERTY(BlueprintAssignable)
	FOnDownloadPngFailure OnFailure;

	// UBlueprintAsyncActionBase
	virtual void Activate() override;

private:
	// Params copied from factory
	UPROPERTY()
	UObject* WorldContextObject = nullptr;

	FString Url;
	EZeroPay_ImageStorage StorageMode = EZeroPay_ImageStorage::Cache;
	FString FileName;
	FString DirectoryName;

	// Live request – keep it alive while in-flight
	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> Request;

	FString BuildTargetPath() const;
	static bool IsLikelyPng(const TArray<uint8>& Bytes);
	void BroadcastFailure(const FString& Error);
	void BroadcastSuccess(const FString& FullPath);
};
// (c) Ginger Ninja Games Ltd

#pragma once

#include "ModioSubsystem.h"
#include "CoreMinimal.h"
#include "ZeroPay_ModGlobal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPay_HTTPEngine.generated.h"


UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_GetModioFileResult : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString message;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 file_id;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 date_updated ;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString filename;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 UncompressedSize;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString binaryurl;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModioFileResultReceived, UZeroPayMod_GetModioFileResult*, Result);


// Delegate for progress updates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnModIoFileDownloadProgress, int32, BytesReceived, int32, BytesTotal);

// Delegate for completion
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModIoFileDownloadFinished, bool, bSuccess);

UCLASS()
class ZEROPAYMOD_API UZeroPayModAsync_GetModioFile : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnModioFileResultReceived OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnModioFileResultReceived OnFailure;

	// Get (temporary / dynamic) details on a modIO file for a given platform
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "ZeroPay Modio Support")
	static UZeroPayModAsync_GetModioFile* GetModioFileInfoAsync(FModioModID ModID, FModioPlatform Platform);

	void StartFileInfoRequest(FModioModID ModID, FModioPlatform Platform);

	void ParseModioFileInfoJSON(FString ResponseString) ;

private:
	void HandleRequestCompleted(bool bSuccess, const FString& Message, const FString& Filename, int64 FileID, int64 DateUpdated, int64 UncompressedSize, const FString& BinaryURL);
};


UCLASS()
class ZEROPAYMOD_API UZeroPayMod_AsyncHttpDownload : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Blueprint node to start the download
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "ZeroPay Modio Support")
	static UZeroPayMod_AsyncHttpDownload* DownloadFile(FString URL);

	// Progress event
	UPROPERTY(BlueprintAssignable, Category = "ZeroPay Modio Support")
	FOnModIoFileDownloadProgress OnProgress;
	
	// Finish event
	UPROPERTY(BlueprintAssignable, Category = "ZeroPay Modio Support")
	FOnModIoFileDownloadFinished OnFinish;

	// Internal function to trigger download
	void StartDownload(FString URL);

private:
	void HandleProgress(int32 BytesSent, int32 BytesReceived);
	void HandleResponse(bool bWasSuccessful);

	uint64 TotalContentLength = 0;
};

/**
 * 
 */
class ZEROPAYMOD_API ZeroPay_HTTPEngine
{
public:
	ZeroPay_HTTPEngine();
	~ZeroPay_HTTPEngine();
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "ModioSubsystem.h"
#include "CoreMinimal.h"
#include "ZeroPay_ModGlobal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPay_HTTPEngine.generated.h"

//void HandleRequestCompleted(bool bSuccess, const FString& Message, const FString& Filename, int64 FileID, int64 DateUpdated, int64 UncompressedSize, const FString& BinaryURL,
//	FString Summary, FString Author, int64 Ratings, int64 Category, FString LogoURL);


UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_GetModInfoResult : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString message;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 mod_id;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString display_name;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString summary;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString author;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 ratings;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 category;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 download_size ;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	int64 file_size;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString binaryurl;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString logourl;
};


UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_GetFilesResult : public UObject
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
	FString display_name ;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
	FString binaryurl;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModioFileResultReceived, UZeroPayMod_GetFilesResult*, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModioGetModInfoResultReceived, UZeroPayMod_GetModInfoResult*, Result);

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
	static UZeroPayModAsync_GetModioFile* GetModioFilesAsync(FModioModID ModID, FModioPlatform Platform);

	void StartModioGetFilesRequest(FModioModID ModID, FModioPlatform Platform);

	void ParseModioFilesJSON(FString ResponseString) ;

private:
	void HandleRequestCompleted(bool bSuccess, const FString& Message, const FString& Filename, int64 FileID, int64 DateUpdated, int64 UncompressedSize, const FString& BinaryURL);
};


UCLASS()
class ZEROPAYMOD_API UZeroPayModAsync_GetModioModInfo : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnModioGetModInfoResultReceived OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnModioGetModInfoResultReceived OnFailure;

	// Get (temporary / dynamic) details on a modIO file for a given platform
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "ZeroPay Modio Support")
	static UZeroPayModAsync_GetModioModInfo* GetModioModInfoAsync(FModioModID ModID);

	void StartModioGetModInfoRequest(FModioModID ModID);

	void ParseModioModInfoJSON(FString ResponseString);

private:
	void HandleRequestCompleted(bool bSuccess, const FString& ErrorMessage, int64 ModID, const FString& Username, const FString& Name, const FString& Summary, const FString& ThumbURL,
								const TArray<FString>& TagNames, float RatingsPercentage, int64 Filesize, int64 FilesizeUncompressed, const FString& BinaryURL);

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

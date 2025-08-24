#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModSupport/ZeroPay_ModGlobal.h"
#include "Interfaces/IHttpRequest.h"   // for FHttpRequestPtr
#include "Interfaces/IHttpResponse.h"  // for FHttpResponsePtr
#include "ZeroPay_ModUploadFunctionLib.generated.h"

// ----- Delegates -----
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultipartOnFinished, const FString&, ResponseJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultipartOnFailure, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMultipartOnProgress, int64, BytesSentTotal, int64, BytesTotal, const FString&, Status);

// Forward decl
class IHttpRequest;
class IHttpResponse;

/**
 * Lightweight async task UObject (no UBlueprintAsyncActionBase).
 * AddToRoot is used to keep it alive while the async chain runs.
 */
UCLASS(BlueprintType, Blueprintable)
class ZEROPAYMOD_API UZeroPay_MultipartUploadTask : public UObject
{
    GENERATED_BODY()

public:
    // Event pins
    UPROPERTY(BlueprintAssignable) FMultipartOnFinished OnFinished;
    UPROPERTY(BlueprintAssignable) FMultipartOnFailure OnFailure;
    UPROPERTY(BlueprintAssignable) FMultipartOnProgress OnProgress;

    // Entry point called by the function library
    void Initialize(FModioPlatform InPlatform, int64 InModID, const FString& InAccessToken, const FString& InVersion, const FString& InChangelog, int64 InChunkSizeBytes = 50 * 1024 * 1024, int32 InPollSeconds = 2, int32 InPollMaxAttempts = 120);

    UFUNCTION(BlueprintCallable, Category = "ZeroPay|ModUpload")
    void StartUpload();

private:
    // Flow
    void CreateUploadSession();
    void HandleCreateSessionResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);
    void UploadNextPart();
    void HandleChunkProgress(FHttpRequestPtr Req, uint64 BytesSent, uint64 BytesReceived);
    void HandleUploadPartResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);
    void CompleteUpload();
    void HandleCompleteResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);
    void SchedulePollInSeconds(float Seconds);
    void QuerySessionStatus();
    void HandleQuerySessionResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);
    void AddModfile();
    void HandleAddModfileResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);
    void ActivateNewModfile();
    void HandleActivateResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess);

    // Helpers
    FString GetApiBaseUrl() const;
    FString PlatformFolderName() const;          // e.g. "Windows", "Android", "LinuxServer"
    FString PlatformNameForModio() const;        // e.g. "windows", "android", "linux"
    FString BuildVersionTag() const;
    void BuildMultipartFormData(const TArray<TPair<FString, FString>>& Fields, const FString& Boundary, TArray<uint8>& OutBody) const;
    void AppendFormField(TArray<uint8>& Out, const FString& Boundary, const FString& Name, const FString& Value) const;

private:
    // Inputs
    FModioPlatform Platform = FModioPlatform::ModIOPlatform_Windows;
    int64 ModID = 0;
    FString AccessToken;
    FString VersionString;
    FString ChangelogString;
    int64 ChunkSize = 50 * 1024 * 1024;          // 50MB
    int64 UpdateAtBytesSent ;
    int64 UpdateByteSize = 1024 * 1024;              // 1Mb
    int32 PollSeconds = 2;
    int32 PollMaxAttempts = 120;

    // State
    FString PakPath;
    FString PakFileName;
    TArray<uint8> FileData;
    int64 FileSize = 0;
    int64 BytesUploaded = 0;
    int64 CurrentPartIndex = 0;
    int32 RetryCount = 0;
    int32 MaxRetries = 3;

    // Progress granularity (~1MB)
    int64 ProgressLastBroadcastBytes = 0;
    int64 BytesUploadedBaseForThisPart = 0;

    // Polling
    int32 PollAttempt = 0;

    // Session / results
    FString UploadSessionId;
    int64 NewFileId = 0;
};

/**
 * Function library entry to kick off upload as a UObject you can bind to in BP.
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_MultipartUploadFunctionLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ZeroPay|ModUpload", meta = (DisplayName = "UploadPakModFile"))
    static UZeroPay_MultipartUploadTask* UploadPakModFile(FModioPlatform Platform, int64 ModID, const FString& AccessToken, const FString& Version = TEXT(""), const FString& Changelog = TEXT(""), int64 ChunkSizeBytes = 52428800, int32 PollSeconds = 2, int32 PollMaxAttempts = 120);
};

#include "ZeroPay_ModUploadFunctionLib.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ModSupport/ZeroPay_ModGlobal.h"

// -------------------- Function Library Entry --------------------
UZeroPay_MultipartUploadTask* UZeroPay_MultipartUploadFunctionLib::UploadPakModFile(FModioPlatform InPlatform, int64 InModID, const FString& InAccessToken, const FString& InVersion, const FString& InChangelog, int64 ChunkSizeBytes, int32 PollSeconds, int32 PollMaxAttempts)
{
    UZeroPay_MultipartUploadTask* Task = NewObject<UZeroPay_MultipartUploadTask>();
    Task->AddToRoot();
    Task->Initialize(InPlatform, InModID, InAccessToken, InVersion, InChangelog, ChunkSizeBytes, PollSeconds, PollMaxAttempts);
    return Task;
}

// -------------------- Task Implementation --------------------
void UZeroPay_MultipartUploadTask::Initialize(FModioPlatform InPlatform, int64 InModID, const FString& InAccessToken, const FString& InVersion, const FString& InChangelog, int64 InChunkSizeBytes, int32 InPollSeconds, int32 InPollMaxAttempts)
{
    Platform = InPlatform;
    ModID = InModID;
    AccessToken = InAccessToken;
    VersionString = InVersion;
    ChangelogString = InChangelog;
    ChunkSize = InChunkSizeBytes > 0 ? InChunkSizeBytes : 50 * 1024 * 1024;
    PollSeconds = InPollSeconds > 0 ? InPollSeconds : 2;
    PollMaxAttempts = InPollMaxAttempts > 0 ? InPollMaxAttempts : 120;
}

FString UZeroPay_MultipartUploadTask::PlatformFolderName() const
{
    switch (Platform)
    {
    case FModioPlatform::ModIOPlatform_Windows: return TEXT("Windows");
    case FModioPlatform::ModIOPlatform_Android: return TEXT("Android");
    case FModioPlatform::ModIOPlatform_LinuxServer: return TEXT("LinuxServer");
    default: return TEXT("Windows");
    }
}

FString UZeroPay_MultipartUploadTask::PlatformNameForModio() const
{
    switch (Platform)
    {
    case FModioPlatform::ModIOPlatform_Windows: return TEXT("windows");
    case FModioPlatform::ModIOPlatform_Android: return TEXT("android");
    case FModioPlatform::ModIOPlatform_LinuxServer: return TEXT("linux");
    default: return TEXT("windows");
    }
}

FString UZeroPay_MultipartUploadTask::GetApiBaseUrl() const
{
    // FGameID is provided in your project (ZeroPay_ModGlobal.h)
    return FString::Printf(TEXT("https://g-%s.modapi.io/v1/games/%s/mods/%lld"), *FGameID, *FGameID, ModID);
}

FString UZeroPay_MultipartUploadTask::BuildVersionTag() const
{
    if (!VersionString.IsEmpty())
    {
        return VersionString;
    }

    // yyyy.MM.dd-HHmm
    const FDateTime Now = FDateTime::Now();
    return Now.ToString(TEXT("%Y.%m.%d-%H%M"));
}

void UZeroPay_MultipartUploadTask::StartUpload()
{
    const FString PlatformDir = PlatformFolderName();

    // Saved/Workshop/<Platform>/<Platform>.zip
    const FString BasePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Workshop"));
    const FString PlatformBasePath = FPaths::Combine(BasePath, PlatformDir);
    PakPath = FPaths::Combine(PlatformBasePath, PlatformDir + TEXT(".zip"));
    PakFileName = PlatformDir + TEXT(".zip");

    if (!FPaths::FileExists(PakPath))
    {
        OnFailure.Broadcast(FString::Printf(TEXT("Pak file not found: %s"), *PakPath));
        RemoveFromRoot();
        return;
    }

    if (!FFileHelper::LoadFileToArray(FileData, *PakPath))
    {
        OnFailure.Broadcast(TEXT("Failed to read pak file."));
        RemoveFromRoot();
        return;
    }

    FileSize = FileData.Num();
    BytesUploaded = 0;
    CurrentPartIndex = 0;
    RetryCount = 0;
    ProgressLastBroadcastBytes = 0;
    BytesUploadedBaseForThisPart = 0;

    CreateUploadSession();
}

void UZeroPay_MultipartUploadTask::CreateUploadSession()
{
    UE_LOG(LogTemp, Log, TEXT(""));
    UE_LOG(LogTemp, Log, TEXT(" >>> ZEROPAY VR - Upload of Mod commencing <<< "));
    UE_LOG(LogTemp, Log, TEXT(""));
    UE_LOG(LogTemp, Log, TEXT("> Begin upload session... "));

    const FString Url = FString::Printf(TEXT("%s/files/multipart"), *GetApiBaseUrl());
    const FString FormBody = FString::Printf(TEXT("filename=%s"), *PakFileName);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    Req->SetContentAsString(FormBody);
    Req->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleCreateSessionResponse);
    Req->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleCreateSessionResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    if (!bSuccess || !Res.IsValid())
    {
        OnFailure.Broadcast(TEXT("Failed to create upload session (no response)"));
        UE_LOG(LogTemp, Log, TEXT("   + Failed to create upload session (no response)"));
        RemoveFromRoot();
        return;
    }

    const int32 Code = Res->GetResponseCode();
    const FString Content = Res->GetContentAsString();

    if (Code != 200)
    {
        FString ErrorMessage = FString::Printf(TEXT("Failed to create upload session - HTTP %d: %s"), Code, *Content.Left(512));
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Content);
        if (FJsonSerializer::Deserialize(R, Json) && Json.IsValid() && Json->HasField(TEXT("message")))
        {
            ErrorMessage = Json->GetStringField(TEXT("message"));
        }
        OnFailure.Broadcast(ErrorMessage);
        UE_LOG(LogTemp, Log, TEXT("   + Create session failed [%s]"), *ErrorMessage);
        RemoveFromRoot();
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Content);
    if (FJsonSerializer::Deserialize(R, Json) && Json.IsValid() && Json->HasField(TEXT("upload_id")))
    {
        UploadSessionId = Json->GetStringField(TEXT("upload_id"));
    }
    else
    {
        OnFailure.Broadcast(TEXT("Invalid session response, missing upload_id."));
        UE_LOG(LogTemp, Log, TEXT("   + Create session failed, missing upload_id"));
        RemoveFromRoot();
        return;
    }

    UploadNextPart();
}

void UZeroPay_MultipartUploadTask::UploadNextPart()
{
    if (BytesUploaded >= FileSize)
    {
        CompleteUpload();
        return;
    }

    const int64 StartByte = CurrentPartIndex * ChunkSize;
    const int64 EndByte = FMath::Min(StartByte + ChunkSize, FileSize) - 1;
    const int64 ThisSize = EndByte - StartByte + 1;

    TArray<uint8> ChunkData;
    ChunkData.Append(FileData.GetData() + StartByte, ThisSize);

    const FString Url = FString::Printf(TEXT("%s/files/multipart?upload_id=%s"), *GetApiBaseUrl(), *UploadSessionId);
    const FString ContentRange = FString::Printf(TEXT("bytes %lld-%lld/%lld"), StartByte, EndByte, FileSize);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> PartReq = FHttpModule::Get().CreateRequest();
    PartReq->SetURL(Url);
    PartReq->SetVerb(TEXT("PUT")); // IMPORTANT: PUT (matches working PowerShell script)
    PartReq->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    PartReq->SetHeader(TEXT("Accept"), TEXT("application/json"));
    PartReq->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded")); // matches script
    PartReq->SetHeader(TEXT("Content-Range"), ContentRange);
    PartReq->SetContent(MoveTemp(ChunkData));

    // Progress across ~1MB ticks
    BytesUploadedBaseForThisPart = StartByte;
    UpdateAtBytesSent = StartByte + UpdateByteSize ;
    PartReq->OnRequestProgress64().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleChunkProgress);

    PartReq->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleUploadPartResponse);

    UE_LOG(LogTemp, Log, TEXT("Uploading chunk %lld [%s] (%lld bytes)"), static_cast<long long>(CurrentPartIndex), *ContentRange, ThisSize);

    PartReq->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleChunkProgress(FHttpRequestPtr Req, uint64 BytesSent, uint64 BytesReceived)
{
    const int64 GlobalSent = BytesUploadedBaseForThisPart + static_cast<int64>(BytesSent);
    if (GlobalSent >= UpdateAtBytesSent)
    {
        UpdateAtBytesSent = GlobalSent + UpdateByteSize;
        const FString Status = FString::Printf(TEXT("Uploading... %lld / %lld bytes"), GlobalSent, FileSize);
        OnProgress.Broadcast(GlobalSent, FileSize, Status);
    }
}

void UZeroPay_MultipartUploadTask::HandleUploadPartResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    if (!bSuccess || !Res.IsValid())
    {
        if (RetryCount < MaxRetries)
        {
            RetryCount++;
            const FString Status = FString::Printf(TEXT("Chunk failed (no response). Retrying %d/%d..."), RetryCount, MaxRetries);
            UE_LOG(LogTemp, Log, TEXT("   + Upload failed, retrying %d/%d..."), RetryCount, MaxRetries);
            OnProgress.Broadcast(BytesUploaded, FileSize, Status);
            UploadNextPart();
            return;
        }
        OnFailure.Broadcast(TEXT("Upload part failed too many times (no response)."));
        RemoveFromRoot();
        return;
    }

    const int32 Code = Res->GetResponseCode();
    if (Code < 200 || Code >= 300)
    {
        if (RetryCount < MaxRetries)
        {
            RetryCount++;
            const FString Status = FString::Printf(TEXT("Chunk rejected HTTP %d. Retrying %d/%d..."), Code, RetryCount, MaxRetries);
            UE_LOG(LogTemp, Log, TEXT("   + Chunk rejected, retrying %d/%d..."), RetryCount, MaxRetries);
            OnProgress.Broadcast(BytesUploaded, FileSize, Status);
            UploadNextPart();
            return;
        }
        const FString Body = Res->GetContentAsString();
        OnFailure.Broadcast(FString::Printf(TEXT("Upload part failed HTTP %d: %s"), Code, *Body.Left(512)));
        UE_LOG(LogTemp, Log, TEXT("   + Failed, given up retrying..."));
        RemoveFromRoot();
        return;
    }

    // Success for this part
    RetryCount = 0;

    const int64 StartByte = CurrentPartIndex * ChunkSize;
    const int64 EndByte = FMath::Min(StartByte + ChunkSize, FileSize) - 1;
    BytesUploaded = EndByte + 1;
    CurrentPartIndex++;

    OnProgress.Broadcast(BytesUploaded, FileSize, TEXT("Part accepted"));
    UploadNextPart();
}

void UZeroPay_MultipartUploadTask::CompleteUpload()
{
    const FString Url = FString::Printf(TEXT("%s/files/multipart/complete?upload_id=%s"), *GetApiBaseUrl(), *UploadSessionId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    Req->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleCompleteResponse);
    Req->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleCompleteResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    if (!bSuccess || !Res.IsValid() || Res->GetResponseCode() != 200)
    {
        const int32 Code = Res.IsValid() ? Res->GetResponseCode() : -1;
        const FString Body = Res.IsValid() ? Res->GetContentAsString() : TEXT("(no response)");
        OnFailure.Broadcast(FString::Printf(TEXT("Failed to complete multipart session. HTTP %d: %s"), Code, *Body.Left(512)));
        UE_LOG(LogTemp, Log, TEXT("   + Complete Response failed [%s]"), *Body.Left(512));
        RemoveFromRoot();
        return;
    }

    // Now poll until status == 3 (COMPLETE)
    PollAttempt = 0;
    SchedulePollInSeconds(static_cast<float>(PollSeconds));
}

void UZeroPay_MultipartUploadTask::SchedulePollInSeconds(float Seconds)
{
    // One-shot ticker to avoid blocking the game thread.
    TWeakObjectPtr<UZeroPay_MultipartUploadTask> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->QuerySessionStatus();
            }
            return false;
        }), Seconds);
}

void UZeroPay_MultipartUploadTask::QuerySessionStatus()
{
    const FString Url = FString::Printf(TEXT("%s/files/multipart/sessions?upload_id=%s"), *GetApiBaseUrl(), *UploadSessionId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleQuerySessionResponse);
    Req->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleQuerySessionResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    int32 StatusVal = -1;

    if (bSuccess && Res.IsValid())
    {
        const FString Body = Res->GetContentAsString();
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(R, Json) && Json.IsValid())
        {
            if (Json->HasTypedField<EJson::Number>(TEXT("status")))
            {
                StatusVal = static_cast<int32>(Json->GetNumberField(TEXT("status")));
            }
            else if (Json->HasField(TEXT("data")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
                if (Json->TryGetArrayField(TEXT("data"), Arr))
                {
                    for (const TSharedPtr<FJsonValue>& V : *Arr)
                    {
                        const TSharedPtr<FJsonObject>* Obj = nullptr;
                        if (V.IsValid() && V->TryGetObject(Obj) && Obj && (*Obj)->HasField(TEXT("upload_id")))
                        {
                            if ((*Obj)->GetStringField(TEXT("upload_id")) == UploadSessionId && (*Obj)->HasTypedField<EJson::Number>(TEXT("status")))
                            {
                                StatusVal = static_cast<int32>((*Obj)->GetNumberField(TEXT("status")));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (StatusVal != -1)
    {
        OnProgress.Broadcast(BytesUploaded, FileSize, FString::Printf(TEXT("Session status=%d (0=INCOMPLETE,1=PENDING,2=PROCESSING,3=COMPLETE,4=CANCELLED)"), StatusVal));
    }

    if (StatusVal == 3)
    {
        AddModfile();
        return;
    }

    if (StatusVal == 4)
    {
        OnFailure.Broadcast(TEXT("Multipart session CANCELLED by server."));
        UE_LOG(LogTemp, Log, TEXT("   + Session cancelled by server"));
        RemoveFromRoot();
        return;
    }

    PollAttempt++;
    if (PollAttempt >= PollMaxAttempts)
    {
        // Proceed anyway (matches script behavior)
        OnProgress.Broadcast(BytesUploaded, FileSize, TEXT("Timed out waiting for COMPLETE; proceeding to create modfile."));
        UE_LOG(LogTemp, Log, TEXT("   + Timed out waiting for complete"));
        AddModfile();
        return;
    }

    SchedulePollInSeconds(static_cast<float>(PollSeconds));
}

void UZeroPay_MultipartUploadTask::BuildMultipartFormData(const TArray<TPair<FString, FString>>& Fields, const FString& Boundary, TArray<uint8>& OutBody) const
{
    OutBody.Reset();

    auto AppendString = [&OutBody](const FString& S)
        {
            FTCHARToUTF8 Conv(*S);
            OutBody.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
        };

    for (const TPair<FString, FString>& KV : Fields)
    {
        AppendString(TEXT("--") + Boundary + TEXT("\r\n"));
        AppendString(TEXT("Content-Disposition: form-data; name=\"") + KV.Key + TEXT("\"\r\n\r\n"));
        AppendString(KV.Value + TEXT("\r\n"));
    }

    AppendString(TEXT("--") + Boundary + TEXT("--\r\n"));
}

void UZeroPay_MultipartUploadTask::AppendFormField(TArray<uint8>& Out, const FString& Boundary, const FString& Name, const FString& Value) const
{
    auto AppendString = [&Out](const FString& S)
        {
            FTCHARToUTF8 Conv(*S);
            Out.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
        };

    AppendString(TEXT("--") + Boundary + TEXT("\r\n"));
    AppendString(TEXT("Content-Disposition: form-data; name=\"") + Name + TEXT("\"\r\n\r\n"));
    AppendString(Value + TEXT("\r\n"));
}

void UZeroPay_MultipartUploadTask::AddModfile()
{
    const FString Url = FString::Printf(TEXT("%s/files"), *GetApiBaseUrl());

    // Fields to match your PowerShell
    const FString VersionTag = BuildVersionTag();
    const FString Changelog = ChangelogString.IsEmpty() ? TEXT("Uploaded via UE") : ChangelogString;
    const FString PlatformForApi = PlatformNameForModio();        // "windows" | "android" | "linux"

    // Build multipart/form-data body
    const FString Boundary = FString::Printf(TEXT("----UEBoundary%08X%08X"), FMath::Rand(), FMath::Rand());

    TArray<uint8> Body;
    {
        TArray<TPair<FString, FString>> Fields;
        Fields.Add(TPair<FString, FString>(TEXT("upload_id"), UploadSessionId));
        Fields.Add(TPair<FString, FString>(TEXT("version"), VersionTag));
        Fields.Add(TPair<FString, FString>(TEXT("changelog"), Changelog));
        Fields.Add(TPair<FString, FString>(TEXT("active"), TEXT("1")));
        // Array-style: platforms[]
        Fields.Add(TPair<FString, FString>(TEXT("platforms[]"), PlatformForApi));

        BuildMultipartFormData(Fields, Boundary, Body);
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
    Req->SetContent(MoveTemp(Body));
    Req->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleAddModfileResponse);
    Req->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleAddModfileResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    if (!bSuccess || !Res.IsValid())
    {
        OnFailure.Broadcast(TEXT("Add Modfile failed (no response)."));
        UE_LOG(LogTemp, Log, TEXT("   + Add Modfile failed (timed out)"));
        RemoveFromRoot();
        return;
    }

    const int32 Code = Res->GetResponseCode();
    const FString Body = Res->GetContentAsString();

    if (!(Code == 200 || Code == 201))
    {
        OnFailure.Broadcast(FString::Printf(TEXT("Add Modfile failed HTTP %d: %s"), Code, *Body.Left(1024)));
        UE_LOG(LogTemp, Log, TEXT("   + Add Modfile failed failed HTTP %d: %s"), Code, *Body.Left(1024));
        RemoveFromRoot();
        return;
    }

    // Parse id
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
    if (FJsonSerializer::Deserialize(R, Json) && Json.IsValid() && Json->HasTypedField<EJson::Number>(TEXT("id")))
    {
        NewFileId = static_cast<int64>(Json->GetNumberField(TEXT("id")));
    }
    else
    {
        OnFailure.Broadcast(TEXT("Add Modfile response missing 'id'."));
        RemoveFromRoot();
        return;
    }

    // Optional but per your script, activate explicitly
    ActivateNewModfile();
}

void UZeroPay_MultipartUploadTask::ActivateNewModfile()
{
    const FString Url = FString::Printf(TEXT("%s/files/%lld"), *GetApiBaseUrl(), NewFileId);
    const FString Body = TEXT("active=1");

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("PUT"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    Req->SetContentAsString(Body);
    Req->OnProcessRequestComplete().BindUObject(this, &UZeroPay_MultipartUploadTask::HandleActivateResponse);
    Req->ProcessRequest();
}

void UZeroPay_MultipartUploadTask::HandleActivateResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
{
    if (!bSuccess || !Res.IsValid())
    {
        OnFailure.Broadcast(TEXT("Activate modfile failed (no response)."));
        UE_LOG(LogTemp, Log, TEXT("   + Add Modfile failed (no response)"));
        RemoveFromRoot();
        return;
    }

    const int32 Code = Res->GetResponseCode();
    const FString Body = Res->GetContentAsString();

    if (Code < 200 || Code >= 300)
    {
        OnFailure.Broadcast(FString::Printf(TEXT("Activate modfile failed HTTP %d: %s"), Code, *Body.Left(1024)));
        UE_LOG(LogTemp, Log, TEXT("   + Activate Modfile failed failed HTTP %d: %s"), Code, *Body.Left(1024));
        RemoveFromRoot();
        return;
    }

    // Success: return the Add Modfile JSON (more useful than the small activate payload)
    OnFinished.Broadcast(Body);
    UE_LOG(LogTemp, Log, TEXT("   + Completed successfull"));
    RemoveFromRoot();
}

// (c) Ginger Ninja Games Ltd

#include "ZeroPay_HTTPEngine.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ZeroPay_HTTPEngine::ZeroPay_HTTPEngine()
{
}

ZeroPay_HTTPEngine::~ZeroPay_HTTPEngine()
{
}

UZeroPayModAsync_GetModioFile* UZeroPayModAsync_GetModioFile::GetModioFileInfoAsync(FModioModID ModID, FModioPlatform Platform)
{
    UZeroPayModAsync_GetModioFile* Node = NewObject<UZeroPayModAsync_GetModioFile>();
    Node->StartFileInfoRequest(ModID, Platform);
    return Node;
}

void UZeroPayModAsync_GetModioFile::StartFileInfoRequest(FModioModID ModID, FModioPlatform Platform)
{
    FHttpModule* Http = &FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

    Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
            if (bSuccess && Res.IsValid())
            {
                ParseModioFileInfoJSON(Res->GetContentAsString());
            }
            else
            {
                // Failed
                HandleRequestCompleted(false, TEXT("HTTP Request to mod.io failed"), TEXT(""), 0, 0, TEXT("") );
            }
        });

    FString ModIDString = ModID.ToString();
    FString URL = FString::Printf(TEXT("https://g-%s.modapi.io/v1/games/%s/mods/%s/files?api_key=%s&_limit=1"),*FGameID, *FGameID, *ModIDString, *FAPIKey);
    Request->SetURL(URL);
    switch (Platform)
    {
        case FModioPlatform::ModIOPlatform_Windows : Request->SetHeader(TEXT("X-Modio-Platform"), TEXT("windows")); break;
        case FModioPlatform::ModIOPlatform_Android: Request->SetHeader(TEXT("X-Modio-Platform"), TEXT("android")); break;
        case FModioPlatform::ModIOPlatform_LinuxServer : Request->SetHeader(TEXT("X-Modio-Platform"), TEXT("linux")); break;
       default: break;
    }
    
    Request->SetVerb("GET");
    Request->ProcessRequest();
}

void UZeroPayModAsync_GetModioFile::ParseModioFileInfoJSON(FString ResponseString)
{
    // Parse the JSON string
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Extract "data" array
        const TArray<TSharedPtr<FJsonValue>>* DataArray;
        if (JsonObject->TryGetArrayField(TEXT("data"), DataArray) && DataArray->Num() > 0)
        {
            // Get the first object in "data"
            const TSharedPtr<FJsonObject> FirstDataObject = (*DataArray)[0]->AsObject();

            if (FirstDataObject.IsValid())
            {
                // Extract "id"
                FString DataString;
                int64 FileID = 0;
                if (FirstDataObject->TryGetStringField(TEXT("id"), DataString))
                    FileID = FCString::Strtoui64(*DataString, nullptr, 10);

                // Extract "data_updated"
                int64 DateUpdated = 0;
                if (FirstDataObject->TryGetStringField(TEXT("date_updated"), DataString))
                    DateUpdated = FCString::Strtoui64(*DataString, nullptr, 10);

                // Extract "filename"
                FString Filename = FirstDataObject->GetStringField(TEXT("filename"));

                // Extract "download" object and then "binary_url"
                TSharedPtr<FJsonObject> DownloadObject = FirstDataObject->GetObjectField(TEXT("download"));
                FString BinaryURL = DownloadObject->GetStringField(TEXT("binary_url"));

                HandleRequestCompleted(true, TEXT(""), Filename, FileID, DateUpdated, BinaryURL);

            }
        }
        else
        {
            /* Extract the error */
            if (JsonObject->HasField(TEXT("error")))
            {
                TSharedPtr<FJsonObject> ErrorObject = JsonObject->GetObjectField(TEXT("error"));

                // Extract fields
                int32 ErrorCode = ErrorObject->GetIntegerField(TEXT("code"));
                int32 ErrorRef = ErrorObject->GetIntegerField(TEXT("error_ref"));
                FString ErrorMessage = ErrorObject->GetStringField(TEXT("message"));

                // Build the error string
                FString ErrorString = FString::Printf(TEXT("Error Code: %d | Ref: %d | Message: %s"), ErrorCode, ErrorRef, *ErrorMessage);

                HandleRequestCompleted(false, *ErrorString, TEXT(""), 0, 0, TEXT(""));
            }
            else
            {
                HandleRequestCompleted(false, TEXT("Received unknown response from mod.io server"), TEXT(""), 0, 0, TEXT(""));
            }
        }
    }
    else
    {
        HandleRequestCompleted(false, TEXT("Received bad JSON from mod.io server"), TEXT(""), 0, 0, TEXT(""));
    }
}

void UZeroPayModAsync_GetModioFile::HandleRequestCompleted(bool bSuccess, const FString& Message, const FString& Filename, int64 FileID, int64 DateUpdated, const FString& BinaryURL)
{
    UZeroPayMod_GetModioFileResult* Result = NewObject<UZeroPayMod_GetModioFileResult>();
    Result->message = Message;
    Result->filename = Filename;
    Result->file_id = FileID;
    Result->date_updated = DateUpdated ;
    Result->binaryurl = BinaryURL;

    if (bSuccess)
    {
        OnSuccess.Broadcast(Result);
    }
    else
    {
        OnFailure.Broadcast(Result);
    }
}


UZeroPayMod_AsyncHttpDownload* UZeroPayMod_AsyncHttpDownload::DownloadFile(FString URL)
{
    UZeroPayMod_AsyncHttpDownload* Downloader = NewObject<UZeroPayMod_AsyncHttpDownload>();
    Downloader->StartDownload(URL);
    return Downloader;
}

void UZeroPayMod_AsyncHttpDownload::StartDownload(FString URL)
{
    FHttpModule* Http = &FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

    // Bind progress (we'll extract content length in OnHeaderReceived)
    Request->OnRequestProgress64().BindLambda([this](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
        {
            if (TotalContentLength == 0 && Request->GetResponse().IsValid())
            {
                TotalContentLength = Request->GetResponse()->GetContentLength();
            }
            HandleProgress(BytesReceived, TotalContentLength);
        });

    // Bind completion
    Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            bool bSuccess = bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode());

            if (bSuccess)
            {
                FString SavePath = FPaths::ProjectSavedDir() / TEXT("DownloadedFile.zip");
                FFileHelper::SaveArrayToFile(Response->GetContent(), *SavePath);
                UE_LOG(LogTemp, Log, TEXT("Download saved to: %s"), *SavePath);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Download failed."));
            }

            HandleResponse(bSuccess);
        });

    Request->SetURL(URL);
    Request->SetVerb(TEXT("GET"));
    Request->ProcessRequest();
}

void UZeroPayMod_AsyncHttpDownload::HandleProgress(int32 BytesReceived, int32 BytesTotal)
{
    OnProgress.Broadcast(BytesReceived, BytesTotal);
}

void UZeroPayMod_AsyncHttpDownload::HandleResponse(bool bWasSuccessful)
{
    OnFinish.Broadcast(bWasSuccessful);
}
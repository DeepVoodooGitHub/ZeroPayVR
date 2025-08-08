// (c) Ginger Ninja Games Ltd

#include "ZeroPay_HTTPEngine.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/*******************************************************************************************************************************/
/*                                    >>> GetFiles - Pulls the .PAK file information for a mod <<<                             *
/*******************************************************************************************************************************/

UZeroPayModAsync_GetModioFile* UZeroPayModAsync_GetModioFile::GetModioFilesAsync(FModioModID ModID, FModioPlatform Platform)
{
    UZeroPayModAsync_GetModioFile* Node = NewObject<UZeroPayModAsync_GetModioFile>();
    Node->StartModioGetFilesRequest(ModID, Platform);
    return Node;
}

void UZeroPayModAsync_GetModioFile::StartModioGetFilesRequest(FModioModID ModID, FModioPlatform Platform)
{
    FHttpModule* Http = &FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

    Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {

#if WITH_EDITOR
            /* If we're dying, ignore */
            if (IsEngineExitRequested() || GExitPurge)
                return;

            if (GEditor && !GEditor->PlayWorld)
                return;

            if (!IsValid(this))
                return;
#endif

            if (bSuccess && Res.IsValid())
            {
                ParseModioFilesJSON(Res->GetContentAsString());
            }
            else
            {
                // Failed
                HandleRequestCompleted(false, TEXT("HTTP Request to mod.io failed"), TEXT(""), 0, 0, 0, TEXT("") );
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

void UZeroPayModAsync_GetModioFile::ParseModioFilesJSON(FString ResponseString)
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

                // Uncompress size
                int64 UncompressedSize = 0;
                if (FirstDataObject->TryGetStringField(TEXT("filesize_uncompressed"), DataString))
                    UncompressedSize = FCString::Strtoui64(*DataString, nullptr, 10);

                // Extract "download" object and then "binary_url"
                TSharedPtr<FJsonObject> DownloadObject = FirstDataObject->GetObjectField(TEXT("download"));
                FString BinaryURL = DownloadObject->GetStringField(TEXT("binary_url"));

                HandleRequestCompleted(true, TEXT(""), Filename, FileID, DateUpdated, UncompressedSize, BinaryURL);

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

                HandleRequestCompleted(false, *ErrorString, TEXT(""), 0, 0, 0, TEXT(""));
            }
            else
            {
                HandleRequestCompleted(false, TEXT("Received unknown response from mod.io server"), TEXT(""), 0, 0, 0, TEXT(""));
            }
        }
    }
    else
    {
        HandleRequestCompleted(false, TEXT("Received bad JSON from mod.io server"), TEXT(""), 0, 0, 0, TEXT(""));
    }
}

void UZeroPayModAsync_GetModioFile::HandleRequestCompleted(bool bSuccess, const FString& Message, const FString& Filename, int64 FileID, int64 DateUpdated, int64 UncompressedSize, const FString& BinaryURL)
{
    UZeroPayMod_GetFilesResult* Result = NewObject<UZeroPayMod_GetFilesResult>();
    Result->message = Message;
    Result->filename = Filename;
    Result->file_id = FileID;
    Result->date_updated = DateUpdated ;
    Result->UncompressedSize = UncompressedSize ;
    Result->binaryurl = BinaryURL;
    //Result->summary = Summary;
    //Result->author = Author;
    //Result->ratings = Ratings;
    //Result->category = Category;
    //Result->logourl = LogoURL;

    if (bSuccess)
    {
        OnSuccess.Broadcast(Result);
    }
    else
    {
        OnFailure.Broadcast(Result);
    }
}



/*******************************************************************************************************************************/
/*                                        >>> GetModInfo - Pulls common info about the mod <<<                                 *
/*******************************************************************************************************************************/

UZeroPayModAsync_GetModioModInfo* UZeroPayModAsync_GetModioModInfo::GetModioModInfoAsync(FModioModID ModID)
{
    UZeroPayModAsync_GetModioModInfo* Node = NewObject<UZeroPayModAsync_GetModioModInfo>();
    Node->StartModioGetModInfoRequest(ModID);
    return Node;
}

void UZeroPayModAsync_GetModioModInfo::StartModioGetModInfoRequest(FModioModID ModID)
{
    FHttpModule* Http = &FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

    Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
#if WITH_EDITOR
            /* If we're dying, ignore */
            if (IsEngineExitRequested() || GExitPurge)
                return;

            if (GEditor && !GEditor->PlayWorld) 
                return;

            if (!IsValid(this))
                return;
#endif

            if (bSuccess && Res.IsValid())
            {
                ParseModioModInfoJSON(Res->GetContentAsString());
            }
            else
            {
                // Failed
                HandleRequestCompleted(false, TEXT("HTTP Request to mod.io failed"), 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TArray<FString>(), 0.0f, 0, 0, TEXT(""));
            }
        });

    FString ModIDString = ModID.ToString();
    FString URL = FString::Printf(TEXT("https://g-%s.modapi.io/v1/games/%s/mods/%s/?api_key=%s&_limit=1"), *FGameID, *FGameID, *ModIDString, *FAPIKey);
    Request->SetURL(URL);

    Request->SetVerb("GET");
    Request->ProcessRequest();
}

void UZeroPayModAsync_GetModioModInfo::ParseModioModInfoJSON(const FString ResponseString)
{
    // Parse the JSON string
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        int64 ModID = JsonObject->GetIntegerField(TEXT("id"));
        FString Username;
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("submitted_by")))
        {
            TSharedPtr<FJsonObject> SubmittedBy = JsonObject->GetObjectField(TEXT("submitted_by"));
            Username = SubmittedBy->GetStringField(TEXT("username"));
        }

        FString Name = JsonObject->GetStringField(TEXT("name"));
        FString Summary = JsonObject->GetStringField(TEXT("summary"));

        // Logo thumbnail
        FString ThumbURL;
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("logo")))
        {
            TSharedPtr<FJsonObject> LogoObject = JsonObject->GetObjectField(TEXT("logo"));
            ThumbURL = LogoObject->GetStringField(TEXT("thumb_640x360"));
        }

        // Tags array
        TArray<FString> TagNames;
        const TArray<TSharedPtr<FJsonValue>>* TagsArray;
        if (JsonObject->TryGetArrayField(TEXT("tags"), TagsArray))
        {
            for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
            {
                if (TagValue->Type == EJson::Object)
                {
                    TSharedPtr<FJsonObject> TagObject = TagValue->AsObject();
                    if (TagObject.IsValid() && TagObject->HasField(TEXT("name")))
                    {
                        TagNames.Add(TagObject->GetStringField(TEXT("name")));
                    }
                }
            }
        }

        // Rating
        float RatingsPercentage = 0.0f;
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("stats")))
        {
            TSharedPtr<FJsonObject> StatsObject = JsonObject->GetObjectField(TEXT("stats"));
            RatingsPercentage = StatsObject->GetNumberField(TEXT("ratings_percentage_positive"));
        }

        // Modfile info
        int64 Filesize = 0;
        int64 FilesizeUncompressed = 0;
        FString BinaryURL;
        if (JsonObject->HasTypedField<EJson::Object>(TEXT("modfile")))
        {
            TSharedPtr<FJsonObject> ModfileObject = JsonObject->GetObjectField(TEXT("modfile"));

            Filesize = ModfileObject->GetIntegerField(TEXT("filesize"));
            FilesizeUncompressed = ModfileObject->GetIntegerField(TEXT("filesize_uncompressed"));

            if (ModfileObject->HasTypedField<EJson::Object>(TEXT("download")))
            {
                TSharedPtr<FJsonObject> DownloadObject = ModfileObject->GetObjectField(TEXT("download"));
                BinaryURL = DownloadObject->GetStringField(TEXT("binary_url"));
            }
        }

        // Now pass everything to your handler
        HandleRequestCompleted(true, TEXT(""), ModID, Username, Name, Summary, ThumbURL, TagNames, RatingsPercentage, Filesize, FilesizeUncompressed, BinaryURL);
    }
    else
    {
        HandleRequestCompleted(
            false,
            TEXT("Received invalid JSON from mod.io server"), 0, TEXT(""), TEXT(""), TEXT(""), TEXT(""), TArray<FString>(), 0.0f, 0, 0, TEXT("")
        );
    }
}

void UZeroPayModAsync_GetModioModInfo::HandleRequestCompleted(bool bSuccess, const FString& ErrorMessage, int64 ModID, const FString& Username, const FString& Name, const FString& Summary, const FString& ThumbURL,
                                                                const TArray<FString>& TagNames, float RatingsPercentage, int64 Filesize, int64 FilesizeUncompressed, const FString& BinaryURL)
{
    UZeroPayMod_GetModInfoResult* Result = NewObject<UZeroPayMod_GetModInfoResult>();
    Result->message = ErrorMessage;
    Result->mod_id = ModID;
    Result->display_name = Name;
    Result->summary = Summary;
    Result->author = Username;
    Result->ratings = static_cast<int64>(RatingsPercentage);
    Result->category = TagNames.Num() > 0 ? static_cast<int64>(FCrc::StrCrc32(*TagNames[0])) : 0; // Optional: hash first tag name
    Result->download_size = FilesizeUncompressed;
    Result->file_size = Filesize;
    Result->binaryurl = BinaryURL;
    Result->logourl = ThumbURL;

    if (!OnSuccess.IsBound() || !OnFailure.IsBound())
        return ;

    if (bSuccess)
    {
        OnSuccess.Broadcast(Result);
    }
    else
    {
        OnFailure.Broadcast(Result);
    }
}



/*******************************************************************************************************************************/
/*                                >>> Download Mod - Actually grabs the compress zip from mod.io <<<                           *
/*******************************************************************************************************************************/

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
    TWeakObjectPtr<UBlueprintAsyncActionBase> WeakActor = this ;

    // Bind progress (we'll extract content length in OnHeaderReceived)
    Request->OnRequestProgress64().BindLambda([this, WeakActor](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
        {
            if (TotalContentLength == 0 && Request->GetResponse().IsValid())
            {
                TotalContentLength = Request->GetResponse()->GetContentLength();
            }
            /* in PIE, we can be killed - check before trying to do logic.. */
            if (WeakActor.IsValid())
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
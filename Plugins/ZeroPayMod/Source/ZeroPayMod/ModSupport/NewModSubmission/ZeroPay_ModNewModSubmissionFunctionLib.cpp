#include "ZeroPay_ModNewModSubmissionFunctionLib.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#include "ModSupport/ZeroPay_ModGlobal.h"

// --------- helpers ---------
static void AppendRaw(TArray<uint8>& Out, const void* Data, int64 Size)
{
    const int64 Offset = Out.AddUninitialized(Size);
    FMemory::Memcpy(Out.GetData() + Offset, Data, Size);
}

static void AppendString(TArray<uint8>& Out, const FString& S) // UTF-8
{
    FTCHARToUTF8 Conv(*S);
    AppendRaw(Out, Conv.Get(), Conv.Length());
}

static FString GuessMime(const FString& Path)
{
    const FString Ext = FPaths::GetExtension(Path, false).ToLower();
    if (Ext == TEXT("png"))
        return TEXT("image/png");
    if (Ext == TEXT("jpg") || Ext == TEXT("jpeg")) 
        return TEXT("image/jpeg");
    return TEXT("application/octet-stream");
}

static bool BuildMultipartBody(const TMap<FString, FString>& TextFields, const FString& FileFieldName, const FString& FilePath, TArray<uint8>& OutBody, FString& OutBoundary, FString& OutError)
{
    OutBody.Reset();
    OutError.Reset();

    if (!FPaths::FileExists(FilePath))
    {
        OutError = FString::Printf(TEXT("Logo file does not exist: %s"), *FilePath);
        return false;
    }

    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        OutError = FString::Printf(TEXT("Failed to read logo file: %s"), *FilePath);
        return false;
    }

    OutBoundary = FString::Printf(TEXT("---------------------------%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString CRLF = TEXT("\r\n");

    // Text fields
    for (const auto& KVP : TextFields)
    {
        AppendString(OutBody, TEXT("--") + OutBoundary + CRLF);
        AppendString(OutBody, FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"%s%s"), *KVP.Key, *CRLF, *CRLF));
        AppendString(OutBody, KVP.Value);
        AppendString(OutBody, CRLF);
    }

    // File part
    const FString FileName = FPaths::GetCleanFilename(FilePath);
    const FString Mime = GuessMime(FilePath);

    AppendString(OutBody, TEXT("--") + OutBoundary + CRLF);
    AppendString(OutBody, FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"%s"), *FileFieldName, *FileName, *CRLF));
    AppendString(OutBody, FString::Printf(TEXT("Content-Type: %s%s%s"), *Mime, *CRLF, *CRLF));
    if (FileBytes.Num() > 0)
    {
        AppendRaw(OutBody, FileBytes.GetData(), FileBytes.Num());
    }
    AppendString(OutBody, CRLF);

    // Closing boundary
    AppendString(OutBody, TEXT("--") + OutBoundary + TEXT("--") + CRLF);

    return true;
}

// --------- Async Action ---------

UAddModWithLogoAsync* UAddModWithLogoAsync::AddModWithLogo(const FString& InAccessToken, const FString& InLogoFilePath, const FString& InName, const FString& InSummary)
{
    UAddModWithLogoAsync* Node = NewObject<UAddModWithLogoAsync>();
    Node->AccessToken = InAccessToken;
    Node->LogoFilePath = InLogoFilePath;
    Node->Name = InName;
    Node->Summary = InSummary;
    return Node;
}

void UAddModWithLogoAsync::Activate()
{
    if (AccessToken.IsEmpty())
    {
        OnFailure.Broadcast(ENewModSubmitResult::Failure, TEXT("Missing AccessToken."));
        return;
    }
    if (LogoFilePath.IsEmpty())
    {
        OnFailure.Broadcast(ENewModSubmitResult::Failure, TEXT("LogoFilePath is empty."));
        return;
    }
    if (Name.IsEmpty())
    {
        OnFailure.Broadcast(ENewModSubmitResult::Failure, TEXT("Name is required."));
        return;
    }

    // Build multipart body
    TMap<FString, FString> Fields;
    Fields.Add(TEXT("name"), Name);
    Fields.Add(TEXT("summary"), Summary);

    TArray<uint8> Body;
    FString Boundary, BuildError;
    if (!BuildMultipartBody(Fields, TEXT("logo"), LogoFilePath, Body, Boundary, BuildError))
    {
        OnFailure.Broadcast(ENewModSubmitResult::Failure,
            BuildError.IsEmpty() ? TEXT("Failed to build multipart body.") : BuildError);
        return;
    }

    const FString Url = FString::Printf(TEXT("https://g-%s.modapi.io/v1/games/%s/mods"), *FGameID, *FGameID);

    auto& Http = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), *FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), *FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
    Req->SetContent(MoveTemp(Body));

    Req->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Res, bool bOK)
        {
            FString ResultMsg;
            bool bSuccess = false;

            if (bOK && Res.IsValid())
            {
                const FString Content = Res->GetContentAsString();

                TSharedPtr<FJsonObject> Root;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
                if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
                {
                    if (Root->HasField(TEXT("error")))
                    {
                        const TSharedPtr<FJsonObject> Err = Root->GetObjectField(TEXT("error"));
                        Err->TryGetStringField(TEXT("message"), ResultMsg);
                        bSuccess = false;
                    }
                    else if (Root->HasField(TEXT("id")))
                    {
                        int64 IdNum = 0;
                        if (Root->TryGetNumberField(TEXT("id"), IdNum))
                        {
                            ResultMsg = LexToString(IdNum);
                            bSuccess = true;
                        }
                        else
                        {
                            FString IdStr;
                            if (Root->TryGetStringField(TEXT("id"), IdStr))
                            {
                                ResultMsg = IdStr;
                                bSuccess = true;
                            }
                        }
                    }
                    else
                    {
                        ResultMsg = FString::Printf(TEXT("HTTP %d: %s"), Res->GetResponseCode(), *Content.Left(512));
                        bSuccess = (Res->GetResponseCode() >= 200 && Res->GetResponseCode() < 300);
                    }
                }
                else
                {
                    ResultMsg = FString::Printf(TEXT("HTTP %d: %s"), Res->GetResponseCode(), *Content.Left(512));
                    bSuccess = (Res->GetResponseCode() >= 200 && Res->GetResponseCode() < 300);
                }
            }
            else
            {
                ResultMsg = TEXT("No response from server.");
                bSuccess = false;
            }

            if (bSuccess)
            {
                OnSuccess.Broadcast(ENewModSubmitResult::Success, ResultMsg);
            }
            else
            {
                OnFailure.Broadcast(ENewModSubmitResult::Failure, ResultMsg);
            }
        });

    Req->ProcessRequest();
}

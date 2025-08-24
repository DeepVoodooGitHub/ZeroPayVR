#include "ZeroPay_ModEditModFunctionLib.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ModSupport/ZeroPay_ModGlobal.h"   

UEditModAsync* UEditModAsync::SubmitModChanges(const FString& InAccessToken, int64 InModId, const FString& InName, const FString& InSummary, const FString& InDescription, const TArray<FString>& InTags)
{
    UEditModAsync* Node = NewObject<UEditModAsync>();
    Node->AccessToken = InAccessToken;
    Node->ModId = InModId;
    Node->Name = InName;
    Node->Summary = InSummary;
    Node->Description = InDescription;
    Node->Tags = InTags;
    return Node;
}

void UEditModAsync::Activate()
{
    if (AccessToken.IsEmpty())
    {
        OnFailure.Broadcast(EEditModResult::Failure, TEXT("Missing AccessToken."));
        return;
    }
    if (ModId <= 0)
    {
        OnFailure.Broadcast(EEditModResult::Failure, TEXT("Invalid ModId."));
        return;
    }

    // Build form-data body
    FString Boundary = FString::Printf(TEXT("---------------------------%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString CRLF = TEXT("\r\n");

    TArray<uint8> Body;

    auto AppendString = [&Body](const FString& S)
        {
            FTCHARToUTF8 Conv(*S);
            Body.Append((uint8*)Conv.Get(), Conv.Length());
        };

    auto AppendField = [&](const FString& Key, const FString& Value)
        {
            AppendString(TEXT("--") + Boundary + CRLF);
            AppendString(FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"%s%s"), *Key, *CRLF, *CRLF));
            AppendString(Value + CRLF);
        };

    if (!Name.IsEmpty()) AppendField(TEXT("name"), Name);
    if (!Summary.IsEmpty()) AppendField(TEXT("summary"), Summary);
    if (!Description.IsEmpty()) AppendField(TEXT("description"), Description);
    for (const FString& Tag : Tags)
    {
        AppendField(TEXT("tags[]"), Tag);
    }

    AppendString(TEXT("--") + Boundary + TEXT("--") + CRLF);

    // Build URL
    const FString Url = FString::Printf(TEXT("https://g-%s.modapi.io/v1/games/%s/mods/%lld"), *FGameID, *FGameID, ModId);

    auto& Http = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
    Req->SetContent(Body);

    Req->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Res, bool bOK)
        {
            FString ResultMsg;
            bool bSuccess = false;

            if (bOK && Res.IsValid())
            {
                FString Content = Res->GetContentAsString();
                TSharedPtr<FJsonObject> Root;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);

                if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
                {
                    if (Root->HasField(TEXT("error")))
                    {
                        const TSharedPtr<FJsonObject> Err = Root->GetObjectField(TEXT("error"));
                        Err->TryGetStringField(TEXT("message"), ResultMsg);
                    }
                    else if (Root->HasField(TEXT("id")))
                    {
                        ResultMsg = FString::Printf(TEXT("Updated Mod %lld successfully."), ModId);
                        bSuccess = true;
                    }
                }

                if (!bSuccess)
                {
                    ResultMsg = FString::Printf(TEXT("HTTP %d: %s"), Res->GetResponseCode(), *Content.Left(512));
                }
            }
            else
            {
                ResultMsg = TEXT("No response from server.");
            }

            if (bSuccess)
            {
                OnSuccess.Broadcast(EEditModResult::Success, ResultMsg);
            }
            else
            {
                OnFailure.Broadcast(EEditModResult::Failure, ResultMsg);
            }
        });

    Req->ProcessRequest();
}

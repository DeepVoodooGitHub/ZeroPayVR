#include "ZeroPay_ModAuthExchangeFunctionLib.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "LatentActions.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

/** Latent action that owns the HTTP request */
class FRequestAuthExchangeCodeAction final : public FPendingLatentAction
{
public:
    FName ExecutionFunction;
    int32 OutputLink = 0;
    TWeakObjectPtr<UObject> CallbackTarget;

    EAuthExchangeRequestResult* BranchesRef = nullptr;
    FString* AcesssCodeRef = nullptr;
    FString* MessageRef = nullptr;

    bool bCompleted = false;

    FRequestAuthExchangeCodeAction(const FLatentActionInfo& LatentInfo,
        EAuthExchangeRequestResult& InBranches,
        FString& FAcesssCode,
        FString& InMessage)
        : ExecutionFunction(LatentInfo.ExecutionFunction)
        , OutputLink(LatentInfo.Linkage)
        , CallbackTarget(LatentInfo.CallbackTarget)
        , BranchesRef(&InBranches)
        , AcesssCodeRef(&FAcesssCode)
        , MessageRef(&InMessage)
    {
    }

    void Start(const FString& Endpoint, const FString& Body)
    {
        auto& Http = FHttpModule::Get();
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();

        Req->SetURL(Endpoint);
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
        Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
        Req->SetContentAsString(Body);

        // DO NOT MakeShareable(this). Just capture raw 'this'.
        Req->OnProcessRequestComplete().BindLambda(
            [this](FHttpRequestPtr, FHttpResponsePtr Res, bool bOK)
            {
                int32 JsonCode = -1;
                FString JsonMsg;
                FString JsonAccessCode;

                if (bOK && Res.IsValid())
                {
                    const FString Content = Res->GetContentAsString();
                    TSharedPtr<FJsonObject> Root;
                    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
                    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
                    {
                        Root->TryGetNumberField(TEXT("code"), JsonCode);
                        Root->TryGetStringField(TEXT("message"), JsonMsg);
                        Root->TryGetStringField(TEXT("access_token"), JsonAccessCode);
                    }
                    else
                    {
                        JsonMsg = FString::Printf(TEXT("HTTP %d: %s"), Res->GetResponseCode(), *Content.Left(512));
                    }
                }
                else
                {
                    JsonMsg = TEXT("No response from server.");
                }

                const bool bOK200 = (JsonCode == 200);

                if (MessageRef)   
                    *MessageRef = JsonMsg.IsEmpty() ? TEXT("Unknown response.") : JsonMsg;
                if (AcesssCodeRef)
                    *AcesssCodeRef = JsonAccessCode ;
                if (BranchesRef)  
                    *BranchesRef = bOK200 ? EAuthExchangeRequestResult::Success : EAuthExchangeRequestResult::Failure;

                bCompleted = true;
            });

        Req->ProcessRequest();
    }

    virtual void UpdateOperation(FLatentResponse& Response) override
    {
        Response.FinishAndTriggerIf(bCompleted, ExecutionFunction, OutputLink, CallbackTarget.Get());
    }
};

// Helper to create a shared latent action
static FRequestAuthExchangeCodeAction* MakeLatentAction(FLatentActionInfo& LatentInfo, EAuthExchangeRequestResult& Branches, FString& AcesssCode, FString& Message)
{
    return new FRequestAuthExchangeCodeAction(LatentInfo, Branches, AcesssCode, Message);
}

// URL-encode (RFC 3986-ish) — used for api_key
FString UZeroPay_ModAuthExchangeFunctionLib::UrlEncodeStrict(const FString& In)
{
    return FGenericPlatformHttp::UrlEncode(In); // produces %20 for spaces
}

// Form-URL-encode for application/x-www-form-urlencoded (space => '+')
FString UZeroPay_ModAuthExchangeFunctionLib::FormUrlEncode(const FString& In)
{
    FString Enc = FGenericPlatformHttp::UrlEncode(In); // first percent-encode everything needed
    Enc.ReplaceInline(TEXT("%20"), TEXT("+"));         // then convert spaces to '+'
    return Enc;
}

void UZeroPay_ModAuthExchangeFunctionLib::RequestAuthExchangeCode(FLatentActionInfo LatentInfo, const FString& SecurityCode, EAuthExchangeRequestResult& Branches, FString& AcesssCode, FString& Message)
{
    Message.Empty();
    Branches = EAuthExchangeRequestResult::Failure;

    UWorld* World ; 
    /* Silently fail if outside editor */
#if !WITH_EDITOR
    return;
#else
    if (!GIsEditor || !GEditor)
        return;
    World = GEditor->GetEditorWorldContext().World();
#endif
    
    FLatentActionManager& LatentMgr = World->GetLatentActionManager();
    if (LatentMgr.FindExistingAction<FRequestAuthExchangeCodeAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
    {
        LatentMgr.RemoveActionsForObject(LatentInfo.CallbackTarget);
    }

    FRequestAuthExchangeCodeAction* Action = MakeLatentAction(LatentInfo, Branches, AcesssCode, Message);

    const FString Endpoint = FString::Printf(TEXT("https://g-%s.modapi.io/v1/oauth/emailexchange"), *FGameID);
    const int64 ExpiryTs = FDateTime::UtcNow().ToUnixTimestamp() + 31535999 ;
    const FString Body = FString::Printf(
        TEXT("api_key=%s&%s=%s&%s=%s"),
        *UrlEncodeStrict(*FAPIKey),
        *FormUrlEncode(TEXT("security_code")),
        *FormUrlEncode(SecurityCode),
        * FormUrlEncode(TEXT("date_expires")),
        *FormUrlEncode(LexToString(ExpiryTs))
    );

    Action->Start(Endpoint, Body);

    // IMPORTANT: pass the raw pointer, not &Action.Get() and not a shared ref
    LatentMgr.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
}

#include "ZeroPay_ModAuthRequestFunctionLib.h"

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
class FRequestEmailAuthCodeAction final : public FPendingLatentAction
{
public:
    FName ExecutionFunction;
    int32 OutputLink = 0;
    TWeakObjectPtr<UObject> CallbackTarget;

    EEmailAuthRequestResult* BranchesRef = nullptr;
    bool* bSuccessRef = nullptr;
    FString* MessageRef = nullptr;

    bool bCompleted = false;

    FRequestEmailAuthCodeAction(const FLatentActionInfo& LatentInfo,
        EEmailAuthRequestResult& InBranches,
        bool& InSuccess,
        FString& InMessage)
        : ExecutionFunction(LatentInfo.ExecutionFunction)
        , OutputLink(LatentInfo.Linkage)
        , CallbackTarget(LatentInfo.CallbackTarget)
        , BranchesRef(&InBranches)
        , bSuccessRef(&InSuccess)
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

                if (bOK && Res.IsValid())
                {
                    const FString Content = Res->GetContentAsString();
                    TSharedPtr<FJsonObject> Root;
                    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
                    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
                    {
                        Root->TryGetNumberField(TEXT("code"), JsonCode);
                        Root->TryGetStringField(TEXT("message"), JsonMsg);
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

                if (bSuccessRef)  
                    *bSuccessRef = bOK200;
                if (MessageRef)   
                    *MessageRef = JsonMsg.IsEmpty() ? TEXT("Unknown response.") : JsonMsg;
                if (BranchesRef)  
                    *BranchesRef = bOK200 ? EEmailAuthRequestResult::Success : EEmailAuthRequestResult::Failure;

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
static FRequestEmailAuthCodeAction* MakeLatentAction(FLatentActionInfo& LatentInfo, EEmailAuthRequestResult& Branches, bool& bSuccess, FString& Message)
{
    return new FRequestEmailAuthCodeAction(LatentInfo, Branches, bSuccess, Message);
}

// URL-encode (RFC 3986-ish) — used for api_key
FString UZeroPay_ModAuthRequestFunctionLib::UrlEncodeStrict(const FString& In)
{
    return FGenericPlatformHttp::UrlEncode(In); // produces %20 for spaces
}

// Form-URL-encode for application/x-www-form-urlencoded (space => '+')
FString UZeroPay_ModAuthRequestFunctionLib::FormUrlEncode(const FString& In)
{
    FString Enc = FGenericPlatformHttp::UrlEncode(In); // first percent-encode everything needed
    Enc.ReplaceInline(TEXT("%20"), TEXT("+"));         // then convert spaces to '+'
    return Enc;
}

void UZeroPay_ModAuthRequestFunctionLib::RequestEMailAuthCode(FLatentActionInfo LatentInfo, const FString& Email, EEmailAuthRequestResult& Branches, bool& bSuccess, FString& Message)
{
    bSuccess = false;
    Message.Empty();
    Branches = EEmailAuthRequestResult::Failure;

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
    if (LatentMgr.FindExistingAction<FRequestEmailAuthCodeAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
    {
        LatentMgr.RemoveActionsForObject(LatentInfo.CallbackTarget);
    }

    FRequestEmailAuthCodeAction* Action = MakeLatentAction(LatentInfo, Branches, bSuccess, Message);

    const FString Endpoint = FString::Printf(TEXT("https://g-%s.modapi.io/v1/oauth/emailrequest"), *FGameID);
    const FString Body = FString::Printf(
        TEXT("api_key=%s&%s=%s"),
        *UrlEncodeStrict(*FAPIKey),
        *FormUrlEncode(TEXT("email")),
        *FormUrlEncode(Email)
    );

    Action->Start(Endpoint, Body);

    // IMPORTANT: pass the raw pointer, not &Action.Get() and not a shared ref
    LatentMgr.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
}

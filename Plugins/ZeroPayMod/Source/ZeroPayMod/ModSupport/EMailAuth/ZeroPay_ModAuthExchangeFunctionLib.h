// (c) Ginger Ninja Games Ltd

#pragma once

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ModSupport/ZeroPay_ModGlobal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModSupport/ZeroPay_ModGlobal.h"
#include "ZeroPay_ModAuthExchangeFunctionLib.generated.h"

UENUM(BlueprintType)
enum class EAuthExchangeRequestResult : uint8
{
	Success,
	Failure
};

UCLASS()
class ZEROPAYMOD_API UZeroPay_ModAuthExchangeFunctionLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
private:
    // URL-encode (RFC 3986-ish) — used for api_key
    static FString UrlEncodeStrict(const FString& In);
    static FString FormUrlEncode(const FString& In);

public:
    /**
     * Requests a mod.io email security code.
     * POST https://g-<GameID>.modapi.io/v1/oauth/emailrequest
     * Body: api_key, email (form-urlencoded)
     * Success when JSON.code == 201, message always reported.
     */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload", meta = (Latent, LatentInfo = "LatentInfo", ExpandEnumAsExecs = "Branches", DisplayName = "Request E-Mail Auth Code"))
    static void RequestAuthExchangeCode(FLatentActionInfo LatentInfo, const FString& SecurityCode, EAuthExchangeRequestResult& Branches, FString& AcesssCode, FString& Message);
};

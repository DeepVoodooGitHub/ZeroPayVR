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
#include "ZeroPay_ModAuthRequestFunctionLib.generated.h"

UENUM(BlueprintType)
enum class EEmailAuthRequestResult : uint8
{
	Success,
	Failure
};

UCLASS()
class ZEROPAYMOD_API UZeroPay_ModAuthRequestFunctionLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * Requests a mod.io email security code.
     * POST https://g-<GameID>.modapi.io/v1/oauth/emailrequest
     * Body: api_key, email (form-urlencoded)
     * Success when JSON.code == 201, message always reported.
     */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload", meta = (WorldContext = "WorldContextObject", Latent, LatentInfo = "LatentInfo", ExpandEnumAsExecs = "Branches", DisplayName = "Request E-Mail Auth Code"))
    static void RequestEMailAuthCode(FLatentActionInfo LatentInfo, const FString& Email, EEmailAuthRequestResult& Branches, bool& bSuccess, FString& Message);
};

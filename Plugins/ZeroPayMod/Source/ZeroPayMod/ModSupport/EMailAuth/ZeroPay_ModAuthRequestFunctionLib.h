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
private:
    static FString UrlEncodeStrict(const FString& In) ;
    static FString FormUrlEncode(const FString& In) ;

public:
    /**
     * Requests a mod.io email security code.
     * POST https://g-<GameID>.modapi.io/v1/oauth/emailrequest
     * Body: api_key, email (form-urlencoded)
     * Success when JSON.code == 201, message always reported.
     */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload", meta = (WorldContext = "WorldContextObject", Latent, LatentInfo = "LatentInfo", ExpandEnumAsExecs = "Branches", DisplayName = "Request E-Mail Auth Code"))
    static void RequestEMailAuthCode(FLatentActionInfo LatentInfo, const FString& Email, EEmailAuthRequestResult& Branches, bool& bSuccess, FString& Message);


    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Upload")
    static inline bool IsValidEMailAddress(const FString& Email)
    {
        if (Email.IsEmpty())
            return false;

        const FString Trimmed = Email.TrimStartAndEnd();
        if (Trimmed.Len() != Email.Len())
            return false;

        // Matches: local part [A-Za-z0-9._%+-]+, "@", domain labels, ".", alpha TLD (>=2)
        static const FRegexPattern Pattern(TEXT("^[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
        FRegexMatcher Matcher(Pattern, Email);

        if (!Matcher.FindNext())
            return false;

        // Ensure full-string match (anchors should make this true; belt-and-braces)
        if (Matcher.GetMatchBeginning() != 0 || Matcher.GetMatchEnding() != Email.Len())
            return false;

        return true;
    }
};

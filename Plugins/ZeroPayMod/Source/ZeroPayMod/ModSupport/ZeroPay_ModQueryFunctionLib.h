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
#include "ZeroPay_ModQueryFunctionLib.generated.h"

USTRUCT(BlueprintType)
struct FZeroPay_ModSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int64 ID = -1;

	UPROPERTY(BlueprintReadOnly)
	FString Username;

	UPROPERTY(BlueprintReadOnly)
	int64 DateUpdated = 0;

	UPROPERTY(BlueprintReadOnly)
	FString Thumb_640;

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Summary;

	UPROPERTY(BlueprintReadOnly)
	float RatingsWeightedAggregate = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int64 Filesize = 0;

	UPROPERTY(BlueprintReadOnly)
	FString FirstTag;
};

USTRUCT(BlueprintType)
struct FZeroPay_ModFilter
{
	GENERATED_BODY()

	/** Max results to return */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Filter")
	int32 Limit = 100 ;

	/** Wildcard match for name (e.g. "VH" matches *VH*) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Filter")
	FString NameLike;

	/** Single tag to filter mods by */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Filter")
	FString TagFilter;

	/** Sort string (e.g. "-name" for descending name) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Filter")
	FString SortBy;
};


DECLARE_DYNAMIC_DELEGATE_OneParam(FOnModListSuccess, const TArray<FZeroPay_ModSummary>&, Mods);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnModListFailure, const FString&, Error);

UCLASS()
class ZEROPAYMOD_API UZeroPay_ModQueryFunctionLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "ZeroPay Modio Support")
	static void QueryFilteredMods(const FZeroPay_ModFilter& Filter, int pageIndex, const FOnModListSuccess& OnSuccess, const FOnModListFailure& OnFailure)
	{
		FHttpModule* Http = &FHttpModule::Get();
		if (!Http) return;

		// Build query string
		TArray<FString> QueryParams;
		QueryParams.Add(FString::Printf(TEXT("_limit=%d"), Filter.Limit));
		if (!Filter.NameLike.IsEmpty())
			QueryParams.Add(FString::Printf(TEXT("name-lk=*%s*"), *Filter.NameLike));
		if (!Filter.TagFilter.IsEmpty())
			QueryParams.Add(FString::Printf(TEXT("tags=%s"), *Filter.TagFilter));
		if (!Filter.SortBy.IsEmpty())
			QueryParams.Add(FString::Printf(TEXT("_sort=%s"), *Filter.SortBy));

		FString FullURL = FString::Printf(
			TEXT("https://g-%s.modapi.io/v1/games/%s/mods?api_key=%s&%s"),
			*FGameID, *FGameID, *FAPIKey, *FString::Join(QueryParams, TEXT("&")));

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
		Request->SetURL(FullURL);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

		Request->OnProcessRequestComplete().BindLambda(
			[OnSuccess, OnFailure](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
			{
				if (!bSuccess || !Res.IsValid() || Res->GetResponseCode() != 200)
				{
					FString Error = Res.IsValid() ? Res->GetContentAsString() : TEXT("No HTTP Response");
					OnFailure.ExecuteIfBound(Error);
					return;
				}

				TSharedPtr<FJsonObject> Root;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());

				if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
				{
					OnFailure.ExecuteIfBound(TEXT("Failed to parse JSON"));
					return;
				}

				const TArray<TSharedPtr<FJsonValue>>* ModsArray;
				if (!Root->TryGetArrayField(TEXT("data"), ModsArray))
				{
					OnFailure.ExecuteIfBound(TEXT("No 'data' array in response"));
					return;
				}

				TArray<FZeroPay_ModSummary> ParsedMods;

				for (const TSharedPtr<FJsonValue>& ModVal : *ModsArray)
				{
					const TSharedPtr<FJsonObject>* ModObj;
					if (!ModVal->TryGetObject(ModObj)) continue;

					FZeroPay_ModSummary Mod;

					(*ModObj)->TryGetNumberField(TEXT("id"), Mod.ID);
					(*ModObj)->TryGetStringField(TEXT("name"), Mod.Name);
					(*ModObj)->TryGetStringField(TEXT("summary"), Mod.Summary);
					(*ModObj)->TryGetNumberField(TEXT("date_updated"), Mod.DateUpdated);

					const TSharedPtr<FJsonObject>* SubmittedBy;
					if ((*ModObj)->TryGetObjectField(TEXT("submitted_by"), SubmittedBy))
					{
						(*SubmittedBy)->TryGetStringField(TEXT("username"), Mod.Username);
					}

					const TSharedPtr<FJsonObject>* Logo;
					if ((*ModObj)->TryGetObjectField(TEXT("logo"), Logo))
					{
						(*Logo)->TryGetStringField(TEXT("thumb_640x360"), Mod.Thumb_640);
					}

					const TSharedPtr<FJsonObject>* Stats;
					if ((*ModObj)->TryGetObjectField(TEXT("stats"), Stats))
					{
						double Rating = 0.0;
						if ((*Stats)->TryGetNumberField(TEXT("ratings_weighted_aggregate"), Rating))
						{
							Mod.RatingsWeightedAggregate = Rating;
						}
					}

					const TSharedPtr<FJsonObject>* Modfile;
					if ((*ModObj)->TryGetObjectField(TEXT("modfile"), Modfile))
					{
						(*Modfile)->TryGetNumberField(TEXT("filesize"), Mod.Filesize);
					}

					const TArray<TSharedPtr<FJsonValue>>* TagsArray;
					if ((*ModObj)->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray->Num() > 0)
					{
						const TSharedPtr<FJsonObject>* FirstTag;
						if ((*TagsArray)[0]->TryGetObject(FirstTag))
						{
							(*FirstTag)->TryGetStringField(TEXT("name"), Mod.FirstTag);
						}
					}

					ParsedMods.Add(Mod);
				}

				OnSuccess.ExecuteIfBound(ParsedMods);
			});

		Request->ProcessRequest();
	}
};

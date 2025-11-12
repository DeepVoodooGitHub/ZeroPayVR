// ZeroPay_ModDeleteMetadataFunctionLib.cpp

#include "ZeroPay_ModDeleteMetadataFunctionLib.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// Your existing header that exposes FGameID (string) like in your EditMod node
#include "ModSupport/ZeroPay_ModGlobal.h"

UDeleteModMetadataAsync* UDeleteModMetadataAsync::DeleteModMetadata(const FString& InAccessToken, int64 InModId, const TArray<FString>& InMetadataEntries)
{
	UDeleteModMetadataAsync* Node = NewObject<UDeleteModMetadataAsync>();
	Node->AccessToken = InAccessToken;
	Node->ModId = InModId;
	Node->MetadataEntries = InMetadataEntries;
	return Node;
}

void UDeleteModMetadataAsync::Activate()
{
	// ----- Basic validation -----
	if (AccessToken.IsEmpty())
	{
		OnFailure.Broadcast(EDeleteMetadataResult::Failure, TEXT("Missing AccessToken."));
		return;
	}
	if (ModId <= 0)
	{
		OnFailure.Broadcast(EDeleteMetadataResult::Failure, TEXT("Invalid ModId."));
		return;
	}
	if (MetadataEntries.Num() == 0)
	{
		OnFailure.Broadcast(EDeleteMetadataResult::Failure, TEXT("At least one metadata[] entry is required (\"key\" or \"key:value\")."));
		return;
	}

	// ----- URL (game-scoped host; matches your edit node style) -----
	const FString Url = FString::Printf(
		TEXT("https://g-%s.modapi.io/v1/games/%s/mods/%lld/metadatakvp"),
		*FGameID, *FGameID, ModId
	);

	// ----- Build application/x-www-form-urlencoded body -----
	// Each entry must be: metadata[]=key  OR  metadata[]=key:value
	TArray<FString> FormPairs;
	FormPairs.Reserve(MetadataEntries.Num());

	for (const FString& Entry : MetadataEntries)
	{
		const FString Clean = Entry.TrimStartAndEnd();

		// Encode the value portion; the field name "metadata[]=" must be url-encoded as metadata%5B%5D=
		const FString EncValue = FGenericPlatformHttp::UrlEncode(Clean);
		FormPairs.Add(FString::Printf(TEXT("metadata%%5B%%5D=%s"), *EncValue));
	}

	const FString BodyForm = FString::Join(FormPairs, TEXT("&"));

	TArray<uint8> BodyUtf8;
	{
		FTCHARToUTF8 Conv(*BodyForm);
		BodyUtf8.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	}

	// ----- HTTP request -----
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("DELETE"));
	Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded")); // IMPORTANT: exact content type
	Req->SetHeader(TEXT("X-Modio-Platform"), TEXT("windows")); // optional but harmless
	Req->SetContent(BodyUtf8);

	Req->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr, FHttpResponsePtr Res, bool bOK)
		{
			auto BuildErrorFromResponse = [&Res]() -> FString
				{
					if (!Res.IsValid())
					{
						return TEXT("No response from server.");
					}

					const int32 Code = Res->GetResponseCode();
					const FString Content = Res->GetContentAsString();

					// Try parse {"error":{"message":"..."}}
					TSharedPtr<FJsonObject> Root;
					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
					if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
					{
						const TSharedPtr<FJsonObject>* ErrObj = nullptr;
						if (Root->TryGetObjectField(TEXT("error"), ErrObj) && ErrObj && ErrObj->IsValid())
						{
							FString Msg;
							if ((*ErrObj)->TryGetStringField(TEXT("message"), Msg) && !Msg.IsEmpty())
							{
								return FString::Printf(TEXT("HTTP %d: %s"), Code, *Msg);
							}
						}
					}

					// Fallback to raw content (trimmed)
					return FString::Printf(TEXT("HTTP %d: %s"), Code, *Content.Left(512));
				};

			bool bSuccess = false;
			FString Msg;

			if (bOK && Res.IsValid())
			{
				const int32 Code = Res->GetResponseCode();
				if ( (Code == 204) || (Code == 200)) // No Content on success, or HTTP 200 (nothing to do)
				{
					bSuccess = true;
					Msg = FString::Printf(TEXT("Deleted %d metadata entr%s on Mod %lld."),
						MetadataEntries.Num(),
						MetadataEntries.Num() == 1 ? TEXT("y") : TEXT("ies"),
						ModId);
				}
				else
				{
					Msg = BuildErrorFromResponse();
				}
			}
			else
			{
				Msg = TEXT("No response from server.");
			}

			if (bSuccess)
			{
				OnSuccess.Broadcast(EDeleteMetadataResult::Success, Msg);
			}
			else
			{
				OnFailure.Broadcast(EDeleteMetadataResult::Failure, Msg);
			}
		});

	Req->ProcessRequest();
}

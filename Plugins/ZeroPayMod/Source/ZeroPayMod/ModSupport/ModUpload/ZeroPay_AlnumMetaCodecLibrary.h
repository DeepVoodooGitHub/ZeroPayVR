#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPay_AlnumMetaCodecLibrary.generated.h"

/**
 * BP-serializable definition for a configurable field.
 */
USTRUCT(BlueprintType)
struct FZeroPaySimpleMeta
{
	GENERATED_BODY()

	/** e.g. "AllowBots" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString Name;

	/** e.g. "bool", "int", "float", "string" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString Type;

	/** e.g. "true", "42", "0.25", "PlayerName" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString DefaultValue;

	/** Optional min bound (for numeric types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString RangeMin;

	/** Optional max bound (for numeric types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString RangeMax;

	/** Tooltip / description for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|Meta")
	FString ToolTip;
};

/**
 * Blueprint utilities to encode/decode FZeroPaySimpleMeta to a strictly alphanumeric string.
 *
 * Encoding format (ASCII, strictly A–Z0–9):
 *   "V1" +
 *   [Len4Hex][Hex(UTF8(Name))] +
 *   [Len4Hex][Hex(UTF8(Type))] +
 *   [Len4Hex][Hex(UTF8(DefaultValue))] +
 *   [Len4Hex][Hex(UTF8(RangeMin))] +
 *   [Len4Hex][Hex(UTF8(RangeMax))] +
 *   [Len4Hex][Hex(UTF8(ToolTip))]
 *
 * Len4Hex = 4 uppercase hex chars (0000–FFFF) giving the number of HEX CHARACTERS that follow.
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_AlnumMetaCodecLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Encode a meta record to a strictly alphanumeric string (A–Z, 0–9). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ZeroPay|MetaCodec", meta = (DisplayName = "Encode Simple Meta (Alphanumeric)"))
	static FString EncodeSimpleMeta(const FZeroPaySimpleMeta& Meta);

	/**
	 * Decode an alphanumeric payload back to a meta record.
	 * @return true if the payload is valid and was fully consumed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|MetaCodec", meta = (DisplayName = "Decode Simple Meta (Alphanumeric)"))
	static bool DecodeSimpleMeta(const FString& Encoded, FZeroPaySimpleMeta& OutMeta);

	/** Convenience: build and encode in one call. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ZeroPay|MetaCodec", meta = (DisplayName = "Make+Encode Simple Meta"))
	static FString MakeAndEncodeSimpleMeta(
		const FString& Name,
		const FString& Type,
		const FString& DefaultValue,
		const FString& RangeMin,
		const FString& RangeMax,
		const FString& ToolTip);

private:
	// Helpers
	static void AppendLenAndHex(FString& Builder, const FString& Field);
	static bool ReadLenAndHex(const FString& Src, int32& Cursor, FString& OutField);
	static FString BytesToHexUpper(const TArray<uint8>& Bytes);
	static bool HexToBytes(const FString& Hex, TArray<uint8>& OutBytes);
};

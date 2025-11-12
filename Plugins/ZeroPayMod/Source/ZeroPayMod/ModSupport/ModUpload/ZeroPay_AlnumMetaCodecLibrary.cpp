#include "ZeroPay_AlnumMetaCodecLibrary.h"
#include "Misc/Char.h"

FString UZeroPay_AlnumMetaCodecLibrary::BytesToHexUpper(const TArray<uint8>& Bytes)
{
	static const TCHAR* HexMap = TEXT("0123456789ABCDEF");
	FString Out;
	Out.Reserve(Bytes.Num() * 2);
	for (uint8 b : Bytes)
	{
		Out.AppendChar(HexMap[(b >> 4) & 0xF]);
		Out.AppendChar(HexMap[b & 0xF]);
	}
	return Out;
}

bool UZeroPay_AlnumMetaCodecLibrary::HexToBytes(const FString& Hex, TArray<uint8>& OutBytes)
{
	if ((Hex.Len() & 1) != 0)
		return false;

	OutBytes.Reset();
	OutBytes.Reserve(Hex.Len() / 2);

	auto Nibble = [](TCHAR c)->int32
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			if (c >= 'a' && c <= 'f') return c - 'a' + 10; // tolerate lowercase
			return -1;
		};

	for (int32 i = 0; i < Hex.Len(); i += 2)
	{
		const int32 hi = Nibble(Hex[i]);
		const int32 lo = Nibble(Hex[i + 1]);
		if (hi < 0 || lo < 0)
			return false;

		OutBytes.Add(uint8((hi << 4) | lo));
	}
	return true;
}

void UZeroPay_AlnumMetaCodecLibrary::AppendLenAndHex(FString& Builder, const FString& Field)
{
	// UTF-8 encode
	TArray<uint8> Utf8;
	{
		FTCHARToUTF8 Conv(*Field);
		Utf8.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	}

	// Hex encode (uppercase)
	const FString Hex = BytesToHexUpper(Utf8);

	// 4-hex-digit length of the HEX string
	const FString Len4 = FString::Printf(TEXT("%04X"), Hex.Len());

	Builder += Len4;
	Builder += Hex;
}

bool UZeroPay_AlnumMetaCodecLibrary::ReadLenAndHex(const FString& Src, int32& Cursor, FString& OutField)
{
	if (Cursor + 4 > Src.Len()) return false;

	// Read 4 hex digits for the length
	const FString Len4 = Src.Mid(Cursor, 4);
	Cursor += 4;

	int32 HexLen = 0;
	for (int i = 0; i < 4; ++i)
	{
		TCHAR c = Len4[i];
		int32 n = 0;
		if (c >= '0' && c <= '9') n = c - '0';
		else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
		else return false;

		HexLen = (HexLen << 4) | n;
	}

	if (HexLen < 0 || Cursor + HexLen > Src.Len())
		return false;

	const FString Hex = Src.Mid(Cursor, HexLen);
	Cursor += HexLen;

	TArray<uint8> Bytes;
	if (!HexToBytes(Hex, Bytes))
		return false;

	// Ensure null-terminated UTF-8 buffer before conversion
	Bytes.Add(0);

	OutField = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
	return true;
}

FString UZeroPay_AlnumMetaCodecLibrary::EncodeSimpleMeta(const FZeroPaySimpleMeta& Meta)
{
	FString Out(TEXT("V1"));
	AppendLenAndHex(Out, Meta.Name);
	AppendLenAndHex(Out, Meta.Type);
	AppendLenAndHex(Out, Meta.DefaultValue);
	AppendLenAndHex(Out, Meta.RangeMin);
	AppendLenAndHex(Out, Meta.RangeMax);
	AppendLenAndHex(Out, Meta.ToolTip);
	return Out; // strictly A–Z0–9
}

bool UZeroPay_AlnumMetaCodecLibrary::DecodeSimpleMeta(const FString& Encoded, FZeroPaySimpleMeta& OutMeta)
{
	if (!Encoded.StartsWith(TEXT("V1")))
		return false;

	int32 Cur = 2;
	const bool bOk =
		ReadLenAndHex(Encoded, Cur, OutMeta.Name) &&
		ReadLenAndHex(Encoded, Cur, OutMeta.Type) &&
		ReadLenAndHex(Encoded, Cur, OutMeta.DefaultValue) &&
		ReadLenAndHex(Encoded, Cur, OutMeta.RangeMin) &&
		ReadLenAndHex(Encoded, Cur, OutMeta.RangeMax) &&
		ReadLenAndHex(Encoded, Cur, OutMeta.ToolTip) &&
		Cur == Encoded.Len(); // must consume exactly

	return bOk;
}

FString UZeroPay_AlnumMetaCodecLibrary::MakeAndEncodeSimpleMeta(
	const FString& Name,
	const FString& Type,
	const FString& DefaultValue,
	const FString& RangeMin,
	const FString& RangeMax,
	const FString& ToolTip)
{
	FZeroPaySimpleMeta M;
	M.Name = Name;
	M.Type = Type;
	M.DefaultValue = DefaultValue;
	M.RangeMin = RangeMin;
	M.RangeMax = RangeMax;
	M.ToolTip = ToolTip;
	return EncodeSimpleMeta(M);
}

#include "ZeroPay_ModExtractSettings.h"

#include "Internationalization/Text.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "StructUtils/InstancedStruct.h" // FInstancedStruct

// ====================== Internal helpers (cpp-only) ======================
namespace
{
	static FString FriendlyType(const FProperty* P)
	{
		if (!P) return TEXT("Unknown");

		// Primitives
		if (P->IsA<FInt8Property>())   return TEXT("int8");
		if (P->IsA<FInt16Property>())  return TEXT("int16");
		if (P->IsA<FIntProperty>())    return TEXT("int32");
		if (P->IsA<FInt64Property>())  return TEXT("int64");
		if (P->IsA<FFloatProperty>())  return TEXT("float");
		if (P->IsA<FDoubleProperty>()) return TEXT("double");
		if (P->IsA<FBoolProperty>())   return TEXT("bool");
		if (P->IsA<FStrProperty>())    return TEXT("string");
		if (P->IsA<FNameProperty>())   return TEXT("name");
		if (P->IsA<FTextProperty>())   return TEXT("text");

		// Enums
		if (const FEnumProperty* EP = CastField<FEnumProperty>(P))
		{
			return FString::Printf(TEXT("enum %s"), *EP->GetEnum()->GetName());
		}
		if (const FByteProperty* BP = CastField<FByteProperty>(P))
		{
			if (BP->Enum) return FString::Printf(TEXT("enum(byte) %s"), *BP->Enum->GetName());
			return TEXT("byte");
		}

		// Containers
		if (const FArrayProperty* AP = CastField<FArrayProperty>(P))
		{
			return FString::Printf(TEXT("TArray<%s>"), *FriendlyType(AP->Inner));
		}
		if (const FMapProperty* MP = CastField<FMapProperty>(P))
		{
			return FString::Printf(TEXT("TMap<%s, %s>"),
				*FriendlyType(MP->KeyProp), *FriendlyType(MP->ValueProp));
		}
		if (const FSetProperty* SP = CastField<FSetProperty>(P))
		{
			return FString::Printf(TEXT("TSet<%s>"), *FriendlyType(SP->ElementProp));
		}

		// Structs & Objects
		if (const FStructProperty* SP = CastField<FStructProperty>(P))
		{
			return FString::Printf(TEXT("struct %s"), *SP->Struct->GetName());
		}
		if (const FObjectProperty* OP = CastField<FObjectProperty>(P))
		{
			return FString::Printf(TEXT("UObject %s*"), *OP->PropertyClass->GetName());
		}
		if (const FClassProperty* CP = CastField<FClassProperty>(P))
		{
			return FString::Printf(TEXT("TSubclassOf<%s>"), *CP->MetaClass->GetName());
		}

		// Fallback
		return P->GetCPPType();
	}

	static FString ExportValue(const FProperty* P, const void* Container)
	{
		if (!P || !Container) return TEXT("");
		FString Out;
		P->ExportText_InContainer(0, Out, Container, Container, nullptr, PPF_None);
		return Out;
	}

	static FText PropToolTip(const FProperty* P)
	{
		if (!P) return FText::GetEmpty();
		if (P->HasMetaData(TEXT("ToolTip")))
		{
			return FText::FromString(P->GetMetaData(TEXT("ToolTip")));
		}
		const FText Auto = P->GetToolTipText();
		if (!Auto.IsEmpty())
		{
			return Auto;
		}
		if (P->HasMetaData(TEXT("DisplayName")))
		{
			return FText::FromString(P->GetMetaData(TEXT("DisplayName")));
		}
		return FText::FromName(P->GetFName());
	}

	// Pull min/max for numeric properties using common UE meta keys.
	// Precedence: ClampMin/ClampMax -> UIMin/UIMax -> SliderMin/SliderMax.
	static void GetNumericMinMaxStrings(const FProperty* P, FString& OutMin, FString& OutMax)
	{
		OutMin.Reset();
		OutMax.Reset();

		const FNumericProperty* Num = CastField<FNumericProperty>(P);
		if (!Num || Num->IsEnum()) // enums are numeric underneath but shouldn't expose min/max as numeric ranges here
		{
			return;
		}

		auto TryGet = [P](const TCHAR* Key) -> FString
			{
				return P->HasMetaData(Key) ? P->GetMetaData(Key) : FString();
			};

		// 1) Clamp*
		FString MinS = TryGet(TEXT("ClampMin"));
		FString MaxS = TryGet(TEXT("ClampMax"));

		// 2) UI*
		if (MinS.IsEmpty()) MinS = TryGet(TEXT("UIMin"));
		if (MaxS.IsEmpty()) MaxS = TryGet(TEXT("UIMax"));

		// 3) Slider*
		if (MinS.IsEmpty()) MinS = TryGet(TEXT("SliderMin"));
		if (MaxS.IsEmpty()) MaxS = TryGet(TEXT("SliderMax"));

		// For ints, authors might write "0.0"; that's fine, we keep strings verbatim.
		OutMin = MoveTemp(MinS);
		OutMax = MoveTemp(MaxS);
	}

	// RAII default instance for a given UScriptStruct type
	struct FScopedDefaultStruct
	{
		const UScriptStruct* Type = nullptr;
		void* Ptr = nullptr;

		explicit FScopedDefaultStruct(const UScriptStruct* InType)
			: Type(InType)
		{
			if (Type)
			{
				Ptr = FMemory::Malloc(Type->GetStructureSize());
				FMemory::Memzero(Ptr, Type->GetStructureSize());
				Type->InitializeStruct(Ptr);
			}
		}
		~FScopedDefaultStruct()
		{
			if (Type && Ptr)
			{
				Type->DestroyStruct(Ptr);
				FMemory::Free(Ptr);
				Ptr = nullptr;
			}
		}
	};

	static FString GetDisplayOrCleanName(const FProperty* Prop)
	{
		if (!Prop)
			return TEXT("");

		if (Prop->HasMetaData(TEXT("DisplayName")))
		{
			return Prop->GetMetaData(TEXT("DisplayName"));
		}

		// Otherwise, strip BP's internal suffix like "AllowBots_2_GUID"
		FString Name = Prop->GetName();
		int32 UnderscoreIndex;
		if (Name.FindChar(TEXT('_'), UnderscoreIndex))
		{
			const FString After = Name.Mid(UnderscoreIndex + 1);
			if (After.Len() > 2 && After[0] >= '0' && After[0] <= '9')
			{
				Name = Name.Left(UnderscoreIndex);
			}
		}
		return Name;
	}

	static TArray<FStructPropertyReportRow> Inspect_Internal(const UScriptStruct* Type, const void* InstanceData)
	{
		TArray<FStructPropertyReportRow> Rows;

		if (!Type)
		{
			UE_LOG(LogTemp, Warning, TEXT("Inspect: StructType is null."));
			return Rows;
		}

		const void* DataToRead = InstanceData;
		FScopedDefaultStruct DefaultInstance(Type);
		if (!DataToRead)
		{
			DataToRead = DefaultInstance.Ptr; // default-initialized instance
		}

		for (TFieldIterator<FProperty> It(const_cast<UScriptStruct*>(Type)); It; ++It)
		{
			const FProperty* P = *It;
			if (!P) continue;

			FStructPropertyReportRow R;
			R.Name = FName(*GetDisplayOrCleanName(P));
			R.Type = FriendlyType(P);
			R.DefaultValue = ExportValue(P, DataToRead);
			R.ToolTip = PropToolTip(P);

			// NEW: capture numeric min/max if available
			GetNumericMinMaxStrings(P, R.MinValue, R.MaxValue);

			Rows.Add(MoveTemp(R));
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogTemp, Verbose, TEXT("Inspect: %s -> %d fields"), *Type->GetName(), Rows.Num());
#endif
		return Rows;
	}
} // namespace

// ====================== UZeroPay_ModExtractSettings ======================

TArray<FStructPropertyReportRow> UZeroPay_ModExtractSettings::InspectStructDefaults(const UScriptStruct* StructType)
{
	return Inspect_Internal(StructType, nullptr);
}

TArray<FStructPropertyReportRow> UZeroPay_ModExtractSettings::InspectInstancedStruct(const FInstancedStruct& Instance)
{
	if (!Instance.IsValid() || !Instance.GetScriptStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("InspectInstancedStruct: invalid instance or type."));
		return {};
	}
	return Inspect_Internal(Instance.GetScriptStruct(), Instance.GetMemory());
}

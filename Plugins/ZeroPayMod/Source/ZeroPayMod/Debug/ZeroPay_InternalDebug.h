#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "ZeroPay_InternalDebug.generated.h"

// Forward declaration so UE generates boilerplate
UINTERFACE(Blueprintable)
class ZEROPAYMOD_API UZeroPay_PrintInternalString_Interface : public UInterface
{
	GENERATED_BODY()
};

class ZEROPAYMOD_API IZeroPay_PrintInternalString_Interface
{
	GENERATED_BODY()

public:
	/* Log a message to the somewhere (can be implemented in BP) */
	UFUNCTION(BlueprintNativeEvent, Category = "ZeroPay Internal PrintString")
	void PrintInternalString(const FString& ObjectName, const FString& LogText, FDebugConsoleLevel Level);
};

// Forward declaration so UE generates boilerplate
UINTERFACE(Blueprintable)
class ZEROPAYMOD_API UZeroPay_DebugConsole_Interface : public UInterface
{
	GENERATED_BODY()
};

class ZEROPAYMOD_API IZeroPay_DebugConsole_Interface
{
	GENERATED_BODY()

public:
	/** Log a message to the console (can be implemented in BP) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ZeroPay Debug Console")
	void LogToDebugConsole(const FString& ObjectName, const FString& LogText, FDebugConsoleLevel Level);
};

UCLASS()
class ZEROPAYMOD_API UZeroPay_InternalDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Internal print string, useful in editor and game (shown on main menu 'logs', any anything else that implements ZeroPay_InternalDebugConsole_Interface
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target", WorldContext = "WorldContextObject", CallableWithoutWorldContext, AdvancedDisplay = "debugConsoleLevel, bIncludeObjectName"))
	static void PrintInternalString(const UObject* WorldContextObject, AActor* target, const FString& value = "", FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true)
	{
		// Note the GameInstance uses a null target, as it's not an AActor so we default to that here
		FString ObjectName = "[GameInstance]";

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		FString Prefix;
		if (World)
		{
			if (World->WorldType == EWorldType::PIE)
			{
				switch (World->GetNetMode())
				{
				case NM_Client:
					// GPlayInEditorID 0 is always the server, so 1 will be first client.
					// You want to keep this logic in sync with GeneratePIEViewportWindowTitle and UpdatePlayInEditorWorldDebugString
					Prefix = FString::Printf(TEXT("Client %d: "), UE::GetPlayInEditorID());
					break;
				case NM_DedicatedServer:
				case NM_ListenServer:
					Prefix = FString::Printf(TEXT("Server: "));
					break;
				case NM_Standalone:
					break;
				}
			}
		}
		// Append
		FString combinedPrefixValue = Prefix + value;

		/* Include name  */
		if (bIncludeObjectName)
		{
			if (target != nullptr)
			{
				ObjectName = target->GetFName().ToString();

				/* Strip UAID */
				int32 Index = ObjectName.Find(TEXT("_UAID_"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
				if (Index != INDEX_NONE)
					ObjectName = ObjectName.Left(Index); // Keep everything before _UAID_
			}
		}

		/* Also replicate to anything "listening" via the IZeroPay_DebugConsole_Interface */
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Widget = *It;

			// Must be valid, not pending kill, and implement the interface
			if (IsValid(Widget) && Widget->GetClass()->ImplementsInterface(UZeroPay_PrintInternalString_Interface::StaticClass()))
			{
				IZeroPay_PrintInternalString_Interface::Execute_PrintInternalString(
					Widget,
					*ObjectName,
					*combinedPrefixValue,
					debugConsoleLevel
				);
			}
		}
	}
} ;
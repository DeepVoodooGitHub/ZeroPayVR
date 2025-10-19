#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "EngineUtils.h"
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
	/* Can accept a message? I.e READY */
	UFUNCTION(BlueprintNativeEvent, Category = "ZeroPay Internal PrintString")
	bool IsPrintInternalReady();

	/* Log a message to the somewhere (can be implemented in BP) */
	UFUNCTION(BlueprintNativeEvent, Category = "ZeroPay Internal PrintString")
	void PrintInternalString(const FString& Prefix, const FString& ObjectName, const FString& LogText, FDebugConsoleLevel Level);
};

struct ZEROPAYMOD_API FZeroPayStoredPrintStringParams
{
	FString Prefix;
	FString ObjectName;
	FString Value;
	FDebugConsoleLevel DebugConsoleLevel = FDebugConsoleLevel::Log;
};

/* Global vars (until we find a better solution) - Reset via ZeroPay_GameInstance_r1 Init (to avoid crashes on multiple PIE plays) */
static TArray<FZeroPayStoredPrintStringParams> StoredLogEntries;
static TWeakObjectPtr<AActor> InternalDebugTargetActor = nullptr ;
static bool bPipeToPrintString = true ;
static bool bPipeToLogOutput = true;

UCLASS()
class ZEROPAYMOD_API UZeroPay_InternalDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

private:
	/* Internal helper to push any message to screen or logs.. */
	static void PipeEntryAsRequired(FString Prefix, FString ObjectName, FString Value,	FDebugConsoleLevel debugConsoleLevel)
	{
		/* Echo to screen, if exists? */
		if (bPipeToPrintString)
		{
			if (GEngine)
			{
				const FString FullMessage = Prefix + TEXT(" ") + Value;
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FullMessage);
			}
		}
		/* Echo to logs? */
		if (bPipeToLogOutput)
		{
			const FString FullMessage = Prefix + TEXT(" ") + Value;

			switch (debugConsoleLevel)
			{
			case FDebugConsoleLevel::Log:
				UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
				break;

			case FDebugConsoleLevel::Warn:
				UE_LOG(LogTemp, Warning, TEXT("%s"), *FullMessage);
				break;

			case FDebugConsoleLevel::Error:
				UE_LOG(LogTemp, Error, TEXT("%s"), *FullMessage);
				break;

			default:
				UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
				break;
			}
		}

	}


public:
	// Register an actor to receive debug messages (there can only be one..)
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target") )
	static void RegisterInternalStringTarget(AActor* target = nullptr)
	{
		if (target->GetClass()->ImplementsInterface(UZeroPay_PrintInternalString_Interface::StaticClass()))
			InternalDebugTargetActor = target;
		else
			UE_LOG(LogTemp, Error, TEXT("Actor %s is not a valid target, does not implement ZeroPay_PrintInternalString_Interface"), *target->GetName());
	}

	// Internal print string, useful in editor and game (shown on main menu 'logs', any anything else that implements ZeroPay_InternalDebugConsole_Interface
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target", WorldContext = "WorldContextObject", CallableWithoutWorldContext, AdvancedDisplay = "WorldContextObject, debugConsoleLevel, bIncludeObjectName"))
	static void PrintInternalString(const UObject* WorldContextObject, AActor* target = nullptr, const FString& value = "", FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true)
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

		/* Include name? */
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

		bool bOutputSunk = false ;
		if (InternalDebugTargetActor.IsValid())
		{
			if (IZeroPay_PrintInternalString_Interface::Execute_IsPrintInternalReady(InternalDebugTargetActor.Get()))
			{
				/* Any stored data for when we had no actor to sink the logs? */
				if (StoredLogEntries.Num() > 0)
				{
					for (const FZeroPayStoredPrintStringParams& Entry : StoredLogEntries)
					{
						IZeroPay_PrintInternalString_Interface::Execute_PrintInternalString(InternalDebugTargetActor.Get(), Entry.Prefix, Entry.ObjectName, Entry.Value, Entry.DebugConsoleLevel);
						PipeEntryAsRequired(Entry.Prefix, Entry.ObjectName, Entry.Value, Entry.DebugConsoleLevel);
					}
					StoredLogEntries.Empty();
				}

				/* Send data */
				IZeroPay_PrintInternalString_Interface::Execute_PrintInternalString(InternalDebugTargetActor.Get(), Prefix, ObjectName, value, debugConsoleLevel);
				PipeEntryAsRequired(Prefix, ObjectName, value, debugConsoleLevel);
				bOutputSunk = true;
			}
		}

		if (!bOutputSunk)
		{
			/* Standard UE logs.. */
			FString FinalLog = FString::Printf(TEXT("%s(%s): %s"), *Prefix, *ObjectName, *value);

			switch (debugConsoleLevel)
			{
			case FDebugConsoleLevel::Log:
				UE_LOG(LogTemp, Log, TEXT("%s"), *FinalLog);
				break;

			case FDebugConsoleLevel::Warn:
				UE_LOG(LogTemp, Warning, TEXT("%s"), *FinalLog);
				break;

			case FDebugConsoleLevel::Error:
				UE_LOG(LogTemp, Error, TEXT("%s"), *FinalLog);
				break;

			default:
				UE_LOG(LogTemp, Log, TEXT("%s"), *FinalLog);
				break;
			}

			// Create a new log entry struct
			FZeroPayStoredPrintStringParams NewEntry;
			NewEntry.Prefix = Prefix;
			NewEntry.ObjectName = ObjectName;
			NewEntry.Value = value;
			NewEntry.DebugConsoleLevel = debugConsoleLevel;

			// Add it to the array
			StoredLogEntries.Add(NewEntry);
			return;
		}
	}
} ;


#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "EngineUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPayMod.h"
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

	/* Implemented in debug console (or other if you desire) */
	UFUNCTION(BlueprintNativeEvent, Category = "ZeroPay Internal PrintString")
	void PrintInternalStringImpl(const FString& ExecutionZone, const FString& ClientID, FDebugConsoleLevel DebugLevel, const FString& Timestamp, const FString& ObjectName, const FString& LogText);
};

struct ZEROPAYMOD_API FZeroPayStoredPrintInternalStringParams
{
	FString ExecutionZone;
	FString ClientID;
	FDebugConsoleLevel DebugLevel = FDebugConsoleLevel::Log;
	FString Timestamp;
	FString ObjectName;
	FString LogText;
};

/* Global vars (until we find a better solution) - Reset via ZeroPay_GameInstance_r1 Init (to avoid crashes on multiple PIE plays) */
static TArray<FZeroPayStoredPrintInternalStringParams> StoredLogEntries;
static TWeakObjectPtr<AActor> InternalDebugTargetActor = nullptr ;
static bool bPipeToClientPrintString = true;
	/* If set, on client, print string the message (so user can see in PIE or debug client build) */
static bool bPipeToClientLogOutput = true;
	/* If set, on client, write to LogZeroPay log output */

UCLASS()
class ZEROPAYMOD_API UZeroPay_InternalDebugFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

private:
	/* Internal helper to push any message to screen or logs.. */
	static void PipeEntryAsRequired(const FString& ExecutionZone, const FString& ClientID, FDebugConsoleLevel DebugLevel, const FString& Timestamp, const FString& ObjectName, const FString& LogText)
	{
		/* Dedicated server pumps directly on PrintInternalString to log */
		if (IsRunningDedicatedServer())
			return;

		if (!bPipeToClientPrintString && !bPipeToClientLogOutput)
			return ;

		const FString FullMessage = TEXT("[") + Timestamp + TEXT("] (") + ExecutionZone + TEXT(" ") + ClientID + TEXT(") [") + UEnum::GetValueAsString(DebugLevel) + TEXT("] (") + ObjectName + TEXT(") ") + LogText ;

		/* Echo to screen, if exists? */
		if (bPipeToClientPrintString)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FullMessage);
			}
		}
		/* Echo to logs? */
		if (bPipeToClientLogOutput)
		{
			switch (DebugLevel)
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
	static void PrintInternalString(const UObject* WorldContextObject, UObject* target, const FString& value = "", FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true)
	{
		// Note the GameInstance uses a null target, as it's not an AActor so we default to that here
		FString ExecutionZone = "Local";  // or Server
		FString ClientID = "-"; // Single digit ID
		FString Timestamp = "----.---";
		FString ObjectName = "[GameInstance]";		// We assume null = target is game instance
		FString LogText = value;

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		FString Prefix;
		if (World)
		{
			switch (World->GetNetMode())
				{
				case NM_Client:
				{
					/* PIE includes a client ID we can use to know "who" we are when multiple windows are open for multi-user PIE testing */
					if (World->WorldType == EWorldType::PIE)
					{
						ExecutionZone = "Clnt:";
						ClientID = FString::Printf(TEXT("%d"), UE::GetPlayInEditorID());
					}
					else
					{
						/* Just use "client" */
						ExecutionZone = "Clien";
						ClientID = "t";
					}
					break;
				}
				case NM_DedicatedServer:
				{
					ExecutionZone = "DSver";
					ClientID = " ";
					break;
				}
				case NM_ListenServer:
				{
					ExecutionZone = "LSver";
					ClientID = " ";
					break;
				}
				case NM_Standalone:
				{
					ExecutionZone = "Alone";
					ClientID = " ";
					break;
				}
			}
		
			/* If we have a world, get the time.. */
			Timestamp = FString::Printf(TEXT("%08.3f"), World->GetTimeSeconds());
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

		bool bOutputSunk = false;
		if (InternalDebugTargetActor.IsValid())
		{
			if (IZeroPay_PrintInternalString_Interface::Execute_IsPrintInternalReady(InternalDebugTargetActor.Get()))
			{
				/* Any stored data for when we had no actor to sink the logs? */
				if (StoredLogEntries.Num() > 0)
				{
					for (const FZeroPayStoredPrintInternalStringParams& Entry : StoredLogEntries)
					{
						IZeroPay_PrintInternalString_Interface::Execute_PrintInternalStringImpl(InternalDebugTargetActor.Get(), Entry.ExecutionZone, Entry.ClientID, Entry.DebugLevel, Entry.Timestamp, Entry.ObjectName, Entry.LogText);
						PipeEntryAsRequired(Entry.ExecutionZone, Entry.ClientID, Entry.DebugLevel, Entry.Timestamp, Entry.ObjectName, Entry.LogText);
					}
					StoredLogEntries.Empty();
				}

				/* Debug target is active, send directly to it  */
				IZeroPay_PrintInternalString_Interface::Execute_PrintInternalStringImpl(InternalDebugTargetActor.Get(), ExecutionZone, ClientID, debugConsoleLevel, Timestamp, ObjectName, LogText);
				PipeEntryAsRequired(ExecutionZone, ClientID, debugConsoleLevel, Timestamp, ObjectName, LogText);
				bOutputSunk = true;
			}
		}
		
		/* Did we fail to write? Record it.. */
		if (!bOutputSunk)
		{
			// Create a new log entry struct
			FZeroPayStoredPrintInternalStringParams NewEntry;
			NewEntry.ExecutionZone = ExecutionZone;
			NewEntry.ClientID = ClientID;
			NewEntry.DebugLevel = debugConsoleLevel;
			NewEntry.Timestamp = Timestamp;
			NewEntry.ObjectName = ObjectName;
			NewEntry.LogText = LogText;

			// Add it to the array (but don't grow if we're too big; they may not have any debug enabled and/or don't care...)
			if (StoredLogEntries.Num() < 256)
				StoredLogEntries.Add(NewEntry);
		}

		// Dedicated server's just log to standard UE5 output
		if (IsRunningDedicatedServer())
		{
			const FString FullMessage = TEXT("[") + Timestamp + TEXT("] (") + ExecutionZone + TEXT(" ") + ClientID + TEXT(") (") + ObjectName + TEXT(") ") + LogText;
			switch (debugConsoleLevel)
			{
			case FDebugConsoleLevel::None:
			case FDebugConsoleLevel::Log:
			{
				// Green text
				UE_LOG(LogZeroPay, Log, TEXT("\x1b[32m%s\x1b[0m"), *FullMessage);
				break;
			}

			case FDebugConsoleLevel::Warn:
			{
				// Yellow warnings
				UE_LOG(LogZeroPay, Warning, TEXT("\x1b[33m%s\x1b[0m"), *FullMessage);
				break;
			}

			case FDebugConsoleLevel::Error:
			{
				// Red errors
				UE_LOG(LogZeroPay, Error, TEXT("\x1b[31m%s\x1b[0m"), *FullMessage);
				break;
			}
			}
		}
	}
} ;


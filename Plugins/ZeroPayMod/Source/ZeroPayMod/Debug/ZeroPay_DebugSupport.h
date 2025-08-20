// (c) Ginger Ninja Games Ltd

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPayMod.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "ZeroPay_DebugConsoleComponent.h"
#include "ZeroPay_DebugSupport.generated.h"

static bool bClientOutputLogsToDisk = false;

UCLASS()
class ZEROPAYMOD_API UZeroPay_DebugSupport : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:	

	// CLIENT: Adds a debug line to the in-world console (if it exists) this is replicated
	// across the network and reports whether the node executed on the server or client
	// Can be disabled for this entire object by using SetDebugConsoleEnabled()
	// SERVER: This will always output to the console, with colour coding (Log = Green, Warn = Yellow, Error = Red)
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target", AdvancedDisplay = "debugConsoleLevel, bIncludeObjectName"))
	static void AddDebugConsoleLine(AActor* target, const FString& value = "", FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true)
	{
		// Note the GameInstance uses a null target, as it's not an AActor so we default to that here
		FString ObjectName = "[GameInstance]";

		// Dedicated server's just log to standard UE5 output
		if (IsRunningDedicatedServer() || target == nullptr || bClientOutputLogsToDisk)
		{
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

			switch (debugConsoleLevel)
			{
				case None:
				case Log:
				{
					/* Output in Green the message */
					UE_LOG(LogZeroPay, Log, TEXT("\x1b[93m(%s)\x1b[0m \x1b[32m%s\x1b[0m"), *ObjectName, *value);
					break;
				}
				case Warn:
				{
					/* Yellow warnings */
					UE_LOG(LogZeroPay, Warning, TEXT("\x1b[93m(%s)\x1b[0m \x1b[33m%s\x1b[0m"), *ObjectName, *value);
					break;
				}
				case Error:
				{
					/* Red errors */
					UE_LOG(LogZeroPay, Error, TEXT("\x1b[93m(%s)\x1b[0m \x1b[31m%s\x1b[0m"), *ObjectName, *value);
					break;
				}
			}
		}

		if (target)
		{
			UZeroPay_DebugConsoleComponent* DebugConsoleComponent = target->FindComponentByClass<UZeroPay_DebugConsoleComponent>();

			if (!DebugConsoleComponent)
			{
				DebugConsoleComponent = NewObject<UZeroPay_DebugConsoleComponent>(target);
				DebugConsoleComponent->RegisterComponent(); // Make sure it gets ticking/network support

				UE_LOG(LogZeroPay, Warning, TEXT("AddDebugConsoleLine() called on actor %s with ZeroPay_DebugConsole component. Created component but first output line may not RPC correctly or disconnection will occur!"), *target->GetName());
			}

			DebugConsoleComponent->AddDebugConsoleLine(debugConsoleLevel, bIncludeObjectName, value);
		}

	}

	// Disable (for this actor) any AddDebugConsoleLine's
	// This MUST be called prior to any AddDebugConsoleLine lines to disable
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target"))
	static void SetDebugConsoleDisabled(AActor* target, bool bDisableDebugOutput = false)
	{
		if (!target) return;

		UZeroPay_DebugConsoleComponent* DebugConsoleComponent = target->FindComponentByClass<UZeroPay_DebugConsoleComponent>();

		if (!DebugConsoleComponent)
		{
			DebugConsoleComponent = NewObject<UZeroPay_DebugConsoleComponent>(target);
			DebugConsoleComponent->RegisterComponent(); // Make sure it gets ticking/network support

			UE_LOG(LogZeroPay, Warning, TEXT("SetDebugConsoleDisabled() called on actor %s with ZeroPay_DebugConsole component. Created component but first output line may not RPC correctly or disconnection will occur!"), *target->GetName());
		}

		DebugConsoleComponent->SetDebugConsoleDisabled(bDisableDebugOutput);
	}

	// All "Add Debug Console Logs" will be piped to log file (client only)
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target"))
	static void SetDebugConsoleToLogFile(bool bEnableLogOutput)
	{
		bClientOutputLogsToDisk = true;
	}
	
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "ZeroPay_DebugConsoleComponent.h"
#include "ZeroPay_DebugConsole.generated.h"

UCLASS()
class ZEROPAYMOD_API AZeroPay_DebugConsole : public AActor
{
	GENERATED_BODY()
	
public:	
	AZeroPay_DebugConsole();

protected:
	virtual void BeginPlay() override;

public:	

	// Adds a debug line to the in-world console (if it exists) this is replicated
	// across the network and reports whether the node executed on the server or client
	// Can be disabled for this entire object by using SetDebugConsoleEnabled()
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target"))
	static void AddDebugConsoleLine(AActor* target, FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true, const FString& value = "")
	{
		// Dedicated server's just log to standard UE5 output
		if (IsRunningDedicatedServer())
		{
			switch(debugConsoleLevel)
			{
				case None:
				case Log:
				case Warn:
				{
					/* We always print it as "warning" (orange) so that it's clear to see what's happening.. */
					UE_LOG(LogTemp, Warning, TEXT("%s"), *value);
					return;
				}
				case Error:
				{
					UE_LOG(LogTemp, Error, TEXT("%s"), *value);
					return;
				}
				
			}
			return;
		}			

		if (!target) return;

		UZeroPay_DebugConsoleComponent* DebugConsoleComponent = target->FindComponentByClass<UZeroPay_DebugConsoleComponent>();

		if (!DebugConsoleComponent)
		{
			DebugConsoleComponent = NewObject<UZeroPay_DebugConsoleComponent>(target);
			DebugConsoleComponent->RegisterComponent(); // Make sure it gets ticking/network support

			UE_LOG(LogTemp, Warning, TEXT("AddDebugConsoleLine() called on actor %s with ZeroPay_DebugConsole component. Created component but first output line may not RPC correctly or disconnection will occur!"), *target->GetName());
		}

		DebugConsoleComponent->AddDebugConsoleLine(debugConsoleLevel, bIncludeObjectName, value );
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

			UE_LOG(LogTemp, Warning, TEXT("SetDebugConsoleDisabled() called on actor %s with ZeroPay_DebugConsole component. Created component but first output line may not RPC correctly or disconnection will occur!"), *target->GetName());
		}

		DebugConsoleComponent->SetDebugConsoleDisabled(bDisableDebugOutput);
	}
	
};

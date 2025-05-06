// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "ZeroPay_DebugConsole.generated.h"

UENUM(BlueprintType)
enum FDebugConsoleLevel
{
	None,
	Log,
	Warn,
	Error
};

UCLASS()
class ZEROPAYMOD_API AZeroPay_DebugConsole : public AActor
{
	GENERATED_BODY()
	
public:	
	AZeroPay_DebugConsole();

protected:
	virtual void BeginPlay() override;

public:	

	// Debug helper functions
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Debug", meta = (DefaultToSelf = "target"))
	static void AddDebugConsoleLine(AActor* target, FDebugConsoleLevel debugConsoleLevel = Log, const FString& value = "")
	{
		if (!IsValid(target))
			return;
		UWorld* MyWorld = target->GetWorld();
		if (!IsValid(MyWorld))
			return;

		/* Find console BP Class */
		FString BPClassPath = FString("Blueprint'/ZeroPayMod/Blueprints/Library/DebugUtils/BP_ZPVR_DebugConsole.BP_ZPVR_DebugConsole_C'");
		TSubclassOf<AActor> DebugConsoleBPClass = Cast<UClass>(StaticLoadObject(UObject::StaticClass(), nullptr, *BPClassPath));
		if (DebugConsoleBPClass == nullptr)
			return ;

		/* Append time */
		FString TimeString = FString::SanitizeFloat(MyWorld->GetTimeSeconds()) + TEXT(" ") ;

		/* Build output string */
		FString OutputString ; 
		switch (debugConsoleLevel)
		{
			case Log: OutputString = TEXT("[Log] ") + TimeString + value; break;
			case Warn: OutputString = TEXT("[Warn] ") + TimeString + value; break;
			case Error: OutputString = TEXT("[Error] ") + TimeString + value; break;
			default: OutputString = TimeString + value; break;
		}


		/* Find all actors */
		TArray<AActor*> FoundDebugConsoleActors ;
		UGameplayStatics::GetAllActorsOfClass(target->GetWorld(), DebugConsoleBPClass, FoundDebugConsoleActors);

		/* Iterate them.. */
		for (int nCounter = 0; nCounter < FoundDebugConsoleActors.Num(); nCounter++)
		{
			/* Execute a BP function on that actor */
			FOutputDeviceNull ar;
			char funcCallBuf[2048];
			_snprintf_s(funcCallBuf, sizeof(funcCallBuf), "AddLine \"%s\"", TCHAR_TO_ANSI(*OutputString) );
			FoundDebugConsoleActors[nCounter]->CallFunctionByNameWithArguments(ANSI_TO_TCHAR(funcCallBuf), ar, NULL, true);
		}

	}


	
};

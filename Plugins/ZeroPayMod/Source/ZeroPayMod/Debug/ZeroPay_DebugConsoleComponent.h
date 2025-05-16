// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Support/ZeroPay_MiscSupportUtils.h"
#include "ZeroPay_DebugConsoleComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZEROPAYMOD_API UZeroPay_DebugConsoleComponent : public UActorComponent
{
	GENERATED_BODY()


public:	
	// Sets default values for this component's properties
	UZeroPay_DebugConsoleComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	void SetDebugConsoleDisabled(bool bDisable) ;
	void AddDebugConsoleLine(FDebugConsoleLevel debugConsoleLevel = Log, bool bIncludeObjectName = true, const FString& value = "");
		
	// Turn on to prevent any debug output from this actor going to the in-game debug console
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug support settings")
	bool bDisableConsoleOutput;

	UFUNCTION(Server, Reliable)
	void AddDebugConsoleLine_SERVER(const FString& value = "");

	UFUNCTION(NetMulticast, Reliable)
	void AddDebugConsoleLine_MULTICAST(const FString& value = "");
};

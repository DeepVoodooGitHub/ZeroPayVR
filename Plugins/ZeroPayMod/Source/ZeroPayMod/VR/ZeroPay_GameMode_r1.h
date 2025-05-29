// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ZeroPay_GameMode_r1.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API AZeroPay_GameMode_r1 : public AGameMode
{
	GENERATED_BODY()
	
public:
	void StartPlay() ;

	// An event that is called after UWorld BeginPlay() but before any other game logic
	UFUNCTION(BlueprintImplementableEvent)
	void ZeroPayStartPlay();
};

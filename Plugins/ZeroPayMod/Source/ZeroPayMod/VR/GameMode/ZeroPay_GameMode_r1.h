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
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// An event that is called after UWorld BeginPlay() but before any other game logic
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayStartPlay();

	// An event called at the very start of the game-modes existance
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayInitGameMode(const FString& MapName, const FString& Options);

	void HandleSeamlessTravelPlayer(AController*& C) override;
};

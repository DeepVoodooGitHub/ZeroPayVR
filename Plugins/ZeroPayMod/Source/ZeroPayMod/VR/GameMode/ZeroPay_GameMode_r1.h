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
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	
	virtual void StartPlay() override;

	// SERVER ONLY - Called at the very start of the game-modes existance, nothing may exist in the world
	// Used to generally process configuration and such things; shouldn't do any game-logic
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayInitGameMode(const FString& MapName, const FString& Options);

	// SERVER ONLY - Called before BeginPlay() occurs on any in world actors, used to initialise the game-mode
	// or other related logic operations. 
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayStartPlay();

	void HandleSeamlessTravelPlayer(AController*& C) override;
};

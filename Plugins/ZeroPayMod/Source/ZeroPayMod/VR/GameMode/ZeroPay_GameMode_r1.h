// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Items/Interfaces/ZeroPay_VRItem_Interface_r1.h"
#include "VR/ZeroPay_VRCharacterBase_r1.h"
#include "ZeroPay_GameMode_r1.generated.h"

class AZeroPay_VRCharacter_r1;
class UGripMotionControllerComponent;

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API AZeroPay_GameMode_r1 : public AGameMode
{
	GENERATED_BODY()
	
public:
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void InitGameState() override;
	
	virtual void StartPlay() override;

	// SERVER ONLY - Called during early mode game-mode initialise; game-state, game instance is valid and world should
	// be valid but objects within it (and all player actors) may not exist or be initialised
	// Used to generally process configuration and such things; shouldn't do any game-logic here just register things
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayInitGameMode();

	// SERVER ONLY - Called before BeginPlay() occurs on any in world actors, used to initialise the game-mode
	// or other related logic operations. 
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	void ZeroPayStartPlay();

	void HandleSeamlessTravelPlayer(AController*& C) override;

	// INTERNAL - Used for Item Spawn Logic
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	AActor* Internal_SpawnActor(const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision);

	// INTERNAL - Used for Item Spawn Logic
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	AActor* Internal_SpawnItem(const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision);

	// INTERNAL - Used within main code base to grab a specific actor cleanly
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode")
	bool Internal_GrabActor( AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision, AActor* SpawnedActor);

protected:
	// SERVER ONLY - Fired once the map this GameMode lives in has finished loading
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|GameMode", meta = (DisplayName = "On Post Load Map"))
	void OnPostLoadMap();
private:
	void HandlePostLoadMap(UWorld* LoadedWorld);

	FDelegateHandle PostLoadMapHandle;
};

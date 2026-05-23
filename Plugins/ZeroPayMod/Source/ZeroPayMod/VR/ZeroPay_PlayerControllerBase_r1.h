// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRPlayerController.h"
#include "ZeroPay_PlayerControllerBase_r1.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API AZeroPay_PlayerControllerBase_r1 : public AVRPlayerController
{
	GENERATED_BODY()	
public:
	/** Standard lifecycle overrides */
	virtual void BeginPlay() override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	virtual void PostSeamlessTravel() override;

	/**
	 * Called whenever the controller should ensure a valid pawn exists.
	 * Blueprint must implement this and perform the spawn/possess logic.
	 * InitialSpawn - Set to true when the first pawn is spawned by the controller, useful for Spectator "first" spawns..
	 * AuxData - Used as required by your own GameMode / Controllers
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, BlueprintAuthorityOnly, Category = "ZeroPayVR|Player Controller")
	void SpawnAndPossessPawnIfNeeded();

	/* Used to handle Windows / Quest 3 fades using different underlying mechanisms */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "ZeroPayVR|Player Controller")
	void TriggerVRHeadsetFade(bool fadeDown, float duration);

	/* Used to handle Windows / Quest 3 fades using different underlying mechanisms */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "ZeroPayVR|Player Controller")
	void SetVRHeadsetBlack();

};

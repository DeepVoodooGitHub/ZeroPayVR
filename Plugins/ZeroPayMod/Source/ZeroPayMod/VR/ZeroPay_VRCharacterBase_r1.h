// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VRCharacter.h"
#include "ZeroPay_VRCharacterBase_r1.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API AZeroPay_VRCharacterBase_r1 : public AVRCharacter
{
	GENERATED_BODY()	
	
public:
	/* Implemented in any "pawn" that want's to allow debug messages to be sent to other clients / server */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "ZeroPay|DebugConsole")
	void OnEscalateDebugLineToServer_OWNER(const FString& Value, APawn* Sender);	
};

// Fill out your copyright notice in the Description page of Project Settings.

#include "VR/ZeroPay_PlayerControllerBase_r1.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

void AZeroPay_PlayerControllerBase_r1::BeginPlay()
{
	Super::BeginPlay();
}

void AZeroPay_PlayerControllerBase_r1::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);

	/* Fade to black in 200ms */
	TriggerVRHeadsetFade(true, 0.2f);
}

void AZeroPay_PlayerControllerBase_r1::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
}

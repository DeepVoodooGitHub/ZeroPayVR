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
#if 0
	/* Turn screen black - means we don't see issues when seamless travel (i.e. pawn is not spawned, we see camera at 0,0,0 until it is) */
	APlayerCameraManager* CamMgr = PlayerCameraManager;
	if (CamMgr)
	{
		CamMgr->StartCameraFade(0.f, 1.f, 0.1f, FLinearColor::Black, true, true);
	}
#endif
}

void AZeroPay_PlayerControllerBase_r1::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!PostSeamlessTravel"));

	/* Due to a "issue" we delay creating the pawn until 250ms later otherwise it won't spawn.. */
	if (HasAuthority())
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer( Handle, [this]()
			{
				SpawnAndPossessPawnIfNeeded();
			}, 0.25f, false
		);
	}
}

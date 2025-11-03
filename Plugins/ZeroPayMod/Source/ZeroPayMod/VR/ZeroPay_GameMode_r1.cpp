// (c) Ginger Ninja Games Ltd

#include "VR/ZeroPay_GameMode_r1.h"

void AZeroPay_GameMode_r1::StartPlay()
{
	Super::StartPlay();

	ZeroPayStartPlay();
}


void AZeroPay_GameMode_r1::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	UE_LOG(LogTemp, Log, TEXT(">> AZeroPay_GameMode_r1::HandleSeamlessTravelPlayer called"));
}

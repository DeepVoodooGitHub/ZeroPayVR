// (c) Ginger Ninja Games Ltd

#include "ZeroPay_GameLogic_r1.h"

// Sets default values
AZeroPay_GameLogic_r1::AZeroPay_GameLogic_r1()
{
	// Create a default scene root if one doesn't exist
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

// Called when the game starts AZeroPay_GameLogic_r1 when spawned
void AZeroPay_GameLogic_r1::BeginPlay()
{
	Super::BeginPlay();
	
}



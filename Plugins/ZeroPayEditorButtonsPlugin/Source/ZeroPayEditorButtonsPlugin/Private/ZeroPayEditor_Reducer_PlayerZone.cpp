// (c) Ginger Ninja Games Ltd


#include "ZeroPayEditor_Reducer_PlayerZone.h"


// Sets default values
AZeroPayEditor_Reducer_PlayerZone::AZeroPayEditor_Reducer_PlayerZone()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;

	PlayerZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PlayerZoneBox"));
	RootComponent = PlayerZoneBox;

	PlayerZoneBox->SetBoxExtent(FVector(100.0f)); // default size
	PlayerZoneBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerZoneBox->SetHiddenInGame(true);
	PlayerZoneBox->SetVisibility(true);
#endif
}

// Called when the game starts or when spawned
void AZeroPayEditor_Reducer_PlayerZone::BeginPlay()
{
	Super::BeginPlay();
}

FBox AZeroPayEditor_Reducer_PlayerZone::GetWorldBoundingBox() const
{
	if (!PlayerZoneBox)
		return FBox(EForceInit::ForceInit);

	FVector Origin = PlayerZoneBox->GetComponentLocation();
	FVector Extent = PlayerZoneBox->GetScaledBoxExtent();
	return FBox::BuildAABB(Origin, Extent);
}
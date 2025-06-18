// (c) Ginger Ninja Games Ltd

#include "ZeroPayEditor_Reducer_Zone.h"

AZeroPayEditor_Reducer_Zone::AZeroPayEditor_Reducer_Zone()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;

	OverrideReducerZone = CreateDefaultSubobject<UBoxComponent>(TEXT("OverrideReducerZone"));
	RootComponent = OverrideReducerZone;

	OverrideReducerZone->SetBoxExtent(FVector(100.0f)); // default size
	OverrideReducerZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverrideReducerZone->SetHiddenInGame(true);
	OverrideReducerZone->SetVisibility(true);
#endif
}

// Called when the game starts or when spawned
void AZeroPayEditor_Reducer_Zone::BeginPlay()
{
	Super::BeginPlay();	
}

FBox AZeroPayEditor_Reducer_Zone::GetWorldBoundingBox() const
{
	if (!OverrideReducerZone)
		return FBox(EForceInit::ForceInit);

	FVector Origin = OverrideReducerZone->GetComponentLocation();
	FVector Extent = OverrideReducerZone->GetScaledBoxExtent();
	return FBox::BuildAABB(Origin, Extent);
}
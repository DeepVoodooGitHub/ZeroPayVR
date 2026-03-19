// (c) Ginger Ninja Games Ltd


#include "Guns/Attachments/ZeroPayGunAttachmentPoint_r1.h"


#include "ZeroPayGunAttachmentPoint_r1.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

UZeroPayGunAttachmentPoint_r1::UZeroPayGunAttachmentPoint_r1()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UZeroPayGunAttachmentPoint_r1::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UZeroPayGunAttachmentPoint_r1::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


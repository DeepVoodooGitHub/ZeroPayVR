// (c) Ginger Ninja Games Ltd


#include "Guns/Attachments/ZeroPayGunAttachmentPoint_r1.h"


// Sets default values for this component's properties
UZeroPayGunAttachmentPoint_r1::UZeroPayGunAttachmentPoint_r1()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UZeroPayGunAttachmentPoint_r1::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UZeroPayGunAttachmentPoint_r1::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


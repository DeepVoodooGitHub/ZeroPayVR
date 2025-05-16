// (c) Ginger Ninja Games Ltd

#include "Support/ZeroPay_MiscSupportUtils.h"
#include "GameFramework/Character.h"


AZeroPay_MiscSupportUtils::AZeroPay_MiscSupportUtils()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

AZeroPay_MiscSupportUtils::~AZeroPay_MiscSupportUtils()
{
}

// Called when the game starts or when spawned
void AZeroPay_MiscSupportUtils::BeginPlay()
{
	Super::BeginPlay();

}

void AZeroPay_MiscSupportUtils::IsLocallyControlled(AActor* target, EZeroPay_NetControllerStatus& Result)
{
	AActor* Owner = target->GetOwner() ;
	if (Owner)
	{
		ACharacter* Character = Cast<ACharacter>(Owner);
		if (Character->IsLocallyControlled())
			Result = EZeroPay_NetControllerStatus::Local;

		return;
	}

	Result = EZeroPay_NetControllerStatus::Remote;
}
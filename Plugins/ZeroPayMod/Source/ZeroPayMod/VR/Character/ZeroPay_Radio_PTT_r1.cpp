// (c) Ginger Ninja Games Ltd

#include "VR/Character/ZeroPay_Radio_PTT_r1.h"

UZeroPay_Radio_PTT_r1::UZeroPay_Radio_PTT_r1()
{
	// Query overlaps only (no blocking, no physics)
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetGenerateOverlapEvents(true);

	// Pick an appropriate object type for THIS sensor (often WorldDynamic)
	SetCollisionObjectType(ECC_WorldDynamic);

	// Start from "ignore everything"
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// (Optional) sensible defaults
	InitSphereRadius(20.f);
	bHiddenInGame = true;          // sensor only
	SetCanEverAffectNavigation(false);
}


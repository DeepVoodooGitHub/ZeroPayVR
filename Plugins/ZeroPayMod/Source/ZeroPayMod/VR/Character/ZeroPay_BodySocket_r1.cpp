// (c) Ginger Ninja Games Ltd

#include "VR/Character/ZeroPay_BodySocket_r1.h"


UZeroPay_BodySocket_r1::UZeroPay_BodySocket_r1()
{
	// Query overlaps only (no blocking, no physics)
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetGenerateOverlapEvents(true);

	// Pick an appropriate object type for THIS sensor (often WorldDynamic)
	SetCollisionObjectType(ECC_BodySocket);

	// Start from "ignore everything"
	SetCollisionResponseToAllChannels(ECR_Ignore);

	// (Optional) sensible defaults
	InitSphereRadius(20.f);
	bHiddenInGame = true;          // sensor only
	SetCanEverAffectNavigation(false);
}
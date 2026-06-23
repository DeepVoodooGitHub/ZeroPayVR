// (c) Ginger Ninja Games Ltd

#include "VR/Character/ZeroPay_AmmoPocket_r1.h"


UZeroPay_AmmoPocket_r1::UZeroPay_AmmoPocket_r1()
{
	// Query overlaps only (no blocking, no physics)
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetGenerateOverlapEvents(true);

	// Pick an appropriate object type for THIS sensor (often WorldDynamic)
	SetCollisionObjectType(ECC_AmmoPocket);

	// Start from "ignore everything"
	SetCollisionResponseToAllChannels(ECR_Ignore);

	// (Optional) sensible defaults
	InitCapsuleSize(5.0f, 20.0f);
	bHiddenInGame = true;          // sensor only
	SetCanEverAffectNavigation(false);
	ShapeColor = FColor(0, 255, 0, 255) ;
}

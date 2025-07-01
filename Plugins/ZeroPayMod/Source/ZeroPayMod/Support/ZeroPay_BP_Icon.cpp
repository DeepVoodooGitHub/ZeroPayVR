// (c) Ginger Ninja Games Ltd


#include "Support/ZeroPay_BP_Icon.h"

UZeroPay_BP_Icon::UZeroPay_BP_Icon() : Super()
{
#if WITH_EDITOR
	SetIsVisualizationComponent(true);
#endif
	bHiddenInGame = true;
}


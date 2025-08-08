// (c) Ginger Ninja Games Ltd

#include "VR/ZeroPay_GameInstance_r1.h"
#include "Debug/ZeroPay_InternalDebug.h"

void UZeroPay_GameInstance_r1::Init()
{
    Super::Init();

    /* Clear globals (until we find a better solution) */
    StoredLogEntries.Empty() ;
    InternalDebugTargetActor = nullptr;

    UE_LOG(LogTemp, Warning, TEXT("Test GameInstance: %p, World: %s, WorldType: %d - %s"), this, *GetWorld()->GetName(), (int32)GetWorld()->WorldType, *GetWorld()->PersistentLevel->GetName());
}


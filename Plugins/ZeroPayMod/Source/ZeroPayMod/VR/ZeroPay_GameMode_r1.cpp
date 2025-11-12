// (c) Ginger Ninja Games Ltd

#include "VR/ZeroPay_GameMode_r1.h"

void AZeroPay_GameMode_r1::StartPlay()
{
	Super::StartPlay();

	ZeroPayStartPlay();
}


void AZeroPay_GameMode_r1::HandleSeamlessTravelPlayer(AController*& C)
{
    // Base wires PlayerState and some session bits.
    Super::HandleSeamlessTravelPlayer(C);

    APlayerController* OldPC = Cast<APlayerController>(C);
    if (!OldPC) return;

    /* Look at desired class; if we can't use the current GM's (i.e. bad or set to none) use the previous.. should be ZeroPayVR BP one.. */
    UClass* DesiredPCClass = PlayerControllerClass ? PlayerControllerClass.Get() : OldPC->GetClass() ;

    /* If already correct class, you're done. */
    if (OldPC->IsA(DesiredPCClass))
    {
        RestartPlayer(OldPC);
        return;
    }

    // Spawn the correct controller class in the NEW world
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient; // controllers are transient by default
    APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(DesiredPCClass, Params);
    if (!NewPC) return;

    // Preserve network connection + ownership without disconnecting the client
    SwapPlayerControllers(OldPC, NewPC);

    // Hand the engine back the new controller pointer
    C = NewPC;

    // Keep/transfer PlayerState (Super() should have set this up; keep it explicit)
    NewPC->PlayerState = OldPC->PlayerState;

    // Spawn/possess the correct pawn for this map
    RestartPlayer(NewPC);

    // Clean up the old controller
    OldPC->Destroy();
}

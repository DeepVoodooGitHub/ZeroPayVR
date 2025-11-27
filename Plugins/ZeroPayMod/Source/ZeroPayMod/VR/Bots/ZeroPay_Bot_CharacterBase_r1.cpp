// (c) Ginger Ninja Games Ltd


#include "VR/Bots/ZeroPay_Bot_CharacterBase_r1.h"


void AZeroPay_Bot_CharacterBase_r1::OnRep_PlayerState()
{
	OnPlayerStateReplicated_Bind.Broadcast(GetPlayerState());
	Super::OnRep_PlayerState();
}

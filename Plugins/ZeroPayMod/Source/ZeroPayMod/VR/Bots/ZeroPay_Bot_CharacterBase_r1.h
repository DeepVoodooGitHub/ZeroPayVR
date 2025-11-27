// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZeroPay_Bot_CharacterBase_r1.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBotPlayerStateReplicatedSignature, const APlayerState*, NewPlayerState);

UCLASS()
class ZEROPAYMOD_API AZeroPay_Bot_CharacterBase_r1 : public ACharacter
{
	GENERATED_BODY()

public:
	// Give my users direct access to an event for when the player state has changed
	UPROPERTY(BlueprintAssignable, Category = "VRMovement")
	FBotPlayerStateReplicatedSignature OnPlayerStateReplicated_Bind;

	// Called when player state is replicated
	virtual void OnRep_PlayerState() override;		
};

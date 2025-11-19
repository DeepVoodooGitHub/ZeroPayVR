// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "VRCharacter.h"
#include "CharacterMovementCompTypes.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "ZeroPay_VRFloatingPawnMovement.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_VRFloatingPawnMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

private:
	bool DoMASnapTurn(FVRMoveActionContainer& MoveAction);


	FVector RoundDirectMovement(FVector InMovement) const ;

	/** BaseVR Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AVRCharacter> BaseVRCharacterOwner;

public:
	UZeroPay_VRFloatingPawnMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);

	// When true will use the default engines behavior of setting rotation to match the clients instead of simulating rotations, this is really only here for FPS test pawns
	// And non VRCharacter classes (simple character will use this)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRBaseCharacterMovementComponent")
	bool bUseClientControlRotation;

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Pawn Movement")
	void PerformMoveAction_SnapTurn(float SnapTurnDeltaYaw, EVRMoveActionVelocityRetention VelocityRetention = EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_None, bool bFlagGripTeleport = false, bool bFlagCharacterTeleport = false, bool bRotateAroundCapsule = true);
		
};

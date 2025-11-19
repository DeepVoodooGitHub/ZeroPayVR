// (c) Ginger Ninja Games Ltd

#include "VR/Character/ZeroPay_VRFloatingPawnMovement.h"


UZeroPay_VRFloatingPawnMovement::UZeroPay_VRFloatingPawnMovement(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bUseClientControlRotation = true;
}

void  UZeroPay_VRFloatingPawnMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	BaseVRCharacterOwner = Cast<AVRCharacter>(PawnOwner);
}

void UZeroPay_VRFloatingPawnMovement::PerformMoveAction_SnapTurn(float DeltaYawAngle, EVRMoveActionVelocityRetention VelocityRetention, bool bFlagGripTeleport, bool bFlagCharacterTeleport, bool bRotateAroundCapsule)
{
	FVRMoveActionContainer MoveAction;
	MoveAction.MoveAction = EVRMoveAction::VRMOVEACTION_SnapTurn;

	// Removed 2 decimal precision rounding in favor of matching the actual replicated short fidelity instead.
	// MoveAction.MoveActionRot = FRotator(0.0f, FMath::RoundToFloat(((FRotator(0.f,DeltaYawAngle, 0.f).Quaternion() * UpdatedComponent->GetComponentQuat()).Rotator().Yaw) * 100.f) / 100.f, 0.0f);

	// Setting to the exact same fidelity as the replicated value ends up being, losing some precision
	FRotator TargetRotation = (UpdatedComponent->GetComponentQuat() * FRotator(0.f, DeltaYawAngle, 0.f).Quaternion()).Rotator();
	TargetRotation.Yaw = FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort(TargetRotation.Yaw));
	TargetRotation.Pitch = FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort(TargetRotation.Pitch));
	TargetRotation.Roll = FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort(TargetRotation.Roll));
	MoveAction.MoveActionRot = TargetRotation;
	//MoveAction.MoveActionRot = FRotator( 0.0f, FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort(DeltaYawAngle)), 0.0f);
		//FRotator(0.0f, FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort((FRotator(0.f, DeltaYawAngle, 0.f).Quaternion() * UpdatedComponent->GetComponentQuat()).Rotator().Yaw)), 0.0f);

	if (bFlagCharacterTeleport)
		MoveAction.MoveActionFlags = 0x02;// .MoveActionRot.Roll = 2.0f;
	else if (bFlagGripTeleport)
		MoveAction.MoveActionFlags = 0x01;//MoveActionRot.Roll = bFlagGripTeleport ? 1.0f : 0.0f;

	if (bRotateAroundCapsule)
	{
		MoveAction.MoveActionFlags |= 0x08;
	}

	if (VelocityRetention == EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_Turn)
	{
		//MoveAction.MoveActionRot.Pitch = FMath::RoundToFloat(DeltaYawAngle * 100.f) / 100.f;
		//MoveAction.MoveActionRot.Pitch = DeltaYawAngle;
		MoveAction.MoveActionDeltaYaw = FRotator::DecompressAxisFromShort(FRotator::CompressAxisToShort(DeltaYawAngle));
	}

	MoveAction.VelRetentionSetting = VelocityRetention;

	DoMASnapTurn(MoveAction);
}

FVector UZeroPay_VRFloatingPawnMovement::RoundDirectMovement(FVector InMovement) const
{
	// Match FVector_NetQuantize100 (2 decimal place of precision).
	UE::Net::QuantizeVector(100, InMovement);
	//InMovement.X = FMath::RoundToFloat(InMovement.X * 100.f) / 100.f;
	//InMovement.Y = FMath::RoundToFloat(InMovement.Y * 100.f) / 100.f;
	//InMovement.Z = FMath::RoundToFloat(InMovement.Z * 100.f) / 100.f;
	return InMovement;
}

bool UZeroPay_VRFloatingPawnMovement::DoMASnapTurn(FVRMoveActionContainer& MoveAction)
{
	if (BaseVRCharacterOwner)
	{
		FRotator TargetRot = MoveAction.MoveActionRot;
		FQuat OrigRot = BaseVRCharacterOwner->GetActorQuat();

		if (BaseVRCharacterOwner->SeatInformation.bSitting)
		{
			FRotator DeltaRot(0.f, MoveAction.MoveActionDeltaYaw, 0.f);
			TargetRot = (OrigRot * DeltaRot.Quaternion()).Rotator();
		}

		FTransform OriginalRelativeTrans = BaseVRCharacterOwner->GetRootComponent()->GetRelativeTransform();

		bool bRotateAroundCapsule = MoveAction.MoveActionFlags & 0x08;

		// Clamp to 2 decimal precision
		/*TargetRot = TargetRot.Clamp();
		TargetRot.Pitch = (TargetRot.Pitch * 100.f) / 100.f;
		TargetRot.Yaw = (TargetRot.Yaw * 100.f) / 100.f;
		TargetRot.Roll = (TargetRot.Roll * 100.f) / 100.f;
		TargetRot.Normalize();*/
		//bIsBlendingOrientation = true;

		if (this->BaseVRCharacterOwner && this->BaseVRCharacterOwner->IsLocallyControlled())
		{
			if (this->bUseClientControlRotation)
			{
				MoveAction.MoveActionLoc = BaseVRCharacterOwner->SetActorRotationVR(TargetRot, false, false, bRotateAroundCapsule);
				MoveAction.MoveActionFlags |= 0x04; // Flag that we are using loc only
			}
			else
			{
				BaseVRCharacterOwner->SetActorRotationVR(TargetRot, false, false, bRotateAroundCapsule);
			}
		}
		else
		{
			if (MoveAction.MoveActionFlags & 0x04)
			{
				BaseVRCharacterOwner->SetActorLocation(BaseVRCharacterOwner->GetActorLocation() + MoveAction.MoveActionLoc);
			}
			else
			{
				BaseVRCharacterOwner->SetActorRotationVR(TargetRot, false, false, bRotateAroundCapsule);
			}
		}

		switch (MoveAction.VelRetentionSetting)
		{
		case EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_None:
		{

		}break;
		case EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_Clear:
		{
			this->Velocity = FVector::ZeroVector;
		}break;
		case EVRMoveActionVelocityRetention::VRMOVEACTION_Velocity_Turn:
		{
			if (BaseVRCharacterOwner->IsLocallyControlled())
			{
				MoveAction.MoveActionVel = RoundDirectMovement((TargetRot.Quaternion() * OrigRot.Inverse()).RotateVector(this->Velocity));
				this->Velocity = MoveAction.MoveActionVel;
			}
			else
			{
				this->Velocity = MoveAction.MoveActionVel;
			}
		}break;
		}

		// If we are flagged to teleport the grips
		if (MoveAction.MoveActionFlags & 0x01 || MoveAction.MoveActionFlags & 0x02)
		{
			BaseVRCharacterOwner->NotifyOfTeleport(MoveAction.MoveActionFlags & 0x02);
		}

		if (BaseVRCharacterOwner->SeatInformation.bSitting)
		{
			BaseVRCharacterOwner->SeatInformation.StoredTargetTransform = (OriginalRelativeTrans.Inverse() * BaseVRCharacterOwner->GetRootComponent()->GetRelativeTransform()) * BaseVRCharacterOwner->SeatInformation.StoredTargetTransform;
			if (BaseVRCharacterOwner->IsLocallyControlled() && GetNetMode() == ENetMode::NM_Client)
			{
				BaseVRCharacterOwner->Server_SeatedSnapTurn(MoveAction.MoveActionDeltaYaw);
			}
		}
	}

	return false;
}
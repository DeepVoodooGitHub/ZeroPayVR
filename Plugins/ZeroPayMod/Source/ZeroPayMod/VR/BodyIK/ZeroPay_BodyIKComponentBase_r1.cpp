// ZeroPay_BodyIKComponentBase_r1.cpp

#include "ZeroPay_BodyIKComponentBase_r1.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "DrawDebugHelpers.h"

namespace
{
	FORCEINLINE float SideSelF(ESide S, float L, float R) { return (S == ESide::Left) ? L : R; }

	FORCEINLINE float Square(float A) { return A * A; }

	// Viewer camera location, cached once per engine frame so that N avatars all
	// querying it in the same frame only pay for one PlayerCameraManager lookup.
	FVector GetCachedViewLocation(const UWorld* World)
	{
		static uint64 CachedFrame = TNumericLimits<uint64>::Max();
		static FVector CachedLocation = FVector::ZeroVector;

		if (!World)
		{
			return CachedLocation;
		}

		if (GFrameCounter != CachedFrame)
		{
			CachedFrame = GFrameCounter;
			if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(World, 0))
			{
				CachedLocation = PCM->GetCameraLocation();
			}
		}
		return CachedLocation;
	}

	float GetOwnerVirtualHeightOffset(AActor* Owner)
	{
		if (!Owner) { return 0.0f; }
		if (FProperty* Prop = Owner->GetClass()->FindPropertyByName(TEXT("VirtualHeightOffset")))
		{
			if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				return FloatProp->GetPropertyValue_InContainer(Owner);
			}
			if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
			{
				return static_cast<float>(DoubleProp->GetPropertyValue_InContainer(Owner));
			}
		}
		return 0.0f;
	}

	void SetOwnerVirtualHeightOffset(AActor* Owner, float Value)
	{
		if (!Owner) { return; }
		if (FProperty* Prop = Owner->GetClass()->FindPropertyByName(TEXT("VirtualHeightOffset")))
		{
			if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				FloatProp->SetPropertyValue_InContainer(Owner, Value);
			}
			else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
			{
				DoubleProp->SetPropertyValue_InContainer(Owner, static_cast<double>(Value));
			}
		}
	}
}

// =============================================================================
// Construction
// =============================================================================
UZeroPay_BodyIKComponentBase_r1::UZeroPay_BodyIKComponentBase_r1()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UZeroPay_BodyIKComponentBase_r1::GetWorldDeltaSecondsSafe() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetDeltaSeconds();
	}
	return 0.0f;
}

// =============================================================================
// Initialize
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::Initialize(USceneComponent* InHead, USceneComponent* InLeftWrist, USceneComponent* InRightWrist, USceneComponent* InCapsuleBase, USkeletalMeshComponent* InCharacterMesh, UCharacterMovementComponent* InCharacterMovementComponent)
{
	Head = InHead;
	LeftWrist = InLeftWrist;
	RightWrist = InRightWrist;
	CapsuleBase = InCapsuleBase;
	CharacterMesh = InCharacterMesh;
	CharacterMovementComponent = InCharacterMovementComponent;

	if (IsValid(CharacterMovementComponent))
	{
		MaxWalkSpeed = CharacterMovementComponent->MaxWalkSpeed;
		CharacterJumpZVelocity = CharacterMovementComponent->JumpZVelocity;

		CharacterName = FindCharacterNameFromMesh(CharacterMesh);
		ChangeCharacter(CharacterName);
		Initialized = true;
	}
	else
	{
		CharacterName = FindCharacterNameFromMesh(CharacterMesh);
		ChangeCharacter(CharacterName);
		Initialized = true;
	}
}

// =============================================================================
// UpdateBody (ExecutionSequence then_0..then_4)
// =============================================================================
// =============================================================================
// UpdateBody  -  detail-tier dispatcher + per-tier rate gate
//
// What causes degradation, in priority order:
//   1. Local player  -> never degrades (Hero).
//   2. Not recently rendered (off-screen / occluded) -> Culled (skipped entirely).
//   3. Distance from the viewer camera -> Near / Mid / Far tier selection.
// Each non-Hero tier also throttles how often it actually solves.
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::UpdateBody()
{
	if (!Initialized)
	{
		return;
	}

	const EBodyIKDetailTier Tier = ComputeDetailTier();
	CurrentDetailTier = Tier;

	if (Tier == EBodyIKDetailTier::Culled)
	{
		// Leave the last solved pose in place; the mesh isn't being rendered.
		return;
	}

	// Per-tier rate gate. Hero solves every frame (interval 0); others accumulate
	// delta time and only solve once the tier's interval has elapsed. On skipped
	// frames the last pose is reused (Update Rate Optimization / anim interpolation
	// smooths the visual gap).
	const float Interval = GetSolveIntervalForTier(Tier);
	if (Interval > 0.0f)
	{
		TimeSinceLastSolve += GetWorldDeltaSecondsSafe();
		if (TimeSinceLastSolve < Interval)
		{
			return;
		}
		TimeSinceLastSolve = 0.0f;
	}

	switch (Tier)
	{
	case EBodyIKDetailTier::Hero:
	case EBodyIKDetailTier::Near:
		SolveFull();
		break;
	case EBodyIKDetailTier::Mid:
		SolveReduced();
		break;
	case EBodyIKDetailTier::Far:
		SolveMinimal();
		break;
	default:
		break;
	}
}

// =============================================================================
// ComputeDetailTier
// =============================================================================
EBodyIKDetailTier UZeroPay_BodyIKComponentBase_r1::ComputeDetailTier() const
{
	if (!bEnableDetailScaling || bIsLocalPlayer)
	{
		return EBodyIKDetailTier::Hero;
	}

	// Off-screen / occluded avatars are the cheapest, highest-value cull.
	if (IsValid(CharacterMesh) && !CharacterMesh->WasRecentlyRendered(RenderedTolerance))
	{
		return EBodyIKDetailTier::Culled;
	}

	const AActor* Owner = GetOwner();
	const FVector BodyLoc = Owner
		? Owner->GetActorLocation()
		: (IsValid(CharacterMesh) ? CharacterMesh->GetComponentLocation() : FVector::ZeroVector);

	const float Dist = FVector::Dist(GetCachedViewLocation(GetWorld()), BodyLoc);

	if (Dist <= NearTierDistance) { return EBodyIKDetailTier::Near; }
	if (Dist <= MidTierDistance) { return EBodyIKDetailTier::Mid; }
	if (Dist <= FarTierDistance) { return EBodyIKDetailTier::Far; }
	return EBodyIKDetailTier::Culled;
}

// =============================================================================
// GetSolveIntervalForTier
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::GetSolveIntervalForTier(EBodyIKDetailTier Tier) const
{
	switch (Tier)
	{
	case EBodyIKDetailTier::Near: return 1.0f / FMath::Max(NearTierRateHz, 1.0f);
	case EBodyIKDetailTier::Mid:  return 1.0f / FMath::Max(MidTierRateHz, 1.0f);
	case EBodyIKDetailTier::Far:  return 1.0f / FMath::Max(FarTierRateHz, 1.0f);
	case EBodyIKDetailTier::Hero:
	default:
		return 0.0f; // every frame
	}
}

// =============================================================================
// SolveFull  -  full fidelity (original UpdateBody body)
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SolveFull()
{
	// then_0
	CalculateCharacterScale();
	ProcessInputs();
	CalculateBodyPrerequisites();
	CalculateShoulderCenterAndNeck();
	CalculateDependentTransforms();

	// then_1  (arms - left then right)
	{
		CalculateClavicleOffset(ESide::Left);
		const float LeftHandElbowInfluence = CalculateShoulderTransform(ESide::Left);
		SolveArm(ESide::Left, LeftHandElbowInfluence);
		ClampHandPosition(ESide::Left);

		CalculateClavicleOffset(ESide::Right);
		const float RightHandElbowInfluence = CalculateShoulderTransform(ESide::Right);
		SolveArm(ESide::Right, RightHandElbowInfluence);
		ClampHandPosition(ESide::Right);
	}

	// Legs are driven by animation now; only the jump-blend state is still needed.
	CalculateJumpState();

	// then_3  (hand poses)
	{
		UpdateHandPose(ESide::Left);
		UpdateHandPose(ESide::Right);
	}

	// then_4
	DrawDebugBodyIK();
}

// =============================================================================
// SolveReduced  -  Mid tier: full torso + arms, cheap feet (NO line traces),
//                  no per-bone finger pose, no debug draw.
// The dropped pair of LineTraceSingleByProfile calls is the dominant saving.
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SolveReduced()
{
	CalculateCharacterScale();
	ProcessInputs();
	CalculateBodyPrerequisites();
	CalculateShoulderCenterAndNeck();
	CalculateDependentTransforms();

	CalculateClavicleOffset(ESide::Left);
	const float LeftHandElbowInfluence = CalculateShoulderTransform(ESide::Left);
	SolveArm(ESide::Left, LeftHandElbowInfluence);
	ClampHandPosition(ESide::Left);

	CalculateClavicleOffset(ESide::Right);
	const float RightHandElbowInfluence = CalculateShoulderTransform(ESide::Right);
	SolveArm(ESide::Right, RightHandElbowInfluence);
	ClampHandPosition(ESide::Right);

	CalculateJumpState();
}

// =============================================================================
// SolveMinimal  -  Far tier: full torso, straight-line arms (no twist/interp),
//                  cheap feet, no finger pose, no debug draw.
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SolveMinimal()
{
	CalculateCharacterScale();
	ProcessInputs();
	CalculateBodyPrerequisites();
	CalculateShoulderCenterAndNeck();
	CalculateDependentTransforms();

	SolveArmCheap(ESide::Left);
	SolveArmCheap(ESide::Right);

	CalculateJumpState();
}

// =============================================================================
// SolveArmCheap  -  shoulder transform + straight midpoint elbow, no hand twist,
//                   no RInterp, no stretch clamp.
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SolveArmCheap(ESide Side)
{
	FRotator& ClavicleRotOffset = (Side == ESide::Left) ? LeftClavicleRotationOffset : RightClavicleRotationOffset;
	ClavicleRotOffset = FRotator::ZeroRotator; // skip CalculateClavicleOffset

	const float HandElbowInfluence = CalculateShoulderTransform(Side); // sets shoulder transform
	(void)HandElbowInfluence;

	const FTransform& ShoulderTransform = (Side == ESide::Left) ? LeftShoulderTransform : RightShoulderTransform;
	const FTransform& HandTM = (Side == ESide::Left) ? LeftHandTransform : RightHandTransform;
	FTransform& ElbowTM = (Side == ESide::Left) ? LeftElbowTransform : RightElbowTransform;
	FVector& ClampedLoc = (Side == ESide::Left) ? LeftClampedHandLocation : RightClampedHandLocation;

	const FVector ElbowLoc = ShoulderTransform.GetLocation() + (HandTM.GetLocation() - ShoulderTransform.GetLocation()) * 0.5f;
	ElbowTM = FTransform(ShoulderTransform.Rotator(), ElbowLoc, FVector(1, 1, 1));
	ClampedLoc = HandTM.GetLocation(); // skip stretch clamp at distance
}



// =============================================================================
// ProcessInputs
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::ProcessInputs()
{
	// then_0
	HeadTransform = Head ? Head->GetComponentTransform() : FTransform::Identity;
	NormalizedHeadRotation = NormalizeHeadRotation(HeadTransform.Rotator());

	// then_1
	const FTransform LeftWristTM = LeftWrist ? LeftWrist->GetComponentTransform() : FTransform::Identity;
	const FTransform RightWristTM = RightWrist ? RightWrist->GetComponentTransform() : FTransform::Identity;
	LeftHandTransform = ProcessHandInput(LeftWristTM, ESide::Left);
	RightHandTransform = ProcessHandInput(RightWristTM, ESide::Right);
}

// =============================================================================
// ProcessHandInput  -> pass-through (Hand argument unused in the BP)
// =============================================================================
FTransform UZeroPay_BodyIKComponentBase_r1::ProcessHandInput(const FTransform& HandInput, ESide /*Hand*/) const
{
	return HandInput;
}

// =============================================================================
// NormalizeHeadRotation -> FVector (X = roll-ish, Y = pitch, Z = yaw)
// =============================================================================
FVector UZeroPay_BodyIKComponentBase_r1::NormalizeHeadRotation(const FRotator& HeadRotation) const
{
	const FRotator PureYaw = GetHMDYaw(HeadRotation);

	const FRotator DeltaFromYaw = UKismetMathLibrary::NormalizedDeltaRotator(HeadRotation, PureYaw);
	const FVector Fwd = UKismetMathLibrary::GetForwardVector(DeltaFromYaw);

	const float Pitch = UKismetMathLibrary::DegAtan2(Fwd.Z, Fwd.X);

	const FRotator PitchRot = FRotator(Pitch, PureYaw.Yaw, 0.0f); // FRotator(Pitch, Yaw, Roll)
	const FRotator DeltaFromPitch = UKismetMathLibrary::NormalizedDeltaRotator(HeadRotation, PitchRot);

	const float X = FMath::Clamp(DeltaFromPitch.Roll * FMath::Max(Fwd.X, 0.0f), -89.0f, 89.0f);
	const float Y = FMath::Clamp(Pitch, -89.0f, 89.0f);
	const float Z = PureYaw.Yaw;

	return FVector(X, Y, Z);
}

// =============================================================================
// CalculateShoulderCenterAndNeck
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateShoulderCenterAndNeck()
{
	LastBodyYaw = BodyYaw(NormalizedHeadRotation.Z);
	LastBodyPitch = BodyPitch(NormalizedHeadRotation.Y);
	BodyYawDueToVelocity = GetBodyDeltaYawFromVelocity(LastBodyYaw);

	const FRotator BodyRotation = FRotator(
		LastBodyPitch,                       // Pitch
		LastBodyYaw + BodyYawDueToVelocity,  // Yaw
		BodyRoll(NormalizedHeadRotation.X)); // Roll

	const FVector NeckLocation =
		VectorToRotator(NormalizedHeadRotation).RotateVector(NeckFromHMD) + HeadTransform.GetLocation();

	const FVector ShoulderCenterLoc = NeckLocation + BodyRotation.RotateVector(ShoulderCenterFromNeck);

	ShoulderCenterTransform = FTransform(BodyRotation, ShoulderCenterLoc, FVector(1.0f, 1.0f, 1.0f));
}

// =============================================================================
// BodyRoll
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::BodyRoll(float HeadRoll) const
{
	return (AddPastThreshold(FMath::Abs(HeadRoll), 45.0f, 0.35f) * FMath::Sign(HeadRoll)) * BodyPitchRollScalar;
}

// =============================================================================
// BodyPitch
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::BodyPitch(float HeadPitch) const
{
	const float HeightTerm = FMath::Pow(1.0f - HeightAlpha, 1.3f) * -45.0f;
	const float PitchTerm = (AddPastThreshold(FMath::Abs(HeadPitch), 55.0f, 0.5f) * FMath::Sign(HeadPitch)) * BodyPitchRollScalar;
	return FMath::Clamp(HeightTerm + PitchTerm, -60.0f, 0.0f);
}

// =============================================================================
// BodyYaw
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::BodyYaw(float HeadYaw)
{
	const FRotator Current = FRotator(0.0f, LastBodyYaw, 0.0f);
	const FRotator Target = FRotator(0.0f, GetBodyDeltaYawFromHands(HeadYaw) + HeadYaw, 0.0f);
	const FRotator Result = UKismetMathLibrary::RInterpTo(Current, Target, GetWorldDeltaSecondsSafe(), BodyYawInterpSpeed);
	return Result.Yaw;
}

// =============================================================================
// GetBodyDeltaYawFromHands
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::GetBodyDeltaYawFromHands(float HeadYaw) const
{
	const FTransform CenterBase(FRotator(0.0f, HeadYaw, 0.0f), HeadTransform.GetLocation(), FVector(1, 1, 1));
	const FTransform Center = TransformInLS(CenterBase, FVector(-16.0f * CharacterScale, 0.0f, 0.0f));

	const FVector LeftRel = Center.InverseTransformPosition(LeftHandTransform.GetLocation());
	const FVector RightRel = Center.InverseTransformPosition(RightHandTransform.GetLocation());

	const FVector2D LeftHandRelLoc(LeftRel.X, LeftRel.Y);
	const FVector2D RightHandRelLoc(RightRel.X, RightRel.Y);

	const float AngleTerm =
		(FMath::Clamp(UKismetMathLibrary::DegAtan2(LeftHandRelLoc.Y, LeftHandRelLoc.X), -90.0f, 90.0f) +
			FMath::Clamp(UKismetMathLibrary::DegAtan2(RightHandRelLoc.Y, RightHandRelLoc.X), -90.0f, 90.0f)) * HandBodyYawInfluence;

	const float ArmDenom = (CharacterArmLength * ArmBaseScale) * 1.1f;
	const float ExtendTerm =
		(FMath::Clamp(FMath::Pow(LeftHandRelLoc.X / ArmDenom, 3.0f), -1.0f, 1.0f) +
			(FMath::Clamp(FMath::Pow(RightHandRelLoc.X / ArmDenom, 3.0f), -1.0f, 1.0f) * -1.0f)) * BodyYawFromHandExtend;

	return FMath::Clamp(AngleTerm + ExtendTerm, HandMaxBodyYawInfluenceAngle * -1.0f, HandMaxBodyYawInfluenceAngle);
}

// =============================================================================
// GetBodyDeltaYawFromVelocity
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::GetBodyDeltaYawFromVelocity(float Yaw) const
{
	const FVector VelNorm = Char2DVelocity.GetSafeNormal(0.0001f);
	const FRotator YawRot = FRotator(0.0f, Yaw, 0.0f);

	const float FwdDot = FVector::DotProduct(VelNorm, UKismetMathLibrary::GetForwardVector(YawRot));
	const float SignMul = (FwdDot < -0.1f) ? -1.0f : 1.0f;
	const float RightDot = FVector::DotProduct(VelNorm, UKismetMathLibrary::GetRightVector(YawRot));

	const float TargetYaw =
		FMath::Clamp(UKismetMathLibrary::DegAsin(RightDot * SignMul) * Square(SpeedAlphaNoOverride), -45.0f, 45.0f) * 0.5f;

	const FRotator Current = FRotator(0.0f, BodyYawDueToVelocity, 0.0f);
	const FRotator Target = FRotator(0.0f, TargetYaw, 0.0f);
	const FRotator Result = UKismetMathLibrary::RInterpTo(Current, Target, GetWorldDeltaSecondsSafe(), BodyYawInterpSpeed * 0.2f);
	return Result.Yaw;
}

// =============================================================================
// CalculateDependentTransforms
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateDependentTransforms()
{
	const FVector PelvisLoc =
		ShoulderCenterTransform.Rotator().RotateVector(PelvisFromShoulderCenter + BodyOffset) + ShoulderCenterTransform.GetLocation();

	PelvisTransform = FTransform(ShoulderCenterTransform.Rotator(), PelvisLoc, FVector(1, 1, 1));
}

// =============================================================================
// CalculateBodyPrerequisites
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateBodyPrerequisites()
{
	GetHeight(HeightAlpha, CurrentHMDHeight);

	if (IsValid(CharacterMovementComponent))
	{
		const FVector Velocity = CharacterMovementComponent->Velocity;
		const FVector NewChar2D(Velocity.X, Velocity.Y, 0.0f);

		CharZVelocity = Velocity.Z;
		Char2DVelocity = NewChar2D;
		CharacterSpeed = Char2DVelocity.Size();
		SpeedAlphaNoOverride = CharacterSpeed / FMath::Max(MaxWalkSpeed, 600.0f);

		if (CharacterSpeed == 0.0f)
		{
			Char2DVelocity = UKismetMathLibrary::GetForwardVector(FRotator(0.0f, NormalizedHeadRotation.Z, 0.0f));
		}
	}
}

// =============================================================================
// CalculateCharacterScale
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateCharacterScale()
{
	CharacterScale = CharacterHMDHeight / 170.0f;
}

// =============================================================================
// CalculateShoulderTransform -> returns HandElbowInfluence
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::CalculateShoulderTransform(ESide Side)
{
	const float SideMul = SideSelF(Side, -1.0f, 1.0f);                 // clavicle Y offset side
	const float SideMulPos = SideSelF(Side, 1.0f, -1.0f);             // various sign flips
	FRotator& ClavicleRotOffset = (Side == ESide::Left) ? LeftClavicleRotationOffset : RightClavicleRotationOffset;
	const FTransform& HandTM = (Side == ESide::Left) ? LeftHandTransform : RightHandTransform;
	FTransform& OutShoulderTM = (Side == ESide::Left) ? LeftShoulderTransform : RightShoulderTransform;

	const FRotator ClavLocalRot = ClavicleFromShoulderCenter.Rotator();
	const FRotator MirroredClavRot = FRotator(
		ClavLocalRot.Pitch * SideMul,   // Pitch (NewEnumerator0:-1, NewEnumerator1:1)
		ClavLocalRot.Yaw * SideMulPos,  // Yaw   (NewEnumerator0:1, NewEnumerator1:-1)
		ClavLocalRot.Roll * SideMulPos);// Roll  (NewEnumerator0:1, NewEnumerator1:-1)

	const FRotator ComposedRot = UKismetMathLibrary::ComposeRotators(ClavicleRotOffset * SideMulPos, MirroredClavRot);

	const FVector LocalYOffset(0.0f, ClavicleBoneLength * SideMul, 0.0f);
	const FVector RotatedY = ComposedRot.RotateVector(LocalYOffset);
	const FVector ClavLocOffset = ClavicleFromShoulderCenter.GetLocation() * FVector(1.0f, SideMulPos, 1.0f);

	const FVector ShoulderLoc = ShoulderCenterTransform.TransformPosition(RotatedY + ClavLocOffset);

	const FVector NormalizedShoulderToHand = (HandTM.GetLocation() - ShoulderLoc).GetSafeNormal(0.0001f);

	const FVector SC_Up = UKismetMathLibrary::GetUpVector(ShoulderCenterTransform.Rotator());
	const FVector SC_Right = UKismetMathLibrary::GetRightVector(ShoulderCenterTransform.Rotator());
	const float UpDot = FVector::DotProduct(NormalizedShoulderToHand, SC_Up);

	const FVector SlerpInner = UKismetMathLibrary::Vector_SlerpNormals(
		SC_Up,
		FVector::CrossProduct(NormalizedShoulderToHand, SC_Right),
		FMath::Abs(UpDot));

	const FVector SlerpZ = UKismetMathLibrary::Vector_SlerpNormals(
		SlerpInner,
		SC_Right * SideMulPos,
		FMath::Max(UpDot, 0.0f));

	OutShoulderTM = FTransform(
		UKismetMathLibrary::MakeRotFromXZ(NormalizedShoulderToHand, SlerpZ),
		ShoulderLoc,
		FVector(1, 1, 1));

	return 1.0f - Square(FMath::Abs(UpDot));
}

// =============================================================================
// CalculateClavicleOffset
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateClavicleOffset(ESide Side)
{
	const FVector& HandLoc = (Side == ESide::Left) ? LeftHandTransform.GetLocation() : RightHandTransform.GetLocation();
	FRotator& OutRot = (Side == ESide::Left) ? LeftClavicleRotationOffset : RightClavicleRotationOffset;
	const float SideMul = SideSelF(Side, 1.0f, -1.0f);

	const FVector HandRelativeToShoulder =
		ShoulderCenterTransform.InverseTransformPosition(HandLoc) / ((CharacterHMDHeight * ArmBaseScale) * 0.36f);

	const float PitchLerp = FMath::Lerp(1.0f, 1.2f, FMath::Min(LastBodyPitch / -60.0f, 1.0f));

	// X component
	const float CompX = FMath::Clamp(
		((FMath::Sign(HandRelativeToShoulder.Z) * Square(HandRelativeToShoulder.Z)) * 45.0f) *
		((HandRelativeToShoulder.Z < 0.0f) ? 0.15f : 1.0f),
		-20.0f, 45.0f);

	// Z component
	const float ZInnerScaled = PitchLerp * HandRelativeToShoulder.X;
	const float CompZ_PartA = FMath::Clamp((FMath::Sign(ZInnerScaled) * Square(ZInnerScaled)) * 35.0f, -20.0f, 50.0f) * PitchLerp;
	const float CompZ_PartB = FMath::Clamp((HandRelativeToShoulder.Y * SideMul) * 60.0f, -15.0f, 30.0f);
	const float CompZ = FMath::Clamp(CompZ_PartA + CompZ_PartB, -50.0f, 70.0f);

	const FVector OffsetVec = FVector(CompX, 0.0f, CompZ) * ClavicleRotationWeight;

	// MakeRotator(Roll=Vec.X, Pitch=Vec.Y, Yaw=Vec.Z)
	OutRot = FRotator(OffsetVec.Y, OffsetVec.Z, OffsetVec.X);
}

// =============================================================================
// SolveArm
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SolveArm(ESide Side, float HandElbowInfluence)
{
	const FTransform& ShoulderTransform = (Side == ESide::Left) ? LeftShoulderTransform : RightShoulderTransform;
	const FTransform& HandTM = (Side == ESide::Left) ? LeftHandTransform : RightHandTransform;
	float& HandRollRaw = (Side == ESide::Left) ? LeftHandRollRaw : RightHandRollRaw;
	FRotator& LastElbowRot = (Side == ESide::Left) ? LeftLastElbowRotation : RightLastElbowRotation;
	FTransform& ElbowTM = (Side == ESide::Left) ? LeftElbowTransform : RightElbowTransform;
	const float SideMul = SideSelF(Side, 1.0f, -1.0f);

	// HandRotation = relative rotation of hand vs shoulder (locations zeroed)
	const FTransform HandRotOnly(HandTM.Rotator(), FVector::ZeroVector, FVector(1, 1, 1));
	const FTransform ShoulderRotOnly(ShoulderTransform.Rotator(), FVector::ZeroVector, FVector(1, 1, 1));
	const FRotator HandRotation = UKismetMathLibrary::MakeRelativeTransform(HandRotOnly, ShoulderRotOnly).Rotator();

	HandRollRaw = GetTwistFromRotation(HandRotation);

	const float ElbowRollInput = HandRollRaw * SideMul;
	const FRotator ElbowTarget = FRotator(0.0f, 0.0f, CalculateElbowRoll(ElbowRollInput) * HandElbowInfluence); // Roll only
	LastElbowRot = UKismetMathLibrary::RInterpTo(LastElbowRot, ElbowTarget, GetWorldDeltaSecondsSafe(), ElbowRotationSpeed);

	const FVector ElbowBaseLoc =
		ShoulderTransform.GetLocation() + (HandTM.GetLocation() - ShoulderTransform.GetLocation()) * 0.5f;

	const FTransform ElbowBaseTM(ShoulderTransform.Rotator(), ElbowBaseLoc, FVector(1, 1, 1));
	const FTransform ElbowRotated = AddRotation(ElbowBaseTM, LastElbowRot * SideMul);

	ElbowTM = TransformInLS(ElbowRotated, FVector(0.0f, 0.0f, CharacterScale * -60.0f));
}

// =============================================================================
// CalculateElbowRoll
// =============================================================================
float UZeroPay_BodyIKComponentBase_r1::CalculateElbowRoll(float HandRoll) const
{
	// Select(idx=(HandRoll<0); Opt0/false:ElbowSensitivity, Opt1/true:ElbowSensitivity*0.5)
	const float Sensitivity = (HandRoll < 0.0f) ? (ElbowSensitivity * 0.5f) : ElbowSensitivity;
	return FMath::Clamp((HandRoll * Sensitivity) + ElbowRestingAngle, ElbowRotationLimits.Min, ElbowRotationLimits.Max);
}

// =============================================================================
// ClampHandPosition
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::ClampHandPosition(ESide Hand)
{
	const FTransform& HandTM = (Hand == ESide::Left) ? LeftHandTransform : RightHandTransform;
	const FTransform& ShoulderTM = (Hand == ESide::Left) ? LeftShoulderTransform : RightShoulderTransform;
	float& ArmScaleFactor = (Hand == ESide::Left) ? CharacterArmRealScaleFactor_L : CharacterArmRealScaleFactor_R;
	FVector& ClampedLoc = (Hand == ESide::Left) ? LeftClampedHandLocation : RightClampedHandLocation;

	const FVector TempRelativeHandPosition = HandTM.GetLocation() - ShoulderTM.GetLocation();

	const float StretchClamp = ScaleArmsPastLimit
		? FMath::Clamp(TempRelativeHandPosition.Size() / ((CharacterArmLength * ArmBaseScale) * MaxArmStretch), 1.0f, ArmStretchLimitFactor)
		: 1.0f;
	ArmScaleFactor = StretchClamp * ArmBaseScale;

	const float MaxSize = ArmScaleFactor * CharacterArmLength * MaxArmStretch;
	ClampedLoc = TempRelativeHandPosition.GetClampedToMaxSize(MaxSize) + ShoulderTM.GetLocation();
}

// =============================================================================
// GetHeight -> Alpha, Height
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::GetHeight(float& Alpha, float& Height) const
{
	const FVector CapsuleLoc = CapsuleBase ? CapsuleBase->GetComponentLocation() : FVector::ZeroVector;
	const float RawHeight = (HeadTransform.GetLocation() - CapsuleLoc).Z - CharacterHeightOffset;

	Alpha = UKismetMathLibrary::MapRangeClamped(
		RawHeight,
		MinCrouchedHeightFactor * CharacterHMDHeight,
		CharacterHMDHeight,
		0.0f,
		1.0f);
	Height = RawHeight;
}

// =============================================================================
// CalculateJumpState  -  produces IsGrounded + JumpStageAlpha for the AnimBP's
// jump blend. All procedural foot/step logic was removed (legs are animated).
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CalculateJumpState()
{
	if (IsValid(CharacterMovementComponent))
	{
		IsGrounded = CharacterMovementComponent->IsMovingOnGround();
	}

	JumpStageAlpha = FMath::Clamp(CharZVelocity / (CharacterJumpZVelocity * 0.715f), -1.0f, 1.0f);
}

// =============================================================================
// UpdateHandPose
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::UpdateHandPose(ESide Hand)
{
	USkeletalMeshComponent* HandMesh = (Hand == ESide::Left) ? LeftHandMesh : RightHandMesh;
	const ESide SourceHand = (Hand == ESide::Left) ? LeftSourceHandSide : RightSourceHandSide;

	const FPoseSnapshot Pose = CopyHandPose(HandMesh, CharacterMesh, Hand, SourceHand);

	switch (Hand)
	{
	case ESide::Left:
		LeftHandPose = Pose;
		break;
	case ESide::Right:
		RightHandPose = Pose;
		break;
	default:
		break;
	}
}

// =============================================================================
// SetHandTargetMesh
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::SetHandTargetMesh(USkeletalMeshComponent* InLeftHandMesh, ESide InLeftSourceHandSide, USkeletalMeshComponent* InRightHandMesh, ESide InRightSourceHandSide)
{
	LeftHandMesh = InLeftHandMesh;
	LeftSourceHandSide = InLeftSourceHandSide;
	RightHandMesh = InRightHandMesh;
	RightSourceHandSide = InRightSourceHandSide;
}

// =============================================================================
// FindCharacterNameFromMesh
// =============================================================================
FName UZeroPay_BodyIKComponentBase_r1::FindCharacterNameFromMesh(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh)
	{
		return NAME_None;
	}

	// Compare each calibration entry's soft mesh path against the mesh's current asset path.
	const FString TargetPath = Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetPathName() : FString();

	for (const TPair<FName, FCharacterCalibrationData>& Pair : CharacterCalibrationDataLibrary.CharacterCalibrationData)
	{
		const TSoftObjectPtr<USkeletalMesh> SoftMesh(Pair.Value.CharacterMesh);
		const FString EntryPath = SoftMesh.ToSoftObjectPath().ToString();
		if (EntryPath == TargetPath)
		{
			return Pair.Key;
		}
	}

	return NAME_None;
}

// =============================================================================
// ChangeCharacter
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::ChangeCharacter(FName InCharacterName)
{
	const FCharacterCalibrationData* Found = CharacterCalibrationDataLibrary.CharacterCalibrationData.Find(InCharacterName);
	if (!Found)
	{
		UKismetSystemLibrary::PrintString(this, TEXT("Character name not found in library"),
			true, true, FLinearColor(0.0f, 0.66f, 1.0f, 1.0f), 2.0f);
		return;
	}

	CharacterName = InCharacterName;

	if (IsValid(CharacterMesh))
	{
		CharacterMesh->SetSkeletalMeshAsset(Found->CharacterMesh);
		CharacterHMDHeight = Found->HMDHeight;
		NeckFromHMD = Found->NeckFromHMD;
		ShoulderCenterFromNeck = Found->ShoulderCenterFromNeck;
		PelvisFromShoulderCenter = Found->PelvisFromShoulderCenter;
		ShoulderDistance = Found->ShoulderJointDistance;
		ClavicleBoneLength = Found->ClavicleBoneLength;
		CharacterArmLength = Found->ArmLength;
		ClavicleFromShoulderCenter = Found->ClavicleFromShoulderCenter;
		CharacterRestingPelvisRotation = Found->CharacterRestingPelvisRotation;

		Calibrate(PlayerRealHMDHeight, HeightAlpha);

		UGameplayStatics::PlaySoundAtLocation(
			this, ChangeCharacterSound, ShoulderCenterTransform.GetLocation(), FRotator::ZeroRotator,
			1.0f, 1.0f, 0.0f, nullptr, nullptr, nullptr);
	}
	else
	{
		UKismetSystemLibrary::PrintString(this,
			TEXT("Character mesh refrence is invalid! Please asign a character mesh on Initilize()"),
			true, true, FLinearColor(0.0f, 0.66f, 1.0f, 1.0f), 2.0f);
	}
}

// =============================================================================
// ChangeCharacterAsync (BP graph not supplied; forwards to ChangeCharacter)
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::ChangeCharacterAsync(FName InCharacterName)
{
	ChangeCharacter(InCharacterName);
}

// =============================================================================
// CycleCharacters (ExecutionSequence then_0..then_1)
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::CycleCharacters()
{
	// then_0 : find index of the current character, pick the next (wrapping).
	TArray<FName> Keys;
	CharacterCalibrationDataLibrary.CharacterCalibrationData.GetKeys(Keys);

	// idx ends at the matched index (break) or the last index (no match) in the BP.
	int32 FoundIndex = FMath::Max(Keys.Num() - 1, 0);
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (CharacterName == Keys[Index])
		{
			FoundIndex = Index; // break
			break;
		}
	}

	FName NextCharacterName = NAME_None;
	if (Keys.Num() > 0)
	{
		if (FoundIndex == (Keys.Num() - 1))
		{
			NextCharacterName = Keys[0];
		}
		else
		{
			NextCharacterName = Keys[FoundIndex + 1];
		}
	}

	// then_1
	ChangeCharacterAsync(NextCharacterName);
}

// =============================================================================
// Calibrate (ExecutionSequence then_0..then_1)
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::Calibrate(float OptionalHMDHeightOverride, float OldHeightAlpha)
{
	// then_0
	const float OldWorldToMeters = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this);

	if (OptionalHMDHeightOverride == 0.0f)
	{
		FRotator DeviceRotation;
		FVector DevicePosition;
		UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(DeviceRotation, DevicePosition);
		PlayerRealHMDHeight = FMath::Clamp(
			(DevicePosition.Z / UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this)) * 100.0f,
			RealPlayerHMDHeightClamp.Min, RealPlayerHMDHeightClamp.Max);
	}
	else
	{
		PlayerRealHMDHeight = OptionalHMDHeightOverride;
	}

	UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(this, (CharacterHMDHeight / PlayerRealHMDHeight) * 100.0f);

	// Owner cast -> read & write VirtualHeightOffset (see header note).
	AActor* Owner = GetOwner();
	const float NewWorldToMeters = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(this);
	const FVector CapsuleLoc = CapsuleBase ? CapsuleBase->GetComponentLocation() : FVector::ZeroVector;
	const float HeadHeightZ = (HeadTransform.GetLocation() - CapsuleLoc).Z;

	const float NewVirtualHeightOffset =
		GetOwnerVirtualHeightOffset(Owner) +
		(UKismetMathLibrary::MapRangeClamped(OldHeightAlpha, 0.0f, 1.0f,
			MinCrouchedHeightFactor * CharacterHMDHeight, CharacterHMDHeight) -
			((NewWorldToMeters / OldWorldToMeters) * HeadHeightZ));
	SetOwnerVirtualHeightOffset(Owner, NewVirtualHeightOffset);

	// then_1 : arm-scaling branch is an unconnected stub in the Blueprint.
	if (ScaleArmsDuringCalibration)
	{
		// (BP: empty)
	}
	else
	{
		// (BP: empty)
	}
}

// =============================================================================
// DrawDebugBodyIK (Blueprint graph "DrawDebug")
// =============================================================================
void UZeroPay_BodyIKComponentBase_r1::DrawDebugBodyIK()
{
	if (!DrawDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor White = FColor::White;

	// then_0 : head -> neck -> shoulder-center -> pelvis
	const FVector HeadLoc = HeadTransform.GetLocation();
	const FVector NeckLoc = VectorToRotator(NormalizedHeadRotation).RotateVector(NeckFromHMD) + HeadLoc;

	DrawDebugLine(World, HeadLoc, NeckLoc, White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, NeckLoc, ShoulderCenterTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, ShoulderCenterTransform.GetLocation(), ShoulderCenterTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, ShoulderCenterTransform.GetLocation(), PelvisTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, PelvisTransform.GetLocation(), PelvisTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);

	// then_1 : arms
	DrawDebugCoordinateSystem(World, LeftShoulderTransform.GetLocation(), LeftShoulderTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, RightShoulderTransform.GetLocation(), RightShoulderTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, LeftElbowTransform.GetLocation(), LeftElbowTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, RightElbowTransform.GetLocation(), RightElbowTransform.Rotator(), 10.0f, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, LeftShoulderTransform.GetLocation(), LeftElbowTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, LeftElbowTransform.GetLocation(), LeftHandTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, RightShoulderTransform.GetLocation(), RightElbowTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, RightElbowTransform.GetLocation(), RightHandTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, LeftShoulderTransform.GetLocation(), LeftHandTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
	DrawDebugLine(World, RightShoulderTransform.GetLocation(), RightHandTransform.GetLocation(), White, false, 0.0f, 0, 1.0f);
}

// AddPastThreshold(Input, Threshold, Scalar) = FMax(Input - Threshold, 0) * Scalar
float UZeroPay_BodyIKComponentBase_r1::AddPastThreshold(float Value, float Threshold, float Scalar)
{
	return FMath::Max(Value - Threshold, 0.0f) * Scalar;
}

// Deadzone(Input, Start, End) = (FClamp(Input, Start, End) - Start) / (End - Start)
float UZeroPay_BodyIKComponentBase_r1::Deadzone(float Value, float RangeMin, float RangeMax)
{
	return (FMath::Clamp(Value, RangeMin, RangeMax) - RangeMin) / (RangeMax - RangeMin);
}

// TransformInLS(Transform, Vector): keep Transform's rotation & scale, move location to Transform.TransformPosition(Vector)
FTransform UZeroPay_BodyIKComponentBase_r1::TransformInLS(const FTransform& LocalSpace, const FVector& Transform)
{
	return FTransform(LocalSpace.Rotator(), LocalSpace.TransformPosition(Transform), LocalSpace.GetScale3D());
}

// AddRotation(Transform, Rotation): keep location & scale, compose Rotation onto Transform.Rotation
FTransform UZeroPay_BodyIKComponentBase_r1::AddRotation(const FTransform& Transform, const FRotator& Rotation)
{
	return FTransform(
		UKismetMathLibrary::ComposeRotators(Rotation, Transform.Rotator()),
		Transform.GetLocation(),
		Transform.GetScale3D());
}

// VectorToRotator(V) = FRotator(Roll=V.X, Pitch=V.Y, Yaw=V.Z)
FRotator UZeroPay_BodyIKComponentBase_r1::VectorToRotator(const FVector& Vector)
{
	return FRotator(Vector.Y, Vector.Z, Vector.X); // FRotator(Pitch, Yaw, Roll)
}

// GetTwistFromRotation(R): twist around X axis (quat X/W component)
float UZeroPay_BodyIKComponentBase_r1::GetTwistFromRotation(const FRotator& Rotation)
{
	const FQuat Q = Rotation.Quaternion();
	FQuat TwistQuat(Q.X, 0.0f, 0.0f, Q.W);
	TwistQuat.Normalize(0.0001f);
	return TwistQuat.Rotator().Roll;
}

// GetHMDYaw(HMDRotation) -> PureYaw rotator (a stable, levelled yaw even when looking up/down)
FRotator UZeroPay_BodyIKComponentBase_r1::GetHMDYaw(const FRotator& Rotation)
{
	const FVector Fwd = UKismetMathLibrary::GetForwardVector(Rotation);
	const FVector ForwardLeveled = FVector(Fwd.X, Fwd.Y, 0.0f).GetSafeNormal(0.0001f);

	const float UpFlip = (Fwd.Z > 0.0f) ? -1.0f : 1.0f;
	const FVector UpScaled = UKismetMathLibrary::GetUpVector(Rotation) * UpFlip;
	const FVector Mixed = FVector(UpScaled.X, UpScaled.Y, 0.0f).GetSafeNormal(0.0001f);

	const float Alpha = Square(FMath::Clamp(FVector::DotProduct(Fwd, ForwardLeveled), 0.0f, 1.0f));
	const FVector LerpedX = FMath::Lerp(Mixed, ForwardLeveled, Alpha);

	return UKismetMathLibrary::MakeRotFromXZ(LerpedX, FVector(0.0f, 0.0f, 1.0f));
}

// CopyHandPose: rebuild a finger-bone pose snapshot, remapping side suffix when sides differ.
FPoseSnapshot UZeroPay_BodyIKComponentBase_r1::CopyHandPose(USkeletalMeshComponent* HandMesh, USkeletalMeshComponent* TargetMesh, ESide TargetHand, ESide SourceHand)
{
	FPoseSnapshot NewPose;

	if (!IsValid(HandMesh) || !IsValid(TargetMesh))
	{
		return NewPose;
	}

	const FString SideSuffix = (SourceHand == ESide::Left) ? TEXT("_l") : TEXT("_r");
	const FString TargetSideChar = (TargetHand == ESide::Left) ? TEXT("l") : TEXT("r");
	const bool bSidesDiffer = (TargetHand != SourceHand);

	const int32 NumBones = HandMesh->GetNumBones();
	for (int32 i = 0; i <= (NumBones - 1); ++i)
	{
		const FName OriginalBoneName = HandMesh->GetBoneName(i);
		const FString OriginalStr = OriginalBoneName.ToString();

		// Only finger bones: contain the source side suffix AND a numbered joint ("_0").
		if (OriginalStr.Contains(SideSuffix, ESearchCase::IgnoreCase, ESearchDir::FromEnd) &&
			OriginalStr.Contains(TEXT("_0"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			FName NewBoneName = OriginalBoneName;
			if (bSidesDiffer)
			{
				NewBoneName = FName(*(OriginalStr.LeftChop(1) + TargetSideChar));
			}

			NewPose.BoneNames.Add(NewBoneName);

			const FRotator DeltaRotation =
				HandMesh->GetDeltaTransformFromRefPose(OriginalBoneName, NAME_None).Rotator();

			// Axis-remapped delta: Roll=-Yaw, Pitch=Pitch, Yaw=Roll
			const FRotator RemappedRot = FRotator(DeltaRotation.Pitch, DeltaRotation.Roll, DeltaRotation.Yaw * -1.0f);

			const int32 TargetBoneIndex = TargetMesh->GetBoneIndex(NewBoneName);
			const FTransform TargetRefPose = TargetMesh->GetRefPoseTransform(TargetBoneIndex);

			NewPose.LocalTransforms.Add(AddRotation(TargetRefPose, RemappedRot));
		}
	}

	NewPose.SkeletalMeshName = FName(TEXT("Mesh"));
	NewPose.SnapshotName = FName(TEXT("HandPose"));
	NewPose.bIsValid = true;
	return NewPose;
}
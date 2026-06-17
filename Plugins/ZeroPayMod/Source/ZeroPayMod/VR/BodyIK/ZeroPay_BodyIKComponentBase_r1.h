#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "Curves/CurveFloat.h"
#include "ZeroPay_BodyIKComponentBase_r1.generated.h"

class USceneComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class ESide : uint8
{
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	None UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class EBodyIKDetailTier : uint8
{
	Full		UMETA(DisplayName = "Full (all IK)"),    
	IgnoreHands UMETA(DisplayName = "Ingore hands"),
	Culled		 UMETA(DisplayName = "Culled (skipped)")     
};

USTRUCT(BlueprintType)
struct FSMinMax
{
	GENERATED_BODY()

public:
	FSMinMax() = default;

	FSMinMax(float InMin, float InMax)
		: Min(InMin)
		, Max(InMax)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MinMax")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MinMax")
	float Max = 0.0f;
};

USTRUCT(BlueprintType)
struct FCharacterCalibrationData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	float HMDHeight = 168.957255f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FVector NeckFromHMD = FVector(-13.604625f, -0.0f, -6.382154f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FVector ShoulderCenterFromNeck = FVector(-3.220374f, 0.000002f, -18.991853f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FVector PelvisFromShoulderCenter = FVector(4.838118f, -0.000002f, -47.686466f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	float ShoulderJointDistance = 38.019762f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	float ClavicleBoneLength = 17.809523f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FVector FootJointFromBall = FVector(-14.923062f, 1.6902f, 8.237634f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	float ArmLength = 55.022213f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FTransform ClavicleFromShoulderCenter = FTransform(
		FRotator(-8.777083f, -0.000038f, -2.661866f),
		FVector(0.817438f, -1.427906f, 2.717803f),
		FVector(1.0f, 1.0f, 1.0f)
	);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manny")
	FRotator CharacterRestingPelvisRotation = FRotator(-90.0f, 86.366893f, 180.0f);
};

USTRUCT(BlueprintType)
struct FCharacterCalibrationDataLibrary
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BodyIK")
	TMap<FName, FCharacterCalibrationData> CharacterCalibrationData;
};

UCLASS(Blueprintable, ClassGroup = (ZeroPay), meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_BodyIKComponentBase_r1 : public UActorComponent
{
	GENERATED_BODY()

public:
	UZeroPay_BodyIKComponentBase_r1();

protected:

	// -------------------------------------------------------------------------
	// Components
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Head = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> LeftWrist = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> RightWrist = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> CapsuleBase = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> LeftHandMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> RightHandMesh = nullptr;


	// -------------------------------------------------------------------------
	// Runtime
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float LastBodyYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FVector NormalizedHeadRotation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float MaxWalkSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterHMDHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FTransform ShoulderCenterTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FVector NeckFromHMD = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FVector ShoulderCenterFromNeck = FVector::ZeroVector;

	// [BP-FIX] was float; the Blueprint treats this as a positional offset vector.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FVector PelvisFromShoulderCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float HeightAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FVector Char2DVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FTransform LeftShoulderTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FTransform RightShoulderTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float ShoulderDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float ClavicleBoneLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FRotator LeftLastElbowRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FRotator RightLastElbowRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float BodyYawDueToVelocity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float LastBodyPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float JumpStageAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	bool IsGrounded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharZVelocity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FName CharacterName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterJumpZVelocity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterArmLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FTransform ClavicleFromShoulderCenter = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FRotator CharacterRestingPelvisRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CurrentHMDHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float SpeedAlphaNoOverride = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FPoseSnapshot LeftHandPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FPoseSnapshot RightHandPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	FCharacterCalibrationDataLibrary CharacterCalibrationDataLibrary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMesh> CharactersInMemory = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	bool Initialized = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	ESide LeftSourceHandSide = ESide::Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	ESide RightSourceHandSide = ESide::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterArmRealScaleFactor_L = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (AllowPrivateAccess = "true"))
	float CharacterArmRealScaleFactor_R = 1.0f;


	// -------------------------------------------------------------------------
	// Transforms
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform HeadTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform LeftHandTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform RightHandTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform PelvisTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FRotator LeftClavicleRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FRotator RightClavicleRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform LeftElbowTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FTransform RightElbowTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	float LeftHandRollRaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	float RightHandRollRaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FVector LeftClampedHandLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms", meta = (AllowPrivateAccess = "true"))
	FVector RightClampedHandLocation = FVector::ZeroVector;


	// -------------------------------------------------------------------------
	// Settings - General
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|General", meta = (AllowPrivateAccess = "true"))
	float PlayerRealHMDHeight = 177.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|General", meta = (AllowPrivateAccess = "true"))
	FSMinMax RealPlayerHMDHeightClamp = FSMinMax(140.0f, 210.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|General", meta = (AllowPrivateAccess = "true"))
	bool DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|General", meta = (AllowPrivateAccess = "true"))
	bool LoadAllCharactersIntoMemory = true;


	// -------------------------------------------------------------------------
	// Settings - Body
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float BodyPitchRollScalar = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float BodyYawInterpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float HandBodyYawInfluence = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float BodyYawFromHandExtend = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float HandMaxBodyYawInfluenceAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	float MinCrouchedHeightFactor = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Body", meta = (AllowPrivateAccess = "true"))
	FVector BodyOffset = FVector::ZeroVector;


	// -------------------------------------------------------------------------
	// Settings - Arms
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ClavicleRotationWeight = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ElbowRotationSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ElbowRestingAngle = 22.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	FSMinMax ElbowRotationLimits = FSMinMax(-40.0f, 135.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ElbowSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ArmBaseScale = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float MaxArmStretch = 0.975f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	bool ScaleArmsPastLimit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	float ArmStretchLimitFactor = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Arms", meta = (AllowPrivateAccess = "true"))
	bool ScaleArmsDuringCalibration = true;


	// -------------------------------------------------------------------------
	// Settings - Sounds
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Sounds", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> ChangeCharacterSound = nullptr;

	// -------------------------------------------------------------------------
	// Settings - Detail Scaling (LOD)
	// -------------------------------------------------------------------------

	// Distance (cm) beyond which the avatar hands are not updated
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HandUpdateTierDistance = 1000.0f;

	// Distance (cm) beyond which the avatar is culled (not solved) unless still rendered closer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FarTierDistance = 2500.0f;

	// Runtime: tier chosen on the last UpdateBody call (read-only, for debugging/inspection).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true"))
	EBodyIKDetailTier CurrentDetailTier = EBodyIKDetailTier::Full ;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings|DetailScaling")
	float CharacterDist;

public:
	
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void Initialize(USceneComponent* InHead, USceneComponent* InLeftWrist, USceneComponent* InRightWrist, USceneComponent* InCapsuleBase, USkeletalMeshComponent* InCharacterMesh, UCharacterMovementComponent* InCharacterMovementComponent);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void UpdateBody();

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void Calibrate(float OptionalHMDHeightOverride = 0.0f, float OldHeightAlpha = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void ChangeCharacter(FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void ChangeCharacterAsync(FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void SetHandTargetMesh(USkeletalMeshComponent* InLeftHandMesh, ESide InLeftSourceHandSide, USkeletalMeshComponent* InRightHandMesh, ESide InRightSourceHandSide);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void CycleCharacters();

protected:
	// ---- Detail scaling (LOD) ----
	EBodyIKDetailTier ComputeDetailTier() ;
	void SolveIK();
	void SolveArmCheap(ESide Side); 

	void ProcessInputs();
	FTransform ProcessHandInput(const FTransform& HandInput, ESide Hand) const;
	FVector NormalizeHeadRotation(const FRotator& HeadRotation) const;
	void CalculateShoulderCenterAndNeck();
	float BodyRoll(float HeadRoll) const;
	float BodyPitch(float HeadPitch) const;
	float BodyYaw(float HeadYaw);
	float GetBodyDeltaYawFromHands(float HeadYaw) const;
	float GetBodyDeltaYawFromVelocity(float Yaw) const;
	void CalculateDependentTransforms();
	void CalculateBodyPrerequisites();
	void CalculateJumpState();       // IsGrounded + JumpStageAlpha for the AnimBP jump blend
	void DrawDebugBodyIK();
	void CalculateClavicleOffset(ESide Side);
	float CalculateShoulderTransform(ESide Side);
	void SolveArm(ESide Side, float HandElbowInfluence);
	float CalculateElbowRoll(float HandRoll) const;
	void ClampHandPosition(ESide Hand);
	void GetHeight(float& Alpha, float& Height) const;
	void CalculateCharacterScale();
	FName FindCharacterNameFromMesh(USkeletalMeshComponent* Mesh) const;
	void UpdateHandPose(ESide Hand);

	float GetWorldDeltaSecondsSafe() const;

	static FRotator GetHMDYaw(const FRotator& Rotation);                
	static float AddPastThreshold(float Value, float Threshold, float Scalar); 
	static FTransform TransformInLS(const FTransform& LocalSpace, const FVector& Transform);
	static float Deadzone(float Value, float RangeMin, float RangeMax);
	static FTransform AddRotation(const FTransform& Transform, const FRotator& Rotation);
	static FRotator VectorToRotator(const FVector& Vector);
	static float GetTwistFromRotation(const FRotator& Rotation);          // [BP-FIX] twist around X, no Axis arg
	static FPoseSnapshot CopyHandPose(USkeletalMeshComponent* HandMesh, USkeletalMeshComponent* TargetMesh, ESide TargetHand, ESide SourceHand); // [BP-FIX]
};
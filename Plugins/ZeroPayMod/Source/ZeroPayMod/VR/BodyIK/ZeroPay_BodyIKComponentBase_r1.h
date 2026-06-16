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

// Level-of-detail tier chosen each frame for the body IK solve. Higher tiers are
// cheaper. The local player is always Hero; everyone else is selected by whether
// their mesh is on-screen and how far they are from the viewer.
UENUM(BlueprintType)
enum class EBodyIKDetailTier : uint8
{
	Hero  UMETA(DisplayName = "Hero (local, full rate)"),       // full solve, every frame
	Near  UMETA(DisplayName = "Near (full, rate-limited)"),     // full solve, throttled rate
	Mid   UMETA(DisplayName = "Mid (no foot traces)"),          // torso + arms, cheap feet, no traces
	Far   UMETA(DisplayName = "Far (cheap arms)"),              // torso + cheap arms, cheap feet, low rate
	Culled UMETA(DisplayName = "Culled (skipped)")              // off-screen / beyond cutoff - not solved
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

	// [BP-FIX] was bool; the Blueprint stores the body yaw in degrees here.
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

	// [BP-FIX] was bool; the Blueprint stores the body pitch in degrees here.
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

	// Blueprint variable name "SpeedAlphaNoOverdrive".
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
	//
	// Drives how UpdateBody degrades for non-local avatars. The local player
	// (bIsLocalPlayer) is always solved at full rate. Everyone else is gated
	// first by whether their mesh was recently rendered, then tiered by distance
	// from the viewer's camera. Each tier below Hero also throttles its solve
	// rate, so distant players solve a few times a second instead of every frame.
	// -------------------------------------------------------------------------

	// Master switch. When false, every avatar solves at full rate (original behaviour).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true"))
	bool bEnableDetailScaling = true;

	// Set true on the locally-controlled avatar so it never degrades.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true"))
	bool bIsLocalPlayer = false;

	// Distance (cm) from the viewer camera at/under which a visible avatar uses the full solve.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float NearTierDistance = 900.0f;

	// Distance (cm) up to which the avatar keeps full arms but drops foot traces.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MidTierDistance = 1500.0f;

	// Distance (cm) beyond which the avatar is culled (not solved) unless still rendered closer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FarTierDistance = 2500.0f;

	// Solve rates (Hz) per tier. Hero is always every frame and ignores these.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float NearTierRateHz = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MidTierRateHz = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float FarTierRateHz = 8.0f;

	// How recently (seconds) the mesh must have been rendered to avoid being culled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RenderedTolerance = 0.5f;

	// Runtime: tier chosen on the last UpdateBody call (read-only, for debugging/inspection).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings|DetailScaling", meta = (AllowPrivateAccess = "true"))
	EBodyIKDetailTier CurrentDetailTier = EBodyIKDetailTier::Hero;

	// Runtime: accumulates delta time to drive the per-tier rate gate.
	float TimeSinceLastSolve = 0.0f;

public:
	// Call once on the locally-controlled avatar (e.g. on possession) so it is treated as Hero.
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void SetIsLocalPlayer(bool bInIsLocalPlayer) { bIsLocalPlayer = bInIsLocalPlayer; }

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void Initialize(USceneComponent* InHead, USceneComponent* InLeftWrist, USceneComponent* InRightWrist, USceneComponent* InCapsuleBase, USkeletalMeshComponent* InCharacterMesh, UCharacterMovementComponent* InCharacterMovementComponent);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void UpdateBody();

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void Calibrate(float OptionalHMDHeightOverride = 0.0f, float OldHeightAlpha = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void ChangeCharacter(FName InCharacterName);

	// [BP-FIX] CycleCharacters drives ChangeCharacterAsync in the Blueprint. The async
	// graph itself is not part of the supplied export; here it forwards to ChangeCharacter.
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void ChangeCharacterAsync(FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void SetHandTargetMesh(USkeletalMeshComponent* InLeftHandMesh, ESide InLeftSourceHandSide, USkeletalMeshComponent* InRightHandMesh, ESide InRightSourceHandSide);

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|BodyIK")
	void CycleCharacters();

protected:
	// ---- Detail scaling (LOD) ----
	EBodyIKDetailTier ComputeDetailTier() const;
	float GetSolveIntervalForTier(EBodyIKDetailTier Tier) const;
	void SolveFull();      // full fidelity (torso + arms + hand poses + debug)
	void SolveReduced();   // torso + full arms, no finger pose, no debug
	void SolveMinimal();   // torso + cheap arms, no finger pose, no debug
	void SolveArmCheap(ESide Side); // shoulder + straight-line elbow, no twist/interp

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

	static FRotator GetHMDYaw(const FRotator& Rotation);                  // [BP-FIX] returns FRotator (PureYaw)
	static float AddPastThreshold(float Value, float Threshold, float Scalar); // [BP-FIX] gains Scalar
	static FTransform TransformInLS(const FTransform& LocalSpace, const FVector& Transform);
	static float Deadzone(float Value, float RangeMin, float RangeMax);
	static FTransform AddRotation(const FTransform& Transform, const FRotator& Rotation);
	static FRotator VectorToRotator(const FVector& Vector);
	static float GetTwistFromRotation(const FRotator& Rotation);          // [BP-FIX] twist around X, no Axis arg
	static FPoseSnapshot CopyHandPose(USkeletalMeshComponent* HandMesh, USkeletalMeshComponent* TargetMesh, ESide TargetHand, ESide SourceHand); // [BP-FIX]
};
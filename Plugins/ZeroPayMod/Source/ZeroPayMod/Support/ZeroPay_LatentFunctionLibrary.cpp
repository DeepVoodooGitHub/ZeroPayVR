#include "ZeroPay_LatentFunctionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LatentActions.h"
#include "GripMotionControllerComponent.h"
#include "VR/GameMode/ZeroPay_GameMode_r1.h"

class FZeroPay_SpawnActor_LatentAction : public FPendingLatentAction
{
public:

	FWeakObjectPtr TargetGameModePtr;
	FString ItemID;
	TWeakObjectPtr<AZeroPay_VRCharacterBase_r1> OwningCharacterPtr;
	TWeakObjectPtr<UGripMotionControllerComponent> GripMotionControllerPtr;
	EZeroPayVRItemDefaultSpawnLocation SpawnLocation;
	EZeroPayVRItemSpawnCollision SpawnCollision;
	int SpawnLocationIndex;

	TWeakObjectPtr<AActor> SpawnedActorPtr;

	AActor*& SpawnedActorOutput;
	bool& AttachedCorrectlyOutput;

	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	bool bHasWaitedOneTick = false;

	FZeroPay_SpawnActor_LatentAction(AZeroPay_GameMode_r1* InTargetGameMode, const FString& InItemID, AZeroPay_VRCharacterBase_r1* InOwningCharacter, UGripMotionControllerComponent* InGripMotionController, EZeroPayVRItemDefaultSpawnLocation InSpawnLocation, int InSpawnLocationIndex, EZeroPayVRItemSpawnCollision InSpawnCollision, AActor* InSpawnedActor, AActor*& InSpawnedActorOutput, bool& InAttachedCorrectlyOutput, const FLatentActionInfo& InLatentInfo)
		: TargetGameModePtr(InTargetGameMode)
		, ItemID(InItemID)
		, OwningCharacterPtr(InOwningCharacter)
		, GripMotionControllerPtr(InGripMotionController)
		, SpawnLocation(InSpawnLocation)
		, SpawnLocationIndex(InSpawnLocationIndex)
		, SpawnCollision(InSpawnCollision)
		, SpawnedActorPtr(InSpawnedActor)
		, SpawnedActorOutput(InSpawnedActorOutput)
		, AttachedCorrectlyOutput(InAttachedCorrectlyOutput)
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (!bHasWaitedOneTick)
		{
			bHasWaitedOneTick = true;
			Response.DoneIf(false);
			return;
		}

		AZeroPay_GameMode_r1* TargetGameMode = Cast<AZeroPay_GameMode_r1>(TargetGameModePtr.Get());
		AZeroPay_VRCharacterBase_r1* OwningCharacter = OwningCharacterPtr.Get();
		UGripMotionControllerComponent* GripMotionController = GripMotionControllerPtr.Get();
		AActor* SpawnedActor = SpawnedActorPtr.Get();

		bool bAttachedCorrectly = false;

		if (TargetGameMode && SpawnedActor)
		{
			bAttachedCorrectly = TargetGameMode->Internal_GrabActor(ItemID, OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision, SpawnedActor);
		}

		SpawnedActorOutput = SpawnedActor;
		AttachedCorrectlyOutput = bAttachedCorrectly;

		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("ZeroPay latent action: SpawnActor");
	}
#endif
};

void UZeroPay_LatentFunctionLibrary::SpawnActor(UObject* WorldContextObject, AZeroPay_GameMode_r1* TargetGameMode, const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision, AActor*& SpawnedActor, bool& AttachedCorrectly, FLatentActionInfo LatentInfo)
{
	SpawnedActor = nullptr;
	AttachedCorrectly = false;

	if (!WorldContextObject || !TargetGameMode)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);

	if (!World)
	{
		return;
	}

	FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

	if (LatentActionManager.FindExistingAction<FZeroPay_SpawnActor_LatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	SpawnedActor = TargetGameMode->Internal_SpawnActor(ItemID, OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision);

	LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FZeroPay_SpawnActor_LatentAction(TargetGameMode, ItemID, OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision, SpawnedActor, SpawnedActor, AttachedCorrectly, LatentInfo));
}
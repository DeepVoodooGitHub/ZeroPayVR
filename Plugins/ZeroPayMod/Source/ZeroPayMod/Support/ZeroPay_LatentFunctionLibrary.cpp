#include "ZeroPay_LatentFunctionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LatentActions.h"
#include "GripMotionControllerComponent.h"
#include "VR/GameMode/ZeroPay_GameMode_r1.h"

class FZeroPay_SpawnItem_LatentAction : public FPendingLatentAction
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

	FZeroPay_SpawnItem_LatentAction(AZeroPay_GameMode_r1* InTargetGameMode, const FString& InItemID, AZeroPay_VRCharacterBase_r1* InOwningCharacter, UGripMotionControllerComponent* InGripMotionController, EZeroPayVRItemDefaultSpawnLocation InSpawnLocation, int InSpawnLocationIndex, EZeroPayVRItemSpawnCollision InSpawnCollision, AActor* InSpawnedActor, AActor*& InSpawnedActorOutput, bool& InAttachedCorrectlyOutput, const FLatentActionInfo& InLatentInfo)
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
			bAttachedCorrectly = TargetGameMode->Internal_GrabActor(OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision, SpawnedActor);
		}

		SpawnedActorOutput = SpawnedActor;
		AttachedCorrectlyOutput = bAttachedCorrectly;

		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("ZeroPay latent action: SpawnItem");
	}
#endif
};

void UZeroPay_LatentFunctionLibrary::SpawnItem(UObject* WorldContextObject, AZeroPay_GameMode_r1* TargetGameMode, const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision, AActor*& SpawnedActor, bool& AttachedCorrectly, EZeroPaySpawnItemLatentStartResult& StartResult, FLatentActionInfo LatentInfo)
{
	SpawnedActor = nullptr;
	AttachedCorrectly = false;
	StartResult = EZeroPaySpawnItemLatentStartResult::Failure;

	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);

	if (!World)
	{
		return;
	}

	// GameMode only exists on the server, but this makes the authority check explicit.
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (TargetGameMode == nullptr)
	{
		AGameModeBase* AuthGameMode = World->GetAuthGameMode();
		if (!AuthGameMode)
		{
			return;
		}

		// This succeeds for AZeroPay_GameMode_r1 AND any class derived from it.
		TargetGameMode = Cast<AZeroPay_GameMode_r1>(AuthGameMode);
		if (!TargetGameMode)
		{
			UE_LOG(LogTemp, Warning, TEXT("Current GameMode is not based on AZeroPay_GameMode_r1. Found: %s"), *GetNameSafe(AuthGameMode));
			return;
		}
	}

	if (!LatentInfo.CallbackTarget)
	{
		return;
	}

	FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

	if (LatentActionManager.FindExistingAction<FZeroPay_SpawnItem_LatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		// A latent action for this node is already pending.
		// Treat this as success because the latent Completed pin should still fire later.
		StartResult = EZeroPaySpawnItemLatentStartResult::Success;
		return;
	}

	SpawnedActor = TargetGameMode->Internal_SpawnItem(ItemID, OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision);

	if (!SpawnedActor)
	{
		return;
	}

	LatentActionManager.AddNewAction(
		LatentInfo.CallbackTarget,
		LatentInfo.UUID,
		new FZeroPay_SpawnItem_LatentAction(
			TargetGameMode,
			ItemID,
			OwningCharacter,
			GripMotionController,
			SpawnLocation,
			SpawnLocationIndex,
			SpawnCollision,
			SpawnedActor,
			SpawnedActor,
			AttachedCorrectly,
			LatentInfo
		)
	);

	StartResult = EZeroPaySpawnItemLatentStartResult::Success;
}
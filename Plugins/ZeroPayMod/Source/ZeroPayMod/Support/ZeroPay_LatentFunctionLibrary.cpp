#include "ZeroPay_LatentFunctionLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LatentActions.h"
#include "GameFramework/GameModeBase.h"
#include "GripMotionControllerComponent.h"
#include "VR/GameMode/ZeroPay_GameMode_r1.h"

// ---------------------------------------------------------------------------
// Latent action: completes on its first update and reports Failure.
// Used for every early-out so the Blueprint's Failure exec pin still fires.
// ---------------------------------------------------------------------------
class FZeroPay_SpawnItemFailed_LatentAction : public FPendingLatentAction
{
public:

	EZeroPaySpawnItemLatentStartResult& ResultOutput;

	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FZeroPay_SpawnItemFailed_LatentAction(EZeroPaySpawnItemLatentStartResult& InResultOutput, const FLatentActionInfo& InLatentInfo)
		: ResultOutput(InResultOutput)
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		ResultOutput = EZeroPaySpawnItemLatentStartResult::Failure;
		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("ZeroPay latent action: SpawnItem (failed)");
	}
#endif
};

// ---------------------------------------------------------------------------
// Latent action: waits one tick after the spawn, then grabs/attaches the item.
// Reports Success if the spawned actor still exists, otherwise Failure.
// ---------------------------------------------------------------------------
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
	EZeroPaySpawnItemLatentStartResult& ResultOutput;

	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	bool bHasWaitedOneTick = false;

	FZeroPay_SpawnItem_LatentAction(
		AZeroPay_GameMode_r1* InTargetGameMode,
		const FString& InItemID,
		AZeroPay_VRCharacterBase_r1* InOwningCharacter,
		UGripMotionControllerComponent* InGripMotionController,
		EZeroPayVRItemDefaultSpawnLocation InSpawnLocation,
		int InSpawnLocationIndex,
		EZeroPayVRItemSpawnCollision InSpawnCollision,
		AActor* InSpawnedActor,
		AActor*& InSpawnedActorOutput,
		bool& InAttachedCorrectlyOutput,
		EZeroPaySpawnItemLatentStartResult& InResultOutput,
		const FLatentActionInfo& InLatentInfo)
		: TargetGameModePtr(InTargetGameMode)
		, ItemID(InItemID)
		, OwningCharacterPtr(InOwningCharacter)
		, GripMotionControllerPtr(InGripMotionController)
		, SpawnLocation(InSpawnLocation)
		, SpawnCollision(InSpawnCollision)
		, SpawnLocationIndex(InSpawnLocationIndex)
		, SpawnedActorPtr(InSpawnedActor)
		, SpawnedActorOutput(InSpawnedActorOutput)
		, AttachedCorrectlyOutput(InAttachedCorrectlyOutput)
		, ResultOutput(InResultOutput)
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

		// The actor may have been destroyed during the one-tick wait.
		ResultOutput = SpawnedActor
			? EZeroPaySpawnItemLatentStartResult::Success
			: EZeroPaySpawnItemLatentStartResult::Failure;

		Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return TEXT("ZeroPay latent action: SpawnItem");
	}
#endif
};

// ---------------------------------------------------------------------------
// Blueprint entry point.
// Latent nodes only continue execution when a latent action completes, so
// every path after a valid World + CallbackTarget must queue an action -
// otherwise neither the Success nor the Failure exec pin will ever fire.
// ---------------------------------------------------------------------------
void UZeroPay_LatentFunctionLibrary::SpawnItem(UObject* WorldContextObject, AZeroPay_GameMode_r1* TargetGameMode, const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision, AActor*& SpawnedActor, bool& AttachedCorrectly, EZeroPaySpawnItemLatentStartResult& StartResult, FLatentActionInfo LatentInfo)
{
	SpawnedActor = nullptr;
	AttachedCorrectly = false;
	StartResult = EZeroPaySpawnItemLatentStartResult::Failure;

	// Without a world or a callback target there is no way to resume the Blueprint.
	UWorld* World = WorldContextObject
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (!World || !LatentInfo.CallbackTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnItem: No valid World or CallbackTarget - Blueprint execution cannot be resumed."));
		return;
	}

	FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

	// An action for this node is already pending; its completion will fire the correct pin.
	if (LatentActionManager.FindExistingAction<FZeroPay_SpawnItem_LatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) ||
		LatentActionManager.FindExistingAction<FZeroPay_SpawnItemFailed_LatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	auto QueueFailure = [&]()
		{
			LatentActionManager.AddNewAction(
				LatentInfo.CallbackTarget,
				LatentInfo.UUID,
				new FZeroPay_SpawnItemFailed_LatentAction(StartResult, LatentInfo));
		};

	// GameMode only exists on the server, but this makes the authority check explicit.
	if (World->GetNetMode() == NM_Client)
	{
		QueueFailure();
		return;
	}

	if (TargetGameMode == nullptr)
	{
		AGameModeBase* AuthGameMode = World->GetAuthGameMode();
		if (!AuthGameMode)
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnItem: No authoritative GameMode found."));
			QueueFailure();
			return;
		}

		// This succeeds for AZeroPay_GameMode_r1 AND any class derived from it.
		TargetGameMode = Cast<AZeroPay_GameMode_r1>(AuthGameMode);
		if (!TargetGameMode)
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnItem: Current GameMode is not based on AZeroPay_GameMode_r1. Found: %s"), *GetNameSafe(AuthGameMode));
			QueueFailure();
			return;
		}
	}

	AActor* NewActor = TargetGameMode->Internal_SpawnItem(ItemID, OwningCharacter, GripMotionController, SpawnLocation, SpawnLocationIndex, SpawnCollision);

	if (!NewActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnItem: Internal_SpawnItem failed for ItemID '%s'."), *ItemID);
		QueueFailure();
		return;
	}

	SpawnedActor = NewActor;

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
			NewActor,
			SpawnedActor,
			AttachedCorrectly,
			StartResult,
			LatentInfo
		)
	);
}
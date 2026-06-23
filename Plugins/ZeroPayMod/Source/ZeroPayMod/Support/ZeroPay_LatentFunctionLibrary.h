#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LatentActions.h"
#include "Items/Interfaces/ZeroPay_VRItem_Interface_r1.h"
#include "VR/ZeroPay_VRCharacterBase_r1.h"
#include "ZeroPay_LatentFunctionLibrary.generated.h"

class AZeroPay_GameMode_r1;
class AZeroPay_VRCharacterBase_r1;
class UGripMotionControllerComponent;

UENUM(BlueprintType)
enum class EZeroPaySpawnItemLatentStartResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	Failure UMETA(DisplayName = "Failure")
};

UCLASS()
class ZEROPAYMOD_API UZeroPay_LatentFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// SERVER ONLY - Spawn an actor and optionally attach it to the supplier player. This is a latent action and will take a minimum of two frames to complete.
	//               If no "Owning Character" then the actor will be spawned at (0,0,0) and will have no "owner" assigned
	//               If no motion controller is provided then any spawn locations on the hands, defaults to the "dominant" hand
	//               For certain spawn locations (chest, etc.) the Spawn Location index indicates the slot in which the actor is attached
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Items", meta = (WorldContext = "WorldContextObject", Latent, LatentInfo = "LatentInfo", ExpandEnumAsExecs = "StartResult"))
	static void SpawnItem(UObject* WorldContextObject, AZeroPay_GameMode_r1* TargetGameMode, const FString& ItemID, AZeroPay_VRCharacterBase_r1* OwningCharacter, UGripMotionControllerComponent* GripMotionController, EZeroPayVRItemDefaultSpawnLocation SpawnLocation, int SpawnLocationIndex, EZeroPayVRItemSpawnCollision SpawnCollision, AActor*& SpawnedActor, bool& AttachedCorrectly, EZeroPaySpawnItemLatentStartResult& StartResult, FLatentActionInfo LatentInfo);
};
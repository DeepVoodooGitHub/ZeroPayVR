#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZeroPay_VRItem_Interface_r1.generated.h"

// Forward declare
class UZeroPay_VRItem_r1;

UENUM(BlueprintType)
enum class EZeroPayVRItemType : uint8
{
	NotSpecified    UMETA(DisplayName = "Not Specified"),

	// Weapons
	Gun             UMETA(DisplayName = "Gun"),
	MeleeWeapon     UMETA(DisplayName = "Melee Weapon"),
	Throwable       UMETA(DisplayName = "Throwable"),
	Explosive       UMETA(DisplayName = "Explosive"),

	// Weapon-related items
	Magazine        UMETA(DisplayName = "Magazine"),
	Ammo            UMETA(DisplayName = "Ammo"),
	Attachment      UMETA(DisplayName = "Attachment"),

	// Equipment / utility
	Equipment       UMETA(DisplayName = "Equipment"),
	Tool            UMETA(DisplayName = "Tool"),
	Consumable      UMETA(DisplayName = "Consumable"),
	ObjectiveItem    UMETA(DisplayName = "Objective Item"),

	// Mod/custom fallback
	Custom          UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
/* Where an item should be spawned on a players body */
enum class EZeroPayVRItemDefaultSpawnLocation : uint8
{
	GripController  UMETA(DisplayName = "Gripping Controller"),
	DominantHand    UMETA(DisplayName = "Dominant Hand"),
	NonDominantHand UMETA(DisplayName = "Non-Dominant Hand"),
	Chest           UMETA(DisplayName = "Chest"),
	LeftHand        UMETA(DisplayName = "Left Hand"),
	RightHand       UMETA(DisplayName = "Right Hand"),
	LeftArm         UMETA(DisplayName = "Left Arm"),
	RightArm        UMETA(DisplayName = "Right Arm"),
	Waist           UMETA(DisplayName = "Waist"),
	Back            UMETA(DisplayName = "Back"),
	DoNotAttach     UMETA(DisplayName = "Do Not Attach"),
	Default         UMETA(DisplayName = "Default")
};

UENUM(BlueprintType)
/* What to do when a collision occurs (i.e. spawn location already has an actor) */
enum class EZeroPayVRItemSpawnCollision : uint8
{
	DropHeldItem		UMETA(DisplayName = "Drop Held Item"),
	DropSpawningItem    UMETA(DisplayName = "Drop Spawning Item"),
	DoNotSpawn			UMETA(DisplayName = "Do not spawn item"),
};


UINTERFACE(BlueprintType)
class ZEROPAYMOD_API UZeroPay_VRItem_Interface_r1 : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for any object that can decide if it wants to be socketed into a UZeroPay_VRItem_r1
 */
class ZEROPAYMOD_API IZeroPay_VRItem_Interface_r1
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ZeroPay|VR Items")
	FString GetItemID() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ZeroPay|VR Items")
	EZeroPayVRItemDefaultSpawnLocation GetDefaultSpawnLocation() const;
};

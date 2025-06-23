// Deep Voodoo Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "ZeroPayMod_DefinitionDataAsset.generated.h"

UENUM(BlueprintType)
enum class EUGCTagCategory : uint8
{
	FullMod             UMETA(DisplayName = "FullMod"),
	GameMode            UMETA(DisplayName = "GameMode"),
	Level               UMETA(DisplayName = "Level"),
	Weapons             UMETA(DisplayName = "Weapons"),
	UI                  UMETA(DisplayName = "UI"),
	Cosmetics           UMETA(DisplayName = "Cosmetics"),
	Audio               UMETA(DisplayName = "Audio"),
	Characters          UMETA(DisplayName = "Characters"),
	CharacterBodyLayout UMETA(DisplayName = "CharacterBodyLayout"),
	Vehicles            UMETA(DisplayName = "Vehicles"),
	AI                  UMETA(DisplayName = "AI"),
	NPCs                UMETA(DisplayName = "NPCs")
};

USTRUCT(BlueprintType)
struct FZeroPayMod_Definition
{
	GENERATED_USTRUCT_BODY()

	// The UGC, this is automatically assigned by mod.io - (DO NOT CHANGE)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	FString UGCID;

	// The name of the mod, this is set ONCE during creation. Change it here will not be reflected in 
	// mod.io; instead change it in the mod.io page (and probably here)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	FString Name;

	// The description of the mod, this is set ONCE during creation. Change it here will not be reflected in 
	// mod.io; instead change it in the mod.io page (and probably here)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mods", meta = (Bitmask, BitmaskEnum = "/Script/ZeroPayModCore.EUGCTagCategory"))
	int32 ModCategoryFlags;

	// The parent persistent level (REQUIRED) - MUST reference (as sub levels) the other levels
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TSoftObjectPtr<UWorld> persistentlevel;

	// the PCVR sub-level (REQUIRED)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TSoftObjectPtr<UWorld> pcvrlevel;

	// the quest 3 level ((REQUIRED)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TSoftObjectPtr<UWorld> quest3level;

	// DO NOT USE - FUTURE
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TSoftObjectPtr<UWorld> quest4level;

	// DO NOT USE - FUTURE
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TSoftObjectPtr<UWorld> psvrlevel;
};

UCLASS(BlueprintType)
class ZEROPAYMODCORE_API UZeroPayMod_DefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	// Mod information
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	FZeroPayMod_Definition Definition;

	// Dependant UGC's
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TArray<FString> UGCDependancies;

	// Normally true, unless you downloaded a "Source" Mod which contains usable assets but not the permissions to upload changes to it
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZeroPayMod Definition")
	bool bOwner;

	UZeroPayMod_DefinitionDataAsset()
	{
		bOwner = true;
	}
};

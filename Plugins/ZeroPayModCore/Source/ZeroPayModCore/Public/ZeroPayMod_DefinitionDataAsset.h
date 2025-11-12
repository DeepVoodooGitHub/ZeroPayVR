// Ginger Ninja Gaming Ltd

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/UserDefinedStruct.h"
#include "ZeroPayMod_DefinitionDataAsset.generated.h"

class UUserDefinedStruct;

UENUM(BlueprintType)
enum class EUGCTagCategory : uint8
{
	FullMod             UMETA(DisplayName = "Full-Mod", ToolTip = "A complete mod that includes map, game-mode, etc."),
	FullModCustomGM     UMETA(DisplayName = "Full-Mod (supports GM override)", ToolTip = "Full mod, but it allows the game-mode to be overridden in UI"),
	GameMode            UMETA(DisplayName = "Game-Mode", ToolTip = "Provides a GameMode, such as TDM, Zombies, Push, etc."),
	Level               UMETA(DisplayName = "Level", ToolTip = "One or more levels/maps; all will be shown to user if your mod is selected"),
	Weapons             UMETA(DisplayName = "Weapons", ToolTip = "Adds or modifies weapons, fire modes, projectiles, etc."),
	UI                  UMETA(DisplayName = "UI", ToolTip = "Widgets, HUD, menus, or UI styling."),
	Cosmetics           UMETA(DisplayName = "Cosmetics", ToolTip = "Skins, materials, decals, and other visual-only changes."),
	Audio               UMETA(DisplayName = "Audio", ToolTip = "SFX, music, VO, and audio mixes."),
	Characters          UMETA(DisplayName = "Characters", ToolTip = "Classes that override build in pawn visual look, etc."),
	CharacterBodyLayout UMETA(DisplayName = "BodyLayout", ToolTip = "A specific player controller and pawn to be spawned (requires game-mode support)."),
	Vehicles            UMETA(DisplayName = "Vehicles", ToolTip = "Driveable vehicles, including air, land and water based."),
	AI                  UMETA(DisplayName = "AI", ToolTip = "AI controllers/behaviors, perception, etc."),
	NPCs                UMETA(DisplayName = "NPCs", ToolTip = "Non-player characters (with/without AI), requires game-mode support.")
};

UENUM(BlueprintType)
enum class EUGCSupportedGamemodes : uint8
{
	FullMod             UMETA(DisplayName = "Full-Mod"),
	AllGameModes        UMETA(DisplayName = "Any game-mode"),
	Deathmatch          UMETA(DisplayName = "Deathmatch"),
	TeamDeathmatch      UMETA(DisplayName = "Team Deathmatch"),
	GunGame             UMETA(DisplayName = "Gun Game"),
	KingOfTheHill       UMETA(DisplayName = "King of the Hill"),
	Domination          UMETA(DisplayName = "Domination"),
	Conquest            UMETA(DisplayName = "Conquest"),
	Rush                UMETA(DisplayName = "Rush"),
	Push                UMETA(DisplayName = "Push"),
	SearchAndDestroy    UMETA(DisplayName = "Search & Destroy"),
	CaptureTheFlag      UMETA(DisplayName = "Capture the Flag (CTF)"),
	PayloadEscort       UMETA(DisplayName = "Payload (Escort)"),
	PropHunt            UMETA(DisplayName = "Prop Hunt"),
	ZombieHorde         UMETA(DisplayName = "Zombie Horde"),
	Infection           UMETA(DisplayName = "Infection"),
	GroundWar           UMETA(DisplayName = "Ground War"),
	TTT                 UMETA(DisplayName = "TTT"),
	Hide                UMETA(DisplayName = "Hide"),
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

	// What is exposed in this "mod"? Can be multiple (unless "full mod")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mods", meta = (Bitmask, BitmaskEnum = "/Script/ZeroPayModCore.EUGCTagCategory"))
	int32 ModCategoryFlags;

	// What game-modes are supported by any assets (mostly for maps, rather than in-world assets)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mods", meta = (Bitmask, BitmaskEnum = "/Script/ZeroPayModCore.EUGCSupportedGamemodes"))
	int32 SupportedGamemodeFlags = (1 << static_cast<int32>(EUGCSupportedGamemodes::AllGameModes));

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

	// Points to a BP Structure that will be used for settings presented to the user when selecting this "mod" 
	// Assuming the mod is "full-mod", "game-mode", or similar. 
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition", meta = (AllowAbstract = false))
	TObjectPtr<UUserDefinedStruct> ModSettingsStruct;

	// Always cook the content in these paths, for certain items (such as game-modes) you may not have a level or something that can be
    // used to detect references that are actually used. If no references exist then the asset is never cooked and packed into your
	// UGC. A supplied path here (i.e. /Game/ZeroPayMods/UGCxxxxxx/) will ensure everything in that path is included whether referenced
	// or not. 
	//
	// *** DO NOT USE THIS WITHOUT UNDERSTANDING IT CAN BLOAT YOUR UGC - Come talk to us on Discord ***
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
	TArray<FString> AlwaysCookPaths ;

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

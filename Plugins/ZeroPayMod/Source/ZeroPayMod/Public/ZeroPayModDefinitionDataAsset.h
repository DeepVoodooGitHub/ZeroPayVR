// Deep Voodoo Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "ZeroPayModDefinitionDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FZeroPayModDefinition
{
	GENERATED_USTRUCT_BODY()

	// The UGC, this is automatically assigned by mod.io - (DO NOT CHANGE)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FString UGCID;

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
class ZEROPAYMOD_API UZeroPayModDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	// Mod information
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FZeroPayModDefinition Definition ;

	// Dependant UGC's
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TArray<FString> UGCDependancies;

	UZeroPayModDefinitionDataAsset() 
	{
	}
};

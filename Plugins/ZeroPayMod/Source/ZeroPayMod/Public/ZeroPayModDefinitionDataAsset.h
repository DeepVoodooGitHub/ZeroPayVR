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

	// The steam workshop UGC, this is automatically assigned - (DO NOT CHANGE)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FString SteamWorks_UGCID;

	// The mod.io UGC, this is automatically assigned - (DO NOT CHANGE)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FString ModIO_UGCID;

	// Levels 
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TSoftObjectPtr<UWorld> persistentlevel;

	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TSoftObjectPtr<UWorld> pcvrlevel;

	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TSoftObjectPtr<UWorld> quest3level;

	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TSoftObjectPtr<UWorld> quest4level;

	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TSoftObjectPtr<UWorld> psvrlevel;

	// Your name for this level or mod ** will appear on server browser **
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FString modName ;

	// Describe this mod
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FString modDescription ;

	// Image used on workshop (512x512)
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		UTexture2D* workshopImage ;

	// Assets 
	UPROPERTY(EditAnywhere, NotReplicated, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TArray<FString> assetPaths;

};

UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayModDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		FZeroPayModDefinition Definition ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TArray<FString> SteamWorkshopUGCDependancies ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPayMod Definition")
		TArray<FString> ModIOUGCDependancies;

	UZeroPayModDefinitionDataAsset() 
	{
	}
};

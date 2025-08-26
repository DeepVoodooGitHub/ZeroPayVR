// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZeroPayMod_DefinitionDataAsset.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "ZeroPay_UGCSupportUtils.generated.h"

UCLASS()
class ZEROPAYMOD_API AZeroPay_UGCSupportUtils : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AZeroPay_UGCSupportUtils();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Create a new definition file with some initial data
	UFUNCTION(BlueprintCallable, Category = "ZeroPay UGC")
	static void CreateAndSaveModDefinitionFile(FString UGCValue, FString Name, FString Description);

	// Opens a file picker
	UFUNCTION(BlueprintCallable, Category = "ZeroPay UGC")
	static void OpenFilePicker(FString windowTitle, FString fileTypes, TArray<FString>& OutFiles);

	// Returns all installed UGC's in the UE5 project
	UFUNCTION(BlueprintCallable, Category = "ZeroPay UGC")
	static TArray<FString> GetUGCFoldersAsNumbers();

	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static int64 MakeModIDFromInt64(int64 ModIDValue)
	{
		return ModIDValue ;
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static FString GetCookedPakFileDirectory()
	{
		FString PakFullPath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), TEXT("Workshop/"));
		return PakFullPath;	
	}	


	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static TArray<FString> BuildModCategoryArrayFromFlags(int32 ModCategoryFlags)
	{
		TArray<FString> CategoryNames;

		for (int32 i = 0; i <= static_cast<int32>(EUGCTagCategory::NPCs); ++i)
		{
			const int32 Bit = 1 << i;
			if ((ModCategoryFlags & Bit) != 0)
			{
				EUGCTagCategory Category = static_cast<EUGCTagCategory>(i);

				const UEnum* EnumPtr = StaticEnum<EUGCTagCategory>();
				if (EnumPtr)
				{
					FString DisplayName = EnumPtr->GetDisplayNameTextByValue(i).ToString();
					CategoryNames.Add(DisplayName);
				}
			}
		}

		return CategoryNames;
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|UGC|Tags") 
	static bool HasUGCTagCategoryFlag(int32 Flags, EUGCTagCategory Flag)
	{
		return (Flags & static_cast<int32>(Flag)) != 0;
	}

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|UGC|Tags") 
	static void SetUGCTagCategoryFlag(int32& Flags, EUGCTagCategory Flag, bool bEnable)
	{
		if (bEnable)
			Flags |= static_cast<int32>(Flag);
		else
			Flags &= ~static_cast<int32>(Flag);
	}

	/** Convert an integer into an EUGCTagCategory enum. If invalid, returns the first enum value (FullMod). */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|UGC|Tags")
	static EUGCTagCategory IntToUGCTagCategory(int32 Value)
	{
		// Clamp to valid range
		if (UEnum* EnumPtr = StaticEnum<EUGCTagCategory>())
		{
			int64 Max = EnumPtr->NumEnums() - 1;
			if (Value >= 0 && Value <= Max)
			{
				return static_cast<EUGCTagCategory>(Value);
			}
		}
		// fallback
		return EUGCTagCategory::FullMod;
	}

	/** Convert an integer into an EUGCSupportedGamemodes enum.	If invalid, defaults to Standard. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|UGC|Tags")
	static EUGCSupportedGamemodes IntToUGCSupportedGamemode(int32 Value)
	{
		if (UEnum* EnumPtr = StaticEnum<EUGCSupportedGamemodes>())
		{
			int64 Max = EnumPtr->NumEnums() - 1;
			if (Value >= 0 && Value <= Max)
			{
				return static_cast<EUGCSupportedGamemodes>(Value);
			}
		}
		return EUGCSupportedGamemodes::AllGameModes;
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|UGC|Tags")
	static bool HasOnlyThisGamemode(int32 Mask, EUGCSupportedGamemodes Gamemode)
	{
		// Calculate the bit for this gamemode
		const int32 Bit = 1 << static_cast<int32>(Gamemode);

		// True if mask equals exactly that bit
		return Mask == Bit;
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|UGC|Tags")
	static bool HasTagCategory(int32 ModCategoryFlags, EUGCTagCategory InCategory)
	{
		return (ModCategoryFlags & (1 << static_cast<uint8>(InCategory))) != 0;
	}
};

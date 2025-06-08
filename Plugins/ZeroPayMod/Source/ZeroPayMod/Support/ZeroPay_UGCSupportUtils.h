// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZeroPayMod_DefinitionDataAsset.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Misc/Paths.h"
#include "ModioSubsystem.h"
#include "Editor.h" 
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
	static void CreateAndSaveModDefinitionFile(FString UGCValue);

	// Opens a file picker
	UFUNCTION(BlueprintCallable, Category = "ZeroPay UGC")
	static void OpenFilePicker(FString windowTitle, FString fileTypes, TArray<FString>& OutFiles);

	// Returns all installed UGC's in the UE5 project
	UFUNCTION(BlueprintCallable, Category = "ZeroPay UGC")
	static TArray<FString> GetUGCFoldersAsNumbers();

	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static FModioModID MakeModIDFromInt64(int64 ModIDValue)
	{
		return FModioModID{ ModIDValue };
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static FString GetCookedPakFileDirectory()
	{
		FString PakFullPath = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), TEXT("Workshop/"));
		return PakFullPath;	
	}	

	UFUNCTION(BlueprintPure, Category = "ZeroPay UGC")
	static int GetModUploadProgress()
	{
		UModioSubsystem* Subsystem = GEngine->GetEngineSubsystem<UModioSubsystem>() ;
		TOptional<FModioModProgressInfo> result = Subsystem->QueryCurrentModUpdate();
		return 0 ;
	}


};

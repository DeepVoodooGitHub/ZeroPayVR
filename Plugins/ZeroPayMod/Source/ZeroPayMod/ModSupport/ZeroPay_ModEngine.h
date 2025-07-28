#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ModioSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "ZeroPay_ModEngine.generated.h"

// Delegates for Blueprint callbacks
DECLARE_DYNAMIC_DELEGATE(FOnUnzipSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnUnzipFailure, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_SubscribedMod : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 mod_id;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    FString display_name;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 refreshness ;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 popularity_today ;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 total_downloads ;
};

UCLASS(Blueprintable)
class ZEROPAYMOD_API UZeroPay_ModEngine : public UObject
{
    GENERATED_BODY()
public:

    /* >>> Download Operations <<< */

    // Async unzip function
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void UnzipFileAsync(const FString& ZipFilePath, FModioModID ModID, const FOnUnzipSuccess& OnSuccess, const FOnUnzipFailure& OnFailure);

    // Ensure's temp zip is removed
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void CleanTempStorage();

    /* >>> Upload Operations <<< */

    /* >>> Management Operations <<< */

    // Called during init to build list of available mods
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static TArray<UZeroPayMod_SubscribedMod*> InitInstalledMods(UObject* Outer);

    // Writes a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void WriteModStateFile(FModioModID ModID, int64 ModFileID, int64 DateUpdated, FString DisplayName = "");

    // Reads a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool ReadModStateFile(FModioModID ModID, int64& OutModID, int64& OutDateUpdated, FString& DisplayName) ;

};
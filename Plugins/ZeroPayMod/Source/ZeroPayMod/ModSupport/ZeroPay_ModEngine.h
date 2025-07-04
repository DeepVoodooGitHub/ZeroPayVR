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

UCLASS(Blueprintable)
class ZEROPAYMOD_API UZeroPay_ModEngine : public UObject
{
    GENERATED_BODY()

public:
    // Async unzip function
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void UnzipFileAsync(const FString& ZipFilePath, FModioModID ModID, const FOnUnzipSuccess& OnSuccess, const FOnUnzipFailure& OnFailure);

    // Writes a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void WriteModStateFile(FModioModID ModID, int64 ModFileID, int64 DateUpdated) ;

    // Reads a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool ReadModStateFile(FModioModID ModID, int64& OutModID, int64& OutDateUpdated) ;

    // Ensure's temp zip is removed
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void CleanTempStorage();

};
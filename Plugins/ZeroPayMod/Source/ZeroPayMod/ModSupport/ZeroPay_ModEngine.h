#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ModioSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "ZeroPay_ModGlobal.h"
#include "ZeroPay_HTTPEngine.h"
#include "ZeroPayMod_DefinitionDataAsset.h"
#include "ZeroPay_ModEngine.generated.h"

// Delegates for Blueprint callbacks
DECLARE_DYNAMIC_DELEGATE(FOnUnzipSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnUnzipFailure, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubscribedModUpdated, UZeroPayMod_SubscribedMod*, Mod);

UENUM(BlueprintType)
enum FZeroPayMod_SubscribedModState
{
    Unknown,
    Valid,          /* Pak is up-to-date and correct size */
    Invalid,        /* Mod.io failed to update us */
    Updating,       /* Attempt to update mod */
    Unpacking       /* Unzip in operation */
};

UCLASS(BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_SubscribedMod : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 mod_id;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 file_id;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    FString display_name;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    FString summary;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    FString author;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 date_updated;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 popularity_today ;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 total_downloads ;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 ratings_weighted_aggregate ;

    UPROPERTY(BlueprintReadWrite, Category = "ZeroPay Modio Support")
    TEnumAsByte<FZeroPayMod_SubscribedModState> ModState = FZeroPayMod_SubscribedModState::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    EUGCTagCategory UGCCategory ;

    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    int64 uncompressed_size ;

    // Used by BP's to retry operations (such as taking to mod.io)
    UPROPERTY(BlueprintReadWrite, Category = "ZeroPay Modio Support")
    int64 retry_count ;

    // Used by BP's to update progress (such as downloading from mod.io, UI only)
    UPROPERTY(BlueprintReadWrite, Category = "ZeroPay Modio Support")
    float progress ;

    // Internal storage for logo URL (may be out of date, unless just read via GetModInfoAsync)
    UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Modio Support")
    FString logourl;

    // Event for changes to the subscribed item that require re-visualisation / procesrsing
    UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "ZeroPay|Events")
    FOnSubscribedModUpdated OnInternalsUpdated;

    UFUNCTION(BlueprintPure, Category = "ZeroPay Modio Support")
    bool IsOutOfDate(int64 current_date_updated)
    {
        return (current_date_updated > date_updated) ;
    }

    UFUNCTION(BlueprintPure, Category = "ZeroPay Modio Support")
    bool IsWrongFileSize(int64 current_uncompressed_size)
    {
        return (current_uncompressed_size != uncompressed_size);
    }

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

    // Delets a mod from disk
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool RemoveInstalledMod(UZeroPayMod_SubscribedMod* Mod);

    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static UZeroPayMod_SubscribedMod* CreateSubscribedMod(UZeroPayMod_GetModInfoResult* modinfoResult);

    // Returns the file size of any given mod (for a platform) on the file system
    UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Engine")
    static int64 GetPakFileSize(FModioModID ModID, FModioPlatform Platform);

    // Writes a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static void WriteModStateFile(FModioModID ModID, FString DisplayName, int64 ModFileID, int64 DateUpdated, int64 UncompressedSize, FString Summary, FString Author, int64 Ratings, int64 Category);

    // Reads a mod state file (so we can check for updates)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool ReadModStateFile(FModioModID ModID, FString& DisplayName, int64& OutModID, int64& OutDateUpdated, int64& OutUncompressedSize, FString& OutSummary, FString& OutAuthor, int64& OutRatings, int64& OutCategory) ;

    // Changes certain state file information (when updates occur)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool UpdateModStateFile(FModioModID ModID, int64 NewDateUpdated, int64 NewUncompressedSize) ;

    // Writes a mod state file using the mod info, usually used for subscribed modes (creates directory too)
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Engine")
    static bool WriteModStateFileViaModInfo(FModioModID ModID, UZeroPayMod_GetModInfoResult* ModInfo) ;

    /* >>> <Misc> Operations <<< */

    // Return the platform we are running on
    UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Engine")
    static FModioPlatform GetPlatform()
    {
        #if PLATFORM_WINDOWS
            return FModioPlatform::ModIOPlatform_Windows;
        #endif

        #if PLATFORM_LINUX
            return FModioPlatform::ModIOPlatform_LinuxServer;
        #endif

        #if PLATFORM_ANDROID
            return FModioPlatform::ModIOPlatform_Android ;
        #endif
    }

    UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Engine")
    static FString GetModLogoPath(FModioModID ModID)
    {
        FString BasePath = FPaths::ProjectSavedDir();
        FString ModFolderName = FString::Printf(TEXT("Mods/%s/Logo.png"), *ModID.ToString());
        return FPaths::Combine(BasePath, ModFolderName);
    }


};
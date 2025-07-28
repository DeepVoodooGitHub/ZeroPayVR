#include "ZeroPay_ModEngine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Paths.h"

TArray<UZeroPayMod_SubscribedMod*> UZeroPay_ModEngine::InitInstalledMods(UObject* Outer)
{
    TArray<UZeroPayMod_SubscribedMod*> InstalledMods;

    const FString ModsBasePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Mods"));
    IFileManager& FileManager = IFileManager::Get();

    // Check if the Mods directory exists
    if (!FileManager.DirectoryExists(*ModsBasePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Mods directory does not exist: %s"), *ModsBasePath);
        return InstalledMods;
    }

    // Get all subdirectories
    TArray<FString> DirectoryNames;
    FileManager.FindFiles(DirectoryNames, *(ModsBasePath / TEXT("*")), false, true);

    for (const FString& DirName : DirectoryNames)
    {
        FString FullPath = ModsBasePath / DirName;

        // Assume DirName is numeric (ModID)
        FModioModID ModID = FModioModID(FCString::Atoi64(*DirName)); 

        int64 OutModID = 0;
        int64 OutDateUpdated = 0;
        FString DisplayName;

        if (ReadModStateFile(ModID, OutModID, OutDateUpdated, DisplayName))
        {
            UZeroPayMod_SubscribedMod* NewMod = NewObject<UZeroPayMod_SubscribedMod>(Outer);
            NewMod->mod_id = OutModID;
            NewMod->display_name = DisplayName;
            NewMod->refreshness = OutDateUpdated;

            // Optionally fill in popularity_today or total_downloads later
            NewMod->popularity_today = 0;
            NewMod->total_downloads = 0;

            InstalledMods.Add(NewMod);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to read mod state file for ModID %s"), *DirName);
        }
    }

    return InstalledMods;
}


void UZeroPay_ModEngine::WriteModStateFile(FModioModID ModID, int64 ModFileID, int64 DateUpdated, FString DisplayName)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%s"), *ModID.ToString());
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Construct the full path to the state.json file
    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Create a JSON object
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("file_id"), FString::Printf(TEXT("%lld"), ModFileID));
    JsonObject->SetStringField(TEXT("date_updated"), FString::Printf(TEXT("%lld"), DateUpdated));
    JsonObject->SetStringField(TEXT("display_name"), DisplayName);

    // Write JSON to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        // Save the string to file
        if (FFileHelper::SaveStringToFile(OutputString, *StateFilePath))
        {
            UE_LOG(LogTemp, Display, TEXT("Successfully wrote state.json to: %s"), *StateFilePath);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to write state.json to: %s"), *StateFilePath);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON for state.json"));
    }
}



bool UZeroPay_ModEngine::ReadModStateFile(FModioModID ModID, int64& OutModID, int64& OutDateUpdated, FString& DisplayName)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%s"), *ModID.ToString());
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Construct the full path to state.json
    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Check if the file exists
    if (!FPaths::FileExists(StateFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("state.json does not exist at: %s"), *StateFilePath);
        return false;
    }

    // Read the JSON file into a string
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *StateFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load state.json from: %s"), *StateFilePath);
        return false;
    }

    // Parse the JSON string
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse state.json JSON at: %s"), *StateFilePath);
        return false;
    }

    // Extract fields
    FString ModIDStr, DateUpdatedStr, DisplayNameStr ;
    if (!JsonObject->TryGetStringField(TEXT("file_id"), ModIDStr) ||
        !JsonObject->TryGetStringField(TEXT("date_updated"), DateUpdatedStr))
    {
        UE_LOG(LogTemp, Error, TEXT("state.json is missing required fields at: %s"), *StateFilePath);
        return false;
    }

    // Convert strings to int64
    OutModID = FCString::Strtoui64(*ModIDStr, nullptr, 10);
    OutDateUpdated = FCString::Strtoui64(*DateUpdatedStr, nullptr, 10);

    // Display name is optional
    if (JsonObject->TryGetStringField(TEXT("display_name"), DisplayNameStr))
        DisplayName = DisplayNameStr;

    UE_LOG(LogTemp, Display, TEXT("Read state.json: ModID=%lld, DateUpdated=%lld"), OutModID, OutDateUpdated);
    return true;
}

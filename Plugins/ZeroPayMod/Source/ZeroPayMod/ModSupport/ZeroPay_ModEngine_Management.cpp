#include "ZeroPay_ModEngine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "UObject/UObjectGlobals.h"
#include "Debug/ZeroPay_InternalDebug.h"

TArray<UZeroPayMod_SubscribedMod*> UZeroPay_ModEngine::InitInstalledMods(UObject* Outer)
{
    TArray<UZeroPayMod_SubscribedMod*> InstalledMods;

    const FString ModsBasePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Mods"));
    IFileManager& FileManager = IFileManager::Get();

    // Check if the Mods directory exists
    if (!FileManager.DirectoryExists(*ModsBasePath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Mods directory does not exist: %s"), *ModsBasePath), FDebugConsoleLevel::Error);

        UE_LOG(LogTemp, Warning, TEXT("Mods directory does not exist: %s"), *ModsBasePath);
        return InstalledMods;
    }

    // Get all subdirectories
    TArray<FString> DirectoryNames;
    FileManager.FindFiles(DirectoryNames, *(ModsBasePath / TEXT("*")), false, true);

    // Log
    UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Found %d mod directories"), DirectoryNames.Num()), FDebugConsoleLevel::Log);

    // Iterate mods
    for (const FString& DirName : DirectoryNames)
    {
        FString FullPath = ModsBasePath / DirName;

        // Assume DirName is numeric (ModID)
        int64 ModID = uint64(FCString::Atoi64(*DirName)); 

        int64 OutModFileID = 0;
        int64 OutDateUpdated = 0;
        int64 OutUncompressedSize = 0;
        FString OutDisplayName;
        FString OutSummary;
        FString OutAuthor;
        int64 OutRatings = 0;
        int64 OutCategory = 0 ;
        TArray<FString> OutMetadataValues;

        if (ReadModStateFile(ModID, OutDisplayName, OutModFileID, OutDateUpdated, OutUncompressedSize, OutSummary, OutAuthor, OutRatings, OutCategory, OutMetadataValues))
        {
            UZeroPayMod_SubscribedMod* NewMod = NewObject<UZeroPayMod_SubscribedMod>(Outer);
            NewMod->mod_id = FCString::Atoi64(*DirName);
            NewMod->file_id = OutModFileID ;
            NewMod->display_name = OutDisplayName;
            NewMod->ratings_weighted_aggregate = OutRatings ;
            NewMod->summary = OutSummary;
            NewMod->author = OutAuthor;
            NewMod->date_updated = OutDateUpdated;
            NewMod->uncompressed_size = OutUncompressedSize ;
            NewMod->UGCCategory = (EUGCTagCategory) OutCategory ;
            NewMod->metadata_values = OutMetadataValues ;

            // Optionally fill in popularity_today or total_downloads later
            NewMod->popularity_today = 0;
            NewMod->total_downloads = 0;

            InstalledMods.Add(NewMod);
        }
    }

    return InstalledMods;
}

bool UZeroPay_ModEngine::RemoveInstalledMod(UZeroPayMod_SubscribedMod* Mod)
{
    if (!Mod)
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("RemoveInstalledMod() failed, passed invalid mod")), FDebugConsoleLevel::Log);
        return false;
    }

    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), Mod->mod_id);
    FString ModDirectoryPath = FPaths::Combine(BasePath, ModFolderName);

    // Check if the directory exists
    if (!FPaths::DirectoryExists(ModDirectoryPath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("RemoveInstalledMod() failed, invalid folder (%s)"), *ModDirectoryPath), FDebugConsoleLevel::Log);
        return false;
    }

    // Delete the directory and all its contents
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    bool bSuccess = PlatformFile.DeleteDirectoryRecursively(*ModDirectoryPath);

    if (!bSuccess)
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("RemoveInstalledMod() failed, could not remove folder (%s)"), *ModDirectoryPath), FDebugConsoleLevel::Log);

    return bSuccess;
}

UZeroPayMod_SubscribedMod* UZeroPay_ModEngine::CreateSubscribedMod(UZeroPayMod_GetModInfoResult* modinfoResult)
{
    if (!modinfoResult)
    {
        return nullptr;
    }

    UZeroPayMod_SubscribedMod* NewMod = NewObject<UZeroPayMod_SubscribedMod>();

    // Zero any run-time mod file related info
    NewMod->mod_id = modinfoResult->mod_id;
    NewMod->file_id = 0; 
    NewMod->display_name = modinfoResult->display_name;
    NewMod->summary = modinfoResult->summary;
    NewMod->author = modinfoResult->author;
    NewMod->date_updated = 0; 
    NewMod->popularity_today = 0; 
    NewMod->total_downloads = 0; 
    NewMod->ratings_weighted_aggregate = modinfoResult->ratings;
    NewMod->ModState = FZeroPayMod_SubscribedModState::Unknown;
    NewMod->UGCCategory = EUGCTagCategory::FullMod; // fallback default
    NewMod->uncompressed_size = modinfoResult->download_size;
    NewMod->retry_count = 0;
    NewMod->progress = 0.0f;
    NewMod->logourl = modinfoResult->logourl ;
    NewMod->metadata_values = modinfoResult->metadata_values;

    return NewMod;
}

UZeroPayMod_SubscribedMod* UZeroPay_ModEngine::CreateServerSubscribedMod(int64 ModID, EUGCTagCategory Category)
{

    UZeroPayMod_SubscribedMod* NewMod = NewObject<UZeroPayMod_SubscribedMod>();

    // Create a basic structure with ID and category set
    NewMod->mod_id = ModID ;
    NewMod->file_id = 0;
    NewMod->display_name = "Server mod";
    NewMod->summary = "";
    NewMod->author = "" ;
    NewMod->date_updated = 0;
    NewMod->popularity_today = 0;
    NewMod->total_downloads = 0;
    NewMod->ratings_weighted_aggregate = 0 ;
    NewMod->ModState = FZeroPayMod_SubscribedModState::Valid;
    NewMod->UGCCategory = Category;
    NewMod->uncompressed_size = 0 ;
    NewMod->retry_count = 0;
    NewMod->progress = 0.0f;
    NewMod->logourl = 0;
    NewMod->metadata_values.Empty();

    return NewMod;
}

int64 UZeroPay_ModEngine::GetPakFileSize(int64 ModID, FModioPlatform Platform)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);
    FString FilePath ;

    switch (Platform)
    {
    case FModioPlatform::ModIOPlatform_Windows: FilePath = FPaths::Combine(DestinationPath, "Windows.pak"); break;
    case FModioPlatform::ModIOPlatform_Android: FilePath = FPaths::Combine(DestinationPath, "Android.pak"); break;
    case FModioPlatform::ModIOPlatform_LinuxServer: FilePath = FPaths::Combine(DestinationPath, "LinuxServer.pak"); break;
    default: break;
    }

    return IFileManager::Get().FileSize(*FilePath);
}


void UZeroPay_ModEngine::WriteModStateFile(int64 ModID, FString DisplayName, int64 ModFileID, int64 DateUpdated, int64 UncompressedSize, FString Summary, FString Author, int64 Ratings, int64 Category)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Construct the full path to the state.json file
    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Create a JSON object
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("file_id"), FString::Printf(TEXT("%lld"), ModFileID));
    JsonObject->SetStringField(TEXT("date_updated"), FString::Printf(TEXT("%lld"), DateUpdated));
    JsonObject->SetStringField(TEXT("display_name"), DisplayName);
    JsonObject->SetStringField(TEXT("summary"), Summary);
    JsonObject->SetStringField(TEXT("author"), Author);
    JsonObject->SetStringField(TEXT("uncompressed_size"), FString::Printf(TEXT("%lld"), UncompressedSize));
    JsonObject->SetStringField(TEXT("ratings_weighted_aggregate"), FString::Printf(TEXT("%lld"), Ratings));
    JsonObject->SetStringField(TEXT("category"), FString::Printf(TEXT("%lld"), Category));
    
    // Write JSON to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        // Save the string to file
        if (FFileHelper::SaveStringToFile(OutputString, *StateFilePath))
        {
            UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Successfully wrote state.json to: %s"), *StateFilePath), FDebugConsoleLevel::Log);                              
        }
        else
        {
            UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to write state.json to: %s"), *StateFilePath), FDebugConsoleLevel::Error);
        }
    }
    else
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to write state.json to: %s"), *StateFilePath), FDebugConsoleLevel::Error);
    }
}

bool UZeroPay_ModEngine::ReadModStateFile(int64 ModID, FString& OutDisplayName, int64& OutFileID, int64& OutDateUpdated, int64& OutUncompressedSize, FString& OutSummary, FString& OutAuthor,int64& OutRatings, int64& OutCategory, TArray<FString>& OutMetadataValues) 
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Construct the full path to state.json
    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Check if the file exists
    if (!FPaths::FileExists(StateFilePath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("File not found %s"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    // Read the JSON file into a string
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *StateFilePath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to load state.json (%s)"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    // Parse the JSON string
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to parse state.json (%s)"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    // Extract fields
    FString FileIDStr, DateUpdatedStr, DisplayNameStr, UncompressedSizeStr, SummaryStr, AuthorStr, RatingsStr, CategoryStr;
    if (!JsonObject->TryGetStringField(TEXT("file_id"), FileIDStr) ||
        !JsonObject->TryGetStringField(TEXT("date_updated"), DateUpdatedStr) ||
        !JsonObject->TryGetStringField(TEXT("uncompressed_size"), UncompressedSizeStr) ||
        !JsonObject->TryGetStringField(TEXT("display_name"), DisplayNameStr) ||
        !JsonObject->TryGetStringField(TEXT("summary"), SummaryStr) ||
        !JsonObject->TryGetStringField(TEXT("author"), AuthorStr) ||
        !JsonObject->TryGetStringField(TEXT("ratings_weighted_aggregate"), RatingsStr) ||
        !JsonObject->TryGetStringField(TEXT("category"), CategoryStr))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Could not parse state.json (missing fields) (%s)"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    // Convert strings to int64
    OutFileID = FCString::Strtoui64(*FileIDStr, nullptr, 10);
    OutDateUpdated = FCString::Strtoui64(*DateUpdatedStr, nullptr, 10);
    OutUncompressedSize = FCString::Strtoui64(*UncompressedSizeStr, nullptr, 10);
    OutRatings = FCString::Strtoui64(*RatingsStr, nullptr, 10);
    OutCategory = FCString::Strtoui64(*CategoryStr, nullptr, 10);

    // String fields
    OutDisplayName = DisplayNameStr;
    OutSummary = SummaryStr;
    OutAuthor = AuthorStr;

    // NEW: metadata_values[]
    OutMetadataValues.Empty();
    const TArray<TSharedPtr<FJsonValue>>* MetaArrayPtr = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("metadata_values"), MetaArrayPtr) && MetaArrayPtr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *MetaArrayPtr)
        {
            if (Val.IsValid() && Val->Type == EJson::String)
            {
                OutMetadataValues.Add(Val->AsString());
            }
        }
    }
    // If absent, OutMetadataValues just stays empty.

    return true;
}

bool UZeroPay_ModEngine::UpdateModStateFile(int64 ModID, int64 NewDateUpdated, int64 NewUncompressedSize)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    if (!FPaths::FileExists(StateFilePath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("state.json not found for update: %s"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *StateFilePath))
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to load state.json for update: %s"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to parse state.json for update: %s"), *StateFilePath), FDebugConsoleLevel::Error);
        return false;
    }

    // Overwrite only the specified fields
    JsonObject->SetStringField(TEXT("date_updated"), FString::Printf(TEXT("%lld"), NewDateUpdated));
    JsonObject->SetStringField(TEXT("uncompressed_size"), FString::Printf(TEXT("%lld"), NewUncompressedSize));

    // Write back to JSON file
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        if (FFileHelper::SaveStringToFile(OutputString, *StateFilePath))
        {
            UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Successfully updated state.json: %s"), *StateFilePath), FDebugConsoleLevel::Log);
            return true;
        }
    }

    UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, FString::Printf(TEXT("Failed to update state.json: %s"), *StateFilePath), FDebugConsoleLevel::Error);
    return false;
}

bool UZeroPay_ModEngine::WriteModStateFileViaModInfo(int64 ModID, UZeroPayMod_GetModInfoResult* ModInfo)
{
    if (!ModInfo)
    {
        UE_LOG(LogTemp, Error, TEXT("WriteModStateFile: ModInfo is null"));
        return false;
    }

    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Ensure directory exists
    if (!IFileManager::Get().DirectoryExists(*DestinationPath))
    {
        if (!IFileManager::Get().MakeDirectory(*DestinationPath, true))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *DestinationPath);
            return false;
        }
    }

    // Construct the full path to the state.json file
    FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Create a JSON object
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("file_id"), TEXT("0")); // Not available in ModInfo
    JsonObject->SetStringField(TEXT("date_updated"), TEXT("0")); // Not available in ModInfo
    JsonObject->SetStringField(TEXT("display_name"), ModInfo->display_name);
    JsonObject->SetStringField(TEXT("summary"), ModInfo->summary);
    JsonObject->SetStringField(TEXT("author"), ModInfo->author);
    JsonObject->SetStringField(TEXT("uncompressed_size"), FString::Printf(TEXT("%lld"), ModInfo->download_size));
    JsonObject->SetStringField(TEXT("ratings_weighted_aggregate"), FString::Printf(TEXT("%lld"), ModInfo->ratings));
    JsonObject->SetStringField(TEXT("category"), TEXT("0")); // Not available in ModInfo

    // --- NEW: Write metadata_values as string array ---
    if (ModInfo->metadata_values.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> MetaArray;
        for (const FString& Value : ModInfo->metadata_values)
        {
            MetaArray.Add(MakeShared<FJsonValueString>(Value));
        }
        JsonObject->SetArrayField(TEXT("metadata_values"), MetaArray);
    }
    else
    {
        // Include empty array so file structure stays consistent
        JsonObject->SetArrayField(TEXT("metadata_values"), TArray<TSharedPtr<FJsonValue>>{});
    }

    // Write JSON to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        // Save the string to file
        if (FFileHelper::SaveStringToFile(OutputString, *StateFilePath))
        {
            UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(
                nullptr, nullptr,
                FString::Printf(TEXT("Successfully wrote state.json to: %s"), *StateFilePath),
                FDebugConsoleLevel::Log);
            return true;
        }
        else
        {
            UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(
                nullptr, nullptr,
                FString::Printf(TEXT("Failed to write state.json to: %s"), *StateFilePath),
                FDebugConsoleLevel::Error);
        }
    }
    else
    {
        UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(
            nullptr, nullptr,
            FString::Printf(TEXT("Failed to serialize JSON for: %s"), *StateFilePath),
            FDebugConsoleLevel::Error);
    }

    return false;
}

bool UZeroPay_ModEngine::UpdateModStateFileViaModInfo(int64 ModID, UZeroPayMod_GetModInfoResult* ModInfo)
{
    if (!ModInfo)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: ModInfo is null"));
        return false;
    }

    const FString BasePath = FPaths::ProjectSavedDir();
    const FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    const FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);
    const FString StateFilePath = FPaths::Combine(DestinationPath, TEXT("state.json"));

    // Do NOT create directories or files – must already exist
    if (!FPaths::FileExists(StateFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: state.json not found: %s"), *StateFilePath);
        return false;
    }

    // Load existing JSON
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *StateFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: failed to read %s"), *StateFilePath);
        return false;
    }

    // Parse
    TSharedPtr<FJsonObject> JsonObject;
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: bad JSON in %s"), *StateFilePath);
            return false;
        }
    }

    // ---- Update only specified fields ----

    // date_updated -> use current UTC unix timestamp
    const int64 NowUnix = FDateTime::UtcNow().ToUnixTimestamp();
    JsonObject->SetStringField(TEXT("date_updated"), FString::Printf(TEXT("%lld"), NowUnix));

    // display_name / summary / author
    JsonObject->SetStringField(TEXT("display_name"), ModInfo->display_name);
    JsonObject->SetStringField(TEXT("summary"), ModInfo->summary);
    JsonObject->SetStringField(TEXT("author"), ModInfo->author);

    // ratings_weighted_aggregate (kept as string to match existing schema)
    JsonObject->SetStringField(TEXT("ratings_weighted_aggregate"), FString::Printf(TEXT("%lld"), ModInfo->ratings));

    // metadata_values (string array)
    {
        TArray<TSharedPtr<FJsonValue>> MetaArray;
        for (const FString& Value : ModInfo->metadata_values)
        {
            MetaArray.Add(MakeShared<FJsonValueString>(Value));
        }
        JsonObject->SetArrayField(TEXT("metadata_values"), MetaArray);
    }
    // ---- end selective updates ----

    // Serialize back
    FString OutString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: failed to serialize JSON for %s"), *StateFilePath);
        return false;
    }

    // Save in place
    if (!FFileHelper::SaveStringToFile(OutString, *StateFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateModStateFileViaModInfo: failed to write %s"), *StateFilePath);
        return false;
    }

    UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(
        nullptr, nullptr,
        FString::Printf(TEXT("Updated state.json: %s"), *StateFilePath),
        FDebugConsoleLevel::Log);

    return true;
}

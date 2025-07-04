#include "ZeroPay_ModEngine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"

void UZeroPay_ModEngine::UnzipFileAsync(const FString& ZipFilePath, FModioModID ModID, const FOnUnzipSuccess& OnSuccess, const FOnUnzipFailure& OnFailure)
{
#if PLATFORM_WINDOWS
    OnFailure.ExecuteIfBound(TEXT("Not supported under Windows"));
#endif
    if (!FPaths::FileExists(ZipFilePath))
    {
        FString Error = FString::Printf(TEXT("ZIP file does not exist: %s"), *ZipFilePath);
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        OnFailure.ExecuteIfBound(Error);
        return;
    }

    // Get Saved directory and append "Mods/<ModID>/"
    FString RelativeSavedPath = FPaths::ProjectSavedDir();
    FString BasePath = FPaths::ConvertRelativePathToFull(RelativeSavedPath);
    FString ModFolderName = FString::Printf(TEXT("Mods/%s"), *ModID.ToString());
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    // Remove old folder.. 
    if (IFileManager::Get().DirectoryExists(*DestinationPath))
    {
        bool bDeleted = IFileManager::Get().DeleteDirectory(*DestinationPath, false, true);
        if (!bDeleted)
        {
            FString Error = FString::Printf(TEXT("Could not replace existing mod directory (locked?) : %s"), *DestinationPath);
            UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
            OnFailure.ExecuteIfBound(Error);
            return;
        }
    }

    IFileManager::Get().MakeDirectory(*DestinationPath, /*Tree=*/true);
   
    // Build unzip command
// Linux only
#if PLATFORM_LINUX
    // Build unzip command
    FString UnzipBinary = TEXT("unzip");
    FString Arguments = FString::Printf(TEXT("\"%s\" -d \"%s\""), *ZipFilePath, *DestinationPath);
#else
    FString UnzipBinary = TEXT("cmd");
    FString Arguments = FString::Printf(TEXT("/c unzip \"%s\" -d \"%s\""), *ZipFilePath, *DestinationPath);
#endif

    UE_LOG(LogTemp, Log, TEXT("Running unzip: %s %s"), *UnzipBinary, *Arguments);

    // Run on a background thread
    Async(EAsyncExecution::ThreadPool, [ModID, UnzipBinary, Arguments, OnSuccess, OnFailure]()
        {
            int32 ReturnCode = -1;
            FString StdOut, StdErr;

            bool bSuccess = FPlatformProcess::ExecProcess(*UnzipBinary, *Arguments, &ReturnCode, &StdOut, &StdErr);

            if (!bSuccess || ReturnCode != 0)
            {
                AsyncTask(ENamedThreads::GameThread, [OnFailure, ReturnCode, StdErr]()
                    {
                        FString ErrorMsg = FString::Printf(TEXT("Unzip failed (%d): %s"), ReturnCode, *StdErr);
                        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
                        OnFailure.ExecuteIfBound(ErrorMsg);
                    });
            }

            // Back to game thread for Blueprint callbacks
            AsyncTask(ENamedThreads::GameThread, [ModID, ReturnCode, StdOut, StdErr, OnSuccess, OnFailure]()
                {
                    if (ReturnCode == 0)
                    {
                        UE_LOG(LogTemp, Log, TEXT("Unzip succeeded: %s"), *StdOut);
                        OnSuccess.ExecuteIfBound();
                    }
                    else
                    {
                        /* If failed, destroy destination */
                        FString BasePath = FPaths::ProjectSavedDir();
                        FString ModFolderName = FString::Printf(TEXT("Mods/%s"), *ModID.ToString());
                        FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

                        IFileManager::Get().DeleteDirectory(*DestinationPath, false, false);

                        FString ErrorMsg = FString::Printf(TEXT("Unzip failed (%d): %s"), ReturnCode, *StdErr);
                        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
                        OnFailure.ExecuteIfBound(ErrorMsg);
                    }
                });
        });

}


void UZeroPay_ModEngine::WriteModStateFile(FModioModID ModID, int64 ModFileID, int64 DateUpdated)
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



bool UZeroPay_ModEngine::ReadModStateFile(FModioModID ModID, int64& OutModID, int64& OutDateUpdated)
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
    FString ModIDStr, DateUpdatedStr;
    if (!JsonObject->TryGetStringField(TEXT("file_id"), ModIDStr) ||
        !JsonObject->TryGetStringField(TEXT("date_updated"), DateUpdatedStr))
    {
        UE_LOG(LogTemp, Error, TEXT("state.json is missing required fields at: %s"), *StateFilePath);
        return false;
    }

    // Convert strings to int64
    OutModID = FCString::Strtoui64(*ModIDStr, nullptr, 10);
    OutDateUpdated = FCString::Strtoui64(*DateUpdatedStr, nullptr, 10);

    UE_LOG(LogTemp, Display, TEXT("Read state.json: ModID=%lld, DateUpdated=%lld"), OutModID, OutDateUpdated);
    return true;
}


void UZeroPay_ModEngine::CleanTempStorage()
{
    /* Clean any old zip away */
    FString TempZipFilePath = FPaths::ProjectSavedDir() / TEXT("DownloadedFile.zip");
    IFileManager::Get().Delete(*TempZipFilePath, false, true, true);
}
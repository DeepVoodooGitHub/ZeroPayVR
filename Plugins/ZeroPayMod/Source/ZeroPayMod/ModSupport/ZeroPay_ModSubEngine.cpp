// (c) Ginger Ninja Games Ltd


#include "ZeroPay_ModSubEngine.h"


ZeroPay_ModSubEngine::ZeroPay_ModSubEngine()
{
}

ZeroPay_ModSubEngine::~ZeroPay_ModSubEngine()
{
}

TArray<FString> ZeroPay_ModSubEngine::GetInstalledMods()
{
    TArray<FString> Result;

    // Get the Saved directory (absolute path)
    FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());

    // Array to hold all subdirectories
    TArray<FString> SubDirectories;

    // Find all directories inside SavedDir (recursive = false here)
    IFileManager::Get().FindFiles(SubDirectories, *(SavedDir / TEXT("*")), false, true);

    for (const FString& SubDirName : SubDirectories)
    {
        // Build full path to this subdirectory
        FString SubDirFullPath = SavedDir / SubDirName;

        // Build full path to "state.json" in this directory
        FString StateJsonPath = SubDirFullPath / TEXT("state.json");

        // Check if the file exists
        if (FPaths::FileExists(StateJsonPath))
        {
            // Add this directory to the result
            Result.Add(SubDirFullPath);
        }
    }

    return Result;
}
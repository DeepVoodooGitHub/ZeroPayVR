#include "ZeroPay_ModEngine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "miniz_cpp.hpp"

#if PLATFORM_LINUX
/* Linux implementation*/
void UZeroPay_ModEngine::UnzipFileAsync(const FString& ZipFilePath, int64 ModID, const FOnUnzipSuccess& OnSuccess, const FOnUnzipFailure& OnFailure)
{
    OnFailure.ExecuteIfBound(TEXT("Not supported under Windows"));
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
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
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
    FString UnzipBinary = TEXT("unzip");
    FString Arguments = FString::Printf(TEXT("\"%s\" -d \"%s\""), *ZipFilePath, *DestinationPath);

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
                        FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
                        FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

                        IFileManager::Get().DeleteDirectory(*DestinationPath, false, false);

                        FString ErrorMsg = FString::Printf(TEXT("Unzip failed (%d): %s"), ReturnCode, *StdErr);
                        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
                        OnFailure.ExecuteIfBound(ErrorMsg);
                    }
                });
        });

}

void UZeroPay_ModEngine::MakePlatformPakZip(FModioPlatform Platform, FOnZipComplete OnComplete)
{
    /* No supported or needed in Linux */
}

#else

/* Windows/Android */
void UZeroPay_ModEngine::UnzipFileAsync(const FString& ZipFilePath, int64 ModID, const FOnUnzipSuccess& OnSuccess, const FOnUnzipFailure& OnFailure)
{
    // 1) Get saved dir (usually relative on Android)
    FString SavedDir = FPaths::ProjectSavedDir();

    // 2) Resolve it to absolute ONCE
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString AbsSavedDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*SavedDir);

    // 3) Build your two absolute paths from that absolute dir
    const FString AbsZipPath = FPaths::Combine(AbsSavedDir, ZipFilePath);   // e.g. "DownloadedFile.zip"
    FString ModFolderName = FString::Printf(TEXT("Mods/%lld"), ModID);
    const FString AbsDestinationPath = FPaths::Combine(AbsSavedDir, ModFolderName);

    // 4) Sanity check
    if (!FPaths::FileExists(AbsZipPath))
    {
        UE_LOG(LogTemp, Error, TEXT("ZIP file does not exist: %s"), *AbsZipPath);
        OnFailure.ExecuteIfBound(FString::Printf(TEXT("ZIP file does not exist: %s"), *AbsZipPath));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("   ZipPath: %s"), *AbsZipPath);
    UE_LOG(LogTemp, Log, TEXT("   Unzip Path: %s"), *AbsDestinationPath);

    // Remove old folder
    if (IFileManager::Get().DirectoryExists(*AbsDestinationPath))
    {
        TArray<FString> PakFiles;
        IFileManager::Get().FindFiles(PakFiles, *(AbsDestinationPath / TEXT("*.pak")), true, false);

        bool bAllDeleted = true;
        for (const FString& PakFile : PakFiles)
        {
            FString FullPakPath = AbsDestinationPath / PakFile;
            if (!IFileManager::Get().Delete(*FullPakPath, false, true))
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to delete PAK file: %s"), *FullPakPath);
                bAllDeleted = false;
            }
        }
    }
    else
    {
        /* Make folder is not existing.. */
        IFileManager::Get().MakeDirectory(*AbsDestinationPath, true);
    }

    // Copy values to local copies for thread use
    FString LocalZipFilePath = AbsZipPath;
    FString LocalDestinationPath = AbsDestinationPath;

    Async(EAsyncExecution::ThreadPool, [LocalZipFilePath, LocalDestinationPath, OnSuccess, OnFailure, ModID]()
        {
            bool bFailed = false;

            std::string ZipPathStr = TCHAR_TO_UTF8(*LocalZipFilePath);
            std::string DestFolderStr = TCHAR_TO_UTF8(*LocalDestinationPath);

            miniz_cpp::zip_file zip(ZipPathStr);
            zip.extractall(DestFolderStr);

            // Back to game thread
            AsyncTask(ENamedThreads::GameThread, [OnSuccess]()
                {
                    UE_LOG(LogTemp, Log, TEXT("Unzip succeeded."));
                    OnSuccess.ExecuteIfBound();
                });
        });
}

void UZeroPay_ModEngine::MakePlatformPakZip(FModioPlatform Platform, FOnZipComplete OnComplete)
{
    // Run async on thread pool
    Async(EAsyncExecution::ThreadPool, [Platform, OnComplete]()
        {
            FString PlatformStr;
            switch (Platform)
            {
            case FModioPlatform::ModIOPlatform_Windows:     PlatformStr = TEXT("Windows"); break;
            case FModioPlatform::ModIOPlatform_Android:     PlatformStr = TEXT("Android"); break;
            case FModioPlatform::ModIOPlatform_LinuxServer: PlatformStr = TEXT("LinuxServer"); break;
            default:
                AsyncTask(ENamedThreads::GameThread, [OnComplete]() {
                    OnComplete.ExecuteIfBound(false, TEXT("Unknown platform."));
                    });
                return;
            }

            FString BaseDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Workshop"), PlatformStr);
            FString PakPath = FPaths::Combine(BaseDir, PlatformStr + TEXT(".pak"));
            FString ZipPath = FPaths::Combine(BaseDir, PlatformStr + TEXT(".zip"));

            IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
            if (!PF.FileExists(*PakPath))
            {
                AsyncTask(ENamedThreads::GameThread, [OnComplete, PakPath]() {
                    OnComplete.ExecuteIfBound(false, FString::Printf(TEXT("PAK file not found: %s"), *PakPath));
                    });
                return;
            }

            miniz_cpp::zip_file Zip;
            Zip.write(TCHAR_TO_UTF8(*PakPath), TCHAR_TO_UTF8(*(PlatformStr + TEXT(".pak"))));
            Zip.save(TCHAR_TO_UTF8(*ZipPath));

            AsyncTask(ENamedThreads::GameThread, [OnComplete, ZipPath]() {
                OnComplete.ExecuteIfBound(true, ZipPath);
                });
        });
}
#endif

void UZeroPay_ModEngine::CleanTempStorage()
{
    /* Clean any old zip away */
    FString TempZipFilePath = FPaths::ProjectSavedDir() / TEXT("DownloadedFile.zip");
    IFileManager::Get().Delete(*TempZipFilePath, false, true, true);
}


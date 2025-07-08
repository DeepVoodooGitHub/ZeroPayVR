// (c) Ginger Ninja Games Ltd

#include "ZeroPay_PakEngine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/ArrayReader.h"

UZeroPay_PakEngine* UZeroPay_PakEngine::Instance = nullptr;

bool UZeroPay_PakEngineLibrary::MountPak(const FString& PakFilePath, const FString& MountPoint, int32 PakOrder)
{
    return UZeroPay_PakEngine::Get()->MountPak(PakFilePath, MountPoint, PakOrder) ;
}

bool UZeroPay_PakEngineLibrary::UnmountPak(const FString& PakFilePath)
{
    return UZeroPay_PakEngine::Get()->UnmountPak(PakFilePath) ;
}

void UZeroPay_PakEngineLibrary::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
    return UZeroPay_PakEngine::Get()->RegisterMountPoint(RootPath, ContentPath);
}

void UZeroPay_PakEngineLibrary::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
    return UZeroPay_PakEngine::Get()->UnRegisterMountPoint(RootPath, ContentPath);
}

FString UZeroPay_PakEngineLibrary::GetPakPath(FModioModID ModID, FModioPlatform Platform)
{
    FString BasePath = FPaths::ProjectSavedDir();
    FString ModFolderName = FString::Printf(TEXT("Mods/%s"), *ModID.ToString());
    FString DestinationPath = FPaths::Combine(BasePath, ModFolderName);

    switch (Platform)
    {
        case FModioPlatform::ModIOPlatform_Windows: return FPaths::Combine(DestinationPath, "Windows.pak"); break;
        case FModioPlatform::ModIOPlatform_Android: return FPaths::Combine(DestinationPath, "Android.pak"); break;
        case FModioPlatform::ModIOPlatform_LinuxServer: return FPaths::Combine(DestinationPath, "LinuxServer.pak"); break;
        default: break;
    }

    return TEXT("");
}

TArray<FString> UZeroPay_PakEngineLibrary::GetFilesInPak(const FString& PakAssetPath)
{
    return UZeroPay_PakEngine::Get()->GetFilesInPak(PakAssetPath);
}

UClass* UZeroPay_PakEngineLibrary::GetPakFileClass(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadClassFromPak(AssetPath);
}

UObject* UZeroPay_PakEngineLibrary::GetPakFileObject(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<UObject>(AssetPath);
}

UTexture2D* UZeroPay_PakEngineLibrary::GetPakFileTexture2D(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<UTexture2D>(AssetPath);
}

UStaticMesh* UZeroPay_PakEngineLibrary::GetPakFileStaticMesh(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<UStaticMesh>(AssetPath);
}

USkeletalMesh* UZeroPay_PakEngineLibrary::GetPakFileSkeletalMesh(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<USkeletalMesh>(AssetPath);
}

USoundBase* UZeroPay_PakEngineLibrary::GetPakFileSound(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<USoundBase>(AssetPath);
}

UMaterial* UZeroPay_PakEngineLibrary::GetPakFileMaterial(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<UMaterial>(AssetPath);
}

UAnimSequence* UZeroPay_PakEngineLibrary::GetPakFileAnimSequence(const FString& AssetPath)
{
    return UZeroPay_PakEngine::Get()->LoadObjectFromPak<UAnimSequence>(AssetPath);
}

bool UZeroPay_PakEngineLibrary::GetPakFileText(const FString& AssetPath, FString& String)
{
    return UZeroPay_PakEngine::Get()->ReadStringFromPak(AssetPath, String);
}

/* */

UZeroPay_PakEngine::UZeroPay_PakEngine()
{
#if WITH_EDITOR
    OriginalPlatformFile = nullptr ;
#endif
}

UZeroPay_PakEngine::~UZeroPay_PakEngine()
{
#if WITH_EDITOR
    if (OriginalPlatformFile)
    {
        FPlatformFileManager::Get().SetPlatformFile(*OriginalPlatformFile);
    }
#endif
}

bool UZeroPay_PakEngine::MountPak(const FString& PakFilePath, const FString& MountPoint, int32 PakOrder)
{
    if (!FPaths::FileExists(PakFilePath))
        return false;

    bool bResult = false;
    if (MountPoint.Len() > 0)
        bResult = GetPakPlatformFile()->Mount(*PakFilePath, PakOrder, *MountPoint);
    else
        bResult = GetPakPlatformFile()->Mount(*PakFilePath, PakOrder, NULL);
    return bResult;
}


FPakPlatformFile* UZeroPay_PakEngine::GetPakPlatformFile()
{
    if (!PakPlatformFile)
    {
        IPlatformFile* CurrentPlatformFile = FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile"));
        if (CurrentPlatformFile)
        {
            PakPlatformFile = static_cast<FPakPlatformFile*>(CurrentPlatformFile);
        }
        else
        {
            PakPlatformFile = new FPakPlatformFile();

            ensure(PakPlatformFile != nullptr);

            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

#if WITH_EDITOR
            // Record for editor, to unwind
            OriginalPlatformFile = &PlatformFile;
#endif

            if (PakPlatformFile->Initialize(&PlatformFile, TEXT("")))
            {
                FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("UZeroPay_PakEngine::GetPakPlatformFile() failed"));
            }
        }
    }

    ensure(PakPlatformFile != nullptr);
    return PakPlatformFile;
}

bool UZeroPay_PakEngine::UnmountPak(const FString& PakFilePath)
{
    return true;
}

void UZeroPay_PakEngine::RegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
    FPackageName::RegisterMountPoint(RootPath, ContentPath);
}

void UZeroPay_PakEngine::UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath)
{
    FPackageName::UnRegisterMountPoint(RootPath, ContentPath);
}

TArray<FString> UZeroPay_PakEngine::GetFilesInDirectory(const FString& Directory)
{
    FPakLoaderDirectoryVisitor Visitor;
    GetPakPlatformFile()->IterateDirectory(*Directory, Visitor);
    return Visitor.Files;
}

TArray<FString> UZeroPay_PakEngine::GetFilesInDirectoryRecursively(const FString& Directory)
{
    FPakLoaderDirectoryVisitor Visitor;
    GetPakPlatformFile()->IterateDirectoryRecursively(*Directory, Visitor);
    return Visitor.Files;
}

bool UZeroPay_PakEngine::DoesDirectoryExist(const FString& Directory)
{
    return GetPakPlatformFile()->DirectoryExists(*Directory);
}

bool UZeroPay_PakEngine::DoesFileExist(const FString& AssetPath)
{
    return GetPakPlatformFile()->FileExists(*AssetPath);
}

TArray<FString> UZeroPay_PakEngine::GetFilesInPak(const FString& PakFilename)
{
    FPakFile* Pak = nullptr;

    TRefCountPtr<FPakFile> PakFile = new FPakFile(GetPakPlatformFile(), *PakFilename, false);
    Pak = PakFile.GetReference();

    TArray<FString> PakItemsNames;
    if (Pak->IsValid())
    {
        TArray<FPakFile::FFilenameIterator> Records;
        for (FPakFile::FFilenameIterator It(*Pak, false); It; ++It)
            Records.Add(It);

        for (auto& It : Records)
        {
                PakItemsNames.Add(It.Filename());
        }
    }

    PakFile.SafeRelease();
    return PakItemsNames;
}


UClass* UZeroPay_PakEngine::LoadClassFromPak(const FString& AssetPath)
{
    const FString Name = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath) + TEXT("_C");
    return StaticLoadClass(UObject::StaticClass(), nullptr, *Name);
}

bool UZeroPay_PakEngine::ReadStringFromPak(const FString& AssetPath, FString& OutStr)
{
    return FFileHelper::LoadFileToString(OutStr, *AssetPath);
}

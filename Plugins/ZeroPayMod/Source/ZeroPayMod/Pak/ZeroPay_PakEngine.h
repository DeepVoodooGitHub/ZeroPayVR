// (c) Ginger Ninja Games Ltd

#pragma once

#include "IPlatformFilePak.h"
#include "CoreMinimal.h"
#include "ModioSubsystem.h"
#include "ModSupport/ZeroPay_ModGlobal.h"
#include "ZeroPay_PakEngine.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_PakEngineLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /* Mounts a PAK file at the given path */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
    static bool MountPak(const FString& PakFilePath, const FString& MountPoint, int32 PakOrder = 0);

    /* Unmounts a previously mounted PAK file */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
    static bool UnmountPak(const FString& PakFilePath);

	/* Tests if a supplied path is to a valid Pak file (or not) */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
	static bool IsValidMountPak(const FString& PakFilePath);

	/* Registers a mount point */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
	static void RegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	/* Unregister previous mount point */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
	static void UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	/* Registers / installed a new asset registry file */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
	static void RegisterAssetRegistryFile(const FString& GamePath) ;

    /* Get path to a Pak files location */
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Pak Management")
    static FString GetPakPath(FModioModID ModID, FModioPlatform Platform);

	/* Returns all files present in a .pak file (as on disk) */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static TArray<FString> GetFilesInPak(const FString& PakAssetPath);

	/* Loads a given class (i.e as BP) from a specified filepath (i.e. /Game/ZeroPayMods/UGC4991524/Levels/LVL_UGCTest_Persistant) */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UClass* GetPakFileClass(const FString& AssetPath);

	/* Loads an object from a pack file, cast this to a desired type */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UObject* GetPakFileObject(const FString& AssetPath);

	/* Support call to load UTexture2D asset from pak */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UTexture2D* GetPakFileTexture2D(const FString& AssetPath);

	/*  Support call load UStaticMesh asset from pak. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UStaticMesh* GetPakFileStaticMesh(const FString& AssetPath);

	/*  Support call load USkeletalMesh asset from pak. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static USkeletalMesh* GetPakFileSkeletalMesh(const FString& AssetPath);

	/*  Support call load USoundBase asset from pak. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static USoundBase* GetPakFileSound(const FString& AssetPath);

	/*  Support call load UMaterial asset from pak. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UMaterial* GetPakFileMaterial(const FString& AssetPath);

	/*  Support call  load UAnimSequence asset from pak. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static UAnimSequence* GetPakFileAnimSequence(const FString& AssetPath);

	/* Reads content as string */
	UFUNCTION(BlueprintPure, Category = "ZeroPay Pak Management")
	static bool GetPakFileText(const FString& AssetPath, FString& String);
};

class ZEROPAYMOD_API FPakLoaderDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

	TArray<FString> Files;
};

class ZEROPAYMOD_API UZeroPay_PakEngine
{
private:
    static UZeroPay_PakEngine* Instance;

#if WITH_EDITOR
    IPlatformFile* OriginalPlatformFile = nullptr ;
#endif
	FPakPlatformFile* PakPlatformFile = nullptr;

public:
    UZeroPay_PakEngine();
    ~UZeroPay_PakEngine();

    static UZeroPay_PakEngine* Get()
    {
        if (!Instance)
        {
            Instance = new UZeroPay_PakEngine();
        }

        return Instance;
    }

    bool MountPak(const FString& PakFilePath, const FString& MountPoint, int32 PakOrder = 0);
    bool UnmountPak(const FString& PakFilePath);
	bool IsValidMountPak(const FString& PakFilePath);

	void RegisterMountPoint(const FString& RootPath, const FString& ContentPath); 
	void UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	TArray<FString> GetFilesInDirectory(const FString& Directory) ;
	TArray<FString> GetFilesInDirectoryRecursively(const FString& Directory);

	FPakPlatformFile* GetPakPlatformFile();

	bool DoesDirectoryExist(const FString& Directory);

	bool DoesFileExist(const FString& AssetPath) ;

	TArray<FString> GetFilesInPak(const FString& PakFilename) ;

	UClass* LoadClassFromPak(const FString& AssetPath);
	
	template<class T>
	T* LoadObjectFromPak(const FString& Filepath)
	{
		const FString Name = T::StaticClass()->GetName() + TEXT("'") + Filepath + TEXT(".") + FPackageName::GetShortName(Filepath) + TEXT("'");
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *Name));
	}


	bool ReadStringFromPak(const FString& AssetPath, FString& OutStr);
};

// (c) Ginger Ninja Games Ltd

#include "Support/ZeroPay_UGCSupportUtils.h"
#if WITH_EDITOR
#include "ZeroPayEditorButtonsPlugin.h"
#endif
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"


// Sets default values
AZeroPay_UGCSupportUtils::AZeroPay_UGCSupportUtils()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AZeroPay_UGCSupportUtils::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AZeroPay_UGCSupportUtils::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AZeroPay_UGCSupportUtils::CreateAndSaveModDefinitionFile(FString UGCValue)
{
	/* Valid.. create directory */
	FString UGCPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + FString("ZeroPayMods/") + FString::Printf(TEXT("UGC%s"), *UGCValue);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*UGCPath);

	FString sFullPath = UGCPath + FString("/ZeroPayDefinition");
	FString sAssetFullPath = FString("/Game/ZeroPayMods/UGC") + *UGCValue + FString("/Definition");
	FString sAssetName = FString("ZeroPayDefinition");

	auto* package = CreatePackage(*sAssetFullPath);

	UZeroPayMod_DefinitionDataAsset* level_asset = NewObject< UZeroPayMod_DefinitionDataAsset >(package, UZeroPayMod_DefinitionDataAsset::StaticClass(), *sAssetName, RF_Public | RF_Standalone);

	level_asset->Definition.UGCID = UGCValue;

	if (ensure(level_asset != nullptr))
	{
		const auto file_name = FString::Printf(TEXT("%s%s"), *sFullPath, *FPackageName::GetAssetPackageExtension());

		UPackage::SavePackage(package, nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *file_name);
	}
}

void AZeroPay_UGCSupportUtils::OpenFilePicker(FString windowTitle, FString fileTypes, TArray<FString>& OutFiles)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		const void* ParentWindowHandle = nullptr;
		DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			windowTitle,
			TEXT(""),
			TEXT(""),
			fileTypes,
			EFileDialogFlags::None,
			OutFiles
		);
	}
}


TArray<FString> AZeroPay_UGCSupportUtils::GetUGCFoldersAsNumbers()
{
	TArray<FString> Result;

	// Target path
	const FString RootPath = TEXT("/Game/ZeroPayMods");

	// Get AssetRegistry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Scan the folder (recursive false to get only direct children)
	TArray<FString> SubFolders;
	AssetRegistryModule.Get().GetSubPaths(RootPath, SubFolders, false);

	UE_LOG(LogTemp, Display, TEXT("------------------------------------------------- Found folders %d."), SubFolders.Num());

	for (const FString& FolderPath : SubFolders)
	{
		FString FolderName = FPaths::GetCleanFilename(FolderPath); // e.g. "UGC123"

		UE_LOG(LogTemp, Display, TEXT("------------------------------------------------- folders %s."), *FolderName);

		if (FolderName.StartsWith(TEXT("UGC")) && FolderName.Len() > 3)
		{
			FString NumberPart = FolderName.Mid(3); // Remove "UGC"
			if (NumberPart.IsNumeric())
			{
				Result.Add(NumberPart);
			}
		}
	}

	return Result;
}


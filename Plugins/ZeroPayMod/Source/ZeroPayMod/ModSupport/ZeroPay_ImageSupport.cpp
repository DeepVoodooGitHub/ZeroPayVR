#include "ZeroPay_ImageSupport.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"


UZeroPay_ImageLoader* UZeroPay_ImageLoader::LoadPNGTextureAsync(const FString& InFilePath)
{
	UZeroPay_ImageLoader* Node = NewObject<UZeroPay_ImageLoader>();
	Node->FilePath = InFilePath;
	return Node;
}

void UZeroPay_ImageLoader::Activate()
{
	Async(EAsyncExecution::ThreadPool, [this]()
		{
			HandleLoad();
		});
}

void UZeroPay_ImageLoader::HandleLoad()
{
	if (!FPaths::FileExists(FilePath))
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnFailure.Broadcast(TEXT("File does not exist: ") + FilePath);
			});
		return;
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnFailure.Broadcast(TEXT("Failed to load file: ") + FilePath);
			});
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnFailure.Broadcast(TEXT("Invalid PNG data: ") + FilePath);
			});
		return;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData))
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnFailure.Broadcast(TEXT("Failed to decode PNG: ") + FilePath);
			});
		return;
	}

	int32 Width = ImageWrapper->GetWidth();
	int32 Height = ImageWrapper->GetHeight();

	TArray<uint8> RawCopy = MoveTemp(RawData);
	AsyncTask(ENamedThreads::GameThread, [this, RawCopy = MoveTemp(RawCopy), Width, Height]()
		{
			CreateTexture(RawCopy, Width, Height);
		});
}

void UZeroPay_ImageLoader::CreateTexture(const TArray<uint8>& RawData, int32 Width, int32 Height)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
	if (!Texture)
	{
		OnFailure.Broadcast(TEXT("Failed to create texture."));
		return;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

	Texture->UpdateResource();
	OnSuccess.Broadcast(Texture);
}



UZeroPay_DownloadPNGAsync* UZeroPay_DownloadPNGAsync::DownloadPng(UObject* InWorldContextObject, const FString& InUrl, EZeroPay_ImageStorage InStorageMode, const FString& InFileName, const FString& InDirectoryName)
{
	UZeroPay_DownloadPNGAsync* Node = NewObject<UZeroPay_DownloadPNGAsync>();
	Node->WorldContextObject = InWorldContextObject;
	Node->Url = InUrl;
	Node->StorageMode = InStorageMode;
	Node->FileName = InFileName;
	Node->DirectoryName = InDirectoryName;

	// Keep alive across worlds (nice for PIE/multiplayer)
	if (Node->WorldContextObject)
	{
		Node->RegisterWithGameInstance(Node->WorldContextObject);
	}
	else
	{
		Node->AddToRoot();
	}

	return Node;
}

void UZeroPay_DownloadPNGAsync::Activate()
{
	if (Url.IsEmpty())
	{
		BroadcastFailure(TEXT("Empty URL"));
		return;
	}

	// Build path early to validate inputs/dirs
	const FString TargetPath = BuildTargetPath();
	if (TargetPath.IsEmpty())
	{
		BroadcastFailure(TEXT("Invalid target path parameters"));
		return;
	}

	// Ensure directory exists
	const FString TargetDir = FPaths::GetPath(TargetPath);
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.CreateDirectoryTree(*TargetDir))
	{
		BroadcastFailure(FString::Printf(TEXT("Failed to create directory: %s"), *TargetDir));
		return;
	}

	// Fire request
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		BroadcastFailure(TEXT("HttpModule not available"));
		return;
	}

	Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("image/png,*/*"));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, TargetPath](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
		{
			if (!bSuccess || !Res.IsValid())
			{
				BroadcastFailure(TEXT("No HTTP response"));
				return;
			}

			const int32 Code = Res->GetResponseCode();
			if (Code < 200 || Code >= 300)
			{
				BroadcastFailure(FString::Printf(TEXT("HTTP %d: %s"), Code, *Res->GetContentAsString()));
				return;
			}

			const TArray<uint8>& Data = Res->GetContent();

			// Quick PNG signature check (optional but helpful)
			if (!IsLikelyPng(Data))
			{
				// Not fatal in case server is misreporting – but better to fail safe
				BroadcastFailure(TEXT("Downloaded bytes are not a PNG (signature mismatch)"));
				return;
			}

			// Save to disk
			if (!FFileHelper::SaveArrayToFile(Data, *TargetPath))
			{
				BroadcastFailure(FString::Printf(TEXT("Failed to write file: %s"), *TargetPath));
				return;
			}

			BroadcastSuccess(TargetPath);
		});

	Request->ProcessRequest();
}

FString UZeroPay_DownloadPNGAsync::BuildTargetPath() const
{
	// Normalize filename and ensure .png
	FString CleanFile = FileName;
	CleanFile.TrimStartAndEndInline();
	if (CleanFile.IsEmpty()) return FString();

	if (!CleanFile.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		CleanFile += TEXT(".png");
	}

	const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());

	if (StorageMode == EZeroPay_ImageStorage::Cache)
	{
		// Saved/Cache/ModLogos/<FileName>.png
		const FString CacheDir = FPaths::Combine(Saved, TEXT("Cache"), TEXT("ModLogos"));
		return FPaths::Combine(CacheDir, CleanFile);
	}
	else // Directory
	{
		FString DirName = DirectoryName;
		DirName.TrimStartAndEndInline();

		if (DirName.IsEmpty()) return FString();

		// Saved/Mods/<DirectoryName>/<FileName>.png
		const FString ModsDir = FPaths::Combine(Saved, TEXT("Mods"), DirName);
		return FPaths::Combine(ModsDir, CleanFile);
	}
}

bool UZeroPay_DownloadPNGAsync::IsLikelyPng(const TArray<uint8>& Bytes)
{
	// PNG signature: 89 50 4E 47 0D 0A 1A 0A
	static const uint8 Sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (Bytes.Num() < 8) return false;
	return FMemory::Memcmp(Bytes.GetData(), Sig, 8) == 0;
}

void UZeroPay_DownloadPNGAsync::BroadcastFailure(const FString& Error)
{
	OnFailure.Broadcast(Error);
	SetReadyToDestroy();
	if (!WorldContextObject) RemoveFromRoot();
}

void UZeroPay_DownloadPNGAsync::BroadcastSuccess(const FString& FullPath)
{
	OnSuccess.Broadcast(FullPath);
	SetReadyToDestroy();
	if (!WorldContextObject) RemoveFromRoot();
}

bool UZeroPay_ImageLoader::IsLogoInCache(int64 LogoID, float& OutAgeDays, FString& OutFullPath)
{
	OutAgeDays = -1.0f; // default when file doesn't exist
	OutFullPath.Empty();

	// Ensure Saved path is absolute
	const FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());

	// Build path: Saved/Cache/ModLogos/<ID>.png
	const FString LogoPath = FPaths::Combine(
		SavedDir,
		TEXT("Cache"),
		TEXT("ModLogos"),
		FString::Printf(TEXT("%d.png"), LogoID)
	);

	// Check file exists
	if (!FPaths::FileExists(LogoPath))
	{
		return false;
	}

	OutFullPath = LogoPath;

	// Get file timestamp
	const FDateTime FileTime = IFileManager::Get().GetTimeStamp(*LogoPath);

	// In UE, "invalid" time is represented by MinValue or MaxValue
	if (FileTime == FDateTime::MinValue() || FileTime == FDateTime::MaxValue())
	{
		OutAgeDays = -1.0f; // unknown age
		return true;        // file exists, timestamp unusable
	}

	// Calculate age in days
	const FTimespan AgeSpan = FDateTime::UtcNow() - FileTime;
	OutAgeDays = AgeSpan.GetTotalDays();

	return true;
}
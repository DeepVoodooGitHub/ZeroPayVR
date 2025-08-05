#include "ZeroPay_ImageSupport.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"

UZeroPay_ImageLoader* UZeroPay_ImageLoader::LoadPNGTextureAsync(const FString& InFilePath)
{
	UZeroPay_ImageLoader* Node = NewObject<UZeroPay_ImageLoader>();
	Node->FilePath = InFilePath;
	return Node;
}
7
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

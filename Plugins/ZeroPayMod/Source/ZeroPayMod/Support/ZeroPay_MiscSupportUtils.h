// (c) Ginger Ninja Games Ltd

#pragma once

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"
#include "ZeroPay_MiscSupportUtils.generated.h"

UENUM(BlueprintType)
enum FDebugConsoleLevel
{
	None,
	Log,
	Warn,
	Error
};

UENUM(BlueprintType)
enum class EZeroPay_NetControllerStatus : uint8
{
	Remote,
	Local
};

UCLASS()
class ZEROPAYMOD_API AZeroPay_MiscSupportUtils : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	AZeroPay_MiscSupportUtils();
	~AZeroPay_MiscSupportUtils();

	// Call from the BeginPlay event in your levels blueprint, this starts the underlying ZeroPay VR subsystems
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support", meta = (DefaultToSelf = "target"))
	static void InitialiseZeroPayVR(AActor* target);

	// Returns the correct path based on whether the "target" actor is controlled (on the network) locally or remotely
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support", meta = (DefaultToSelf = "target", ExpandEnumAsExecs = "Result"))
	static void IsLocallyControlled(AActor* target, EZeroPay_NetControllerStatus& Result);

	// Returns the current network version, used to keep mismatches servers and clients apart
	UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Support")
	static int GetNetVersionNumber()
	{
		return 1;
	}

	// Reads a command line argument 
	UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Support")
	static FString GetCommandLineOption(FString OptionName)
	{
		FString Result = "";
		FString FullKey = FString("-") + OptionName + TEXT("=");
		FParse::Value(FCommandLine::Get(), *FullKey, Result);
		return Result;
	}


	// Allow audio to continue if the game does not have focus
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static void EnableBackgroundAudio()
	{
		FApp::SetUnfocusedVolumeMultiplier(1.0f);
	}

	// Returns true if on packaged dedicated server build (will return FALSE in editor even if a server) 
	UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Support")
	static bool isDedicatedServer()
	{
#if UE_SERVER
		return true;
#else
		return false;
#endif
	}

	// Gets the project root directory 
	UFUNCTION(BlueprintPure, Category = "ZeroPay Mod Support")
	static FString GetProjectRootDir()
	{
		return FPaths::ProjectDir();
	}

	// Shows an editor utility widget
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static void ShowEditorUtilityWidget(UEditorUtilityWidgetBlueprint* EditorWidget)
	{
		if (EditorWidget)
		{
			UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
			EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidget);
		}
	}

	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static void SetEditorConfigurationBool(FString field, bool bValue)
	{
		GConfig->SetBool(
			TEXT("/Script/ZeroPayVR.EditorSettings"),
			*field,
			bValue,
			GEditorPerProjectIni);

		GConfig->Flush(false, GEditorPerProjectIni);  // Persist immediately
	};


	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static void SetEditorConfigurationString(FString field, FString Value)
	{
		GConfig->SetString(
			TEXT("/Script/ZeroPayVR.EditorSettings"),
			*field,
			*Value,
			GEditorPerProjectIni);

		GConfig->Flush(false, GEditorPerProjectIni);  // Persist immediately
	};

	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static bool GetEditorConfigurationBool(FString field)
	{
		bool returnValue = false ;

		GConfig->GetBool(
			TEXT("/Script/ZeroPayVR.EditorSettings"),
			*field,
			returnValue,
			GEditorPerProjectIni);

		return returnValue;
	};

	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static FString GetEditorConfigurationString(FString field)
	{
		FString returnValue = "";

		GConfig->GetString(
			TEXT("/Script/ZeroPayVR.EditorSettings"),
			*field,
			returnValue,
			GEditorPerProjectIni);

		return returnValue;
	};

	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support")
	static void OpenFilePicker(FString windowTitle, FString fileTypes, TArray<FString>& OutFiles)
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
			// Handle selected file(s)
		}
	}

};

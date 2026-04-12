// (c) Ginger Ninja Games Ltd

#pragma once

#include "ZeroPayMod_DefinitionDataAsset.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#if WITH_EDITOR
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Engine/World.h"
#endif
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/OutputDeviceNull.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include "UObject/Package.h"
#include "Misc/MessageDialog.h"
#include "FileHelpers.h"

#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#endif 

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "BlueprintDataDefinitions.h"

#include "Components/WidgetComponent.h"
#include "HAL/PlatformStackWalk.h"

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

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnIPResolved, const FString&, PublicIP);

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
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support", meta = (DefaultToSelf = "target"))
	static void InitialiseZeroPayVR(AActor* target);

	// Returns the correct path based on whether the "target" actor is controlled by a Player Controller (on the network) locally or remotely
	// Recommended to use "IsLocallyControlledByPawn" for most grabbable in-world actors
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support", meta = (DefaultToSelf = "target", ExpandEnumAsExecs = "Result"))
	static void UnderLocalControl(AActor* target, EZeroPay_NetControllerStatus& Result);

	// Returns the correct path based on whether the "targ\et" actor is controlled by a PAWN (on the network) locally or remotely
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support", meta = (DefaultToSelf = "target", ExpandEnumAsExecs = "Result"))
	static void IsLocallyControlledByPawn(AActor* target, EZeroPay_NetControllerStatus& Result);

	// Returns the current network version, used to keep mismatches servers and clients apart
	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static int GetNetVersionNumber()
	{
		return 1;
	}

	// Reads a command line argument 
	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static FString GetCommandLineOption(FString OptionName)
	{
		FString Result = "";
		FString FullKey = FString("-") + OptionName + TEXT("=");
		FParse::Value(FCommandLine::Get(), *FullKey, Result);
		return Result;
	}


	// Allow audio to continue if the game does not have focus
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
	static void EnableBackgroundAudio()
	{
		FApp::SetUnfocusedVolumeMultiplier(1.0f);
	}

	// Returns true if on packaged dedicated server build (will return FALSE in editor even if a server) 
	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static bool isDedicatedServer()
	{
#if UE_SERVER
		return true;
#else
		return false;
#endif
	}

	// Gets the project root directory 
	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static FString GetProjectRootDir()
	{
		return FPaths::ProjectDir();
	}

	// Editor configuration support
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
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
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
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
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
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
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
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

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static FString FormatStringWithDelimiters(int32 Number)
	{
		return FText::AsNumber(Number).ToString();
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static bool PlayInEditor()
	{
#if WITH_EDITOR
		if (GEditor)
		{
			UWorld* World = GEditor->PlayWorld;
			if (World)
			{
				return World->WorldType == EWorldType::PIE;
			}
		}
#endif
		return false ;
	}

	// Clear and invalidates a timer from a UObject (without world context)
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
	static void ClearAndInvalidateUObjectTimer(FTimerHandle Handle);

	//Exposes Server travel to blueprint
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ZeroPay|Misc Support", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool ServerTravel(UObject* WorldContextObject, const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify) ;

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ZeroPay|Animation")
	static int32 ReplaceAnimSkeleton(const TArray<UAnimationAsset*>& Assets)
	{
		int32 NumChanged = 0;

		// Hard-coded skeleton path
		static const FString NewSkeletonPath = TEXT("/ZeroPayMod/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
		USkeleton* NewSkeleton = LoadObject<USkeleton>(nullptr, *NewSkeletonPath);
		if (!NewSkeleton)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Could not load skeleton: %s"), *NewSkeletonPath)));
			return 0;
		}

		// Get selected assets from Content Browser
		TArray<FAssetData> SelectedAssets;
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		ContentBrowser.GetSelectedAssets(SelectedAssets);

		if (SelectedAssets.Num() == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No assets selected in Content Browser.")));
			return 0;
		}

		for (const FAssetData& AssetData : SelectedAssets)
		{
			UObject* Asset = AssetData.GetAsset();
			if (!Asset)
				continue;

			// Only handle UAnimationAsset (covers AnimSequence, BlendSpaces, AnimMontages, etc.)
			UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Asset);
			if (!AnimAsset)
				continue;

			USkeleton* OldSkeleton = AnimAsset->GetSkeleton();
			if (OldSkeleton == NewSkeleton)
				continue;

			AnimAsset->Modify();
			AnimAsset->SetSkeleton(NewSkeleton);
			AnimAsset->MarkPackageDirty();

			NumChanged++;
		}

		// Save all modified packages in one go
		if (NumChanged > 0)
		{
			TArray<UPackage*> PackagesToSave;
			for (const FAssetData& AssetData : SelectedAssets)
			{
				if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(AssetData.GetAsset()))
				{
					if (AnimAsset->GetSkeleton() == NewSkeleton)
					{
						UPackage* Pkg = AnimAsset->GetOutermost();
						if (Pkg && Pkg->IsDirty())
						{
							PackagesToSave.AddUnique(Pkg);
						}
					}
				}
			}
			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty=*/true, /*bPromptToSave=*/false);
		}

		return NumChanged;
	}
#endif

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static FRotator StripToYawOnly(const FRotator& InRotator)
	{
		// Strip Pitch and Roll, keep only Yaw
		return FRotator(0.f, InRotator.Yaw, 0.f);
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static double YawDifference(const FRotator& A, const FRotator& B)
	{
		// Absolute value of yaw difference
		return FMath::Abs(FMath::FindDeltaAngleDegrees(A.Yaw, B.Yaw));
	}

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
	static void GetPublicIP(const FOnIPResolved& OnComplete)
	{
		if (!OnComplete.IsBound())
			return ;

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(TEXT("GET"));
		Request->SetURL(TEXT("https://api.ipify.org?format=text")); // or ?format=json
		// Bind a lambda that executes the Blueprint delegate when finished
		Request->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bWasSuccessful)
			{
				if (bWasSuccessful && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
				{
					FString IP = Resp->GetContentAsString();
					OnComplete.ExecuteIfBound(IP);
				}
				else
				{
					OnComplete.ExecuteIfBound(FString()); // empty = error
				}
			});
		Request->ProcessRequest();
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static void GetSessionID_AsString(const FBlueprintSessionResult& SessionResult, FString& SessionID)
	{
		const TSharedPtr<class FOnlineSessionInfo> SessionInfo = SessionResult.OnlineResult.Session.SessionInfo;
		if (SessionInfo.IsValid() && SessionInfo->IsValid() && SessionInfo->GetSessionId().IsValid())
		{
			SessionID = SessionInfo->GetSessionId().ToString();
			return;
		}

		// Zero the string out if we didn't have a valid one, in case this is called in c++
		SessionID.Empty();
	}


	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support" )
	static FString GetCurrentSessionID_AsString()
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (!Subsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetCurrentSessionID: No OnlineSubsystem"));
			return TEXT("");
		}

		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (!SessionInterface.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GetCurrentSessionID: Invalid Session Interface"));
			return TEXT("");
		}

		// Default name for active game sessions is "GameSession"
		const FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(TEXT("ZeroPayVRSession"));
		if (!NamedSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetCurrentSessionID: No active named session found"));
			return TEXT("");
		}

		const FOnlineSessionInfo* Info = NamedSession->SessionInfo.Get();
		if (!Info)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetCurrentSessionID: SessionInfo invalid"));
			return TEXT("");
		}

		return Info->GetSessionId().ToString();
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static int32 GetTotalIntegerDigits(int32 N)
	{
		if (N == 0) return 1;
		N = FMath::Abs(N);
		return static_cast<int32>(FMath::FloorToInt(FMath::LogX(10.0, static_cast<double>(N))) + 1);
	}


	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject") )
	static int32 GetPIEInstanceID(const UObject* WorldContextObject)
	{
#if WITH_EDITOR
		const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			return -1;
		}

		// Only meaningful in editor PIE worlds
		if (World->WorldType == EWorldType::PIE)
		{
			if (FWorldContext* WC = GEngine->GetWorldContextFromWorld(World))
			{
				return WC->PIEInstance;   // 0 = first client, 1 = second, etc.
			}
		}
#endif

		return -1; // Not PIE / not editor
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static TSubclassOf<UUserWidget> GetWidgetClassFromWidgetComponent(const UWidgetComponent* WidgetComp)
	{
		if (!WidgetComp)
			return nullptr;

		return WidgetComp->GetWidgetClass();
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support", meta = (DevelopmentOnly))
	static bool ValidateEditorSubLevelsAlwaysLoaded(TArray<FString>& OutFailures)
	{
		OutFailures.Reset();

#if !WITH_EDITOR
		return true;
#else
		if (!GEditor)
		{
			OutFailures.Add(TEXT("GEditor is null (unexpected in editor)."));
			return false;
		}

		// Choose which world to validate.
		UWorld* TargetWorld = GEditor->GetEditorWorldContext().World();

		if (!TargetWorld)
		{
			OutFailures.Add(TEXT("No target world found (Editor world not available)."));
			return false;
		}

		const TArray<ULevelStreaming*>& StreamingLevels = TargetWorld->GetStreamingLevels();

		bool bAllOk = true;

		for (ULevelStreaming* LS : StreamingLevels)
		{
			if (!LS)
			{
				bAllOk = false;
				OutFailures.Add(TEXT("Encountered null ULevelStreaming entry."));
				continue;
			}

			const bool bIsAlwaysLoaded = LS->IsA(ULevelStreamingAlwaysLoaded::StaticClass());
			if (!bIsAlwaysLoaded)
			{
				bAllOk = false;

				const FString LevelPackage = LS->GetWorldAssetPackageName();
				const FString ClassName = LS->GetClass()->GetName();

				OutFailures.Add(FString::Printf(
					TEXT("Sub-level is NOT Always Loaded: Package='%s' StreamingClass='%s'"),
					*LevelPackage, *ClassName
				));
			}
		}

		// Optional: if there are *no* sub-levels, you may want to treat that as OK.
		// If you want to flag it:
		// if (StreamingLevels.Num() == 0) { OutFailures.Add(TEXT("No streaming sub-levels found.")); }

		return bAllOk;
#endif
	}

	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
	static void DumpBlueprintCallstack()
	{
#if !UE_BUILD_SHIPPING
		const FString ScriptStack = FFrame::GetScriptCallstack();

		if (ScriptStack.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Blueprint Callstack: <none available - not executing inside BP VM>"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("==== Blueprint Callstack ====\n%s\n==== End Blueprint Callstack ===="), *ScriptStack);
		}
#endif
	}

	/** Returns TRUE if this is executing in a Client world (NM_Client). This is machine NetMode, not actor authority. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|Networking", meta = (DefaultToSelf = "Actor", HidePin = "Actor"))
	static bool IsClientNetMode(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const UWorld* World = Actor->GetWorld();
		if (!World)
		{
			return false;
		}

		return World->GetNetMode() == NM_Client;
	}


	// Used to force skeletal meshes that are not "refreshing bones" during a tick to update
	// Handy to update hidden bones, etc.
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|Misc Support")
	static void ForceSkeletalMeshRefresh(USkeletalMeshComponent* SkelComp)
	{
		if (!SkelComp) return;
		SkelComp->RefreshBoneTransforms();
		SkelComp->MarkRenderStateDirty();
		SkelComp->UpdateBounds();
	}

	UFUNCTION(BlueprintPure, Category = "ZeroPay|Misc Support")
	static bool IsListenServer() 
	{
		if (GEngine != nullptr && GWorld != nullptr) 
		{ 
			return GEngine->GetNetMode(GWorld) == NM_ListenServer;
		} 
		return false ; 
	}

};

// (c) Ginger Ninja Games Ltd

#include "Support/ZeroPay_MiscSupportUtils.h"
#include "GameFramework/Character.h"
#include "ZeroPayMod.h"
#include "Debug/ZeroPay_InternalDebug.h"
#include "Debug/ZeroPay_DebugSupport.h"


AZeroPay_MiscSupportUtils::AZeroPay_MiscSupportUtils()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

AZeroPay_MiscSupportUtils::~AZeroPay_MiscSupportUtils()
{
}

// Called when the game starts or when spawned
void AZeroPay_MiscSupportUtils::BeginPlay()
{
	Super::BeginPlay();

}

void AZeroPay_MiscSupportUtils::UnderLocalControl(AActor* target, EZeroPay_NetControllerStatus& Result)
{
	AActor* Owner = target->GetOwner();

	if (APlayerController* OwnerPC = Cast<APlayerController>(Owner))
	{
		if (OwnerPC->IsLocalController())
		{
			Result = EZeroPay_NetControllerStatus::Local;
		}
		return;
	}

	Result = EZeroPay_NetControllerStatus::Remote ;
}

void AZeroPay_MiscSupportUtils::IsLocallyControlledByPawn(AActor* target, EZeroPay_NetControllerStatus& Result)
{
	Result = EZeroPay_NetControllerStatus::Remote;

	AActor* Owner = target->GetOwner();
	if (Owner)
	{
		ACharacter* Character = Cast<ACharacter>(Owner);
		if (Character)
		{
			if (Character->IsLocallyControlled())
				Result = EZeroPay_NetControllerStatus::Local;
		}
		return;
	}


}


void AZeroPay_MiscSupportUtils::InitialiseZeroPayVR(AActor* target)
{
	UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, TEXT("[Init] InitialiseZeroPayVR() called."));

	/* Server-side only */
	if (!target->HasAuthority())
	{
		UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, TEXT("       Ignored, we are a client (i.e. no standalone or dedicated server instance)."));
		return;
	}

	FString BPClassPath = FString("Blueprint'/ZeroPayMod/Blueprints/GameLogic/Server/BP_ZP_ServerLogic.BP_ZP_ServerLogic_C'");
	TSubclassOf<AActor> ZeroPayVRBPClass = Cast<UClass>(StaticLoadObject(UObject::StaticClass(), nullptr, *BPClassPath));

	/* Find out if they've already created one.. */
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(target->GetWorld(), ZeroPayVRBPClass, Found);
	if (Found.Num() != 0)
	{
		UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, TEXT("       Warning, InitialiseZeroPayVR() was called twice, ignoring secondary call."), FDebugConsoleLevel::Warn);
		return;
	}

	/* Create it */
	if (ZeroPayVRBPClass != nullptr)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;
		FTransform spawnTransform = FTransform::Identity;
		AActor* ServerLogicBP = target->GetWorld()->SpawnActor(ZeroPayVRBPClass, &spawnTransform, SpawnInfo);
		// Tell server to init itself after spawning
		FOutputDeviceNull ar;
		ServerLogicBP->CallFunctionByNameWithArguments(TEXT("InitialiseServer"), ar, NULL, true);

		if (ServerLogicBP != nullptr)
			UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, TEXT("       Completed."));
		else
			UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr, TEXT("       failed to spawn BP actor."), FDebugConsoleLevel::Error);
	}
	else
	{
		UZeroPay_InternalDebugFunctionLibrary::PrintInternalString(nullptr, nullptr,  TEXT("       failed to spawn server logic, game will be broken!"), FDebugConsoleLevel::Error);
		return;
	}
}


void AZeroPay_MiscSupportUtils::ClearAndInvalidateUObjectTimer(FTimerHandle Handle)
{
	// Note: We cannot extract world from FTimerHandle directly
	// But we can access active timers if any UObject context is globally bound (not guaranteed)

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!World) continue;

		FTimerManager& TimerManager = World->GetTimerManager();

		// Check if this TimerManager has that handle
		if (TimerManager.TimerExists(Handle))
		{
			TimerManager.ClearTimer(Handle);
			Handle.Invalidate();
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Failed to clear timer - no valid world context found or timer not active."));
}

bool AZeroPay_MiscSupportUtils::ServerTravel(UObject* WorldContextObject, const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify)
{
	if (!WorldContextObject)
	{
		return false;
	}

	//using a context object to get the world
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World)
	{
		return World->ServerTravel(FURL, bAbsolute, bShouldSkipGameNotify);
	}

	return false;
}
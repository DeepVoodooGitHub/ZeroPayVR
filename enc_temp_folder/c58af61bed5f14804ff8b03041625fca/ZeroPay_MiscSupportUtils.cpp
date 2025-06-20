// (c) Ginger Ninja Games Ltd

#include "Support/ZeroPay_MiscSupportUtils.h"
#include "GameFramework/Character.h"


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

void AZeroPay_MiscSupportUtils::IsLocallyControlled(AActor* target, EZeroPay_NetControllerStatus& Result)
{
	AActor* Owner = target->GetOwner() ;
	if (Owner)
	{
		ACharacter* Character = Cast<ACharacter>(Owner);
		if (Character->IsLocallyControlled())
			Result = EZeroPay_NetControllerStatus::Local;

		return;
	}

	Result = EZeroPay_NetControllerStatus::Remote;
}


void AZeroPay_MiscSupportUtils::InitialiseZeroPayVR(AActor* target)
{
	UE_LOG(LogTemp, Display, TEXT("[Init] InitialiseZeroPayVR() called."));

	/* Server-side only */
	if (!target->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Init] InitialiseKJModGame() Was not called in authority (on server)"));
		return;
	}

	FString BPClassPath = FString("Blueprint'/ZeroPayMod/Blueprints/GameLogic/Server/BP_ZP_DedicatedServerLogic.BP_ZP_DedicatedServerLogic_C'");
	TSubclassOf<AActor> ZeroPayVRBPClass = Cast<UClass>(StaticLoadObject(UObject::StaticClass(), nullptr, *BPClassPath));


	/* Find out if they've already created one.. */
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(target->GetWorld(), ZeroPayVRBPClass, Found);
	if (Found.Num() != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Error] \\ InitialiseZeroPayVR() was called twice, ignoring secondary call."));
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
			UE_LOG(LogTemp, Display, TEXT("[info] \\ InitialiseZeroPayVR completed correctly"))
		else
			UE_LOG(LogTemp, Display, TEXT("[info] \\ InitialiseZeroPayVR failed to spawn"));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[Error] \\ InitialiseZeroPayVR() failed to spawn server logic, game will be broken!"));
		return;
	}
}
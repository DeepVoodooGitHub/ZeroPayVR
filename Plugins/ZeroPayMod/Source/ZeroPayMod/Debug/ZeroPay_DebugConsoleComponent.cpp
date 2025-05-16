// (c) Ginger Ninja Games Ltd

#include "Debug/ZeroPay_DebugConsoleComponent.h"

UZeroPay_DebugConsoleComponent::UZeroPay_DebugConsoleComponent()
{
	PrimaryComponentTick.bCanEverTick = false ;

	/* Var inits */
	bDisableConsoleOutput = false ;

	/* We are network aware */
	SetIsReplicatedByDefault(true); 
}

void UZeroPay_DebugConsoleComponent::BeginPlay()
{
	Super::BeginPlay();	
}

void UZeroPay_DebugConsoleComponent::SetDebugConsoleDisabled(bool bDisable)
{
	bDisableConsoleOutput = bDisable;
}

void UZeroPay_DebugConsoleComponent::AddDebugConsoleLine(FDebugConsoleLevel debugConsoleLevel, bool bIncludeObjectName, const FString& value)
{
	if (bDisableConsoleOutput)
		return ;

	if (IsBeingDestroyed() && !GetOwner() )
		return;

	AActor* target = GetOwner();

	if (!IsValid(target))
		return;
	UWorld* MyWorld = target->GetWorld();
	if (!IsValid(MyWorld))
		return;

	/* Authority? */
	FString authority = "[Client] ";
	if (target->HasAuthority())
		authority = "[Server] ";

	/* Append time */
	FString TimeString = FString::Printf(TEXT("%.3f "), MyWorld->GetTimeSeconds());

	/* Include name */
	FString ObjectName = "";
	if (bIncludeObjectName)
	{
		ObjectName = target->GetFName().ToString() ;

		/* Strip UAID */
		int32 Index = ObjectName.Find(TEXT("_UAID_"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (Index != INDEX_NONE)
			ObjectName = ObjectName.Left(Index); // Keep everything before _UAID_

		ObjectName = TEXT("(") + ObjectName + TEXT(") ");
	}

	/* Build output string */
	FString OutputString;
	switch (debugConsoleLevel)
	{
		case Log: OutputString = authority + TEXT("[Log] ") + TimeString + ObjectName + value; break;
		case Warn: OutputString = authority + TEXT("[Warn] ") + TimeString + ObjectName + value; break;
		case Error: OutputString = authority + TEXT("[Error] ") + TimeString + ObjectName + value; break;
		default: OutputString = authority + TimeString + value; break;
	}

    if (GetOwnerRole() < ROLE_Authority)
    {
        AddDebugConsoleLine_SERVER(OutputString);
    }
    else
    {
        AddDebugConsoleLine_MULTICAST(OutputString);
    }
}

void UZeroPay_DebugConsoleComponent::AddDebugConsoleLine_SERVER_Implementation(const FString& value)
{
	if (IsBeingDestroyed() && !GetOwner())
		return;
	
	AddDebugConsoleLine_MULTICAST(value);
}

void UZeroPay_DebugConsoleComponent::AddDebugConsoleLine_MULTICAST_Implementation(const FString& value)
{
	if (IsBeingDestroyed() && !GetOwner())
		return;

	AActor* target = GetOwner();

	if (!IsValid(target))
		return;
	UWorld* MyWorld = target->GetWorld();
	if (!IsValid(MyWorld))
		return;
	
	/* Find console BP Class */
	FString BPClassPath = FString("Blueprint'/ZeroPayMod/Blueprints/Library/DebugUtils/BP_ZPVR_DebugConsole.BP_ZPVR_DebugConsole_C'");
	TSubclassOf<AActor> DebugConsoleBPClass = Cast<UClass>(StaticLoadObject(UObject::StaticClass(), nullptr, *BPClassPath));
	if (DebugConsoleBPClass == nullptr)
		return;

	/* Find all actors */
	TArray<AActor*> FoundDebugConsoleActors;
	UGameplayStatics::GetAllActorsOfClass(target->GetWorld(), DebugConsoleBPClass, FoundDebugConsoleActors);

	/* Iterate them.. */
	for (int nCounter = 0; nCounter < FoundDebugConsoleActors.Num(); nCounter++)
	{
		/* Execute a BP function on that actor */
		FOutputDeviceNull ar;
		char funcCallBuf[2048];
		_snprintf_s(funcCallBuf, sizeof(funcCallBuf), "AddLine \"%s\"", TCHAR_TO_ANSI(*value));
		FoundDebugConsoleActors[nCounter]->CallFunctionByNameWithArguments(ANSI_TO_TCHAR(funcCallBuf), ar, NULL, true);
	}
}


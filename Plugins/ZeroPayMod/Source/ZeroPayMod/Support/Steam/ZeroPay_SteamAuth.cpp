#include "ZeroPay_SteamAuth.h"
#include "Async/Async.h"

// Include EIK's async function (adjust path to your plugin)
#include "OnlineSubsystemEIK/AsyncFunctions/Extra/EIK_GetPlatformAuthToken_AsyncFunction.h"
// If your plugin uses a different module path, try:
// #include "EOSIntegrationKit/Public/Async/EIK_GetPlatformAuthToken_AsyncFunction.h"
// or search your EIK plugin for the header to get the exact include.

UZeroPay_SteamAuth* UZeroPay_SteamAuth::GetSteamAuthToken()
{
	return NewObject<UZeroPay_SteamAuth>();
}

void UZeroPay_SteamAuth::Activate()
{
	// Keep alive for the async lifetime
	AddToRoot();

	// 1) Hop off the Game Thread first so any first-use init doesn't hitch a frame
	Async(EAsyncExecution::ThreadPool, [this]()
		{
			// 2) Re-enter GT to call the EXACT EIK async function from the commit
			AsyncTask(ENamedThreads::GameThread, [this]()
				{
					// Create the EIK async task node and bind to its outputs
					UEIK_GetPlatformAuthToken_AsyncFunction* Task = UEIK_GetPlatformAuthToken_AsyncFunction::GetPlatformAuthToken();

					if (!Task)
					{
						OnFailure.Broadcast(TEXT("EIK GetPlatformAuthToken task could not be created"));
						Cleanup();
						return;
					}

					Task->OnSuccess.AddDynamic(this, &UZeroPay_SteamAuth::HandleEIKSuccess);
					Task->OnFailure.AddDynamic(this, &UZeroPay_SteamAuth::HandleEIKFailure);

					if (UBlueprintAsyncActionBase* AsyncBase = Cast<UBlueprintAsyncActionBase>(Task))
					{
						AsyncBase->Activate();
					}
				});
		});
}

void UZeroPay_SteamAuth::HandleEIKSuccess(const FString& Token)
{
	OnSuccess.Broadcast(Token);
	Cleanup();
}

void UZeroPay_SteamAuth::HandleEIKFailure(const FString& Error)
{
	OnFailure.Broadcast(Error);
	Cleanup();
}

void UZeroPay_SteamAuth::Cleanup()
{
	RemoveFromRoot();
	SetReadyToDestroy();
}

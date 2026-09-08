// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "AdvancedFriendsGameInstance.h"
#include "ZeroPay_GameInstance_r1.generated.h"

/**
 * 
 */

class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZeroPayClientMapLoaded, UWorld*, LoadedWorld);

UCLASS()
class ZEROPAYMOD_API UZeroPay_GameInstance_r1 : public UAdvancedFriendsGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// CLIENT ONLY - Trigger on clients when a map is loaded, used for travelling to process mod's (i.e. register things)
	//               Should be called "in editor" for testing mod's that have UGC's dependancies
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "ZeroPay|Travel")
	FZeroPayClientMapLoaded OnClientMapLoaded;
protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "ZeroPay|Travel", meta = (DisplayName = "On Client Map Loaded"))
	void K2_OnClientMapLoaded(UWorld* LoadedWorld);

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);

	FDelegateHandle PostLoadMapHandle;
};

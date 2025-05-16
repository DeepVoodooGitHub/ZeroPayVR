// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	// Debug helper functions
	UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Support", meta = (DefaultToSelf = "target", ExpandEnumAsExecs = "Result"))
	static void IsLocallyControlled(AActor* target, EZeroPay_NetControllerStatus& Result) ;
	
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "ZeroPay_BodySocket_r1.generated.h"

UENUM(BlueprintType)
enum EZeroPay_BodySocket_Location
{
	LeftLeg,
	RightLeg,
	Waist,
	Chest,
	LeftArm,
	RightArm,
	Back
};

/* The body socket channel is trace channel 5 */
#define ECC_BodySocket ECC_GameTraceChannel5

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_BodySocket_r1 : public USphereComponent
{
	GENERATED_BODY()	
public:
	UZeroPay_BodySocket_r1();

	/* Allows a body socket to return a transform where the actor (via interface) will be placed.
	   This can provide logic to "snap" to center, or scale the actor if required */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ZeroPay|Sockets")
	FTransform ProvideSocketTransform(const TScriptInterface<class IZeroPay_BodySocket_Interface_r1>& RequestingInterface) const;

	// Interface Implementation
	virtual FTransform ProvideSocketTransform_Implementation(const TScriptInterface<class IZeroPay_BodySocket_Interface_r1>& RequestingInterface) const;

	UFUNCTION(BlueprintPure, Category = "ZeroPay Misc Support")
	AActor* BodySocketInterfaceToActor(const TScriptInterface<class IZeroPay_BodySocket_Interface_r1> RequestingInterface) const
	{		
		return Cast<AActor>(RequestingInterface.GetObject());
	}
};
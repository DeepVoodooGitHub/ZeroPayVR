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

	/* Allows a body socket to return a transform where the actor will be placed, this can
	   provide logic to "snap" to center, or scale the actor if required */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ZeroPay|Sockets")
	FTransform ProvideSocketTransform(AActor* RequestingActor) const;    

	// Interface Implementation
	FTransform ProvideSocketTransform_Implementation(AActor* RequestingActor) const ;
};
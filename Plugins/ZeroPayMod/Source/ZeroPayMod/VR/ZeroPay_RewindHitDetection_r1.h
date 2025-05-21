// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZeroPay_RewindHitDetection_r1.generated.h"


UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZEROPAYMOD_API UZeroPay_RewindHitDetection_r1 : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UZeroPay_RewindHitDetection_r1();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ZeroPayGunAttachmentPoint_r1.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZEROPAYMOD_API UZeroPayGunAttachmentPoint_r1 : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UZeroPayGunAttachmentPoint_r1();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "ZeroPayGunAttachmentPoint_r1.generated.h"


UENUM(BlueprintType)
enum class EZeroPayGunAttachmentSlotType : uint8
{
	None,
	TopRail,
	SideRailLeft,
	SideRailRight,
	BottomRail,
	Muzzle,
	Stock,
	Optic,
	Underbarrel,
	Custom
};

class USphereComponent;
class UStaticMeshComponent;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class ZEROPAYMOD_API UZeroPayGunAttachmentPoint_r1 : public UBoxComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UZeroPayGunAttachmentPoint_r1();

	// =========================
	// Core State
	// =========================

	// What sort of attachment slot is this?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment Point")
	EZeroPayGunAttachmentSlotType SlotType;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	
};

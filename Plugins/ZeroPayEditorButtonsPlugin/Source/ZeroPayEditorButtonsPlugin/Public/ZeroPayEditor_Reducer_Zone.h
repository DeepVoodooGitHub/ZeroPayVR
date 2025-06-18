// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "ZeroPayEditor_Reducer_Zone.generated.h"

UCLASS()
class ZEROPAYEDITORBUTTONSPLUGIN_API AZeroPayEditor_Reducer_Zone : public AActor
{
	GENERATED_BODY()
	
private:
	#if WITH_EDITORONLY_DATA
	// Editor-only visual representation
	UPROPERTY(VisibleAnywhere, Category = "Overridden reducer zone")
	class UBoxComponent* OverrideReducerZone;
	#endif

public:	
	AZeroPayEditor_Reducer_Zone();

	FBox GetWorldBoundingBox() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
};

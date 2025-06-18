// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "ZeroPayEditor_Reducer_PlayerZone.generated.h"

UCLASS()
class ZEROPAYEDITORBUTTONSPLUGIN_API AZeroPayEditor_Reducer_PlayerZone : public AActor
{
	GENERATED_BODY()

private:
#if WITH_EDITORONLY_DATA
	// Editor-only visual representation
	UPROPERTY(VisibleAnywhere, Category = "Player Active Zone")
	class UBoxComponent* PlayerZoneBox;
#endif

public:
	AZeroPayEditor_Reducer_PlayerZone();

	FBox GetWorldBoundingBox() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Grippables/GrippableStaticMeshActor.h"
#include "VR/Character/Interfaces/ZeroPay_BodySocket_Interface_r1.h"
#include "ZeroPayMagazineBase_r1.generated.h"

/**
 * 
 */
UCLASS()
class ZEROPAYMOD_API AZeroPayMagazineBase_r1 : public AGrippableStaticMeshActor, public IZeroPay_BodySocket_Interface_r1
{
	GENERATED_BODY()
};

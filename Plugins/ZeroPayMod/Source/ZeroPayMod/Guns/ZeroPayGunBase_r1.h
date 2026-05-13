// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grippables/GrippableSkeletalMeshActor.h"
#include "VR/Character/Interfaces/ZeroPay_BodySocket_Interface_r1.h"
#include "Items/Interfaces/ZeroPay_VRItem_Interface_r1.h"
#include "ZeroPayGunBase_r1.generated.h"



UCLASS()
class ZEROPAYMOD_API AZeroPayGunBase_r1 : public AGrippableSkeletalMeshActor, public IZeroPay_BodySocket_Interface_r1, public IZeroPay_VRItem_Interface_r1
{
	GENERATED_BODY()
	
public:	

protected:

public:	
	
	
};

// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ZeroPayMod_DefinitionDataAsset.h"
#include "ZeroPayMod_InitUGCObject.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class ZEROPAYMOD_API UZeroPayMod_InitUGCObject : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZeroPay|Mod")
	bool InitMod(UZeroPayMod_DefinitionDataAsset* ModDefinition,const TArray<int64>& Int64Values,FString& Message);

	virtual bool InitMod_Implementation(UZeroPayMod_DefinitionDataAsset* ModDefinition, const TArray<int64>& Int64Values, FString& Message)
	{
		return true ;
	}
};
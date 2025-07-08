// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class ZEROPAYMOD_API ZeroPay_ModSubEngine
{
public:
	ZeroPay_ModSubEngine();
	~ZeroPay_ModSubEngine();

    // Get installed mods
    UFUNCTION(BlueprintCallable, Category = "ZeroPay Mod Subscription Engine")
    static TArray<FString> GetInstalledMods();
};

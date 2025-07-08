// (c) Ginger Ninja Games Ltd

#pragma once

#include "CoreMinimal.h"

static FString FGameID = TEXT("10012");
static FString FAPIKey = TEXT("518616210eba1dc1c171e0441e227c9c");

UENUM(BlueprintType)
enum class FModioPlatform : uint8
{
	ModIOPlatform_Windows   UMETA(DisplayName = "Windows"),
	ModIOPlatform_Android   UMETA(DisplayName = "Android"),
	ModIOPlatform_LinuxServer UMETA(DisplayName = "Linux Server")
};

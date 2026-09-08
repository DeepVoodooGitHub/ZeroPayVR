// (c) Ginger Ninja Games Ltd

#include "VR/ZeroPay_GameInstance_r1.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameMapsSettings.h"
#include "Engine/World.h"
#include "Debug/ZeroPay_InternalDebug.h"
#include "UObject/UObjectGlobals.h"

void UZeroPay_GameInstance_r1::Init()
{
    Super::Init();

    /* Clear globals (until we find a better solution) */
    InternalStoredLogEntries.Empty();
    InternalDebugTargetActor = nullptr;

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UZeroPay_GameInstance_r1::HandlePostLoadMap);

    UE_LOG(LogTemp, Warning, TEXT("ZeroPay GameInstance Init(): %p, World: %s, WorldType: %d - %s"), this, *GetWorld()->GetName(), (int32)GetWorld()->WorldType, *GetWorld()->PersistentLevel->GetName());
}


void UZeroPay_GameInstance_r1::Shutdown()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    PostLoadMapHandle.Reset();

    Super::Shutdown();
}

void UZeroPay_GameInstance_r1::HandlePostLoadMap(UWorld* LoadedWorld)
{
    // Another PIE instance's world.
    if (!LoadedWorld || LoadedWorld->GetGameInstance() != this)
    {
        return;
    }

    // Client-side only. NM_Client for a real client, NM_ListenServer for a host,
    // NM_Standalone for single player — all have a local viewport.
    if (LoadedWorld->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    // Ignore the brief transition map used by seamless travel.
    const FSoftObjectPath& TransitionMap = UGameMapsSettings::GetGameMapsSettings()->TransitionMap;
    if (!TransitionMap.IsNull() && LoadedWorld->GetMapName().Contains(TransitionMap.GetAssetName()))
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ZeroPay: client map loaded - %s"), *LoadedWorld->GetMapName());

    OnClientMapLoaded.Broadcast(LoadedWorld);
    K2_OnClientMapLoaded(LoadedWorld);
}
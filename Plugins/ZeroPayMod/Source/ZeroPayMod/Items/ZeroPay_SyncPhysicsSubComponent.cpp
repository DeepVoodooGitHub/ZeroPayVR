// ZeroPay_SyncPhysicsSubComponent.cpp

#include "ZeroPay_SyncPhysicsSubComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

UZeroPay_SyncPhysicsSubComponent::UZeroPay_SyncPhysicsSubComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // tick after physics resolves this frame

	bAutoActivate = true;
}

void UZeroPay_SyncPhysicsSubComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveTargetComponent();

	// Only actually ticks while syncing -- StartSync()/StopSync() flip this on/off.
	SetComponentTickEnabled(false);
}

bool UZeroPay_SyncPhysicsSubComponent::ResolveTargetComponent()
{
	TargetComponent = nullptr;

	AActor* Owner = GetOwner();
	if (!Owner || TargetComponentName.IsNone())
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> SkelComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkelComps);

	for (USkeletalMeshComponent* Comp : SkelComps)
	{
		if (Comp && Comp->GetFName() == TargetComponentName)
		{
			TargetComponent = Comp;
			break;
		}
	}

	if (!TargetComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("UZeroPay_SyncPhysicsSubComponent: could not find a USkeletalMeshComponent named '%s' on actor '%s'."),
			*TargetComponentName.ToString(), *GetNameSafe(Owner));
	}

	return TargetComponent != nullptr;
}

void UZeroPay_SyncPhysicsSubComponent::StartSync()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!TargetComponent && !ResolveTargetComponent())
	{
		return;
	}

	if (bZeroVelocityOnStart)
	{
		for (FBodyInstance* Body : TargetComponent->Bodies)
		{
			if (Body && Body->IsValidBodyInstance() && Body->IsInstanceSimulatingPhysics())
			{
				Body->SetLinearVelocity(FVector::ZeroVector, false);
				Body->SetAngularVelocityInRadians(FVector::ZeroVector, false);
			}
		}
	}

	LastActorTransform = Owner->GetActorTransform();
	bIsSyncing = true;
	SetComponentTickEnabled(true);
}

void UZeroPay_SyncPhysicsSubComponent::StopSync()
{
	bIsSyncing = false;
	SetComponentTickEnabled(false);
}

void UZeroPay_SyncPhysicsSubComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsSyncing || !TargetComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FTransform CurrentActorTransform = Owner->GetActorTransform();

	// Actor hasn't moved this tick -- nothing to correct.
	if (!CurrentActorTransform.Equals(LastActorTransform, KINDA_SMALL_NUMBER))
	{
		for (FBodyInstance* Body : TargetComponent->Bodies)
		{
			if (!Body || !Body->IsValidBodyInstance() || !Body->IsInstanceSimulatingPhysics())
			{
				continue;
			}

			const FTransform OldBodyTransform = Body->GetUnrealWorldTransform();

			// Express the body's current world pose relative to where the actor
			// was last tick, then re-apply that same relative pose on top of the
			// actor's new transform -- i.e. "as if it had been rigidly attached
			// for this one frame's movement", without touching its pose relative
			// to the rest of the chain (so the belt keeps whatever sag/shape it
			// currently has).
			const FTransform BodyRelativeToOwner = OldBodyTransform.GetRelativeTransform(LastActorTransform);
			FTransform NewBodyTransform = BodyRelativeToOwner * CurrentActorTransform;

			if (!bSyncRotation)
			{
				NewBodyTransform.SetRotation(OldBodyTransform.GetRotation());
			}

			// TeleportPhysics moves the body directly without the physics engine
			// generating a velocity/collision response for the move itself -- this
			// is what prevents it getting caught/jammed inside the owner's own
			// colliders as it jumps to the new position.
			Body->SetBodyTransform(NewBodyTransform, ETeleportType::TeleportPhysics);
		}

		LastActorTransform = CurrentActorTransform;
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && !ReferenceBoneName.IsNone())
	{
		const FTransform BoneWorldTransform = TargetComponent->GetSocketTransform(ReferenceBoneName, RTS_World);
		DrawDebugSphere(GetWorld(), BoneWorldTransform.GetLocation(), 2.0f, 12, FColor::Green, false, -1.0f, 0, 0.5f);
	}
#endif
}
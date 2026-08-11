// ZeroPay_SyncPhysicsSubComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZeroPay_SyncPhysicsSubComponent.generated.h"

class USkeletalMeshComponent;

/**
 * UZeroPay_SyncPhysicsSubComponent
 *
 * Keeps a physically-simulated skeletal mesh (e.g. an ammo belt jointed to a
 * grippable actor via a PhysicsConstraintComponent) spatially in sync with its
 * owning actor when that actor is moved abruptly (teleported) by an external
 * system such as a VR grip -- without generating a physics/collision reaction
 * during the move.
 *
 * Call StartSync() when the owning actor begins being moved externally (e.g.
 * from your Grippable actor's OnGripped event), and StopSync() when it stops
 * (e.g. OnGripRelease).
 *
 * While syncing, every Tick this component measures how far the owner actor
 * moved since the last tick and re-applies that same delta directly to every
 * simulated physics body on the target skeletal mesh component. This preserves
 * their pose relative to each other (the belt keeps whatever sag/shape it
 * currently has) while preventing them being left behind and getting caught
 * inside the owner's own collision as it jumps to a new position.
 *
 * Usage:
 *   1. Add this component to your Grippable Static Mesh actor (the ammo box).
 *   2. Set TargetComponentName to the name of the simulated skeletal mesh
 *      component to keep synced (e.g. "GrippableAmmoBeltMesh").
 *   3. Optionally set ReferenceBoneName to the bone your PhysicsConstraintComponent
 *      anchors to (e.g. "ammo16_jnt") -- used only for the debug draw, not required
 *      for the sync itself.
 *   4. Wire StartSync() to your grip-begin event and StopSync() to your
 *      grip-release event.
 */
UCLASS(ClassGroup = (ZeroPay), meta = (BlueprintSpawnableComponent))
class ZEROPAYMOD_API UZeroPay_SyncPhysicsSubComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZeroPay_SyncPhysicsSubComponent();

	/** Name of the simulated skeletal mesh component on the owning actor to keep synced (e.g. the ammo belt). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|SyncPhysics")
	FName TargetComponentName;

	/**
	 * Optional bone name, purely for the debug draw (bDrawDebug). Not required for
	 * the sync itself -- every simulated body on the target component is synced
	 * regardless of this value. Useful to point at whatever bone your
	 * PhysicsConstraintComponent is jointed to, so you can visually confirm it's
	 * tracking correctly while syncing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|SyncPhysics")
	FName ReferenceBoneName;

	/** If true, rotation is corrected along with position each tick. If false, only position is corrected and each body keeps its own simulated rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|SyncPhysics")
	bool bSyncRotation = true;

	/** If true, zeroes each simulated body's linear/angular velocity the moment StartSync() is called, so any velocity left over from free simulation doesn't fight the first correction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|SyncPhysics")
	bool bZeroVelocityOnStart = true;

	/** If true, draws a debug sphere at ReferenceBoneName's current world location each tick while syncing (requires ReferenceBoneName to be set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay|SyncPhysics|Debug")
	bool bDrawDebug = false;

	/** Begin syncing the target component's physics bodies to the owning actor's movement. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|SyncPhysics")
	void StartSync();

	/** Stop syncing. The target component resumes simulating with no external correction. */
	UFUNCTION(BlueprintCallable, Category = "ZeroPay|SyncPhysics")
	void StopSync();

	/** Returns true if currently syncing. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|SyncPhysics")
	bool IsSyncing() const { return bIsSyncing; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Resolves TargetComponentName into TargetComponent by searching the owning actor's components. Returns true on success. */
	bool ResolveTargetComponent();

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> TargetComponent;

	bool bIsSyncing = false;

	FTransform LastActorTransform;
};
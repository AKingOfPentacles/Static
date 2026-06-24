#pragma once

#include "CoreMinimal.h"
#include "Actors/WardActorBase.h"
#include "SaltWardActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ─────────────────────────────────────────────────────────────────────────────
// ASaltWardActor
//
//   A salt line the Living pour across a doorway or corridor to block the Dead.
//   Works on two levels:
//
//   LAYER 1 — PHYSICAL BARRIER (blocks normal spectral movement)
//   A UBoxComponent using the "SaltBarrier" collision profile blocks the Dead
//   capsule when it is in the "DeadSpectral" profile (normal movement).
//   The Dead cannot walk through it without going through a nearby door.
//
//   LAYER 2 — ENERGY DRAIN (punishes door pass-through near the salt line)
//   When the Dead passes through a door next to this salt line, their capsule
//   temporarily becomes NoCollision, allowing them to enter the drain sphere.
//   While overlapping, BurnThroughDrainPerSecond is drained every Tick.
//   On exit, BreachDamageAmount reduces barrier integrity.
//
//   BARRIER INTEGRITY:
//   Each full transit (enter + exit the drain zone) reduces integrity by
//   BreachDamageAmount. At zero the barrier collapses — the box is disabled
//   but drain continues until the ward expires. This models scattered salt.
//
//   SHAPE:
//   Spawned at the HitResult surface point oriented to the surface normal.
//   USaltItem places it flat on the floor; the box extends up BarrierHeight cm.
//
//   COLLISION PROFILES REQUIRED (set up in Project Settings → Collision):
//   • "SaltBarrier" preset : blocks DeadMovement channel, ignores everything else
//   • "DeadSpectral" preset : Pawn type, blocks DeadMovement only
//   See Stage 1 of the editor setup guide for exact settings.
//
//   EDITOR SETUP:
//   1. Create BP_SaltWard from this class.
//   2. Assign a salt-line StaticMesh (flat white plane).
//   3. Set BarrierWidth to match your doorway width.
//   4. Assign to USaltItem.SaltWardClass in the InventoryComponent ItemClassMap.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ASaltWardActor : public AWardActorBase
{
    GENERATED_BODY()

public:
    ASaltWardActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * Width of the salt line (cm). Set to the widest doorway in your mansion.
     * The barrier box will be this wide, 10cm deep, and BarrierHeight tall.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt Ward",
        meta = (ClampMin = "50.0"))
    float BarrierWidth = 200.0f;

    /** Height of the blocking barrier (cm). Should cover a full doorway. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt Ward",
        meta = (ClampMin = "50.0"))
    float BarrierHeight = 220.0f;

    /**
     * Energy drained PER SECOND while the Dead is inside the overlap zone.
     * Higher than base DefenseStrength because this is a sustained cost,
     * not a one-time hit. Passing through quickly is cheaper than hovering.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt Ward",
        meta = (ClampMin = "0.0"))
    float BurnThroughDrainPerSecond = 25.0f;

    /**
     * How much barrier integrity is lost each time the Dead fully transits.
     * 100 / BreachDamageAmount = number of transits before the barrier breaks.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt Ward",
        meta = (ClampMin = "1.0", ClampMax = "100.0"))
    float BreachDamageAmount = 34.0f;  // Default: breaks after 3 transits.

    // ── State queries ─────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Salt Ward")
    bool IsBarrierIntact() const { return bBarrierIntact; }

    UFUNCTION(BlueprintPure, Category = "Salt Ward")
    float GetIntegrityNormalized() const
    {
        return FMath::Clamp(BarrierIntegrity / 100.0f, 0.0f, 1.0f);
    }

protected:
    // ── Components ────────────────────────────────────────────────────────────

    /**
     * The physical blocking volume using the "SaltBarrier" collision profile.
     * Stops Dead characters in DeadSpectral mode. Disabled when barrier breaks.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salt Ward|Components")
    UBoxComponent* BarrierBox;

    /**
     * Visible salt-line mesh. A thin plane or decal mesh lying on the floor.
     * Assign in Blueprint. Rotated/scaled to match BarrierWidth.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salt Ward|Components")
    UStaticMeshComponent* SaltMesh;

    // ── Ward base overrides ───────────────────────────────────────────────────

    // Called when Dead enters the drain overlap sphere.
    virtual void OnDeadEntered(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    // Called when Dead exits — this is where we record a breach.
    virtual void OnDeadExited(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex) override;

    // Disabled — salt uses continuous Tick drain, not single-hit.
    virtual void ApplyWardEffect(ADeadCharacter* DeadCharacter) override {}

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    /** 0–100. Shown as a crack visual on the barrier. At 0, barrier breaks. */
    UPROPERTY(ReplicatedUsing = OnRep_BarrierIntegrity)
    float BarrierIntegrity = 100.0f;

    /** False when integrity reaches zero. Disables the blocking box. */
    UPROPERTY(ReplicatedUsing = OnRep_BarrierIntact)
    bool bBarrierIntact = true;

    UFUNCTION()
    void OnRep_BarrierIntegrity();

    UFUNCTION()
    void OnRep_BarrierIntact();

    // ── Server-only tracking ──────────────────────────────────────────────────

    /** Dead characters currently inside the drain sphere — drained each tick. */
    TArray<TWeakObjectPtr<ADeadCharacter>> DeadInsideDrainZone;

    /**
     * Tracks Dead characters currently transiting via a nearby door.
     * When they EXIT the drain zone, we know they completed a breach
     * and apply BreachDamageAmount to barrier integrity.
     */
    TSet<TWeakObjectPtr<ADeadCharacter>> BreachingDead;

    // ── Helpers ───────────────────────────────────────────────────────────────

    void ApplyBarrierBreach(ADeadCharacter* Dead);
    void BreakBarrier();
    void UpdateBarrierCollision();

    /** Scale barrier box extents from the configurable width/height properties. */
    void RefreshBarrierSize();
};
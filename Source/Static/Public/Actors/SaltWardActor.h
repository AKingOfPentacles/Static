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
//   This is the most mechanically complex ward because it works on two levels:
//
//   LAYER 1 — PHYSICAL BARRIER (blocks normal spectral movement)
//   A UBoxComponent set to the "DeadBarrier" custom collision channel blocks
//   the Dead capsule when they are in normal Spectral mode. They simply cannot
//   walk through it without using Pass Through.
//
//   LAYER 2 — ENERGY DRAIN (punishes pass-through)
//   A wider USphereComponent (inherited from AWardActorBase as OverlapSphere)
//   detects when the Dead is inside the ward volume — whether they walked into
//   it normally (impossible without pass-through, so only relevant mid-transit)
//   or phased through it. While overlapping, energy is drained every tick at
//   BurnThroughDrainRate. This makes passing through salt COSTLY.
//
//   BARRIER INTEGRITY:
//   The barrier has an integrity value (0–100). Each time the Dead fully passes
//   through (enters AND exits the drain zone while in pass-through mode), integrity
//   drops by BreachDamagAmount. At zero integrity the salt line is broken —
//   the barrier component is disabled and the ward becomes a normal overlap-drain
//   only until it dissolves. This models salt being scattered by the ghost.
//
//   SHAPE:
//   Spawned at the HitResult surface point and oriented to the surface normal.
//   USaltItem places it flat on the floor; the box extends up ~120cm to block
//   a doorway. Width is configurable (default 200cm = most doorways).
//
//   CUSTOM COLLISION CHANNEL SETUP (required in editor before first compile):
//   1. Edit → Project Settings → Collision → New Trace Channel.
//   2. Name: "DeadMovement", Default Response: Block.
//   3. Open BP_DeadCharacter → Capsule → Collision → Custom…
//      Set DeadMovement to "Block" in Spectral mode, "Ignore" when pass-through.
//      (We handle this in ADeadCharacter via SetCapsuleCollisionForSalt().)
//   4. On BarrierBox below, set DeadMovement to Block, everything else Ignore.
//
//   WHY NOT USE AN OVERLAP + FORCE BACK?
//   Teleporting/pushing characters server-side causes jarring corrections on
//   clients due to movement prediction disagreement. A true blocking collision
//   prevents movement at the client prediction level — far smoother.
//
//   EDITOR SETUP:
//   1. Create BP_SaltWard from this class.
//   2. Assign a salt-line StaticMesh (a flat white line decal mesh or plane).
//   3. Set BarrierWidth to match the doorway you most commonly place it in.
//   4. Assign this to USaltItem.SaltWardClass.
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
     * The actual blocking volume. Uses the "DeadBarrier" custom collision channel.
     * This is what physically stops non-pass-through Dead characters.
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
     * Tracks which Dead characters entered the drain zone while in pass-through.
     * When they EXIT, we know they completed a breach and record the damage.
     */
    TSet<TWeakObjectPtr<ADeadCharacter>> BreachingDead;

    // ── Helpers ───────────────────────────────────────────────────────────────

    void ApplyBarrierBreach(ADeadCharacter* Dead);
    void BreakBarrier();
    void UpdateBarrierCollision();

    /** Scale barrier box extents from the configurable width/height properties. */
    void RefreshBarrierSize();
};
#pragma once

#include "CoreMinimal.h"
#include "Actors/WardActorBase.h"
#include "ChalkCircleWardActor.generated.h"

class ALivingCharacter;
// ─────────────────────────────────────────────────────────────────────────────
// AChalkCircleWardActor
//
//   A ritual chalk circle drawn on the floor by the Living.
//   Has two distinct roles:
//
//   ROLE 1 — WARD (same as any other ward)
//   When a Dead character enters the circle, they take an energy hit and are
//   briefly slowed. The base AWardActorBase handles this via ApplyWardEffect.
//   Unlike Salt it doesn't block — the Dead CAN enter, but it costs them.
//
//   ROLE 2 — SAFE ZONE for the Living inside it
//   Any Living character standing inside the chalk circle during Phase 3
//   receives continuous calm pulses, reducing their cardiac rhythm.
//   This makes the chalk circle a last-stand position — hunker inside
//   while trying to outlast the Dead until dawn.
//
//   ROLE 3 — RITUAL ANCHOR (Phase 3)
//   When the Bones are burned at the Burial Grounds, any Living player
//   standing inside a chalk circle at that moment receives an absolution
//   bonus — their Heart Pain count is reduced by one.
//   This rewards coordinated play: one player burns the Bones at the burial
//   site while the other holds a chalk circle.
//
//   SPEED PENALTY:
//   Dead characters inside the circle move at a reduced speed (SlowFactor).
//   Applied directly to UCharacterMovementComponent::MaxWalkSpeed.
//   
//
//   EDITOR SETUP:
//   1. Create BP_ChalkCircle from this class.
//   2. Assign a chalk-texture decal material to WardDecal.
//   3. Set OverlapRadius to 120cm (fits 1–2 players comfortably).
//   4. Assign to UChalkItem.ChalkCircleClass in the InventoryComponent ItemClassMap.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AChalkCircleWardActor : public AWardActorBase
{
    GENERATED_BODY()

public:
    AChalkCircleWardActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * Speed multiplier applied to Dead characters inside the circle.
     * 0.5 = half speed. Applied to MaxWalkSpeed on the movement component
     * and restored to the ALS default on exit.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chalk Circle",
        meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float SlowFactor = 0.5f;

    /** Cardiac rhythm calmed per second for Living players inside. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chalk Circle",
        meta = (ClampMin = "0.0"))
    float LivingCalmPerSecond = 10.0f;

    /** Whether this circle is currently active as a ritual anchor. */
    UFUNCTION(BlueprintPure, Category = "Chalk Circle")
    bool IsRitualAnchorActive() const { return bRitualAnchorActive; }

    /**
     * Called by ABurialGroundsActor when absolution succeeds.
     * Reduces Heart Pain count for Living players inside by 1.
     */
    UFUNCTION(BlueprintCallable, Category = "Chalk Circle")
    void OnAbsolutionOccurred();

protected:
    // ── Ward overrides ────────────────────────────────────────────────────────

    virtual void OnDeadEntered(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    virtual void OnDeadExited(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex) override;

    // Base ApplyWardEffect fires on Dead entry — we keep it for the energy hit.
    // Override not needed; base class handles it.

private:
    // ── Replicated ────────────────────────────────────────────────────────────

    UPROPERTY(Replicated)
    bool bRitualAnchorActive = false;

    // ── Server-only tracking ──────────────────────────────────────────────────

    TArray<TWeakObjectPtr<ADeadCharacter>>   DeadInside;
    TArray<TWeakObjectPtr<ALivingCharacter>> LivingInside;

    FTimerHandle ZoneTickHandle;

    UFUNCTION()
    void OnZoneTick();

    void OnLivingEntered(class ALivingCharacter* Living);
    void OnLivingExited(class ALivingCharacter* Living);

    void ApplySlowToDead(ADeadCharacter* Dead, bool bSlow);
};
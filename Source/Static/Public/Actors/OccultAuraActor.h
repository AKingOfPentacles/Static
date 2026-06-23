#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OccultAuraActor.generated.h"

class USphereComponent;
class ADeadCharacter;
class ALivingCharacter;

// ─────────────────────────────────────────────────────────────────────────────
// AOccultAuraActor
//
//   Spawned and attached to a Living character when they equip the Occult Item.
//   Destroyed when they unequip it, drop it, or flee.
//
//   WHAT IT DOES EACH SECOND (server tick timer):
//   1. Finds all ADeadCharacter actors within OccultRadius.
//   2. Calls SpecterEnergyComponent::ApplyDefenseHit(DrainPerSecond) on each.
//   3. Calls CardiacRhythmComponent::ApplyCalm(CalmPerSecond) on the carrier.
//
//   MOVEMENT PENALTY:
//   While active, the carrier's MaxWalkSpeed is reduced by SpeedPenaltyFactor.
//   Restored when the aura is destroyed.
//
//   VISUAL:
//   The sphere component has no collision — it's just a trigger for the tick logic.
//   Assign a soft glowing particle system in Blueprint for the held-item VFX.
//   The aura is visible to the Dead (not hidden) — they can see the Living
//   is carrying protection and should avoid them or pay the drain cost.
//
//   EDITOR SETUP:
//   1. Create BP_OccultAura from this class.
//   2. Assign a faint glow particle system to AuraParticles.
//   3. Tune OccultRadius, DrainPerSecond, CalmPerSecond.
//   4. Assign to UOccultItem.OccultAuraClass in the InventoryComponent.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AOccultAuraActor : public AActor
{
    GENERATED_BODY()

public:
    AOccultAuraActor();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Attach to a Living character and activate the aura.
     * Called by UOccultItem::Equip() immediately after spawn.
     */
    UFUNCTION(BlueprintCallable, Category = "Occult Aura")
    void ActivateOn(ALivingCharacter* Carrier);

    /** Manually deactivate — called by UOccultItem::Unequip(). */
    UFUNCTION(BlueprintCallable, Category = "Occult Aura")
    void Deactivate();

    // ── Configuration ─────────────────────────────────────────────────────────

    /** Radius within which Dead characters are drained (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occult Aura",
        meta = (ClampMin = "50.0"))
    float OccultRadius = 250.0f;

    /** Energy drained from each Dead character per second. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occult Aura",
        meta = (ClampMin = "0.0"))
    float DrainPerSecond = 6.0f;

    /** Cardiac rhythm calmed per second on the carrier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occult Aura",
        meta = (ClampMin = "0.0"))
    float CalmPerSecond = 8.0f;

    /**
     * The carrier's walk speed is multiplied by this while the aura is active.
     * Holding a protective talisman aloft is cumbersome.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occult Aura",
        meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float SpeedPenaltyFactor = 0.8f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Occult Aura|Components")
    USphereComponent* AuraSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Occult Aura|Components")
    UParticleSystemComponent* AuraParticles;

private:
    UPROPERTY()
    ALivingCharacter* AuraCarrier = nullptr;

    float OriginalWalkSpeed = 0.0f;
    bool bActive = false;

    FTimerHandle AuraTickHandle;

    UFUNCTION()
    void OnAuraTick();

    void ApplySpeedPenalty(bool bApply);
};
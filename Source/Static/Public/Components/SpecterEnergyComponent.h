#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpecterEnergyComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Delegates
// ─────────────────────────────────────────────────────────────────────────────

/** Fired when energy reaches zero — triggers respawn at burial grounds. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpecterDepleted);

/** Fired when energy is restored after a depletion penalty expires. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpecterRestored);

/** Broadcast to owning client whenever energy changes (for HUD). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnergyChanged, float, NewNormalizedValue);

// ─────────────────────────────────────────────────────────────────────────────
// USpecterEnergyComponent
//
//   Attach to ADeadCharacter.
//
//   HOW IT WORKS:
//   • Dead players spend energy using abilities (Whisper, Move Object, etc.).
//   • Energy is gained during Phase 1 by collecting Memorabilia objects.
//   • If the Dead enters a Living defense (Salt line, Sage zone, Occult item),
//     they lose energy via ApplyDefenseHit(). 
//   • At zero energy: respawn penalty kicks in (DepletionPenaltyDuration seconds
//     during which energy is capped at PenaltyEnergyCap).
//   • After the penalty, energy regenerates passively at PassiveRegenRate.
//
//   EDITOR SETUP:
//   1. Open ADeadCharacter Blueprint → Add Component → SpecterEnergyComponent.
//   2. Tune MaxEnergy, PassiveRegenRate, DepletionPenaltyDuration in Details.
//   3. Bind OnSpecterDepleted to your respawn logic (teleport to burial grounds).
//   4. Bind OnEnergyChanged to your HUD energy bar widget.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(StaticMansion), meta=(BlueprintSpawnableComponent))
class STATIC_API USpecterEnergyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USpecterEnergyComponent();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ── Replication ───────────────────────────────────────────────────────────
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Public API (server-side) ──────────────────────────────────────────────

    /**
     * Spend energy on an ability. Returns true if the cost was paid.
     * Returns false (and does NOT deduct) if energy is insufficient.
     * Always check the return value before allowing the ability to fire.
     */
    UFUNCTION(BlueprintCallable, Category = "Specter")
    bool TrySpendEnergy(float Cost);

    /**
     * Gain energy (from collecting Memorabilia, passive regen, etc.).
     * Clamped to MaxEnergy (or PenaltyEnergyCap when in penalty).
     */
    UFUNCTION(BlueprintCallable, Category = "Specter")
    void GainEnergy(float Amount);

    /**
     * Called when the Dead is hit by a Living defense (salt, sage, occult).
     * DefenseStrength: designer-controlled drain amount per defense type.
     */
    UFUNCTION(BlueprintCallable, Category = "Specter")
    void ApplyDefenseHit(float DefenseStrength);

    /** Returns true if the specter currently has enough energy for Cost. */
    UFUNCTION(BlueprintPure, Category = "Specter")
    bool HasEnoughEnergy(float Cost) const;

    /** Returns energy as 0.0–1.0 normalized. */
    UFUNCTION(BlueprintPure, Category = "Specter")
    float GetNormalizedEnergy() const;

    /** Is the specter currently in the depletion penalty window? */
    UFUNCTION(BlueprintPure, Category = "Specter")
    bool IsInPenalty() const { return bInDepletionPenalty; }

    // ── Tuning ────────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter|Tuning",
        meta = (ClampMin = "1.0"))
    float MaxEnergy = 100.0f;

    /** Passive energy gain per second (active outside of penalty). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter|Tuning",
        meta = (ClampMin = "0.0"))
    float PassiveRegenRate = 3.0f;

    /** How long (seconds) the specter is penalized after depletion. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter|Tuning",
        meta = (ClampMin = "0.0"))
    float DepletionPenaltyDuration = 15.0f;

    /**
     * Energy cap during the penalty window.
     * Keeps the specter weak after a respawn — prevents immediately attacking.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter|Tuning",
        meta = (ClampMin = "0.0"))
    float PenaltyEnergyCap = 20.0f;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Specter")
    FOnSpecterDepleted OnSpecterDepleted;

    UPROPERTY(BlueprintAssignable, Category = "Specter")
    FOnSpecterRestored OnSpecterRestored;

    UPROPERTY(BlueprintAssignable, Category = "Specter")
    FOnEnergyChanged OnEnergyChanged;

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_CurrentEnergy)
    float CurrentEnergy = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_Penalty)
    bool bInDepletionPenalty = false;

    // ── Server-only ───────────────────────────────────────────────────────────

    FTimerHandle PenaltyTimerHandle;

    // ── RepNotify callbacks ───────────────────────────────────────────────────

    UFUNCTION()
    void OnRep_CurrentEnergy();

    UFUNCTION()
    void OnRep_Penalty();

    // ── Internals ─────────────────────────────────────────────────────────────

    void SetEnergy(float NewValue);
    void TriggerDepletion();
    void EndPenalty();

    float GetCurrentEnergyCap() const;

    UFUNCTION(Client, Reliable)
    void Client_NotifyEnergyChanged(float NormalizedValue);
};
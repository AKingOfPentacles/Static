#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DeadPlayerState.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// ADeadPlayerState
//
//   Tracks Dead-side per-player data: memorabilia collected, times depleted,
//   and which abilities have been unlocked by the current phase.
//
//   Memorabilia count matters for Phase 1 progression and end-of-match scoring.
//   AbilityUnlockFlags is a bitmask — we'll expand it when we build the
//   Dead ability system. For now it's wired up so phase transitions can
//   immediately set flags without needing the full ability system.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ADeadPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ADeadPlayerState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Memorabilia ───────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Dead")
    int32 GetMemorabiliaCount() const { return MemorabiliaCollected; }

    /** Called by the Memorabilia pickup actor when collected. */
    UFUNCTION(BlueprintCallable, Category = "Dead")
    void AddMemorabiliaCollected(int32 Amount = 1);

    // ── Depletion tracking ────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Dead")
    int32 GetDepletionCount() const { return DepletionCount; }

    /** Called by SpecterEnergyComponent when energy hits zero. */
    UFUNCTION(BlueprintCallable, Category = "Dead")
    void IncrementDepletionCount();

    // ── Ability unlock flags ──────────────────────────────────────────────────
    //   Bitmask: bit 0 = Whisper, bit 1 = Shiver, bit 2 = Spook,
    //            bit 3 = Manifestation, bit 4 = Full Manifestation
    //   Set by the phase system via GameMode when OnPhaseChanged fires.

    UFUNCTION(BlueprintPure, Category = "Dead")
    int32 GetAbilityUnlockFlags() const { return AbilityUnlockFlags; }

    UFUNCTION(BlueprintCallable, Category = "Dead")
    void SetAbilityUnlockFlags(int32 Flags);

    UFUNCTION(BlueprintPure, Category = "Dead")
    bool IsAbilityUnlocked(int32 BitIndex) const
    {
        return (AbilityUnlockFlags & (1 << BitIndex)) != 0;
    }

private:
    UPROPERTY(Replicated)
    int32 MemorabiliaCollected = 0;

    UPROPERTY(Replicated)
    int32 DepletionCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_AbilityUnlockFlags)
    int32 AbilityUnlockFlags = 0;

    UFUNCTION()
    void OnRep_AbilityUnlockFlags();
};
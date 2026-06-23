#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "LivingPlayerState.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// ALivingPlayerState
//
//   Stores per-player data that needs to be visible to the server AND all
//   clients — things like the player's display name, their heart pain count,
//   and whether they have fled.
//
//   WHY PlayerState and not just the Character?
//   Characters are destroyed on death/respawn; PlayerState persists for the
//   entire session. Spectators and the GameMode can always read it.
//
//   EDITOR SETUP:
//   1. Open your GameMode Blueprint (or C++ GameMode class).
//   2. Set "Player State Class" to BP_LivingPlayerState (or this class directly
//      if you don't need a Blueprint child).
//   3. This only applies to Living players — Dead players use ADeadPlayerState.
//      In your GameMode, assign the correct PlayerState class per team role.
//      (Step: GameMode will handle this routing.)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ALivingPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    ALivingPlayerState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Heart Pain tracking (mirrors CardiacRhythmComponent, but survives respawn) ──

    /** Current number of Heart Pain events this session. Read-only externally. */
    UFUNCTION(BlueprintPure, Category = "Living")
    int32 GetHeartPainCount() const { return HeartPainCount; }

    /** Called by CardiacRhythmComponent when a Heart Pain event fires. */
    UFUNCTION(BlueprintCallable, Category = "Living")
    void IncrementHeartPainCount();

    /** Has this player fled (exceeded Heart Pain limit)? */
    UFUNCTION(BlueprintPure, Category = "Living")
    bool HasFled() const { return bHasFled; }

    /** Called by CardiacRhythmComponent's ForceFlee path. */
    UFUNCTION(BlueprintCallable, Category = "Living")
    void SetFled();

    // ── Absolution tracking ───────────────────────────────────────────────────

    /** Has this player participated in the bone-burning ritual? */
    UFUNCTION(BlueprintPure, Category = "Living")
    bool HasAbsolved() const { return bHasAbsolved; }

    UFUNCTION(BlueprintCallable, Category = "Living")
    void SetAbsolved();

private:
    UPROPERTY(Replicated)
    int32 HeartPainCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Fled)
    bool bHasFled = false;

    UPROPERTY(Replicated)
    bool bHasAbsolved = false;

    UFUNCTION()
    void OnRep_Fled();
};
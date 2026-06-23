#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePhaseManager.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EGamePhase
//   The three phases of a Static Mansion night.
//   IMPORTANT: Keep the numeric values stable — they are replicated as bytes.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
    None            UMETA(DisplayName = "None"),
    Exploration     UMETA(DisplayName = "Exploration (6PM–Midnight)"),
    Protection      UMETA(DisplayName = "Protection (Midnight)"),
    Confrontation   UMETA(DisplayName = "Confrontation (Before Dawn)"),
    GameOver        UMETA(DisplayName = "Game Over")
};

// ─────────────────────────────────────────────────────────────────────────────
// FOnPhaseChangedDelegate
//   Multicast delegate broadcast whenever the phase changes.
//   Any system (components, actors, widgets) can bind to this.
//
//   Signature: void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase)
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnPhaseChangedDelegate,
    EGamePhase, NewPhase,
    EGamePhase, OldPhase
);

// ─────────────────────────────────────────────────────────────────────────────
// UGamePhaseManager
//   Lives as a GameInstance subsystem so it persists across level loads.
//   The server drives transitions via timers; clients learn about phase changes
//   through the replicated GameState (see AStaticMansionGameState, Step 5).
//
//   EDITOR SETUP (do this once):
//   1. Open Project Settings → Maps & Modes.
//   2. Set Game Instance Class to your custom UGameInstance (or leave default
//      — subsystems attach automatically to whatever GameInstance is in use).
//   3. Phase durations below are set in code; you can expose them as
//      UPROPERTY(EditDefaultsOnly) on a DataAsset later for designer tuning.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API UGamePhaseManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Subsystem lifecycle ──────────────────────────────────────────────────

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Phase control (call only on the server) ──────────────────────────────

    /** Start the night. Call this from GameMode::BeginPlay after all players connect. */
    UFUNCTION(BlueprintCallable, Category = "Phase")
    void StartNight();

    /** Manually force a phase (useful for testing; skip in shipping builds). */
    UFUNCTION(BlueprintCallable, Category = "Phase")
    void ForcePhase(EGamePhase TargetPhase);

    /** Called by the Confrontation phase when the Living absolve the spirits. */
    UFUNCTION(BlueprintCallable, Category = "Phase")
    void TriggerLivingVictory();

    /** Called at dawn if absolution has not occurred. */
    UFUNCTION(BlueprintCallable, Category = "Phase")
    void TriggerDeadVictory();

    // ── Queries ──────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Phase")
    EGamePhase GetCurrentPhase() const { return CurrentPhase; }

    /** Seconds remaining in the current phase. Returns -1 if no timer is running. */
    UFUNCTION(BlueprintPure, Category = "Phase")
    float GetPhaseTimeRemaining() const;

    // ── Events ───────────────────────────────────────────────────────────────

    /** Bind to this from any actor or component to react to phase changes. */
    UPROPERTY(BlueprintAssignable, Category = "Phase")
    FOnPhaseChangedDelegate OnPhaseChanged;

    // ── Phase durations (seconds) — tune these for your playtest ─────────────
    //   Exploration  : 6 PM → Midnight  (real minutes, not in-game hours)
    //   Protection   : Midnight → ~3 AM
    //   Confrontation: ~3 AM → Dawn
    //
    //   EDITOR TIP: Right-click these in Blueprint details to "Expose on Spawn"
    //   or move to a DataAsset (recommended for production).

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase|Durations",
        meta = (ClampMin = "10.0"))
    float ExplorationDuration = 10.0f;     // 6 minutes

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase|Durations",
        meta = (ClampMin = "10.0"))
    float ProtectionDuration = 240.0f;      // 4 minutes

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase|Durations",
        meta = (ClampMin = "10.0"))
    float ConfrontationDuration = 180.0f;   // 3 minutes

private:
    // ── Internal state ───────────────────────────────────────────────────────

    EGamePhase CurrentPhase = EGamePhase::None;

    /** The UWorld-scoped timer that drives phase transitions. */
    FTimerHandle PhaseTimerHandle;

    // ── Internal helpers ─────────────────────────────────────────────────────

    void SetPhase(EGamePhase NewPhase);

    /** Timer callbacks — one per transition. */
    void OnExplorationEnd();
    void OnProtectionEnd();
    void OnConfrontationEnd();

    /** Propagates the new phase to the replicated GameState so all clients know. */
    void SyncPhaseToGameState(EGamePhase NewPhase);
};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Systems/GamePhaseManager.h"   // EGamePhase
#include "StaticMansionGameState.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EMatchResult
//   Set once when the match ends. Replicated so all clients can show the
//   correct end-screen without an extra RPC.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EMatchResult : uint8
{
    InProgress  UMETA(DisplayName = "In Progress"),
    LivingWin   UMETA(DisplayName = "Living Win — Absolution"),
    DeadWin     UMETA(DisplayName = "Dead Win — Dawn Without Absolution")
};

// ─────────────────────────────────────────────────────────────────────────────
// Delegates — broadcast on clients when replicated values change
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClientPhaseChanged,
    EGamePhase, NewPhase, EGamePhase, OldPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchResultChanged,
    EMatchResult, Result);

// ─────────────────────────────────────────────────────────────────────────────
// AStaticMansionGameState
//
//   Replicated to every connected client automatically by Unreal.
//   This is the single source of truth for global match state on clients
//   (phase, result, time remaining). The server writes it; clients read it.
//
//   WHY BOTH GamePhaseManager AND GameState?
//   UGamePhaseManager is a GameInstanceSubsystem — it lives only on the server
//   and drives the actual timer logic. GameState is replicated, so it is the
//   channel through which clients learn what phase they're in.
//   Think of GamePhaseManager as the engine and GameState as the dashboard.
//
//   EDITOR SETUP:
//   1. Open your GameMode Blueprint (or AStaticMansionGameMode in C++).
//   2. Set "Game State Class" to BP_StaticMansionGameState (or this class).
//   3. That's it — Unreal spawns and replicates it automatically.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AStaticMansionGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AStaticMansionGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Setters (server only — called by GamePhaseManager / GameMode) ─────────

    /** Called by UGamePhaseManager::SyncPhaseToGameState. */
    void SetCurrentPhase(EGamePhase NewPhase);

    /** Called by AStaticMansionGameMode when the match ends. */
    void SetMatchResult(EMatchResult NewResult);

    /**
     * Updated every second by the GameMode tick so clients can show a
     * countdown timer without needing to know when each phase started.
     */
    void SetPhaseTimeRemaining(float Seconds);

    // ── Getters (safe on any machine) ────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "GameState")
    EGamePhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "GameState")
    EMatchResult GetMatchResult() const { return MatchResult; }

    UFUNCTION(BlueprintPure, Category = "GameState")
    float GetPhaseTimeRemaining() const { return PhaseTimeRemaining; }

    UFUNCTION(BlueprintPure, Category = "GameState")
    bool IsMatchOver() const { return MatchResult != EMatchResult::InProgress; }

    // ── Client-side events (bind in HUD widgets) ──────────────────────────────

    /** Fires on ALL clients when the phase changes. Use for HUD transitions. */
    UPROPERTY(BlueprintAssignable, Category = "GameState")
    FOnClientPhaseChanged OnClientPhaseChanged;

    /** Fires on ALL clients when the match ends. Use for end-screen UI. */
    UPROPERTY(BlueprintAssignable, Category = "GameState")
    FOnMatchResultChanged OnMatchResultChanged;

private:
    // ── Replicated properties ─────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
    EGamePhase CurrentPhase = EGamePhase::None;

    UPROPERTY(ReplicatedUsing = OnRep_MatchResult)
    EMatchResult MatchResult = EMatchResult::InProgress;

    // Replicated with no RepNotify — clients just read it for the HUD display.
    // No event needed since the HUD polls this each frame anyway.
    UPROPERTY(Replicated)
    float PhaseTimeRemaining = 0.0f;

    // ── RepNotify callbacks ───────────────────────────────────────────────────

    UFUNCTION()
    void OnRep_CurrentPhase();

    UFUNCTION()
    void OnRep_MatchResult();

    // Tracks the previous phase so the delegate can include OldPhase.
    EGamePhase PreviousPhase = EGamePhase::None;
};
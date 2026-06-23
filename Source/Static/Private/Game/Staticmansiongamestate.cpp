#include "Game/StaticMansionGameState.h"
#include "Net/UnrealNetwork.h"

AStaticMansionGameState::AStaticMansionGameState()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // All three replicate to every connected client — no conditions needed.
    DOREPLIFETIME(AStaticMansionGameState, CurrentPhase);
    DOREPLIFETIME(AStaticMansionGameState, MatchResult);
    DOREPLIFETIME(AStaticMansionGameState, PhaseTimeRemaining);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetCurrentPhase — called by UGamePhaseManager::SyncPhaseToGameState
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameState::SetCurrentPhase(EGamePhase NewPhase)
{
    // Store old phase so the RepNotify can include it in the delegate.
    PreviousPhase = CurrentPhase;
    CurrentPhase  = NewPhase;

    // On the server, fire the delegate directly since OnRep_ won't run here.
    OnClientPhaseChanged.Broadcast(CurrentPhase, PreviousPhase);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetMatchResult
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameState::SetMatchResult(EMatchResult NewResult)
{
    MatchResult = NewResult;

    // Fire on the server immediately.
    OnMatchResultChanged.Broadcast(MatchResult);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetPhaseTimeRemaining — polled from GameMode Tick
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameState::SetPhaseTimeRemaining(float Seconds)
{
    PhaseTimeRemaining = Seconds;
    // No delegate here — the HUD widget reads this every frame via GetPhaseTimeRemaining().
}

// ─────────────────────────────────────────────────────────────────────────────
// RepNotify callbacks — run on CLIENTS when the server replicates new values
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameState::OnRep_CurrentPhase()
{
    // PreviousPhase is server-only state, so on clients it will always be None
    // the first time. That's acceptable — clients only need the new value for
    // their own phase reactions. The OldPhase parameter is a convenience for
    // Blueprint logic like "was I in Exploration and now I'm in Protection?"
    OnClientPhaseChanged.Broadcast(CurrentPhase, PreviousPhase);
    PreviousPhase = CurrentPhase;
}

void AStaticMansionGameState::OnRep_MatchResult()
{
    OnMatchResultChanged.Broadcast(MatchResult);
}
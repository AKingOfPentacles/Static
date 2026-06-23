#include "Systems/GamePhaseManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

// Forward-declared here; we'll implement this class in a later step.
// For now this include just needs to resolve. If you haven't created
// StaticMansionGameState yet, comment out the include and SyncPhaseToGameState body.
// #include "GameState/StaticMansionGameState.h"

// ─────────────────────────────────────────────────────────────────────────────
// Initialize / Deinitialize
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Nothing to do yet — we wait for StartNight() to be called by the GameMode.
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Initialized. Waiting for StartNight()."));
}

void UGamePhaseManager::Deinitialize()
{
    // Always clear timers before the subsystem goes away to avoid dangling handles.
    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTimerHandle);
    }

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// StartNight
//   Entry point called from AStaticMansionGameMode once players are loaded.
//   SERVER ONLY — wrap the call site in HasAuthority() checks in GameMode.
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::StartNight()
{
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Night begins. Starting Exploration phase."));
    SetPhase(EGamePhase::Exploration);
}

// ─────────────────────────────────────────────────────────────────────────────
// ForcePhase
//   Dev/QA helper. In a shipping build you'd strip this with a preprocessor guard.
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::ForcePhase(EGamePhase TargetPhase)
{
    UE_LOG(LogTemp, Warning, TEXT("[PhaseManager] ForcePhase called → %d"), (int32)TargetPhase);

    // Clear any running timer so we don't get an unwanted transition on top of the forced one.
    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTimerHandle);
    }

    SetPhase(TargetPhase);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetPhase — internal
//   This is the single place where CurrentPhase changes.
//   After updating state, it:
//   1. Broadcasts OnPhaseChanged so every subscribed system reacts.
//   2. Syncs the new phase to the replicated GameState.
//   3. Arms the next timer if applicable.
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::SetPhase(EGamePhase NewPhase)
{
    const EGamePhase OldPhase = CurrentPhase;
    CurrentPhase = NewPhase;

    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Phase changed: %d → %d"), (int32)OldPhase, (int32)NewPhase);

    // 1. Broadcast to all listeners (components, actors, UI).
    //    We broadcast BEFORE arming the timer so listeners can set up during the phase,
    //    not after the next transition has already fired.
    OnPhaseChanged.Broadcast(NewPhase, OldPhase);

    // 2. Propagate to the replicated GameState.
    SyncPhaseToGameState(NewPhase);

    // 3. Arm the appropriate next timer.
    UWorld* World = GetGameInstance()->GetWorld();
    if (!World) return;

    FTimerManager& TM = World->GetTimerManager();
    TM.ClearTimer(PhaseTimerHandle); // Clear any previous timer first.

    switch (NewPhase)
    {
        case EGamePhase::Exploration:
            TM.SetTimer(PhaseTimerHandle, this,
                &UGamePhaseManager::OnExplorationEnd,
                ExplorationDuration, false);
            break;

        case EGamePhase::Protection:
            TM.SetTimer(PhaseTimerHandle, this,
                &UGamePhaseManager::OnProtectionEnd,
                ProtectionDuration, false);
            break;

        case EGamePhase::Confrontation:
            TM.SetTimer(PhaseTimerHandle, this,
                &UGamePhaseManager::OnConfrontationEnd,
                ConfrontationDuration, false);
            break;

        default:
            // GameOver / None — no timer needed.
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase timer callbacks
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::OnExplorationEnd()
{
    // Midnight: lights out, protection phase begins.
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Exploration ended — Midnight strikes."));
    SetPhase(EGamePhase::Protection);
}

void UGamePhaseManager::OnProtectionEnd()
{
    // Before dawn: confrontation window opens.
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Protection ended — Confrontation begins."));
    SetPhase(EGamePhase::Confrontation);
}

void UGamePhaseManager::OnConfrontationEnd()
{
    // Dawn arrives with no absolution — Dead win.
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Dawn arrived with no absolution. Dead win."));
    TriggerDeadVictory();
}

// ─────────────────────────────────────────────────────────────────────────────
// Victory helpers
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::TriggerLivingVictory()
{
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] Living have absolved the spirits — Living win!"));

    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTimerHandle);
    }

    SetPhase(EGamePhase::GameOver);

    // TODO (Step 5): Tell GameMode to end the match with Living victory result.
}

void UGamePhaseManager::TriggerDeadVictory()
{
    UE_LOG(LogTemp, Log, TEXT("[PhaseManager] The Dead have won the night."));

    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        World->GetTimerManager().ClearTimer(PhaseTimerHandle);
    }

    SetPhase(EGamePhase::GameOver);

    // TODO (Step 5): Tell GameMode to end the match with Dead victory result.
}

// ─────────────────────────────────────────────────────────────────────────────
// GetPhaseTimeRemaining
// ─────────────────────────────────────────────────────────────────────────────

float UGamePhaseManager::GetPhaseTimeRemaining() const
{
    if (UWorld* World = GetGameInstance()->GetWorld())
    {
        const float Remaining = World->GetTimerManager().GetTimerRemaining(PhaseTimerHandle);
        return Remaining; // Returns -1.f if no timer is active, which is correct behavior.
    }
    return -1.f;
}

// ─────────────────────────────────────────────────────────────────────────────
// SyncPhaseToGameState
//   Writes the phase enum into the replicated GameState so all clients
//   automatically receive it without a dedicated RPC.
//   We stub this now and fill it in when AStaticMansionGameState is created.
// ─────────────────────────────────────────────────────────────────────────────

void UGamePhaseManager::SyncPhaseToGameState(EGamePhase NewPhase)
{
    // We cast to our custom GameState which holds a replicated CurrentPhase field.
    // This will compile once StaticMansionGameState exists (Step 5).
    // For now leave this as a log so the rest of the code compiles.
    UE_LOG(LogTemp, Verbose, TEXT("[PhaseManager] SyncPhaseToGameState: phase=%d (GameState sync pending Step 5)"), (int32)NewPhase);

    /*  ── UNCOMMENT IN STEP 5 ──────────────────────────────────────────────
    UWorld* World = GetGameInstance()->GetWorld();
    if (!World) return;

    AStaticMansionGameState* GS = World->GetGameState<AStaticMansionGameState>();
    if (GS)
    {
        GS->SetCurrentPhase(NewPhase);
    }
    ─────────────────────────────────────────────────────────────────────── */
}
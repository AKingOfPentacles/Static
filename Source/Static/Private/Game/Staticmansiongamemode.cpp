#include "Game/StaticMansionGameMode.h"
#include "Game/StaticMansionGameState.h"
#include "Systems/GamePhaseManager.h"
#include "Characters/LivingCharacter.h"
#include "Characters/DeadCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "Player/LivingPlayerState.h"
#include "Player/DeadPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/PlayerStartPIE.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AStaticMansionGameMode::AStaticMansionGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // Set the replicated GameState class.
    GameStateClass = AStaticMansionGameState::StaticClass();

    // Default pawn is None — we assign manually in PostLogin.
    DefaultPawnClass = nullptr;

    // Living PlayerState is the default; Dead players get theirs swapped in
    // SpawnPawnForController via the controller's PlayerState assignment.
    PlayerStateClass = ALivingPlayerState::StaticClass();
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Subscribe to phase manager victory events so the GameMode reacts
    // when GamePhaseManager calls TriggerLivingVictory / TriggerDeadVictory.
    if (UGamePhaseManager* PM = GetPhaseManager())
    {
        PM->OnPhaseChanged.AddDynamic(this,
            &AStaticMansionGameMode::HandlePhaseChanged_Internal);
    }

    UE_LOG(LogTemp, Log, TEXT("[GameMode] BeginPlay. Waiting for players..."));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — drives pre-night countdown and time-sync to GameState
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bMatchOver) return;

    // ── Pre-night countdown ────────────────────────────────────────────────
    if (PreNightTimer >= 0.0f && !bNightStarted)
    {
        PreNightTimer -= DeltaTime;
        if (PreNightTimer <= 0.0f)
        {
            StartNight();
        }
        return; // Don't do time-sync until night is running.
    }

    // ── Sync phase time remaining to GameState (once per second) ──────────
    if (bNightStarted)
    {
        TimeSyncAccumulator += DeltaTime;
        if (TimeSyncAccumulator >= 1.0f)
        {
            TimeSyncAccumulator = 0.0f;
            if (AStaticMansionGameState* GS = GetMansionGameState())
            {
                if (UGamePhaseManager* PM = GetPhaseManager())
                {
                    GS->SetPhaseTimeRemaining(PM->GetPhaseTimeRemaining());
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PostLogin — called when a player controller is fully ready
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::PostLogin(APlayerController* NewPlayer)
{
    // We intentionally do NOT call Super::PostLogin here in full.
    // Super::PostLogin calls RestartPlayer() which would spawn a DefaultPawn
    // before we get a chance to spawn the correct team pawn.
    // We manually call only what we need from the Super chain.
 
    // This is the minimum needed from AGameModeBase::PostLogin:
    // — registers the player in the game session
    // — calls GameSession->RegisterPlayer
    // We replicate that here without triggering the pawn spawn.
    if (GameSession)
    {
        FUniqueNetIdRepl UniqueId = FUniqueNetIdRepl();
        
        if  (NewPlayer->PlayerState)        
        {
            UniqueId = NewPlayer->PlayerState->GetUniqueId();
        }
        
        GameSession->RegisterPlayer(NewPlayer, UniqueId, false);
        // GameSession->RegisterPlayer(NewPlayer, NewPlayer->PlayerState
        //     ? NewPlayer->PlayerState->GetUniqueId().GetUniqueNetId()
        //     : nullptr, false);
    }
 
    // Trigger our team assignment and pawn spawn.
    if (!NewPlayer) return;
 
    const EPlayerTeam Team = AssignTeam(NewPlayer);
    if (Team == EPlayerTeam::Unassigned)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GameMode] PostLogin: match is full. Rejecting player."));
        return;
    }
 
    SpawnPawnForController(NewPlayer, Team);
 
    UE_LOG(LogTemp, Log, TEXT("[GameMode] Player '%s' joined as %s. Living: %d, Dead: %d"),
        *NewPlayer->GetName(),
        Team == EPlayerTeam::Living ? TEXT("Living") : TEXT("Dead"),
        LivingControllers.Num(),
        DeadControllers.Num());
 
    CheckAllPlayersReady();
}

// ─────────────────────────────────────────────────────────────────────────────
// Logout — clean up when a player drops
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        const EPlayerTeam Team = GetTeamForController(PC);
        LivingControllers.Remove(PC);
        DeadControllers.Remove(PC);
        TeamMap.Remove(PC);

        UE_LOG(LogTemp, Log, TEXT("[GameMode] Player '%s' (team %d) disconnected."),
            *PC->GetName(), (int32)Team);

        // If all Living have left mid-game, Dead win by default.
        if (bNightStarted && !bMatchOver && LivingControllers.Num() == 0)
        {
            HandleDeadVictory();
        }
    }

    Super::Logout(Exiting);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetDefaultPawnClassForController_Implementation
//   Unreal calls this during RestartPlayer to know what pawn to spawn.
//   We return the correct class based on team assignment.
// ─────────────────────────────────────────────────────────────────────────────

UClass* AStaticMansionGameMode::GetDefaultPawnClassForController_Implementation(
    AController* InController)
{
    if (APlayerController* PC = Cast<APlayerController>(InController))
    {
        const EPlayerTeam Team = GetTeamForController(PC);
        if (Team == EPlayerTeam::Living && LivingCharacterClass)
            return LivingCharacterClass;
        if (Team == EPlayerTeam::Dead   && DeadCharacterClass)
            return DeadCharacterClass;
    }
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// ─────────────────────────────────────────────────────────────────────────────
// AssignTeam
//   First come, first served: first 2 players → Living, next 2 → Dead.
// ─────────────────────────────────────────────────────────────────────────────

EPlayerTeam AStaticMansionGameMode::AssignTeam(APlayerController* PC)
{
    if (LivingControllers.Num() < MaxLivingPlayers)
    {
        LivingControllers.Add(PC);
        TeamMap.Add(PC, EPlayerTeam::Living);
        return EPlayerTeam::Living;
    }
    if (DeadControllers.Num() < MaxDeadPlayers)
    {
        DeadControllers.Add(PC);
        TeamMap.Add(PC, EPlayerTeam::Dead);
        return EPlayerTeam::Dead;
    }
    return EPlayerTeam::Unassigned;
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnPawnForController
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::SpawnPawnForController(
    APlayerController* PC, EPlayerTeam Team)
{
    UClass* PawnClass = (Team == EPlayerTeam::Living)
        ? LivingCharacterClass.Get()
        : DeadCharacterClass.Get();

    if (!PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[GameMode] SpawnPawnForController: No pawn class set for team %d! "
                 "Assign LivingCharacterClass / DeadCharacterClass in the Blueprint defaults."),
            (int32)Team);
        return;
    }

    AActor* StartSpot = FindStartForTeam(Team);
    const FVector  SpawnLoc = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
    const FRotator SpawnRot = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.Owner = PC;

    APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnLoc, SpawnRot, Params);
    if (!NewPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("[GameMode] Failed to spawn pawn for '%s'."),
            *PC->GetName());
        return;
    }

    PC->Possess(NewPawn);

    // For Dead players: swap PlayerState to ADeadPlayerState.
    // Unreal auto-creates a PlayerState in PostLogin using PlayerStateClass (which
    // defaults to ALivingPlayerState). We replace it here for Dead players.
    if (Team == EPlayerTeam::Dead)
    {
        // Destroy the auto-created Living state and create a Dead one.
        if (APlayerState* OldPS = PC->PlayerState)
        {
            OldPS->Destroy();
        }

        FActorSpawnParameters PSParams;
        PSParams.Owner = PC;
        ADeadPlayerState* DeadPS = GetWorld()->SpawnActor<ADeadPlayerState>(
            ADeadPlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, PSParams);

        if (DeadPS)
        {
            PC->PlayerState = DeadPS;
            DeadPS->SetOwner(PC);
            UE_LOG(LogTemp, Log, TEXT("[GameMode] Assigned ADeadPlayerState to '%s'."),
                *PC->GetName());
        }
    }

    // Wire the flee delegate now that we have a pawn.
    BindFleeDelegate(PC);

    UE_LOG(LogTemp, Log, TEXT("[GameMode] Spawned %s pawn for '%s' at %s."),
        Team == EPlayerTeam::Living ? TEXT("Living") : TEXT("Dead"),
        *PC->GetName(),
        *SpawnLoc.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// FindStartForTeam
//   Looks for PlayerStart actors tagged with "Living" or "Dead".
//   Falls back to any PlayerStart if no tagged ones are found.
//
//   EDITOR TIP: Select a PlayerStart in your level → Details → Tags → add "Living"
//   or "Dead". Place 2+ of each so spawns don't overlap.
// ─────────────────────────────────────────────────────────────────────────────

AActor* AStaticMansionGameMode::FindStartForTeam(EPlayerTeam Team) const
{
    const FName TargetTag = (Team == EPlayerTeam::Living)
        ? FName("Living") : FName("Dead");

    AActor* Fallback = nullptr;

    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        APlayerStart* Start = *It;
        if (!Fallback) Fallback = Start;

        if (Start->PlayerStartTag == TargetTag)
            return Start;
    }

    if (!Fallback)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GameMode] FindStartForTeam: No PlayerStart found in the level! "
                 "Add PlayerStart actors to your level and tag them 'Living' or 'Dead'."));
    }

    return Fallback;
}

// ─────────────────────────────────────────────────────────────────────────────
// CheckAllPlayersReady
//   Arm the pre-night countdown once enough players have joined.
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::CheckAllPlayersReady()
{
    if (bNightStarted || PreNightTimer >= 0.0f) return; // Already counting down.

    const bool bFull = (LivingControllers.Num() >= MaxLivingPlayers &&
                        DeadControllers.Num()   >= MaxDeadPlayers);

    const bool bAny = bStartWithAnyPlayers &&
                      (LivingControllers.Num() > 0 || DeadControllers.Num() > 0);

    if (bFull || bAny)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[GameMode] All players ready. Starting night in %.0f seconds."),
            PreNightCountdown);
        PreNightTimer = PreNightCountdown;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// StartNight — called when countdown reaches zero
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::StartNight()
{
    bNightStarted = true;
    PreNightTimer = -1.0f;

    UE_LOG(LogTemp, Log, TEXT("[GameMode] Night begins!"));

    if (UGamePhaseManager* PM = GetPhaseManager())
    {
        PM->StartNight();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HandlePlayerFled
//   Switch a Living player to spectator mode.
//   The player keeps their controller but their pawn is replaced with
//   UE5's built-in SpectatorPawn so they can still observe the match.
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::HandlePlayerFled(APlayerController* FleeingController)
{
    if (!FleeingController || bMatchOver) return;

    UE_LOG(LogTemp, Warning, TEXT("[GameMode] Player '%s' is fleeing — switching to spectator."),
        *FleeingController->GetName());

    // Destroy the Living pawn.
    if (APawn* OldPawn = FleeingController->GetPawn())
    {
        FleeingController->UnPossess();
        OldPawn->Destroy();
    }

    // Switch to spectator mode — Unreal handles this natively via
    // ChangeState("Spectating") on the PlayerController.
    FleeingController->ChangeState(NAME_Spectating);
    FleeingController->ClientGotoState(NAME_Spectating);

    // Check if ALL Living players have now fled — if so, Dead win.
    const bool bAllLivingFled = LivingControllers.ContainsByPredicate(
        [](APlayerController* PC) -> bool
        {
            if (!PC) return true; // Disconnected = effectively fled.
            ALivingPlayerState* PS = PC->GetPlayerState<ALivingPlayerState>();
            return PS && PS->HasFled();
        });

    if (bAllLivingFled)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[GameMode] All Living players have fled. Dead win!"));
        HandleDeadVictory();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Victory handlers
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::HandleLivingVictory()
{
    EndMatch(EMatchResult::LivingWin);
}

void AStaticMansionGameMode::HandleDeadVictory()
{
    EndMatch(EMatchResult::DeadWin);
}

// ─────────────────────────────────────────────────────────────────────────────
// EndMatch — shared logic for all victory paths
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::EndMatch(EMatchResult Result)
{
    if (bMatchOver) return; // Guard against double-calls.
    bMatchOver = true;

    UE_LOG(LogTemp, Log, TEXT("[GameMode] Match ended. Result: %d"), (int32)Result);

    // Write result to GameState so all clients see it.
    if (AStaticMansionGameState* GS = GetMansionGameState())
    {
        GS->SetMatchResult(Result);
    }

    // Disable input on all players — the match is over.
    for (APlayerController* PC : LivingControllers)
    {
        if (PC) PC->SetIgnoreMoveInput(true);
    }
    for (APlayerController* PC : DeadControllers)
    {
        if (PC) PC->SetIgnoreMoveInput(true);
    }

    // After PostMatchDelay seconds, travel to lobby or restart the level.
    FTimerHandle PostMatchHandle;
    GetWorldTimerManager().SetTimer(PostMatchHandle, FTimerDelegate::CreateLambda([this]()
    {
        // Blueprint can override this by binding to OnMatchResultChanged and calling
        // UGameplayStatics::OpenLevel themselves for a lobby map.
        // Default: restart the current level.
        UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));

    }), PostMatchDelay, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// BindFleeDelegate
//   After spawning a Living pawn, wire its cardiac component's OnPlayerFlees
//   delegate to our HandlePlayerFled method.
//   The Dead don't flee, so we skip them here.
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::BindFleeDelegate(APlayerController* PC)
{
    if (!PC) return;

    const EPlayerTeam Team = GetTeamForController(PC);
    if (Team != EPlayerTeam::Living) return;

    ALivingCharacter* Living = Cast<ALivingCharacter>(PC->GetPawn());
    if (!Living) return;

    UCardiacRhythmComponent* Cardiac = Living->GetCardiacComponent();
    if (!Cardiac) return;

    // Capture the PC in the lambda — weak pointer to avoid dangling ref.
    TWeakObjectPtr<APlayerController> WeakPC(PC);
    Cardiac->OnPlayerFlees.AddDynamic(this, &AStaticMansionGameMode::OnLivingPlayerFled);

    // We can't pass parameters to AddDynamic directly, so store the PC→pawn
    // association and look it up in the callback.
    // The lambda workaround isn't possible with AddDynamic (it requires a UFUNCTION).
    // Instead we look up the PC from the pawn in the callback below.
}

// ─────────────────────────────────────────────────────────────────────────────
// OnLivingPlayerFled — UFUNCTION so it can bind to the dynamic delegate
//   We receive no arguments from the delegate (it's parameterless), so we
//   find which Living controller's pawn just fled by checking HasFled() on all.
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::OnLivingPlayerFled()
{
    // Find the controller whose pawn just triggered the flee.
    for (APlayerController* PC : LivingControllers)
    {
        if (!PC) continue;
        ALivingCharacter* Pawn = Cast<ALivingCharacter>(PC->GetPawn());
        if (!Pawn) continue;

        UCardiacRhythmComponent* Cardiac = Pawn->GetCardiacComponent();
        if (Cardiac && Cardiac->IsFleeing())
        {
            HandlePlayerFled(PC);
            return; // Handle one at a time; next Tick will catch others if needed.
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HandlePhaseChanged_Internal — hooked to GamePhaseManager::OnPhaseChanged
//   Routes the GameOver phase to the correct victory handler.
//   Victory is actually triggered by GamePhaseManager calling the public
//   TriggerLivingVictory / TriggerDeadVictory, which set phase to GameOver
//   and then we catch it here.
// ─────────────────────────────────────────────────────────────────────────────

void AStaticMansionGameMode::HandlePhaseChanged_Internal(
    EGamePhase NewPhase, EGamePhase OldPhase)
{
    // Sync to GameState — this is where the stub in GamePhaseManager gets resolved.
    if (AStaticMansionGameState* GS = GetMansionGameState())
    {
        GS->SetCurrentPhase(NewPhase);
    }

    UE_LOG(LogTemp, Log, TEXT("[GameMode] Phase changed: %d → %d"),
        (int32)OldPhase, (int32)NewPhase);
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

EPlayerTeam AStaticMansionGameMode::GetTeamForController(APlayerController* PC) const
{
    const EPlayerTeam* Found = TeamMap.Find(PC);
    return Found ? *Found : EPlayerTeam::Unassigned;
}

// ─────────────────────────────────────────────────────────────────────────────
// Convenience accessors
// ─────────────────────────────────────────────────────────────────────────────

UGamePhaseManager* AStaticMansionGameMode::GetPhaseManager() const
{
    UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UGamePhaseManager>() : nullptr;
}

AStaticMansionGameState* AStaticMansionGameMode::GetMansionGameState() const
{
    return GetGameState<AStaticMansionGameState>();
}
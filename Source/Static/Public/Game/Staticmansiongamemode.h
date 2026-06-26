#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Systems/GamePhaseManager.h"
#include "Game/StaticMansionGameState.h"
#include "Systems/StaticMansionGameData.h"
#include "StaticMansionGameMode.generated.h"

// Forward declarations
class ALivingCharacter;
class ADeadCharacter;
class ALivingPlayerState;
class ADeadPlayerState;

// ─────────────────────────────────────────────────────────────────────────────
// EPlayerTeam
//   Assigned to each player on login. Drives pawn class selection and
//   PlayerState class selection.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EPlayerTeam : uint8
{
    Unassigned  UMETA(DisplayName = "Unassigned"),
    Living      UMETA(DisplayName = "The Living"),
    Dead        UMETA(DisplayName = "The Dead")
};

// ─────────────────────────────────────────────────────────────────────────────
// AStaticMansionGameMode
//
//   SERVER ONLY — GameMode never exists on clients.
//
//   RESPONSIBILITIES:
//   • Accept up to 4 players (2 Living, 2 Dead) and assign their team.
//   • Spawn the correct pawn class per team.
//   • Wait for all 4 players before starting the night.
//   • Listen to phase and victory events and trigger end-of-match flow.
//   • Switch fled Living players to spectator mode.
//   • Sync per-second time remaining to GameState for client HUD.
//
//   TEAM ASSIGNMENT:
//   Players join in order. First 2 become Living, next 2 become Dead.
//   For a production game you would replace this with a lobby/team-select
//   system — but this keeps it simple and testable from the start.
//
//   EDITOR SETUP:
//   1. Open World Settings in your mansion level.
//   2. Set "GameMode Override" to BP_StaticMansionGameMode (or this class).
//   3. Set "Default Pawn Class" to None — we assign pawns manually per team.
//   4. Set "Game State Class"  to BP_StaticMansionGameState.
//   5. Set "Player State Class" to ALivingPlayerState (the Dead override
//      happens in GetPlayerStateClass() below — no editor action needed).
//   6. In the Blueprint child class, assign:
//      - LivingCharacterClass  → BP_LivingCharacter
//      - DeadCharacterClass    → BP_DeadCharacter
//      These are EditDefaultsOnly so they appear in the Blueprint defaults.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AStaticMansionGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStaticMansionGameMode();

    // ── AGameModeBase overrides ───────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * Called when a player controller fully logs in and is ready to receive a pawn.
     * We use this to assign the team and spawn the correct character.
     */
    virtual void PostLogin(APlayerController* NewPlayer) override;

    /**
     * Called when a player disconnects. Clean up their team slot so another
     * player could theoretically fill it (handles mid-game drops gracefully).
     */
    virtual void Logout(AController* Exiting) override;

    /**
     * Unreal calls this to determine what pawn to spawn.
     * We return the correct class based on team assignment.
     */
    virtual UClass* GetDefaultPawnClassForController_Implementation(
        AController* InController) override;

    // ── Victory / loss ────────────────────────────────────────────────────────

    /**
     * Called by UGamePhaseManager::TriggerLivingVictory.
     * Ends the match with Living victory.
     */
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void HandleLivingVictory();

    /**
     * Called by UGamePhaseManager::TriggerDeadVictory.
     * Ends the match with Dead victory.
     */
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void HandleDeadVictory();

    // ── Flee handling ─────────────────────────────────────────────────────────

    /**
     * Switch a Living player to spectator mode.
     * Called when CardiacRhythmComponent fires OnPlayerFlees on the character.
     * We hook into this via the PostLogin delegate binding.
     */
    UFUNCTION(BlueprintCallable, Category = "GameMode")
    void HandlePlayerFled(APlayerController* FleeingController);

    // ── Team query ────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "GameMode")
    EPlayerTeam GetTeamForController(APlayerController* PC) const;

    UFUNCTION(BlueprintPure, Category = "GameMode")
    int32 GetLivingPlayerCount() const { return LivingControllers.Num(); }

    UFUNCTION(BlueprintPure, Category = "GameMode")
    int32 GetDeadPlayerCount()   const { return DeadControllers.Num(); }

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * DataAsset contenant toutes les variables de tuning du jeu.
     * Créer DA_StaticMansionGameData dans le Content Browser et l'assigner ici.
     * Si null, les valeurs par défaut des composants individuels sont utilisées.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Data",
        meta = (ToolTip = "Assigner DA_StaticMansionGameData ici pour centraliser le tuning."))
    TObjectPtr<UStaticMansionGameData> GameData;

    /** Accessor statique — n'importe quel système peut appeler GetGameData(World). */
    UFUNCTION(BlueprintPure, Category = "GameMode|Data", meta = (WorldContext = "WorldContext"))
    static UStaticMansionGameData* GetGameData(const UObject* WorldContext);

    /** Pawn class to spawn for Living players. Assign in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Classes")
    TSubclassOf<ALivingCharacter> LivingCharacterClass;

    /** Pawn class to spawn for Dead players. Assign in Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Classes")
    TSubclassOf<ADeadCharacter> DeadCharacterClass;

    /** How many Living players this match supports. Default 2 for 2v2. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Rules",
        meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxLivingPlayers = 2;

    /** How many Dead players this match supports. Default 2 for 2v2. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Rules",
        meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxDeadPlayers = 2;

    /**
     * Seconds to wait after all players connect before starting the night.
     * Gives clients time to finish loading assets.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Rules",
        meta = (ClampMin = "0.0"))
    float PreNightCountdown = 5.0f;

    /**
     * Seconds after match end before returning to lobby / restarting.
     * Blueprint can use OnMatchResultChanged to show end-screen during this window.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Rules",
        meta = (ClampMin = "0.0"))
    float PostMatchDelay = 10.0f;

    /**
     * If true, the night starts as soon as all expected players connect rather
     * than requiring the full MaxLiving + MaxDead count. Useful for testing
     * with fewer than 4 players.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Rules")
    bool bStartWithAnyPlayers = false;

private:
    // ── Team rosters ──────────────────────────────────────────────────────────

    UPROPERTY()
    TArray<APlayerController*> LivingControllers;

    UPROPERTY()
    TArray<APlayerController*> DeadControllers;

    // Maps each controller to its assigned team for fast lookup.
    TMap<APlayerController*, EPlayerTeam> TeamMap;

    // ── Match flow ────────────────────────────────────────────────────────────

    bool bNightStarted    = false;
    bool bMatchOver       = false;
    float PostMatchTimer  = 0.0f;

    /** Countdown before StartNight fires. Counts down in Tick. */
    float PreNightTimer = -1.0f; // -1 = not yet started

    // ── Internal helpers ──────────────────────────────────────────────────────

    /** Pousse les valeurs du GameData vers tous les systèmes. Appelé dans BeginPlay. */
    void ApplyGameData();

    /** Assign the next available team to a newly joined controller. */
    EPlayerTeam AssignTeam(APlayerController* PC);

    /** Spawn a pawn at an appropriate start point for the given team. */
    void SpawnPawnForController(APlayerController* PC, EPlayerTeam Team);

    /** Find a PlayerStart tagged for Living or Dead. Falls back to any start. */
    AActor* FindStartForTeam(EPlayerTeam Team) const;

    /** Check if all expected players are connected; if so, arm the countdown. */
    void CheckAllPlayersReady();

    /** Called when PreNightTimer reaches zero. */
    void StartNight();

    /** Bind to a LivingCharacter's cardiac flee delegate once it's spawned. */
    void BindFleeDelegate(APlayerController* PC);

    /** Common match-end logic shared by both victory paths. */
    void EndMatch(EMatchResult Result);

    /** Sync time remaining to GameState once per second. */
    float TimeSyncAccumulator = 0.0f;

    // These must be UFUNCTIONs because AddDynamic requires it.
    UFUNCTION()
    void OnLivingPlayerFled();

    UFUNCTION()
    void HandlePhaseChanged_Internal(EGamePhase NewPhase, EGamePhase OldPhase);

    UGamePhaseManager* GetPhaseManager() const;
    AStaticMansionGameState* GetMansionGameState() const;
};
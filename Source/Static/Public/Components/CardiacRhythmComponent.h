#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/GamePhaseManager.h"   // For EGamePhase
#include "CardiacRhythmComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Delegates
// ─────────────────────────────────────────────────────────────────────────────

/** Fired when rhythm reaches max — before the Heart Pain countdown. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeartPainEvent);

/** Fired when the player accumulates enough Heart Pain events to flee. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerFlees);

/** Fired whenever rhythm value changes — useful for updating UI. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRhythmChanged, float, NewNormalizedValue);

// ─────────────────────────────────────────────────────────────────────────────
// UCardiacRhythmComponent
//
//   Attach this to ALivingCharacter.
//   The component is fully replicated: rhythm, heart pain count, and fleeing
//   state are all server-authoritative and pushed to owning clients.
//
//   HOW IT WORKS:
//   • Scare events call AddRhythm(Delta) from the server (Dead abilities, jump
//     scares, etc.). Delta is a 0–100 float.
//   • When calm (no recent scare), rhythm decays at DecayRate per second.
//   • Hitting MaxRhythm triggers a Heart Pain event and resets rhythm to
//     PostPainRhythmReset.
//   • After HeartPainLimit Heart Pain events: ForceFlee() is called.
//
//   EDITOR SETUP:
//   1. Open your ALivingCharacter Blueprint.
//   2. Click "Add Component" → search "CardiacRhythmComponent".
//   3. Adjust the tuning properties in the Details panel.
//   4. Bind OnHeartPainEvent and OnPlayerFlees in the Blueprint event graph
//      to trigger camera shake / audio / spectate logic.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(StaticMansion), meta=(BlueprintSpawnableComponent))
class STATIC_API UCardiacRhythmComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCardiacRhythmComponent();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ── Replication ───────────────────────────────────────────────────────────
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Public API (called server-side) ──────────────────────────────────────

    /**
     * Add to the rhythm meter. Delta is in rhythm units (0–100 range).
     * Positive values raise fear; never use negative — let decay handle calming.
     * Server-authoritative: call this from Dead ability RPCs.
     */
    UFUNCTION(BlueprintCallable, Category = "Cardiac")
    void AddRhythm(float Delta);

    /**
     * Immediately calm the player (e.g. when they step into a Sage zone).
     * Resets the calm timer and reduces rhythm by Amount.
     */
    UFUNCTION(BlueprintCallable, Category = "Cardiac")
    void ApplyCalm(float Amount);

    /** Returns rhythm as 0.0–1.0 (normalized). Safe to call on clients. */
    UFUNCTION(BlueprintPure, Category = "Cardiac")
    float GetNormalizedRhythm() const;

    /** Is the player currently fleeing (i.e. has exceeded HeartPainLimit)? */
    UFUNCTION(BlueprintPure, Category = "Cardiac")
    bool IsFleeing() const { return bFleeing; }

    // ── Tuning properties ────────────────────────────────────────────────────

    /** Maximum rhythm value before a Heart Pain event fires. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac|Tuning",
        meta = (ClampMin = "1.0"))
    float MaxRhythm = 100.0f;

    /** Rhythm decays this many units per second when undisturbed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac|Tuning",
        meta = (ClampMin = "0.0"))
    float DecayRate = 5.0f;

    /** Seconds after last scare event before decay kicks in. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac|Tuning",
        meta = (ClampMin = "0.0"))
    float CalmDelay = 3.0f;

    /** Rhythm value rhythm resets to after a Heart Pain event. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac|Tuning",
        meta = (ClampMin = "0.0"))
    float PostPainRhythmReset = 40.0f;

    /** Number of Heart Pain events before forced flee. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac|Tuning",
        meta = (ClampMin = "1"))
    int32 HeartPainLimit = 3;

    // ── Events ────────────────────────────────────────────────────────────────

    /** Broadcast (on server) each time a Heart Pain event occurs. */
    UPROPERTY(BlueprintAssignable, Category = "Cardiac")
    FOnHeartPainEvent OnHeartPainEvent;

    /** Broadcast (on server) when the player is forced to flee. */
    UPROPERTY(BlueprintAssignable, Category = "Cardiac")
    FOnPlayerFlees OnPlayerFlees;

    /** Broadcast on the owning client whenever rhythm changes (for UI). */
    UPROPERTY(BlueprintAssignable, Category = "Cardiac")
    FOnRhythmChanged OnRhythmChanged;

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    /** Current rhythm value (0 → MaxRhythm). Replicated to owning client only. */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentRhythm)
    float CurrentRhythm = 0.0f;

    /** How many Heart Pain events have occurred this session. */
    UPROPERTY(Replicated)
    int32 HeartPainCount = 0;

    /** True once the player has accumulated HeartPainLimit events. */
    UPROPERTY(ReplicatedUsing = OnRep_Fleeing)
    bool bFleeing = false;

    // ── Server-only tracking ──────────────────────────────────────────────────

    /** Time (in game seconds) since the last scare event. Drives decay delay. */
    float TimeSinceLastScare = 0.0f;

    // ── RepNotify callbacks ───────────────────────────────────────────────────

    UFUNCTION()
    void OnRep_CurrentRhythm();

    UFUNCTION()
    void OnRep_Fleeing();

    // ── Internals ─────────────────────────────────────────────────────────────

    void TriggerHeartPain();
    void ForceFlee();

    /** Helper: clamp and apply rhythm, then fire UI delegate on owning client. */
    void SetRhythm(float NewValue);

    /** Notify the owning client's UI that rhythm changed. */
    UFUNCTION(Client, Reliable)
    void Client_NotifyRhythmChanged(float NormalizedValue);
};
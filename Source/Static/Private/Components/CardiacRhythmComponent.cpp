#include "Components/CardiacRhythmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UCardiacRhythmComponent::UCardiacRhythmComponent()
{
    // We need TickComponent for decay logic.
    PrimaryComponentTick.bCanEverTick = true;

    // Replicate this component so the server can push state to clients.
    SetIsReplicatedByDefault(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::BeginPlay()
{
    Super::BeginPlay();

    // Reset to calm at match start.
    CurrentRhythm = 0.0f;
    HeartPainCount = 0;
    bFleeing = false;
    TimeSinceLastScare = CalmDelay; // Start decaying immediately (no initial scare).
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
//   This macro-driven function tells Unreal which properties to replicate
//   and under what conditions.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // CurrentRhythm: only the owning client needs to see their own fear meter.
    // COND_OwnerOnly saves bandwidth — other players don't need this.
    DOREPLIFETIME_CONDITION(UCardiacRhythmComponent, CurrentRhythm, COND_OwnerOnly);

    // HeartPainCount: replicate to everyone so spectators / HUD can show it.
    DOREPLIFETIME(UCardiacRhythmComponent, HeartPainCount);

    // bFleeing: everyone needs to know when a player is out of the game.
    DOREPLIFETIME(UCardiacRhythmComponent, bFleeing);
}

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent — runs on the SERVER only (authority check inside)
//   Handles rhythm decay when the player is calm.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Only the server mutates rhythm — clients just read replicated data.
    if (!GetOwner() || !GetOwner()->GetWorld() || GetOwner()->GetWorld()->GetNetMode() == NM_Client) return;

    // If the player is already fleeing, no more updates needed.
    if (bFleeing) return;

    // Advance the calm timer.
    TimeSinceLastScare += DeltaTime;

    // Only decay after the calm delay has elapsed.
    if (TimeSinceLastScare >= CalmDelay && CurrentRhythm > 0.0f)
    {
        const float NewRhythm = FMath::Max(0.0f, CurrentRhythm - DecayRate * DeltaTime);
        SetRhythm(NewRhythm);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AddRhythm
//   Called by Dead abilities (server-side) when they scare a Living player.
//   Delta: how many rhythm units to add (designer-controlled per ability).
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::AddRhythm(float Delta)
{
    // Defensive: only mutate on server.
    // We check the component's own authority rather than the owner's,
    // because in multiplayer the owner (Living character) is server-authoritative
    // even though it's possessed by a client controller.
    if (!GetOwner()) return;
    if (!GetOwner()->GetWorld() || GetOwner()->GetWorld()->GetNetMode() == NM_Client) return;
    if (bFleeing) return;
    if (Delta <= 0.0f) return;

    // Reset calm timer — the player is scared again.
    TimeSinceLastScare = 0.0f;

    const float NewRhythm = CurrentRhythm + Delta;

    if (NewRhythm >= MaxRhythm)
    {
        // Clamp to max, trigger Heart Pain.
        SetRhythm(MaxRhythm);
        TriggerHeartPain();
    }
    else
    {
        SetRhythm(NewRhythm);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyCalm
//   Sage zones, Occult items, etc. call this to reduce fear.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::ApplyCalm(float Amount)
{
    if (!GetOwner()) return;
    if (!GetOwner()->GetWorld() || GetOwner()->GetWorld()->GetNetMode() == NM_Client) return;
    if (bFleeing) return;

    // Receiving calm resets the scare timer — you're safe here.
    TimeSinceLastScare = CalmDelay;

    const float NewRhythm = FMath::Max(0.0f, CurrentRhythm - Amount);
    SetRhythm(NewRhythm);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetNormalizedRhythm — safe to call on any machine
// ─────────────────────────────────────────────────────────────────────────────

float UCardiacRhythmComponent::GetNormalizedRhythm() const
{
    if (MaxRhythm <= 0.0f) return 0.0f;
    return FMath::Clamp(CurrentRhythm / MaxRhythm, 0.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// TriggerHeartPain — server only
//   Called when rhythm hits max. Increments the pain counter, resets rhythm,
//   and checks for the flee threshold.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::TriggerHeartPain()
{
    HeartPainCount++;

    UE_LOG(LogTemp, Log, TEXT("[Cardiac] Heart Pain event! Count: %d / %d"),
        HeartPainCount, HeartPainLimit);

    // Broadcast on server — bind GameMode or GameState listeners here.
    OnHeartPainEvent.Broadcast();

    if (HeartPainCount >= HeartPainLimit)
    {
        ForceFlee();
    }
    else
    {
        // Reset rhythm to the post-pain value so there's a brief reprieve.
        SetRhythm(PostPainRhythmReset);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ForceFlee — server only
//   The player has suffered too many Heart Pain events.
//   Marks them as fleeing (replicated) and fires the delegate.
//   The actual spectate/disconnect logic lives in your GameMode (Step 5).
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::ForceFlee()
{
    bFleeing = true;
    SetRhythm(0.0f);

    UE_LOG(LogTemp, Warning, TEXT("[Cardiac] Player is fleeing! Owner: %s"),
        *GetOwner()->GetName());

    OnPlayerFlees.Broadcast();

    // TODO (Step 5): Notify GameMode → switch player to spectator.
}

// ─────────────────────────────────────────────────────────────────────────────
// SetRhythm — internal helper
//   Always call this instead of setting CurrentRhythm directly.
//   Ensures the UI delegate fires on the owning client.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::SetRhythm(float NewValue)
{
    CurrentRhythm = FMath::Clamp(NewValue, 0.0f, MaxRhythm);

    UE_LOG(LogTemp, Log, TEXT("[Cardiac] Rhythm updated: %.1f / %.1f (%.0f%%)"),
        CurrentRhythm, MaxRhythm, GetNormalizedRhythm() * 100.0f);

    // Push UI update to the owning client via RPC.
    Client_NotifyRhythmChanged(GetNormalizedRhythm());
}

// ─────────────────────────────────────────────────────────────────────────────
// Client_NotifyRhythmChanged — runs on the owning client
//   Fires the Blueprint-assignable delegate so HUD Widgets can react.
//   This is a Client Reliable RPC — it will always arrive in order.
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::Client_NotifyRhythmChanged_Implementation(float NormalizedValue)
{
    OnRhythmChanged.Broadcast(NormalizedValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// RepNotify callbacks — fire on clients when replicated values change
// ─────────────────────────────────────────────────────────────────────────────

void UCardiacRhythmComponent::OnRep_CurrentRhythm()
{
    // On the owning client, keep the UI delegate in sync even if the RPC
    // arrived slightly out of order with the replicated value.
    OnRhythmChanged.Broadcast(GetNormalizedRhythm());
}

void UCardiacRhythmComponent::OnRep_Fleeing()
{
    if (bFleeing)
    {
        // On all clients: e.g. hide the player model, show "fled" UI.
        OnPlayerFlees.Broadcast();
    }
}
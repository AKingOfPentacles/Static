#include "Actors/ChalkCircleWardActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/GhostMovementComponent.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/SpecterEnergyComponent.h"
#include "Player/LivingPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

AChalkCircleWardActor::AChalkCircleWardActor()
{
    // Slightly weaker and shorter than salt — chalk is a soft deterrent, not a wall.
    DefenseStrength  = 10.0f;
    Duration         = 180.0f;  // 3 minutes — lasts into Confrontation phase
    OverlapRadius    = 120.0f;
}

void AChalkCircleWardActor::BeginPlay()
{
    Super::BeginPlay(); // Binds overlap events and duration timer.

    if (HasAuthority())
    {
        // 1-second tick for the Living calm aura.
        GetWorldTimerManager().SetTimer(ZoneTickHandle, this,
            &AChalkCircleWardActor::OnZoneTick, 1.0f, true);
    }
}

void AChalkCircleWardActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AChalkCircleWardActor, bRitualAnchorActive);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnDeadEntered
//   Base class fires ApplyWardEffect (energy hit) and we add the slow.
// ─────────────────────────────────────────────────────────────────────────────

void AChalkCircleWardActor::OnDeadEntered(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Let the base class apply the energy hit on entry.
    Super::OnDeadEntered(OverlappedComp, OtherActor, OtherComp,
        OtherBodyIndex, bFromSweep, SweepResult);

    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor))
    {
        DeadInside.AddUnique(Dead);
        ApplySlowToDead(Dead, true);
    }
    else if (ALivingCharacter* Living = Cast<ALivingCharacter>(OtherActor))
    {
        OnLivingEntered(Living);
    }
}

void AChalkCircleWardActor::OnDeadExited(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor))
    {
        DeadInside.RemoveAll([Dead](const TWeakObjectPtr<ADeadCharacter>& W)
            { return W.Get() == Dead; });

        // Restore full speed on exit.
        ApplySlowToDead(Dead, false);
    }
    else if (ALivingCharacter* Living = Cast<ALivingCharacter>(OtherActor))
    {
        OnLivingExited(Living);
    }
}

void AChalkCircleWardActor::OnLivingEntered(ALivingCharacter* Living)
{
    LivingInside.AddUnique(Living);
    UE_LOG(LogTemp, Log, TEXT("[ChalkCircle] Living entered safe zone."));
}

void AChalkCircleWardActor::OnLivingExited(ALivingCharacter* Living)
{
    LivingInside.RemoveAll([Living](const TWeakObjectPtr<ALivingCharacter>& W)
        { return W.Get() == Living; });
}

// ─────────────────────────────────────────────────────────────────────────────
// OnZoneTick — 1/second calm pulse for Living inside
// ─────────────────────────────────────────────────────────────────────────────

void AChalkCircleWardActor::OnZoneTick()
{
    for (TWeakObjectPtr<ALivingCharacter>& W : LivingInside)
    {
        if (ALivingCharacter* Living = W.Get())
        {
            if (UCardiacRhythmComponent* C = Living->GetCardiacComponent())
            {
                C->ApplyCalm(LivingCalmPerSecond);
            }
        }
    }
    LivingInside.RemoveAll([](const TWeakObjectPtr<ALivingCharacter>& W)
        { return !W.IsValid(); });
    DeadInside.RemoveAll([](const TWeakObjectPtr<ADeadCharacter>& W)
        { return !W.IsValid(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplySlowToDead
//   Multiplies or restores GhostMovementComponent::MaxFlySpeed.
// ─────────────────────────────────────────────────────────────────────────────

void AChalkCircleWardActor::ApplySlowToDead(ADeadCharacter* Dead, bool bSlow)
{
    if (!Dead) return;

    UGhostMovementComponent* Ghost = Dead->GetGhostMovementComponent();
    if (!Ghost) return;

    if (bSlow)
    {
        // Store the original speed and apply the slow.
        // We write directly to MaxFlySpeed — the component's SpectralSpeed
        // property is the "true" base; we temporarily lower MaxFlySpeed.
        Ghost->MaxFlySpeed = Ghost->SpectralSpeed * SlowFactor;
        UE_LOG(LogTemp, Log, TEXT("[ChalkCircle] Dead slowed to %.0f cm/s."),
            Ghost->MaxFlySpeed);
    }
    else
    {
        // Restore. Use SpectralSpeed or PassThroughSpeed depending on current mode.
        Ghost->MaxFlySpeed = Ghost->IsPassingThrough()
            ? Ghost->PassThroughSpeed
            : Ghost->SpectralSpeed;
        UE_LOG(LogTemp, Log, TEXT("[ChalkCircle] Dead speed restored."));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnAbsolutionOccurred
//   Called by ABurialGroundsActor when TriggerLivingVictory fires.
//   Reduce Heart Pain for any Living player standing inside by 1 — a small
//   "survivor's reward" for holding the circle during the ritual.
// ─────────────────────────────────────────────────────────────────────────────

void AChalkCircleWardActor::OnAbsolutionOccurred()
{
    if (!HasAuthority()) return;

    bRitualAnchorActive = true;

    for (TWeakObjectPtr<ALivingCharacter>& W : LivingInside)
    {
        if (ALivingCharacter* Living = W.Get())
        {
            if (ALivingPlayerState* PS = Living->GetPlayerState<ALivingPlayerState>())
            {
                // Absolution bonus: claw back one Heart Pain event.
                // We call IncrementHeartPainCount with a negative is not the right
                // API — instead we need a reduce method. For now log it and leave
                // the hook in place; ALivingPlayerState can gain a ReduceHeartPain()
                // method in a polish pass.
                UE_LOG(LogTemp, Log,
                    TEXT("[ChalkCircle] Absolution bonus for %s — Heart Pain relief pending PS update."),
                    *PS->GetPlayerName());
            }
        }
    }
}
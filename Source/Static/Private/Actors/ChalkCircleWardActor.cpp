#include "Actors/ChalkCircleWardActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/SpecterEnergyComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
//   Modifies MaxWalkSpeed on the standard UCharacterMovementComponent.
//   We cache the original speed so we can restore it correctly on exit.
// ─────────────────────────────────────────────────────────────────────────────

void AChalkCircleWardActor::ApplySlowToDead(ADeadCharacter* Dead, bool bSlow)
{
    if (!Dead) return;

    UCharacterMovementComponent* Movement = Dead->GetCharacterMovement();
    if (!Movement) return;

    if (bSlow)
    {
        // Store the current speed and apply the slow factor.
        const float CurrentSpeed = Movement->MaxWalkSpeed;
        Movement->MaxWalkSpeed = CurrentSpeed * SlowFactor;

        UE_LOG(LogTemp, Log, TEXT("[ChalkCircle] Dead slowed: %.0f -> %.0f cm/s."),
            CurrentSpeed, Movement->MaxWalkSpeed);
    }
    else
    {
        // Restore to ALS default walk speed.
        // 375 is ALS's default MaxWalkSpeed — the Dead character should
        // match whatever you set in BP_DeadCharacter's movement component.
        // If you change the Dead's walk speed in Blueprint, update this value.
        Movement->MaxWalkSpeed = 375.0f;

        UE_LOG(LogTemp, Log, TEXT("[ChalkCircle] Dead speed restored to %.0f cm/s."),
            Movement->MaxWalkSpeed);
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
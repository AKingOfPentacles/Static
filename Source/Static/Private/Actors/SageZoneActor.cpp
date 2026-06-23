#include "Actors/SageZoneActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/CardiacRhythmComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASageZoneActor::ASageZoneActor()
{
    // Sage is a wider zone than salt — adjust defaults.
    OverlapRadius    = 200.0f;
    DefenseStrength  = 0.0f;   // Not used — we drain per-second instead.
    Duration         = 90.0f;
}

void ASageZoneActor::BeginPlay()
{
    Super::BeginPlay(); // Binds overlap events and duration timer.

    if (HasAuthority())
    {
        // 1-second interval tick for all zone effects.
        GetWorldTimerManager().SetTimer(TickTimerHandle, this,
            &ASageZoneActor::OnZoneTick, 1.0f, /*bLoop=*/true);
    }
}

void ASageZoneActor::OnDeadEntered(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor))
    {
        DeadInside.AddUnique(Dead);
        UE_LOG(LogTemp, Log, TEXT("[Sage] Dead entered zone."));
    }
    else if (ALivingCharacter* Living = Cast<ALivingCharacter>(OtherActor))
    {
        OnLivingEntered(Living);
    }
}

void ASageZoneActor::OnDeadExited(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor))
    {
        DeadInside.RemoveAll([Dead](const TWeakObjectPtr<ADeadCharacter>& W)
        {
            return W.Get() == Dead;
        });
    }
    else if (ALivingCharacter* Living = Cast<ALivingCharacter>(OtherActor))
    {
        OnLivingExited(Living);
    }
}

void ASageZoneActor::OnLivingEntered(ALivingCharacter* Living)
{
    LivingInside.AddUnique(Living);
}

void ASageZoneActor::OnLivingExited(ALivingCharacter* Living)
{
    LivingInside.RemoveAll([Living](const TWeakObjectPtr<ALivingCharacter>& W)
    {
        return W.Get() == Living;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// OnZoneTick — fires every second while the zone is active
// ─────────────────────────────────────────────────────────────────────────────

void ASageZoneActor::OnZoneTick()
{
    // Drain all Dead inside.
    for (TWeakObjectPtr<ADeadCharacter>& WeakDead : DeadInside)
    {
        if (ADeadCharacter* Dead = WeakDead.Get())
        {
            if (USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent())
            {
                Energy->ApplyDefenseHit(DrainPerSecond);
            }
        }
    }
    // Remove any stale (destroyed) entries.
    DeadInside.RemoveAll([](const TWeakObjectPtr<ADeadCharacter>& W){ return !W.IsValid(); });

    // Calm all Living inside.
    for (TWeakObjectPtr<ALivingCharacter>& WeakLiving : LivingInside)
    {
        if (ALivingCharacter* Living = WeakLiving.Get())
        {
            if (UCardiacRhythmComponent* Cardiac = Living->GetCardiacComponent())
            {
                Cardiac->ApplyCalm(CalmPerSecond);
            }
        }
    }
    LivingInside.RemoveAll([](const TWeakObjectPtr<ALivingCharacter>& W){ return !W.IsValid(); });
}
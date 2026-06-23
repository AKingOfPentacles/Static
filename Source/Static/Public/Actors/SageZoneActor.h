#pragma once

#include "CoreMinimal.h"
#include "Actors/WardActorBase.h"
#include "Characters/LivingCharacter.h"
#include "SageZoneActor.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// ASageZoneActor
//
//   Unlike Salt (single-hit on entry) and Chalk (ritual use), Sage applies
//   a CONTINUOUS drain to any Dead character inside the zone.
//   It also continuously calms any Living characters inside it.
//
//   The continuous effect is driven by a per-second tick timer rather than
//   Tick() — this keeps performance predictable and makes the drain rate
//   directly readable in the tuning properties.
//
//   EDITOR SETUP:
//   Set DrainPerSecond and CalmPerSecond in Blueprint defaults.
//   Larger OverlapRadius = bigger sage cloud.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ASageZoneActor : public AWardActorBase
{
    GENERATED_BODY()

public:
    ASageZoneActor();

    virtual void BeginPlay() override;

    /** Energy drained from the Dead per second while inside. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sage",
        meta = (ClampMin = "0.0"))
    float DrainPerSecond = 8.0f;

    /** Rhythm calmed per second for Living players inside. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sage",
        meta = (ClampMin = "0.0"))
    float CalmPerSecond = 15.0f;

protected:
    // Override: do NOT apply ward effect on entry — we use a repeating timer.
    virtual void ApplyWardEffect(ADeadCharacter* DeadCharacter) override {}

    virtual void OnDeadEntered(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    virtual void OnDeadExited(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex) override;

private:
    // All actors currently inside the zone — ticked every second.
    TArray<TWeakObjectPtr<ADeadCharacter>>    DeadInside;
    TArray<TWeakObjectPtr<ALivingCharacter>>  LivingInside;

    FTimerHandle TickTimerHandle;

    UFUNCTION()
    void OnZoneTick();

    void OnLivingEntered(ALivingCharacter* Living);
    void OnLivingExited(ALivingCharacter* Living);
};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WardActorBase.generated.h"

class USphereComponent;
class UDecalComponent;
class ADeadCharacter;

// ─────────────────────────────────────────────────────────────────────────────
// AWardActorBase
//
//   Base class for all Living defensive placements:
//   ASaltWardActor, ASageZoneActor, AChalkCircleActor.
//
//   HOW WARDS WORK:
//   • Spawned by item UseItem() methods at the hit location from the trace.
//   • Have an overlap sphere that detects ADeadCharacter entering.
//   • On overlap: call SpecterEnergyComponent::ApplyDefenseHit(DefenseStrength).
//   • Have a limited Duration — they fade and destroy themselves after that.
//   • Replicated so all clients see the decal / VFX.
//
//   SUBCLASS DIFFERENCES:
//   • Salt    — line/barrier placed on floors. Moderate drain. Blocks passage
//               while active (the Dead avoid it or burn through it).
//   • Sage    — area zone. Continuous drain per second while Dead is inside.
//               Also applies ApplyCalm() to Living players inside it.
//   • Chalk   — circle/ritual marking. Used as the base for the absolution
//               ritual in Phase 3. Also has ward drain properties.
//
//   EDITOR SETUP:
//   Each subclass is a Blueprint child. Set:
//   - A Decal material for the floor marking
//   - DefenseStrength and Duration in Blueprint defaults
//   - Overlap radius to match the visual decal size
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Abstract)
class STATIC_API AWardActorBase : public AActor
{
    GENERATED_BODY()

public:
    AWardActorBase();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Configuration ─────────────────────────────────────────────────────────

    /** Energy drained from the Dead per activation (or per second for Sage). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward",
        meta = (ClampMin = "0.0"))
    float DefenseStrength = 15.0f;

    /** Seconds this ward remains active before dissolving. 0 = permanent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward",
        meta = (ClampMin = "0.0"))
    float Duration = 120.0f;

    /** Radius of the overlap detection sphere (cm). Match this to your decal size. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ward",
        meta = (ClampMin = "10.0"))
    float OverlapRadius = 100.0f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ward|Components")
    USphereComponent* OverlapSphere;

    /** Floor decal showing the ward marking. Assign material in Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ward|Components")
    UDecalComponent* WardDecal;

    // ── Overlap events — override in subclasses for specific behavior ─────────

    UFUNCTION()
    virtual void OnDeadEntered(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnDeadExited(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    /** Apply the ward's effect to a Dead character. Override for custom logic. */
    virtual void ApplyWardEffect(ADeadCharacter* DeadCharacter);

private:
    FTimerHandle DurationTimerHandle;
    void OnDurationExpired();
};
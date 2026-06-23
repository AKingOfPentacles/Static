#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MemorabiliaActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ADeadCharacter;

// ─────────────────────────────────────────────────────────────────────────────
// AMemorabiliaActor
//
//   Objects scattered through the mansion that only the Dead can collect
//   during Phase 1 (Exploration). Collecting one:
//   1. Grants EnergyGain to the Dead player's SpecterEnergyComponent.
//   2. Increments ADeadPlayerState::MemorabiliaCollected for scoring.
//   3. Destroys itself (or plays a dissolve VFX then destroys).
//
//   Examples: a locket, a faded photograph, a child's toy, a wedding ring.
//   Place these throughout the mansion — in drawers, on shelves, in corners.
//
//   EDITOR SETUP:
//   1. Create BP_Memorabilia from this class.
//   2. Assign a small prop mesh.
//   3. Set EnergyGain (default 20 = 20% of max energy per item).
//   4. Tag the actor "Memorabilia" for designer-level queries.
//   5. Place 8–12 throughout the level for Phase 1 pacing.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AMemorabiliaActor : public AActor
{
    GENERATED_BODY()

public:
    AMemorabiliaActor();

    virtual void BeginPlay() override;

    /** Energy granted to the Dead player on collection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memorabilia",
        meta = (ClampMin = "1.0"))
    float EnergyGain = 20.0f;

    /**
     * If true, only collectable during Phase 1 (Exploration).
     * After Phase 1 ends they still exist but can't be collected,
     * encouraging the Dead to prioritise early collection.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memorabilia")
    bool bPhase1Only = true;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Memorabilia|Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Memorabilia|Components")
    USphereComponent* CollectSphere;

private:
    UFUNCTION()
    void OnDeadOverlap(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    bool bCollected = false;

    bool IsCollectionAllowed() const;
};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "DoorActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ADeadCharacter;

// ─────────────────────────────────────────────────────────────────────────────
// ADoorActor
//
//   A replicated door that behaves differently for Living and Dead:
//
//   LIVING → Interact() opens or closes the door normally.
//
//   DEAD   → Interact() calls PassThrough() instead.
//            The door stays closed. The Dead's capsule collision is disabled
//            for PassThroughDuration seconds, the character is pushed forward
//            through the door with an impulse, then collision re-enables.
//            This costs PassThroughEnergyCost specter energy.
//
//   The Living see the door stay closed — from their perspective, the Dead
//   simply appeared on the other side. The Dead feel a brief 0.5s transit.
//
//   EDITOR SETUP:
//   1. Create BP_Door from this class.
//   2. Assign a door panel mesh to DoorMesh.
//   3. Set OpenRotation (e.g. Yaw = 90) for swing direction.
//   4. The door frame should be a separate static mesh actor — not part of this BP.
//   5. Set PassThroughEnergyCost to match your energy economy (default 12).
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ADoorActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ADoorActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── IInteractable ─────────────────────────────────────────────────────────

    virtual bool Interact_Implementation(
        AActor* Interactor, const FHitResult& HitResult) override;
    virtual FText GetInteractPrompt_Implementation() const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;

    // ── Living door control ───────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsOpen() const { return bIsOpen; }

    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetOpen(bool bOpen);

    // ── Dead pass-through ─────────────────────────────────────────────────────

    /**
     * Called when a Dead character interacts with the door.
     * Checks energy cost, disables capsule, pushes through, re-enables.
     * SERVER ONLY.
     */
    UFUNCTION(BlueprintCallable, Category = "Door|PassThrough")
    void PassThrough(ADeadCharacter* Dead);

    // ── Configuration ─────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    FRotator OpenRotation = FRotator(0.0f, 90.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door",
        meta = (ClampMin = "10.0"))
    float OpenSpeed = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", Replicated)
    bool bIsLocked = false;

    /** Energy cost for the Dead to pass through this door. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|PassThrough",
        meta = (ClampMin = "0.0"))
    float PassThroughEnergyCost = 12.0f;

    /**
     * How long (seconds) the Dead's capsule stays passable during transit.
     * 0.5s gives a brief ghostly slip-through feel.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|PassThrough",
        meta = (ClampMin = "0.1"))
    float PassThroughDuration = 0.5f;

    /**
     * Forward impulse applied to push the Dead through the door (cm/s).
     * Should be enough to cross a standard door thickness (~20cm) in
     * PassThroughDuration seconds.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|PassThrough",
        meta = (ClampMin = "50.0"))
    float PassThroughImpulse = 300.0f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UStaticMeshComponent* DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UBoxComponent* InteractionVolume;

private:
    // ── Door animation state ──────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
    bool bIsOpen = false;

    UFUNCTION()
    void OnRep_IsOpen();

    FRotator CurrentMeshRotation = FRotator::ZeroRotator;
    FRotator TargetMeshRotation  = FRotator::ZeroRotator;
    bool bAnimating = false;

    // ── Pass-through state ────────────────────────────────────────────────────

    /** Multicast to all clients to play the ghostly transit VFX. */
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayPassThroughEffect(ADeadCharacter* Dead);
    void Multicast_PlayPassThroughEffect_Implementation(ADeadCharacter* Dead);

    /** Re-enable the Dead character's capsule after transit completes. */
    void FinishPassThrough(ADeadCharacter* Dead);

    FTimerHandle PassThroughTimerHandle;
};
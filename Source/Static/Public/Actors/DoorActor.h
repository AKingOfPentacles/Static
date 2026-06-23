#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "DoorActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

// ─────────────────────────────────────────────────────────────────────────────
// ADoorActor
//
//   A server-authoritative door that Living players can open and close.
//   The Dead can also move through walls, but open doors still matter for
//   light and line-of-sight, so this is a key environmental element.
//
//   HOW IT WORKS:
//   Interact() toggles bIsOpen. The replicated flag triggers OnRep_IsOpen
//   on all clients, which plays a timeline animation via Blueprint or rotates
//   the mesh directly in C++ using a timer-driven lerp.
//
//   EDITOR SETUP:
//   1. Create BP_Door from this class.
//   2. Assign a door mesh (just the moving panel — frame is a separate static mesh).
//   3. Set OpenRotation to the angle the door swings to (e.g. Y = 90 degrees).
//   4. Set OpenSpeed to control how fast it swings (degrees per second).
//   5. Place BP_Door actors throughout the mansion.
//
//   ANIMATION NOTE:
//   We drive the rotation in Tick using a lerp rather than a UTimelineComponent
//   because timelines don't replicate well. The mesh rotation is driven by
//   bIsOpen (replicated) on every client independently. This is the standard
//   UE5 approach for replicated doors — clients simulate the animation locally
//   based on the authoritative open/close state.
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

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsOpen() const { return bIsOpen; }

    /** Force open or close from external logic (e.g. a puzzle trigger). */
    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetOpen(bool bOpen);

    // ── Configuration ─────────────────────────────────────────────────────────

    /** Rotation of the door mesh when fully open. Closed = FRotator::ZeroRotator. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    FRotator OpenRotation = FRotator(0.0f, 90.0f, 0.0f);

    /** Degrees per second the door swings. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door",
        meta = (ClampMin = "10.0"))
    float OpenSpeed = 180.0f;

    /** If true, the door cannot be interacted with (locked). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", Replicated)
    bool bIsLocked = false;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UStaticMeshComponent* DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UBoxComponent* InteractionVolume;

private:
    UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
    bool bIsOpen = false;

    UFUNCTION()
    void OnRep_IsOpen();

    // Current and target rotations for the lerp animation (local to each client).
    FRotator CurrentMeshRotation  = FRotator::ZeroRotator;
    FRotator TargetMeshRotation   = FRotator::ZeroRotator;
    bool bAnimating = false;
};
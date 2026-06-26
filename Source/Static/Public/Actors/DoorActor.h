#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "DoorActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UArrowComponent;
class USceneComponent;
class ADeadCharacter;

// ─────────────────────────────────────────────────────────────────────────────
// Delegates — Blueprint-assignable events for VFX/SFX/animation hooks
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPassthroughStarted, ADeadCharacter*, Dead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPassthroughEnded,   ADeadCharacter*, Dead);

// ─────────────────────────────────────────────────────────────────────────────
// ADoorActor
//
//   Living → Interact() opens or closes the door normally.
//
//   Dead   → Interact() triggers a controlled pass-through:
//     1. Energy deducted.
//     2. Door mesh collision disabled.
//     3. OnPassthroughStarted fires (Blueprint: shimmer VFX, sound, etc.)
//     4. Player input blocked.
//     5. Dead teleported to EntryPoint (designer-placed scene component).
//     6. Dead moves automatically to ExitPoint at PassThroughSpeed.
//     7. Door collision restored.
//     8. Input re-enabled.
//     9. OnPassthroughEnded fires (Blueprint: clean-up VFX, etc.)
//
//   EDITOR SETUP:
//   1. Create BP_Door from this class.
//   2. Assign a door panel mesh to DoorMesh.
//   3. In the viewport, position the two orange arrows:
//      - EntryPoint : place it in front of the door on one side, at floor level
//      - ExitPoint  : place it on the other side, at floor level
//      The Dead will snap to EntryPoint then walk to ExitPoint.
//      Move them freely — they are child SceneComponents visible in the editor.
//   4. Set PassThroughSpeed and PassThroughEnergyCost in Class Defaults.
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|PassThrough",
        meta = (ClampMin = "0.0"))
    float PassThroughEnergyCost = 12.0f;

    /** Speed the Dead moves from EntryPoint to ExitPoint (cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|PassThrough",
        meta = (ClampMin = "50.0"))
    float PassThroughSpeed = 200.0f;

    // ── Blueprint events ──────────────────────────────────────────────────────

    /** Fired when pass-through begins — bind for shimmer VFX, sound, etc. */
    UPROPERTY(BlueprintAssignable, Category = "Door|PassThrough")
    FOnPassthroughStarted OnPassthroughStarted;

    /** Fired when pass-through ends — bind for clean-up VFX, etc. */
    UPROPERTY(BlueprintAssignable, Category = "Door|PassThrough")
    FOnPassthroughEnded OnPassthroughEnded;

protected:
    // ── Components ────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UStaticMeshComponent* DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    UBoxComponent* InteractionVolume;

    /**
     * Entry point for Dead pass-through.
     * Position this in the editor on the side the Dead approaches from.
     * The Dead will snap here before moving to ExitPoint.
     * Shown as an arrow in the viewport — drag it freely.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|PassThrough")
    USceneComponent* EntryPoint;

    /**
     * Exit point for Dead pass-through.
     * Position this on the other side of the door.
     * The Dead will move here automatically after snapping to EntryPoint.
     * Shown as an arrow in the viewport — drag it freely.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|PassThrough")
    USceneComponent* ExitPoint;

    /**
     * Visual arrows shown in editor viewport for Entry and Exit points.
     * These are children of EntryPoint and ExitPoint for clarity.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|PassThrough")
    UArrowComponent* EntryArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|PassThrough")
    UArrowComponent* ExitArrow;

private:
    // ── Door animation ────────────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
    bool bIsOpen = false;

    UFUNCTION()
    void OnRep_IsOpen();

    FRotator CurrentMeshRotation = FRotator::ZeroRotator;
    FRotator TargetMeshRotation  = FRotator::ZeroRotator;
    bool bAnimating = false;

    // ── Pass-through helpers ──────────────────────────────────────────────────

    FTimerHandle PassThroughTimerHandle;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetDoorCollision(bool bEnable);
    void Multicast_SetDoorCollision_Implementation(bool bEnable);
};
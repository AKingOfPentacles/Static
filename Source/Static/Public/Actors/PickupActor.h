#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Items/ItemBase.h"          // FItemData, EItemType
#include "PickupActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UInventoryComponent;

// ─────────────────────────────────────────────────────────────────────────────
// APickupActor
//
//   The physical world object a Living player sees lying on a shelf or table.
//   When interacted with, it adds an FItemData entry to the player's inventory
//   and destroys itself (or hides, depending on bDestroyOnPickup).
//
//   This is the BASE CLASS. For each item type you place in the level, create
//   a Blueprint child:
//     BP_Pickup_Matches  → set ItemType = Matches,  Quantity = 3
//     BP_Pickup_Salt     → set ItemType = Salt,      Quantity = 1
//     BP_Pickup_Flashlight → set ItemType = Flashlight, Quantity = 1
//   etc.
//
//   EDITOR SETUP (per pickup Blueprint):
//   1. Set ItemType and Quantity in the Blueprint defaults.
//   2. Assign a StaticMesh (the prop visible in the world).
//   3. Adjust the SphereComponent radius to match the mesh size.
//   4. Tag the actor with "Pickup" in Actor Tags for any systems that need to
//      find all pickups (e.g. the Dead can't pick these up — they check tags).
//
//   REPLICATION:
//   The actor replicates so all clients see it disappear when it's picked up.
//   Only the server calls AddItem and Destroy — clients just observe the result.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API APickupActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    APickupActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── IInteractable ─────────────────────────────────────────────────────────

    virtual bool Interact_Implementation(
        AActor* Interactor, const FHitResult& HitResult) override;

    virtual FText GetInteractPrompt_Implementation() const override;

    virtual bool CanInteract_Implementation(AActor* Interactor) const override;

    // ── Configuration ─────────────────────────────────────────────────────────

    /** What item this pickup adds to the player's inventory. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    EItemType ItemType = EItemType::None;

    /** How many units to add (e.g. 3 matches, 1 flashlight). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup",
        meta = (ClampMin = "1"))
    int32 Quantity = 1;

    /** Display name shown in the interaction prompt. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    FText PickupPrompt = FText::FromString("Pick up");

    /**
     * If true, the actor is destroyed when picked up.
     * If false, it hides and re-enables after RespawnTime seconds.
     * Use false for items that should restock (candles, etc.).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool bDestroyOnPickup = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup",
        meta = (EditCondition = "!bDestroyOnPickup", ClampMin = "0.0"))
    float RespawnTime = 30.0f;

protected:
    // ── Components ────────────────────────────────────────────────────────────

    /** The visible prop mesh — assign in Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
    UStaticMeshComponent* MeshComponent;

    /**
     * Interaction trigger volume — player must be within this sphere AND
     * the interact trace must hit the actor. The sphere drives the HUD
     * prompt visibility; the trace prevents picking up through walls.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
    USphereComponent* InteractionSphere;

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    /** Hidden while waiting to respawn (only relevant when bDestroyOnPickup = false). */
    UPROPERTY(ReplicatedUsing = OnRep_Hidden)
    bool bIsHidden = false;

    UFUNCTION()
    void OnRep_Hidden();

    FTimerHandle RespawnTimerHandle;

    // ── Helpers ───────────────────────────────────────────────────────────────

    void DoPickup(AActor* Interactor);
    void Respawn();
};
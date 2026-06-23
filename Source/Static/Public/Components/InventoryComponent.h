#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/ItemBase.h"      // For FItemData, EItemType
#include "InventoryComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Delegates
// ─────────────────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, const TArray<FItemData>&, NewInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUsed, EItemType, UsedItemType);

// ─────────────────────────────────────────────────────────────────────────────
// UInventoryComponent
//
//   Attach to ALivingCharacter.
//   Stores a replicated array of FItemData and maps EItemType → UItemBase class
//   so items know how to execute their UseItem logic.
//
//   INVENTORY RULES:
//   • Max MaxSlots distinct item types at a time.
//   • Stackable items (Matches) can have Quantity > 1.
//   • Non-stackable items (Flashlight, Bones) are limited to Quantity = 1.
//
//   HOW TO USE IN BLUEPRINTS:
//   1. Bind OnInventoryChanged to refresh your hotbar widget.
//   2. Call TryUseItem(EItemType) when the player presses their use key.
//   3. Call AddItem(FItemData) from the pickup actor's Interact() logic.
//
//   EDITOR SETUP:
//   1. Add InventoryComponent to ALivingCharacter Blueprint.
//   2. In the ItemClassMap (TMap on this component), assign the UItemBase
//      subclass for each EItemType. You do this in the Blueprint Details panel
//      → find "Item Class Map" → add entries like: Matches → BP_MatchItem, etc.
//      (Step 6 will provide concrete item subclasses for each item type.)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(StaticMansion), meta=(BlueprintSpawnableComponent))
class STATIC_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Add an item to the inventory (call from pickup actor on the server).
     * Returns true if added, false if inventory is full or item isn't stackable.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FItemData& NewItem);

    /**
     * Remove one unit of an item type. Returns true if removed.
     * Call this automatically after a successful TryUseItem, or
     * explicitly for consumable placement items.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(EItemType Type, int32 Quantity = 1);

    /**
     * Attempt to use an item. SERVER ONLY.
     * Calls CanUse() then UseItem() on the matching UItemBase subclass.
     * If UseItem returns true AND the item is consumable, decrements its count.
     * HitResult: pass the player's interaction trace result so placement items
     *            know where in the world to spawn their effect.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory",
        meta = (AutoCreateRefTerm = "HitResult"))
    bool TryUseItem(EItemType Type, const FHitResult& HitResult);

    /** Does the inventory contain at least one of this item type? */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasItem(EItemType Type) const;

    /** Returns the full inventory array (read-only, for UI binding). */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    const TArray<FItemData>& GetInventory() const { return Items; }

    /** Get quantity of a specific item type. Returns 0 if not present. */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetItemQuantity(EItemType Type) const;

    // ── Configuration ─────────────────────────────────────────────────────────

    /** Maximum number of distinct item types the player can carry. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory",
        meta = (ClampMin = "1"))
    int32 MaxSlots = 6;

    /**
     * Maps each item type to its implementation class.
     * Fill this in the Blueprint Details panel for your ALivingCharacter.
     *
     * EDITOR TIP: You can also set this in your Character's DefaultProperties
     * (C++) using TSubclassOf<UItemBase> if you prefer pure-code setup.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    TMap<EItemType, TSubclassOf<UItemBase>> ItemClassMap;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnItemUsed OnItemUsed;

private:
    // ── Replicated inventory ──────────────────────────────────────────────────

    UPROPERTY(ReplicatedUsing = OnRep_Items)
    TArray<FItemData> Items;

    UFUNCTION()
    void OnRep_Items();

    // ── Helpers ───────────────────────────────────────────────────────────────

    /** Find an item in the array by type. Returns nullptr if not found. */
    FItemData* FindItem(EItemType Type);
    const FItemData* FindItem(EItemType Type) const;

    /** Instantiate (or reuse) a UItemBase for the given type. */
    UItemBase* GetItemInstance(EItemType Type);

    /** Cached item instances — avoids re-creating UObjects every use. */
    UPROPERTY(Transient)
    TMap<EItemType, UItemBase*> ItemInstances;
};
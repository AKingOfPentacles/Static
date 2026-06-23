#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ItemBase.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EItemType
//   Used to quickly identify item categories without casting.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EItemType : uint8
{
    None            UMETA(DisplayName = "None"),
    Matches         UMETA(DisplayName = "Matches"),
    Sage            UMETA(DisplayName = "Sage"),
    Salt            UMETA(DisplayName = "Salt"),
    Chalk           UMETA(DisplayName = "Chalk"),
    Flashlight      UMETA(DisplayName = "Flashlight"),
    OccultItem      UMETA(DisplayName = "Occult Protection Item"),
    PileOfBones     UMETA(DisplayName = "Pile of Bones")
};

// ─────────────────────────────────────────────────────────────────────────────
// FItemData
//   Lightweight data struct stored in the inventory.
//   Designed to be replicated as part of an array.
//
//   WHY A STRUCT not UObject?
//   UObjects inside a replicated array require TArray<TSubclassOf<>> tricks.
//   A plain struct with an EItemType tag + quantity is simpler to replicate
//   and sufficient for our inventory. The actual "use" behavior lives in the
//   UItemBase subclasses, which are spawned on-demand.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct STATIC_API FItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemType ItemType = EItemType::None;

    /** Stack count (e.g. 3 Matches, 1 Flashlight). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    int32 Quantity = 1;

    /** Display name shown in the HUD. Set in the item DataAsset (future step). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    bool operator==(const FItemData& Other) const
    {
        return ItemType == Other.ItemType;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// UItemBase
//   Abstract base for all usable items.
//   Subclasses override UseItem() to implement their specific behavior.
//
//   DESIGN DECISION: Items are UObjects (not Actors).
//   They have no physical presence in the world — the world presence
//   (the thing you see on a shelf) is a separate APickupActor (Step 6).
//   UItemBase only handles the "what happens when I USE this" logic.
//
//   HOW TO CREATE A NEW ITEM:
//   1. Subclass UItemBase in C++ (or Blueprint).
//   2. Override UseItem_Implementation().
//   3. Register the EItemType in the UInventoryComponent map.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(Abstract, Blueprintable, BlueprintType)
class STATIC_API UItemBase : public UObject
{
    GENERATED_BODY()

public:
    // ── Identity ─────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    EItemType ItemType = EItemType::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FText ItemName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item",
        meta = (MultiLine = true))
    FText ItemDescription;

    // ── Stack rules ───────────────────────────────────────────────────────────

    /** Can the player carry more than one of this item at a time? */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    bool bIsStackable = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item",
        meta = (EditCondition = "bIsStackable", ClampMin = "1"))
    int32 MaxStackSize = 5;

    // ── Core interface ────────────────────────────────────────────────────────

    /**
     * Called by the inventory when the player uses this item.
     * User: the character who is using the item.
     * HitResult: trace result from the interaction ray (location, surface normal, etc.)
     *            Used by placement items (Salt, Chalk) to know where to spawn the ward.
     *
     * SERVER ONLY — never call this directly on a client.
     * Return true if the use succeeded (so inventory can decrement quantity).
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item")
    bool UseItem(AActor* User, const FHitResult& HitResult);
    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult);

    /**
     * Override this to return false for items that can't be used right now.
     * Example: Flashlight returns false if battery is dead.
     *          Bones returns false if you're not at the burial grounds.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
    bool CanUse(AActor* User) const;
    virtual bool CanUse_Implementation(AActor* User) const { return true; }

    /**
     * Called when the item is added to the inventory.
     * Use this for one-time setup (e.g. Flashlight initializing battery component).
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Item")
    void OnPickedUp(AActor* NewOwner);
    virtual void OnPickedUp_Implementation(AActor* NewOwner) {}

    /**
     * Called when the item is removed from the inventory (used up, dropped, etc.).
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Item")
    void OnRemoved(AActor* OldOwner);
    virtual void OnRemoved_Implementation(AActor* OldOwner) {}
};
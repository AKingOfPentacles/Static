#include "Components/InventoryComponent.h"
#include "Net/UnrealNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // No per-frame logic needed.
    SetIsReplicatedByDefault(true);
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void UInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Inventory is private to the owning player.
    // If you later want a "trade" feature, you'd change to COND_None.
    DOREPLIFETIME_CONDITION(UInventoryComponent, Items, COND_OwnerOnly);
}

// ─────────────────────────────────────────────────────────────────────────────
// AddItem — server only
// ─────────────────────────────────────────────────────────────────────────────

bool UInventoryComponent::AddItem(const FItemData& NewItem)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    // Check if we already have this item type.
    FItemData* Existing = FindItem(NewItem.ItemType);

    if (Existing)
    {
        // Increment quantity up to MaxStackSize.
        // We read MaxStackSize from the item class if available.
        UItemBase* ItemInstance = GetItemInstance(NewItem.ItemType);
        if (ItemInstance && !ItemInstance->bIsStackable)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Inventory] Item %d instance is not stackable, but data says it is."),
                (int32)NewItem.ItemType);
            return false;
        }
        const int32 StackLimit = ItemInstance ? ItemInstance->MaxStackSize : 1;

        if (Existing->Quantity >= StackLimit)
        {
            UE_LOG(LogTemp, Log, TEXT("[Inventory] Item %d stack is full (%d)."),
                (int32)NewItem.ItemType, StackLimit);
            return false;
        }

        Existing->Quantity = FMath::Min(Existing->Quantity + NewItem.Quantity, StackLimit);
    }
    else
    {
        // New item type — check slot limit.
        if (Items.Num() >= MaxSlots)
        {
            UE_LOG(LogTemp, Log, TEXT("[Inventory] Inventory full (%d slots)."), MaxSlots);
            return false;
        }

        Items.Add(NewItem);

        // Notify item it was picked up.
        UItemBase* Instance = GetItemInstance(NewItem.ItemType);
        if (Instance)
        {
            Instance->OnPickedUp(GetOwner());
        }
    }

    // Mark dirty so the replication system sends the updated array.
    // (TArray replication with ReplicatedUsing handles this automatically.)
    OnInventoryChanged.Broadcast(Items);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveItem — server only
// ─────────────────────────────────────────────────────────────────────────────

bool UInventoryComponent::RemoveItem(EItemType Type, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    FItemData* Item = FindItem(Type);
    if (!Item) return false;

    Item->Quantity -= Quantity;

    if (Item->Quantity <= 0)
    {
        // Notify item it's leaving.
        UItemBase* Instance = GetItemInstance(Type);
        if (Instance)
        {
            Instance->OnRemoved(GetOwner());
        }

        // Remove from array.
        Items.RemoveAll([Type](const FItemData& D){ return D.ItemType == Type; });
        ItemInstances.Remove(Type);
    }

    OnInventoryChanged.Broadcast(Items);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TryUseItem — server only
//   The main entry point for player item usage.
//   Flow: HasItem → CanUse → UseItem → (if success) RemoveItem if consumable
// ─────────────────────────────────────────────────────────────────────────────

bool UInventoryComponent::TryUseItem(EItemType Type, const FHitResult& HitResult)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    if (!HasItem(Type))
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] TryUseItem: player doesn't have item %d."),
            (int32)Type);
        return false;
    }

    UItemBase* Instance = GetItemInstance(Type);
    if (!Instance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Inventory] No UItemBase class registered for type %d."),
            (int32)Type);
        return false;
    }

    // Check use conditions (battery life, phase requirements, etc.).
    if (!Instance->CanUse(GetOwner()))
    {
        UE_LOG(LogTemp, Log, TEXT("[Inventory] Item %d reports CanUse = false."), (int32)Type);
        return false;
    }

    // Execute the item's effect.
    const bool bSuccess = Instance->UseItem(GetOwner(), HitResult);

    if (bSuccess)
    {
        OnItemUsed.Broadcast(Type);

        // Consumable check: non-stackable items with Quantity=1 are removed on use.
        // Stackable items (Matches) decrement their count.
        // NOTE: The item subclass itself can also call RemoveItem() directly if
        // it has more complex consumption logic (e.g. Flashlight only consumes
        // battery, not the item itself).
        const FItemData* Data = FindItem(Type);
        if (Data && Data->Quantity > 0)
        {
            // Let the item decide if it should be consumed — default is yes.
            // Override by adding a bConsumedOnUse flag to the subclass.
            RemoveItem(Type, 1);
        }
    }

    return bSuccess;
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

bool UInventoryComponent::HasItem(EItemType Type) const
{
    return FindItem(Type) != nullptr;
}

int32 UInventoryComponent::GetItemQuantity(EItemType Type) const
{
    const FItemData* Item = FindItem(Type);
    return Item ? Item->Quantity : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

FItemData* UInventoryComponent::FindItem(EItemType Type)
{
    return Items.FindByPredicate([Type](const FItemData& D){ return D.ItemType == Type; });
}

const FItemData* UInventoryComponent::FindItem(EItemType Type) const
{
    return Items.FindByPredicate([Type](const FItemData& D){ return D.ItemType == Type; });
}

UItemBase* UInventoryComponent::GetItemInstance(EItemType Type)
{
    // Return cached instance if we have one.
    if (UItemBase** Found = ItemInstances.Find(Type))
    {
        return *Found;
    }

    // Look up the class in the map and instantiate.
    const TSubclassOf<UItemBase>* ItemClass = ItemClassMap.Find(Type);
    if (!ItemClass || !*ItemClass)
    {
        return nullptr;
    }

    UItemBase* NewInstance = NewObject<UItemBase>(this, *ItemClass);
    ItemInstances.Add(Type, NewInstance);
    return NewInstance;
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRep_Items — fires on clients when inventory changes
// ─────────────────────────────────────────────────────────────────────────────

void UInventoryComponent::OnRep_Items()
{
    OnInventoryChanged.Broadcast(Items);
}
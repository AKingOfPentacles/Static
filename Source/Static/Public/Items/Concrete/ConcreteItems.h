#pragma once

#include "CoreMinimal.h"
#include "Items/ItemBase.h"
#include "ConcreteItems.generated.h"

// Forward declarations
class ASaltWardActor;
class ASageZoneActor;
class AChalkCircleActor;
class ABurialGroundsActor;
class AMatchFlameActor;

// ─────────────────────────────────────────────────────────────────────────────
// UMatchItem
//   Strikes a match, spawning an AMatchFlameActor attached to the player's hand.
//   Stackable — players carry up to 5. Each use consumes one match.
//   The flame extinguishes itself after FlameDuration seconds, or immediately
//   when the player suffers a Heart Pain event (shock snuffs the flame).
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UMatchItem : public UItemBase
{
    GENERATED_BODY()
public:
    UMatchItem();
 
    /** Class to spawn. Set to BP_MatchFlame in the InventoryComponent's ItemClassMap. */
    UPROPERTY(EditDefaultsOnly, Category = "Matches")
    TSubclassOf<AMatchFlameActor> MatchFlameClass;
 
    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;
 
private:
    /** Track the currently active flame so we don't stack two at once. */
    UPROPERTY()
    AMatchFlameActor* ActiveFlame = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// USaltItem
//   Places an ASaltWardActor at the HitResult location.
//   Single-use consumable — one Salt item = one ward placement.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API USaltItem : public UItemBase
{
    GENERATED_BODY()
public:
    USaltItem();

    /** Class to spawn. Set to BP_SaltWard in the InventoryComponent's ItemClassMap. */
    UPROPERTY(EditDefaultsOnly, Category = "Salt")
    TSubclassOf<AActor> SaltWardClass;

    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// USageItem
//   Burns a Sage bundle at the user's feet, spawning an ASageZoneActor.
//   Does not need a surface HitResult — spawns at the player's location.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API USageItem : public UItemBase
{
    GENERATED_BODY()
public:
    USageItem();

    UPROPERTY(EditDefaultsOnly, Category = "Sage")
    TSubclassOf<AActor> SageZoneClass;

    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// UChalkItem
//   Draws a ritual circle on the surface at HitResult location.
//   The chalk circle also serves as a ward AND as a valid location for Phase 3
//   rituals when placed at the burial grounds.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UChalkItem : public UItemBase
{
    GENERATED_BODY()
public:
    UChalkItem();

    UPROPERTY(EditDefaultsOnly, Category = "Chalk")
    TSubclassOf<AActor> ChalkCircleClass;

    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// UFlashlightItem
//   Toggles a flashlight on the player. The actual USpotLightComponent
//   lives on the ALivingCharacter — this item just toggles its visibility.
//   NOT consumed on use. NOT removed from inventory.
//   Battery drains over time via a server-side tick on ALivingCharacter.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UFlashlightItem : public UItemBase
{
    GENERATED_BODY()
public:
    UFlashlightItem();

    /** Total battery life in seconds. */
    UPROPERTY(EditDefaultsOnly, Category = "Flashlight", meta = (ClampMin = "10.0"))
    float MaxBatteryLife = 180.0f;

    /** Current remaining battery. Persists across uses. */
    UPROPERTY(BlueprintReadOnly, Category = "Flashlight")
    float CurrentBattery;

    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;
    virtual void OnPickedUp_Implementation(AActor* NewOwner) override;

    /** Called each second by ALivingCharacter when flashlight is on. */
    UFUNCTION(BlueprintCallable, Category = "Flashlight")
    void DrainBattery(float Seconds);

    UFUNCTION(BlueprintPure, Category = "Flashlight")
    bool IsBatteryDead() const { return CurrentBattery <= 0.0f; }

    UFUNCTION(BlueprintPure, Category = "Flashlight")
    float GetBatteryNormalized() const;

private:
    bool bIsOn = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// UBonesItem
//   The Pile of Bones — the Phase 3 ritual object.
//   UseItem() calls ABurialGroundsActor::AttemptAbsolution().
//   CanUse() returns false if not at the burial grounds or not in Phase 3.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UBonesItem : public UItemBase
{
    GENERATED_BODY()
public:
    UBonesItem();

    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;

private:
    /** Find the burial grounds actor in the world. */
    ABurialGroundsActor* FindBurialGrounds(AActor* User) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// UOccultItem
//
//   A held protective talisman — a crucifix, a warding charm, a blessed object.
//   Unlike ward actors (which are placed in the world), this item is HELD.
//   While selected in the hotbar, it emits a persistent proximity aura that:
//   • Drains specter energy from any Dead character within OccultRadius cm.
//   • Applies calm to the holder's own cardiac rhythm each second.
//
//   HOW IT WORKS:
//   UseItem() EQUIPS/UNEQUIPS the item (toggle, like the flashlight).
//   While equipped, a server-side tick in ALivingCharacter finds nearby Dead
//   and calls OccultDrainNearby(). We do this via a spawned AOccultAuraActor
//   that attaches to the player and handles the radius logic independently —
//   the same pattern as AMatchFlameActor.
//
//   LIMITATIONS:
//   • One Occult Item per player.
//   • Not stackable — you either have it or you don't.
//   • Draining the Dead while holding it is exhausting — the holder's
//     movement speed is reduced by 20% while the aura is active.
//   • Does NOT get consumed on use — it's reusable but wears out after
//     MaxCharges activations (default: unlimited for simplicity).
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UOccultItem : public UItemBase
{
    GENERATED_BODY()
public:
    UOccultItem();
 
    /** Class to spawn when equipped. Set to BP_OccultAura in ItemClassMap. */
    UPROPERTY(EditDefaultsOnly, Category = "Occult")
    TSubclassOf<AActor> OccultAuraClass;
 
    virtual bool UseItem_Implementation(AActor* User, const FHitResult& HitResult) override;
    virtual bool CanUse_Implementation(AActor* User) const override;
    virtual void OnRemoved_Implementation(AActor* OldOwner) override;
 
    UFUNCTION(BlueprintPure, Category = "Occult")
    bool IsEquipped() const { return bIsEquipped; }
 
private:
    bool bIsEquipped = false;
 
    UPROPERTY()
    AActor* ActiveAura = nullptr;
 
    void Equip(AActor* User);
    void Unequip(AActor* User);
};
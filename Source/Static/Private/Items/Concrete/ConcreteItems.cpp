#include "Items/Concrete/ConcreteItems.h"
#include "Actors/BurialGroundsActor.h"
#include "Characters/LivingCharacter.h"
#include "Systems/GamePhaseManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Actors/MatchFlameActor.h"
#include "Actors/OccultAuraActor.h"
#include "Components/SpotLightComponent.h"

// ═════════════════════════════════════════════════════════════════════════════
// USaltItem
// ═════════════════════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════════════════════
// UMatchItem
// ═════════════════════════════════════════════════════════════════════════════
 
UMatchItem::UMatchItem()
{
    ItemType        = EItemType::Matches;
    ItemName        = FText::FromString("Matches");
    ItemDescription = FText::FromString("Strike a match for light. Snuffed by shock.");
    bIsStackable    = true;
    MaxStackSize    = 5;
}
 
bool UMatchItem::CanUse_Implementation(AActor* User) const
{
    // Cannot light a new match while one is already burning.
    // ActiveFlame is a UPROPERTY so it won't dangle — it becomes null when destroyed.
    if (ActiveFlame && ActiveFlame->IsBurning())
    {
        UE_LOG(LogTemp, Log, TEXT("[MatchItem] A match is already burning."));
        return false;
    }
    return true;
}
 
bool UMatchItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    if (!MatchFlameClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[MatchItem] MatchFlameClass not set! "
                 "Assign BP_MatchFlame in the InventoryComponent ItemClassMap."));
        return false;
    }
 
    // Spawn the flame actor at the player's hand location.
    // IgniteOn() will attach it properly to the hand socket.
    FActorSpawnParameters Params;
    Params.Owner = User;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 
    AMatchFlameActor* Flame = User->GetWorld()->SpawnActor<AMatchFlameActor>(
        MatchFlameClass, User->GetActorLocation(), FRotator::ZeroRotator, Params);
 
    if (!Flame)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MatchItem] Failed to spawn match flame."));
        return false;
    }
 
    ActiveFlame = Flame;
    Flame->IgniteOn(User);
 
    UE_LOG(LogTemp, Log, TEXT("[MatchItem] Match struck!"));
 
    // Return true — this match is consumed.
    return true;
}
 
// ═════════════════════════════════════════════════════════════════════════════
// UOccultItem
// ═════════════════════════════════════════════════════════════════════════════
 
UOccultItem::UOccultItem()
{
    ItemType        = EItemType::OccultItem;
    ItemName        = FText::FromString("Occult Talisman");
    ItemDescription = FText::FromString(
        "Hold aloft to drain nearby spirits. Slows your movement.");
    bIsStackable    = false;
    MaxStackSize    = 1;
}
 
bool UOccultItem::CanUse_Implementation(AActor* User) const
{
    // Always usable — toggles equip state.
    return true;
}
 
bool UOccultItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    if (bIsEquipped)
    {
        Unequip(User);
    }
    else
    {
        Equip(User);
    }
 
    // Return false — the item is not consumed on use (it's a toggle).
    return false;
}
 
void UOccultItem::OnRemoved_Implementation(AActor* OldOwner)
{
    // If the player loses the item while it's equipped (e.g. they flee),
    // make sure the aura is cleaned up.
    if (bIsEquipped)
    {
        Unequip(OldOwner);
    }
}
 
void UOccultItem::Equip(AActor* User)
{
    if (!OccultAuraClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[OccultItem] OccultAuraClass not set! "
                 "Assign BP_OccultAura in the InventoryComponent ItemClassMap."));
        return;
    }
 
    ALivingCharacter* Living = Cast<ALivingCharacter>(User);
    if (!Living) return;
 
    FActorSpawnParameters Params;
    Params.Owner = User;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 
    AOccultAuraActor* Aura = User->GetWorld()->SpawnActor<AOccultAuraActor>(
        OccultAuraClass, User->GetActorLocation(), FRotator::ZeroRotator, Params);
 
    if (!Aura)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OccultItem] Failed to spawn aura actor."));
        return;
    }
 
    ActiveAura  = Aura;
    bIsEquipped = true;
 
    Aura->ActivateOn(Living);
    UE_LOG(LogTemp, Log, TEXT("[OccultItem] Equipped — aura active."));
}
 
void UOccultItem::Unequip(AActor* User)
{
    if (AOccultAuraActor* Aura = Cast<AOccultAuraActor>(ActiveAura))
    {
        Aura->Deactivate();
    }
 
    ActiveAura  = nullptr;
    bIsEquipped = false;
 
    UE_LOG(LogTemp, Log, TEXT("[OccultItem] Unequipped — aura deactivated."));
}

USaltItem::USaltItem()
{
    ItemType       = EItemType::Salt;
    ItemName       = FText::FromString("Salt");
    ItemDescription= FText::FromString("Pour a salt line to repel the Dead.");
    bIsStackable   = true;
    MaxStackSize   = 5;
}

bool USaltItem::CanUse_Implementation(AActor* User) const
{
    // Salt needs a valid surface to place on — checked via HitResult in UseItem.
    return true;
}

bool USaltItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    if (!SaltWardClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SaltItem] SaltWardClass not set! Assign BP_SaltWard in the ItemClassMap."));
        return false;
    }

    // We need a valid surface hit to place the ward on.
    if (!HitResult.bBlockingHit)
    {
        UE_LOG(LogTemp, Log, TEXT("[SaltItem] No surface hit — can't place salt here."));
        return false;
    }

    // Spawn the ward slightly above the surface to avoid Z-fighting with the floor.
    const FVector SpawnLocation = HitResult.Location + HitResult.Normal * 2.0f;

    // Orient the ward to lie flat on the surface.
    const FRotator SpawnRotation = HitResult.Normal.Rotation();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Ward = User->GetWorld()->SpawnActor<AActor>(
        SaltWardClass, SpawnLocation, SpawnRotation, Params);

    if (!Ward)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaltItem] Failed to spawn salt ward."));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[SaltItem] Salt ward placed at %s."),
        *SpawnLocation.ToString());

    return true; // Returning true signals the inventory to consume one unit.
}

// ═════════════════════════════════════════════════════════════════════════════
// USageItem
// ═════════════════════════════════════════════════════════════════════════════

USageItem::USageItem()
{
    ItemType        = EItemType::Sage;
    ItemName        = FText::FromString("Sage Bundle");
    ItemDescription = FText::FromString("Burn sage to purify the area and calm your nerves.");
    bIsStackable    = true;
    MaxStackSize    = 3;
}

bool USageItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    if (!SageZoneClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SageItem] SageZoneClass not set! Assign BP_SageZone in the ItemClassMap."));
        return false;
    }

    // Sage spawns at the player's feet — no surface trace needed.
    const FVector SpawnLocation = User->GetActorLocation();
    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Zone = User->GetWorld()->SpawnActor<AActor>(
        SageZoneClass, SpawnLocation, SpawnRotation, Params);

    if (!Zone)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SageItem] Failed to spawn sage zone."));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[SageItem] Sage zone spawned at player feet."));
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// UChalkItem
// ═════════════════════════════════════════════════════════════════════════════

UChalkItem::UChalkItem()
{
    ItemType        = EItemType::Chalk;
    ItemName        = FText::FromString("Chalk");
    ItemDescription = FText::FromString("Draw a protective circle on the floor.");
    bIsStackable    = true;
    MaxStackSize    = 3;
}

bool UChalkItem::CanUse_Implementation(AActor* User) const
{
    return true; // Chalk can be placed anywhere there's a floor.
}

bool UChalkItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    if (!ChalkCircleClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ChalkItem] ChalkCircleClass not set! Assign BP_ChalkCircle in the ItemClassMap."));
        return false;
    }

    if (!HitResult.bBlockingHit)
    {
        UE_LOG(LogTemp, Log, TEXT("[ChalkItem] No surface — can't draw circle here."));
        return false;
    }

    const FVector SpawnLocation  = HitResult.Location + HitResult.Normal * 1.0f;
    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Circle = User->GetWorld()->SpawnActor<AActor>(
        ChalkCircleClass, SpawnLocation, SpawnRotation, Params);

    UE_LOG(LogTemp, Log, TEXT("[ChalkItem] Chalk circle drawn at %s."),
        *SpawnLocation.ToString());

    return Circle != nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
// UFlashlightItem
// ═════════════════════════════════════════════════════════════════════════════

UFlashlightItem::UFlashlightItem()
{
    ItemType        = EItemType::Flashlight;
    ItemName        = FText::FromString("Flashlight");
    ItemDescription = FText::FromString("Toggle the flashlight on or off.");
    bIsStackable    = false;
    MaxStackSize    = 1;
    CurrentBattery  = MaxBatteryLife;
}

void UFlashlightItem::OnPickedUp_Implementation(AActor* NewOwner)
{
    CurrentBattery = MaxBatteryLife;
    bIsOn = false;
}

bool UFlashlightItem::CanUse_Implementation(AActor* User) const
{
    // Can toggle off even when dead — just can't toggle on.
    if (!bIsOn && IsBatteryDead()) return false;
    return true;
}

bool UFlashlightItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    bIsOn = !bIsOn;

    // Find the SpotLightComponent on the character (attached in Blueprint).
    // The designer attaches a SpotLight to the camera bone in BP_LivingCharacter.
    if (USpotLightComponent* Light = User->FindComponentByClass<USpotLightComponent>())
    {
        Light->SetVisibility(bIsOn);
    }

    UE_LOG(LogTemp, Log, TEXT("[Flashlight] Turned %s. Battery: %.0fs remaining."),
        bIsOn ? TEXT("ON") : TEXT("OFF"), CurrentBattery);

    // Flashlight is NOT consumed on use — return false so inventory keeps it.
    return false;
}

void UFlashlightItem::DrainBattery(float Seconds)
{
    if (!bIsOn) return;
    CurrentBattery = FMath::Max(0.0f, CurrentBattery - Seconds);

    if (IsBatteryDead())
    {
        // Battery died — force off.
        bIsOn = false;
        UE_LOG(LogTemp, Warning, TEXT("[Flashlight] Battery dead."));
    }
}

float UFlashlightItem::GetBatteryNormalized() const
{
    if (MaxBatteryLife <= 0.0f) return 0.0f;
    return FMath::Clamp(CurrentBattery / MaxBatteryLife, 0.0f, 1.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
// UBonesItem
// ═════════════════════════════════════════════════════════════════════════════

UBonesItem::UBonesItem()
{
    ItemType        = EItemType::PileOfBones;
    ItemName        = FText::FromString("Pile of Bones");
    ItemDescription = FText::FromString("Burn these at the burial grounds to absolve the spirits.");
    bIsStackable    = false;
    MaxStackSize    = 1;
}

bool UBonesItem::CanUse_Implementation(AActor* User) const
{
    // Must be in Phase 3.
    if (UGameInstance* GI = User->GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            if (PM->GetCurrentPhase() != EGamePhase::Confrontation)
            {
                return false;
            }
        }
    }

    // Must be at the burial grounds.
    ABurialGroundsActor* Grounds = FindBurialGrounds(User);
    return Grounds && Grounds->IsActorInRange(User);
}

bool UBonesItem::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
    ABurialGroundsActor* Grounds = FindBurialGrounds(User);
    if (!Grounds)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[BonesItem] No BurialGroundsActor found in the level! "
                 "Place a BP_BurialGrounds actor in your map."));
        return false;
    }

    const bool bSuccess = Grounds->AttemptAbsolution(User);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[BonesItem] Absolution successful!"));
        return true; // Consumed — remove from inventory.
    }

    return false;
}

ABurialGroundsActor* UBonesItem::FindBurialGrounds(AActor* User) const
{
    if (!User || !User->GetWorld()) return nullptr;

    // Find the first burial grounds actor in the level.
    // There should only ever be one — tag check ensures correctness.
    for (TActorIterator<ABurialGroundsActor> It(User->GetWorld()); It; ++It)
    {
        return *It; // Return the first found.
    }
    return nullptr;
}
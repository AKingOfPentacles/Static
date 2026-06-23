#include "Actors/PickupActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InventoryComponent.h"
#include "Characters/LivingCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

APickupActor::APickupActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    // ── Collision sphere — root ───────────────────────────────────────────────
    // The sphere is the root so the whole actor positions by its center.
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetSphereRadius(60.0f);
    // No physics — this is a trigger only. Pawn channel generates overlap events
    // but does not block movement.
    InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    SetRootComponent(InteractionSphere);

    // ── Visible mesh — child of sphere ────────────────────────────────────────
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(InteractionSphere);
    // Items sit on surfaces — no physics simulation by default.
    MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
    MeshComponent->SetSimulatePhysics(false);
}

void APickupActor::BeginPlay()
{
    Super::BeginPlay();
    // Tag this actor so other systems can find all pickups quickly.
    Tags.AddUnique(FName("Pickup"));
}

void APickupActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APickupActor, bIsHidden);
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable — CanInteract
// ─────────────────────────────────────────────────────────────────────────────

bool APickupActor::CanInteract_Implementation(AActor* Interactor) const
{
    if (bIsHidden) return false;
    if (ItemType == EItemType::None) return false;

    // Only Living characters can pick up items.
    ALivingCharacter* Living = Cast<ALivingCharacter>(Interactor);
    if (!Living) return false;

    // Check inventory has room — CanInteract is a pre-flight, AddItem does the
    // real validation, but surfacing this early avoids a misleading prompt.
    UInventoryComponent* Inv = Living->GetInventoryComponent();
    return Inv != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable — GetInteractPrompt
// ─────────────────────────────────────────────────────────────────────────────

FText APickupActor::GetInteractPrompt_Implementation() const
{
    return PickupPrompt;
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable — Interact
// ─────────────────────────────────────────────────────────────────────────────

bool APickupActor::Interact_Implementation(AActor* Interactor, const FHitResult& HitResult)
{
    // Server only — double check.
    if (!HasAuthority()) return false;

    ALivingCharacter* Living = Cast<ALivingCharacter>(Interactor);
    if (!Living) return false;

    DoPickup(Living);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// DoPickup
// ─────────────────────────────────────────────────────────────────────────────

void APickupActor::DoPickup(AActor* Interactor)
{
    ALivingCharacter* Living = Cast<ALivingCharacter>(Interactor);
    if (!Living) return;

    UInventoryComponent* Inv = Living->GetInventoryComponent();
    if (!Inv) return;

    FItemData Data;
    Data.ItemType    = ItemType;
    Data.Quantity    = Quantity;
    Data.DisplayName = PickupPrompt;

    const bool bAdded = Inv->AddItem(Data);

    if (bAdded)
    {
        UE_LOG(LogTemp, Log, TEXT("[Pickup] %s picked up %s (qty %d)."),
            *Living->GetName(), *UEnum::GetValueAsString(ItemType), Quantity);

        if (bDestroyOnPickup)
        {
            Destroy();
        }
        else
        {
            // Hide and re-enable after RespawnTime seconds.
            bIsHidden = true;
            OnRep_Hidden(); // Apply immediately on server.

            GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
                &APickupActor::Respawn, RespawnTime, false);
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[Pickup] Inventory full or item not stackable for %s."),
            *Living->GetName());
    }
}

void APickupActor::Respawn()
{
    bIsHidden = false;
    OnRep_Hidden();
}

void APickupActor::OnRep_Hidden()
{
    // Show or hide both the mesh and the interaction sphere on all clients.
    SetActorHiddenInGame(bIsHidden);
    InteractionSphere->SetCollisionEnabled(
        bIsHidden ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}
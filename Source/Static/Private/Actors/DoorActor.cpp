#include "Actors/DoorActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(Root);
    DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(Root);
    InteractionVolume->SetBoxExtent(FVector(60.f, 100.f, 100.f));
    InteractionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();
    CurrentMeshRotation = FRotator::ZeroRotator;
    TargetMeshRotation  = FRotator::ZeroRotator;
}

void ADoorActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADoorActor, bIsOpen);
    DOREPLIFETIME(ADoorActor, bIsLocked);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — smooth door animation on all clients
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bAnimating) return;

    const float Alpha = FMath::Clamp(OpenSpeed * DeltaTime / 90.0f, 0.0f, 1.0f);
    CurrentMeshRotation = FMath::Lerp(CurrentMeshRotation, TargetMeshRotation, Alpha);
    DoorMesh->SetRelativeRotation(CurrentMeshRotation);

    if (CurrentMeshRotation.Equals(TargetMeshRotation, 0.5f))
    {
        CurrentMeshRotation = TargetMeshRotation;
        DoorMesh->SetRelativeRotation(CurrentMeshRotation);
        bAnimating = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable
//   Routes to different behaviour based on who is interacting.
// ─────────────────────────────────────────────────────────────────────────────

bool ADoorActor::CanInteract_Implementation(AActor* Interactor) const
{
    // Dead can always attempt a door — PassThrough handles the energy check.
    if (Cast<ADeadCharacter>(Interactor)) return true;

    // Living are blocked by locked doors.
    return !bIsLocked;
}

FText ADoorActor::GetInteractPrompt_Implementation() const
{
    if (Cast<ADeadCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn()))
    {
        return FText::FromString("Pass through");
    }
    if (bIsLocked) return FText::FromString("Locked");
    return bIsOpen ? FText::FromString("Close door") : FText::FromString("Open door");
}

bool ADoorActor::Interact_Implementation(AActor* Interactor, const FHitResult& HitResult)
{
    if (!HasAuthority()) return false;

    // Dead character → pass through instead of opening.
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(Interactor))
    {
        PassThrough(Dead);
        return true;
    }

    // Living character → normal open/close toggle.
    if (Cast<ALivingCharacter>(Interactor))
    {
        SetOpen(!bIsOpen);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetOpen / OnRep_IsOpen
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::SetOpen(bool bOpen)
{
    if (!HasAuthority()) return;
    bIsOpen = bOpen;
    OnRep_IsOpen();
}

void ADoorActor::OnRep_IsOpen()
{
    TargetMeshRotation = bIsOpen ? OpenRotation : FRotator::ZeroRotator;
    bAnimating = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// PassThrough — the core of the new Dead door interaction
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::PassThrough(ADeadCharacter* Dead)
{
    if (!Dead || !HasAuthority()) return;

    // ── Energy check ──────────────────────────────────────────────────────────
    USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent();
    if (!Energy || !Energy->TrySpendEnergy(PassThroughEnergyCost))
    {
        UE_LOG(LogTemp, Log,
            TEXT("[Door] Dead '%s' can't afford to pass through. Cost: %.0f"),
            *Dead->GetName(), PassThroughEnergyCost);
        return;
    }

    // ── Disable capsule collision ─────────────────────────────────────────────
    // The capsule switches to NoCollision so the Dead can move through the door
    // mesh freely. The door itself never opens.
    UCapsuleComponent* Capsule = Dead->GetCapsuleComponent();
    if (Capsule)
    {
        Capsule->SetCollisionProfileName(TEXT("NoCollision"));
    }

    // ── Apply forward impulse ─────────────────────────────────────────────────
    // Push the Dead through the door in the direction they're facing.
    // We override velocity directly — cleaner than LaunchCharacter for flying mode.
    if (UCharacterMovementComponent* Movement = Dead->GetCharacterMovement())
    {
        const FVector ForwardPush = Dead->GetActorForwardVector() * PassThroughImpulse;
        Movement->Velocity = FVector(ForwardPush.X, ForwardPush.Y, Movement->Velocity.Z);
    }

    // ── Play cosmetic effect on all clients ───────────────────────────────────
    Multicast_PlayPassThroughEffect(Dead);

    // ── Re-enable collision after transit duration ─────────────────────────────
    // Bind with a weak lambda — if the Dead is destroyed mid-transit, the timer
    // fires harmlessly.
    TWeakObjectPtr<ADoorActor> WeakSelf(this);
    TWeakObjectPtr<ADeadCharacter> WeakDead(Dead);

    GetWorldTimerManager().SetTimer(PassThroughTimerHandle,
        FTimerDelegate::CreateLambda([WeakSelf, WeakDead]()
        {
            if (WeakSelf.IsValid() && WeakDead.IsValid())
            {
                WeakSelf->FinishPassThrough(WeakDead.Get());
            }
        }),
        PassThroughDuration, false);

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' passing through door '%s'. Cost: %.0f energy."),
        *Dead->GetName(), *GetName(), PassThroughEnergyCost);
}

// ─────────────────────────────────────────────────────────────────────────────
// FinishPassThrough — restore collision after transit
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::FinishPassThrough(ADeadCharacter* Dead)
{
    if (!Dead) return;

    UCapsuleComponent* Capsule = Dead->GetCapsuleComponent();
    if (Capsule)
    {
        // Restore to DeadSpectral — blocked by salt wards but not world geometry.
        // If Stage 1 collision profiles were skipped, fall back to "Pawn".
        Capsule->SetCollisionProfileName(TEXT("DeadSpectral"));
    }

    // Stop the forward impulse so the Dead doesn't keep sliding.
    if (UCharacterMovementComponent* Movement = Dead->GetCharacterMovement())
    {
        Movement->Velocity = FVector::ZeroVector;
    }

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' pass-through complete. Collision restored."),
        *Dead->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// Multicast_PlayPassThroughEffect — cosmetic only, fires on all clients
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::Multicast_PlayPassThroughEffect_Implementation(ADeadCharacter* Dead)
{
    // Blueprint: override this to add:
    // • A brief door shimmer/ripple material effect
    // • A low whoosh sound at the door location
    // • Optional subtle camera blur on the transiting Dead client
    UE_LOG(LogTemp, Log, TEXT("[Door] PassThrough VFX — override in Blueprint."));
}
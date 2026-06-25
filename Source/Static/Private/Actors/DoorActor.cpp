#include "Actors/DoorActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
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

    // ── Determine which side the Dead is on ───────────────────────────────────
    // The door's forward vector points through the door plane.
    // If the Dead is in front (positive dot) we teleport them behind, and vice versa.
    // We offset by PassThroughDepth cm so they land clear of the door mesh.
    const FVector DoorForward  = GetActorForwardVector();
    const FVector ToDead       = Dead->GetActorLocation() - GetActorLocation();
    const float   Side         = FVector::DotProduct(DoorForward, ToDead);
    const FVector ExitOffset   = (Side >= 0.0f ? -DoorForward : DoorForward)
                                 * PassThroughDepth;

    const FVector ExitLocation = GetActorLocation()
                                 + ExitOffset
                                 + FVector(0.0f, 0.0f, 90.0f); // Keep at character height

    // ── Schedule the teleport after PassThroughDuration ──────────────────────
    // Brief delay gives the feel of slipping through rather than instant warp.
    // During the delay the Dead walks into the door normally — the door is
    // the only thing blocking them, so they visually press against it.
    Multicast_PlayPassThroughEffect(Dead);

    TWeakObjectPtr<ADoorActor>    WeakSelf(this);
    TWeakObjectPtr<ADeadCharacter> WeakDead(Dead);

    GetWorldTimerManager().SetTimer(PassThroughTimerHandle,
        FTimerDelegate::CreateLambda([WeakSelf, WeakDead, ExitLocation]()
        {
            if (WeakSelf.IsValid() && WeakDead.IsValid())
            {
                WeakSelf->FinishPassThrough(WeakDead.Get(), ExitLocation);
            }
        }),
        PassThroughDuration, false);

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' passing through '%s'. Exit: %s"),
        *Dead->GetName(), *GetName(), *ExitLocation.ToString());
    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' passing through door '%s'. Cost: %.0f energy."),
    *Dead->GetName(), *GetName(), PassThroughEnergyCost);
}


// ─────────────────────────────────────────────────────────────────────────────
// FinishPassThrough — restore collision after transit
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::FinishPassThrough(ADeadCharacter* Dead, FVector ExitLocation)
{
    if (!Dead) return;

    // Teleport the Dead to the other side of the door.
    // ETeleportType::TeleportPhysics ensures the movement component
    // doesn't fight the position change.
    Dead->SetActorLocation(ExitLocation, false, nullptr,
        ETeleportType::TeleportPhysics);

    UE_LOG(LogTemp, Log, TEXT("[Door] '%s' teleported to other side: %s"),
        *Dead->GetName(), *ExitLocation.ToString());
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
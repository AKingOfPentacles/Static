#include "Actors/DoorActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // The frame/wall stays in place — only the door panel mesh rotates.
    // We use a SceneComponent as root so we can pivot the DoorMesh separately.
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
// Tick — animate the door rotation on all clients
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bAnimating) return;

    // Lerp toward target rotation. Use a fixed angular step based on OpenSpeed.
    const float Alpha = FMath::Clamp(OpenSpeed * DeltaTime / 90.0f, 0.0f, 1.0f);
    CurrentMeshRotation = FMath::Lerp(CurrentMeshRotation, TargetMeshRotation, Alpha);
    DoorMesh->SetRelativeRotation(CurrentMeshRotation);

    // Stop animating once close enough.
    if (CurrentMeshRotation.Equals(TargetMeshRotation, 0.5f))
    {
        CurrentMeshRotation = TargetMeshRotation;
        DoorMesh->SetRelativeRotation(CurrentMeshRotation);
        bAnimating = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable
// ─────────────────────────────────────────────────────────────────────────────

bool ADoorActor::CanInteract_Implementation(AActor* Interactor) const
{
    return !bIsLocked;
}

FText ADoorActor::GetInteractPrompt_Implementation() const
{
    if (bIsLocked)  return FText::FromString("Locked");
    return bIsOpen  ? FText::FromString("Close door")
                    : FText::FromString("Open door");
}

bool ADoorActor::Interact_Implementation(AActor* Interactor, const FHitResult& HitResult)
{
    if (!HasAuthority()) return false;
    SetOpen(!bIsOpen);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetOpen / OnRep_IsOpen
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::SetOpen(bool bOpen)
{
    if (!HasAuthority()) return;
    bIsOpen = bOpen;
    OnRep_IsOpen(); // Apply on server immediately.
}

void ADoorActor::OnRep_IsOpen()
{
    // Set animation target — Tick will interpolate toward it.
    TargetMeshRotation = bIsOpen ? OpenRotation : FRotator::ZeroRotator;
    bAnimating = true;

    UE_LOG(LogTemp, Log, TEXT("[Door] %s — now %s."),
        *GetName(), bIsOpen ? TEXT("opening") : TEXT("closing"));
}
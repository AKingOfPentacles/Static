#include "Actors/DoorActor.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // ── Root ──────────────────────────────────────────────────────────────────
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // ── Door mesh ─────────────────────────────────────────────────────────────
    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(Root);
    DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // ── Interaction volume ────────────────────────────────────────────────────
    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(Root);
    InteractionVolume->SetBoxExtent(FVector(60.f, 100.f, 100.f));
    InteractionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    // ── Entry point ───────────────────────────────────────────────────────────
    // Default position: 80cm in front of the door on one side, at floor level.
    // Designer repositions this in the viewport to match the actual door geometry.
    EntryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EntryPoint"));
    EntryPoint->SetupAttachment(Root);
    EntryPoint->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));

    EntryArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("EntryArrow"));
    EntryArrow->SetupAttachment(EntryPoint);
    EntryArrow->ArrowColor = FColor::Green;
    EntryArrow->bHiddenInGame = true; // Visible in editor only.

    // ── Exit point ────────────────────────────────────────────────────────────
    // Default position: 80cm on the other side of the door.
    ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
    ExitPoint->SetupAttachment(Root);
    ExitPoint->SetRelativeLocation(FVector(-80.0f, 0.0f, 0.0f));

    ExitArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitArrow"));
    ExitArrow->SetupAttachment(ExitPoint);
    ExitArrow->ArrowColor = FColor::Red;
    ExitArrow->bHiddenInGame = true;
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
// Tick — smooth door swing
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
// ─────────────────────────────────────────────────────────────────────────────

bool ADoorActor::CanInteract_Implementation(AActor* Interactor) const
{
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(Interactor))
        return !Dead->IsFloating();
    return !bIsLocked;
}

FText ADoorActor::GetInteractPrompt_Implementation() const
{
    if (bIsLocked) return FText::FromString("Locked");
    return bIsOpen
        ? FText::FromString("Close door")
        : FText::FromString("Open door");
}

bool ADoorActor::Interact_Implementation(AActor* Interactor, const FHitResult& HitResult)
{
    if (!HasAuthority()) return false;

    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(Interactor))
    {
        PassThrough(Dead);
        return true;
    }

    if (Cast<ALivingCharacter>(Interactor))
    {
        SetOpen(!bIsOpen);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetOpen
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
// PassThrough
//
//   Uses designer-placed EntryPoint and ExitPoint scene components.
//   No geometry math — the designer positions the arrows in the editor.
//
//   Entry and Exit world positions are computed from the scene component
//   transforms at runtime, so they move correctly if the door is rotated.
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::PassThrough(ADeadCharacter* Dead)
{
    if (!Dead || !HasAuthority()) return;

    // ── Energy check ──────────────────────────────────────────────────────────
    USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent();
    if (!Energy || !Energy->TrySpendEnergy(PassThroughEnergyCost))
    {
        UE_LOG(LogTemp, Log, TEXT("[Door] Not enough energy. Cost: %.0f"),
            PassThroughEnergyCost);
        return;
    }

    // ── Get world-space entry and exit positions ───────────────────────────────
    // The designer placed these relative to the door root in the editor.
    // GetComponentLocation() gives us the world position automatically.
    //
    // We choose which point is Entry vs Exit based on which is closer
    // to the Dead's current position — so the interaction works correctly
    // regardless of which side the Dead approaches from.
    const FVector EntryWorld = EntryPoint->GetComponentLocation();
    const FVector ExitWorld  = ExitPoint->GetComponentLocation();
    const FVector DeadLoc    = Dead->GetActorLocation();

    const float DistToEntry = FVector::Dist2D(DeadLoc, EntryWorld);
    const float DistToExit  = FVector::Dist2D(DeadLoc, ExitWorld);

    // Closest point = entry (Dead approaches from this side).
    // Furthest point = exit (Dead emerges here).
    const FVector SnapPosition   = (DistToEntry <= DistToExit) ? EntryWorld : ExitWorld;
    const FVector MoveToPosition = (DistToEntry <= DistToExit) ? ExitWorld  : EntryWorld;

    UE_LOG(LogTemp, Log,
        TEXT("[Door] PassThrough: Entry=%s Exit=%s"),
        *SnapPosition.ToString(), *MoveToPosition.ToString());

    // ── Disable door collision ────────────────────────────────────────────────
    Multicast_SetDoorCollision(false);

    // ── Fire Blueprint event ──────────────────────────────────────────────────
    OnPassthroughStarted.Broadcast(Dead);

    // ── Block player input ────────────────────────────────────────────────────
    if (APlayerController* PC = Cast<APlayerController>(Dead->GetController()))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
    }

    // Exit rotation comes from whichever SceneComponent is the exit.
    const FRotator ExitRotation = (DistToEntry <= DistToExit)
        ? ExitPoint->GetComponentRotation()
        : EntryPoint->GetComponentRotation();

    // ── Start smooth float: current pos → entry → exit ───────────────────────
    Dead->StartFloating(SnapPosition, MoveToPosition, ExitRotation, PassThroughSpeed);

    // ── Timer: restore after full transit ────────────────────────────────────
    // Cover: Dead's current pos → entry + entry → exit, with buffer.
    const float DistDeadToEntry = FVector::Dist(Dead->GetActorLocation(), SnapPosition);
    const float DistEntry2Exit  = FVector::Dist(SnapPosition, MoveToPosition);
    const float Duration = ((DistDeadToEntry + DistEntry2Exit) / FMath::Max(PassThroughSpeed, 1.0f)) + 0.3f;

    TWeakObjectPtr<ADoorActor>     WeakSelf(this);
    TWeakObjectPtr<ADeadCharacter> WeakDead(Dead);

    GetWorldTimerManager().SetTimer(PassThroughTimerHandle,
        FTimerDelegate::CreateLambda([WeakSelf, WeakDead]()
        {
            if (!WeakSelf.IsValid()) return;

            // Always restore door collision.
            WeakSelf->Multicast_SetDoorCollision(true);

            if (WeakDead.IsValid())
            {
                // StopFloating may have already been called by Tick on arrival.
                // IsFloating() guards against double calls.
                if (WeakDead->IsFloating())
                {
                    WeakDead->StopFloating();
                }

                // Restore input in case Tick didn't fire for some reason.
                if (APlayerController* PC = Cast<APlayerController>(
                    WeakDead->GetController()))
                {
                    PC->ResetIgnoreMoveInput();
                    PC->ResetIgnoreLookInput();
                }

                // Fire Blueprint end event.
                WeakSelf->OnPassthroughEnded.Broadcast(WeakDead.Get());
            }

            UE_LOG(LogTemp, Log, TEXT("[Door] Pass-through timer complete."));
        }),
        Duration, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Multicast_SetDoorCollision
// ─────────────────────────────────────────────────────────────────────────────

void ADoorActor::Multicast_SetDoorCollision_Implementation(bool bEnable)
{
    if (!DoorMesh) return;
    DoorMesh->SetCollisionEnabled(
        bEnable ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision);
}
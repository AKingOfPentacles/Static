#include "Actors/SaltWardActor.h"
#include "Characters/DeadCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/GhostMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ASaltWardActor::ASaltWardActor()
{
    // We need Tick for continuous energy drain.
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    // ── BarrierBox — the physical blocker ────────────────────────────────────
    // This is the root so placement aligns the barrier to the surface.
    BarrierBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierBox"));
    // Half-extents: depth is thin (5cm), width and height set in BeginPlay.
    BarrierBox->SetBoxExtent(FVector(5.0f, BarrierWidth * 0.5f, BarrierHeight * 0.5f));
    SetRootComponent(BarrierBox);

    // ── COLLISION SETUP ───────────────────────────────────────────────────────
    // The barrier uses a custom collision profile "SaltBarrier" which you must
    // create in Project Settings → Collision → Preset → New.
    // Settings for "SaltBarrier":
    //   CollisionEnabled : Query and Physics
    //   Object Type      : WorldStatic
    //   All channels     : Ignore
    //   DeadMovement     : Block   ← the custom channel you created
    //
    // This means the barrier is INVISIBLE to everything except the Dead capsule,
    // which is set to Block DeadMovement in its normal collision profile.
    // When the Dead enters pass-through mode, their capsule switches to
    // NoCollision — so they ignore this barrier entirely.
    BarrierBox->SetCollisionProfileName(TEXT("SaltBarrier"));

    // ── Salt mesh — visual floor line ────────────────────────────────────────
    SaltMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaltMesh"));
    SaltMesh->SetupAttachment(BarrierBox);
    // Lies flat on the floor. The designer assigns a thin plane mesh in Blueprint.
    SaltMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -BarrierHeight * 0.5f));
    SaltMesh->SetCollisionProfileName(TEXT("NoCollision")); // Visual only.

    // ── Overlap sphere (inherited from AWardActorBase) ────────────────────────
    // The OverlapSphere is created in AWardActorBase constructor.
    // We attach it here as a child so it moves with the barrier.
    // Its radius is set wider than the barrier to create a drain "aura"
    // that triggers slightly before the Dead physically hits the wall.
    // We'll set the exact radius in BeginPlay after we read BarrierWidth.

    // ── Inherited ward decal ──────────────────────────────────────────────────
    // WardDecal from AWardActorBase is already created. We'll let Blueprint
    // assign its material (salt texture). It auto-attaches to OverlapSphere.
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::BeginPlay()
{
    // Call base — this binds the overlap events and starts the duration timer.
    Super::BeginPlay();

    // Size the barrier to match the configured dimensions.
    RefreshBarrierSize();

    // The overlap sphere radius should be slightly larger than the barrier
    // so energy drain begins just as the Dead approaches, not after impact.
    // We override whatever OverlapRadius was set in the base defaults.
    OverlapSphere->SetSphereRadius(FMath::Max(BarrierWidth, BarrierHeight) * 0.6f);

    // Reattach the overlap sphere and decal to the barrier box root.
    // The base constructor attached them to themselves — we correct that here.
    OverlapSphere->AttachToComponent(BarrierBox,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    WardDecal->AttachToComponent(BarrierBox,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    // Position the decal flat on the floor surface beneath the barrier.
    WardDecal->SetRelativeLocation(FVector(0.0f, 0.0f, -BarrierHeight * 0.5f + 1.0f));
    WardDecal->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    WardDecal->DecalSize = FVector(4.0f, BarrierWidth * 0.5f, BarrierWidth * 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASaltWardActor, BarrierIntegrity);
    DOREPLIFETIME(ASaltWardActor, bBarrierIntact);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — continuous energy drain for Dead inside the zone
//   Runs SERVER ONLY (we return early if not authority).
//   This handles the "burning through" cost when the Dead passes through.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) return;
    if (DeadInsideDrainZone.Num() == 0) return;

    // Drain all Dead currently overlapping the zone.
    for (TWeakObjectPtr<ADeadCharacter>& WeakDead : DeadInsideDrainZone)
    {
        ADeadCharacter* Dead = WeakDead.Get();
        if (!Dead) continue;

        USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent();
        if (!Energy) continue;

        // Drain at the configured per-second rate.
        // The Dead CHOSE to enter pass-through to cross the salt — they pay for it.
        const float Drain = BurnThroughDrainPerSecond * DeltaTime;
        Energy->ApplyDefenseHit(Drain);
    }

    // Clean up any stale entries (Dead destroyed mid-transit, etc.).
    DeadInsideDrainZone.RemoveAll([](const TWeakObjectPtr<ADeadCharacter>& W)
    {
        return !W.IsValid();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// OnDeadEntered — base class overlap fires this
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::OnDeadEntered(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor);
    if (!Dead) return;

    DeadInsideDrainZone.AddUnique(Dead);

    // If the Dead is in pass-through mode when they enter, they're actively
    // breaching. Record this so we can apply breach damage when they exit.
    UGhostMovementComponent* Ghost = Dead->GetGhostMovementComponent();
    if (Ghost && Ghost->IsPassingThrough())
    {
        BreachingDead.Add(Dead);
        UE_LOG(LogTemp, Log, TEXT("[SaltWard] Dead entering breach transit."));
    }
    else
    {
        // This shouldn't happen normally (barrier blocks them) but could occur
        // if a Dead player somehow slipped in. Apply a hard energy hit immediately.
        if (USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent())
        {
            Energy->ApplyDefenseHit(DefenseStrength);
        }
        UE_LOG(LogTemp, Warning,
            TEXT("[SaltWard] Dead inside barrier without pass-through — applying entry hit."));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnDeadExited
//   When the Dead leaves the drain zone we check if they completed a breach.
//   A complete breach = entered in pass-through AND exited on the other side.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::OnDeadExited(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor);
    if (!Dead) return;

    // Remove from active drain list.
    DeadInsideDrainZone.RemoveAll([Dead](const TWeakObjectPtr<ADeadCharacter>& W)
    {
        return W.Get() == Dead;
    });

    // If this Dead was tracked as breaching, they've now fully crossed.
    // Apply structural damage to the barrier.
    if (BreachingDead.Contains(Dead))
    {
        BreachingDead.Remove(Dead);
        ApplyBarrierBreach(Dead);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyBarrierBreach
//   Reduce barrier integrity after the Dead fully transits the line.
//   Called on exit from the drain zone (not entry) so we know they made it
//   all the way through — not just touched and retreated.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::ApplyBarrierBreach(ADeadCharacter* Dead)
{
    if (!bBarrierIntact) return; // Already broken — nothing to damage.

    BarrierIntegrity = FMath::Max(0.0f, BarrierIntegrity - BreachDamageAmount);

    UE_LOG(LogTemp, Log, TEXT("[SaltWard] Barrier breached. Integrity: %.0f%%"),
        BarrierIntegrity);

    // Fire the RepNotify immediately on the server so the visual updates here too.
    OnRep_BarrierIntegrity();

    if (BarrierIntegrity <= 0.0f)
    {
        BreakBarrier();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BreakBarrier
//   Integrity reached zero — disable the physical block.
//   The salt line still exists visually (and still drains energy from the drain
//   sphere) until its Duration expires, but it no longer blocks movement.
//   This models salt being scattered: still on the floor, still burns the Dead
//   a little, but no longer an impassable wall.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::BreakBarrier()
{
    bBarrierIntact = false;
    OnRep_BarrierIntact(); // Apply on server immediately.

    UE_LOG(LogTemp, Warning, TEXT("[SaltWard] Salt line broken — barrier collapsed."));
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateBarrierCollision
//   Enable or disable the blocking box based on bBarrierIntact.
//   Called from OnRep_BarrierIntact on all clients.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::UpdateBarrierCollision()
{
    if (bBarrierIntact)
    {
        BarrierBox->SetCollisionProfileName(TEXT("SaltBarrier"));
    }
    else
    {
        // Broken — disable blocking. Keep overlap queries so drain zone still works.
        BarrierBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RefreshBarrierSize
//   Applies BarrierWidth and BarrierHeight to the box and salt mesh.
//   Called in BeginPlay so designers can tune these in Blueprint defaults.
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::RefreshBarrierSize()
{
    // BoxExtent is half-size in each axis.
    // X = 5cm depth (thin wall), Y = half of width, Z = half of height.
    BarrierBox->SetBoxExtent(FVector(5.0f, BarrierWidth * 0.5f, BarrierHeight * 0.5f));

    // Scale the salt mesh along Y to match the width.
    // Assumes the base mesh is 100cm wide — scale to BarrierWidth.
    const float MeshScale = BarrierWidth / 100.0f;
    SaltMesh->SetRelativeScale3D(FVector(1.0f, MeshScale, 1.0f));
    SaltMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -(BarrierHeight * 0.5f)));
}

// ─────────────────────────────────────────────────────────────────────────────
// RepNotify callbacks
// ─────────────────────────────────────────────────────────────────────────────

void ASaltWardActor::OnRep_BarrierIntegrity()
{
    // Clients use this to drive visual crack effects on the salt line.
    // Blueprint: bind to OnRep_BarrierIntegrity to lerp a material parameter
    // (e.g. a "Cracks" scalar from 0 = pristine to 1 = shattered).
    //
    // Example material parameter update (requires a dynamic material instance):
    // if (UMaterialInstanceDynamic* MID = SaltMesh->CreateAndSetMaterialInstanceDynamic(0))
    // {
    //     MID->SetScalarParameterValue("CrackAmount", 1.0f - GetIntegrityNormalized());
    // }
    //
    // We don't do this in C++ because the material is designer-assigned in Blueprint.
    // The Blueprint child of this class handles visual feedback.

    UE_LOG(LogTemp, Verbose, TEXT("[SaltWard] OnRep: integrity = %.0f%%"), BarrierIntegrity);
}

void ASaltWardActor::OnRep_BarrierIntact()
{
    UpdateBarrierCollision();

    if (!bBarrierIntact)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[SaltWard] OnRep: barrier broken — collision disabled on client."));

        // Blueprint: play a scatter VFX here (salt particles flying away).
        // Bind to the OnRep via a Blueprint override of this function.
    }
}
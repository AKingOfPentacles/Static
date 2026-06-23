#include "Components/GhostMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UGhostMovementComponent::UGhostMovementComponent()
{
    // Ghosts float — disable gravity entirely in Flying mode.
    GravityScale = 0.0f;

    // Default speed for spectral movement.
    MaxFlySpeed     = SpectralSpeed;
    MaxAcceleration = 1200.0f;     // Snappy acceleration feels ghostly
    BrakingDecelerationFlying = 800.0f;

    // We'll switch to Flying in BeginPlay.
    // Don't set it here — the character hasn't been fully initialized yet.
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache the capsule's original profile for reference (may be "Pawn" by default).
    if (ACharacter* Owner = GetCharacterOwner())
    {
        if (UCapsuleComponent* Capsule = Owner->GetCapsuleComponent())
        {
            OriginalCollisionProfileName = Capsule->GetCollisionProfileName();

            // Switch to the DeadSpectral profile immediately.
            // This lets salt barriers block the Dead from the moment they spawn,
            // without colliding with walls or other pawns.
            Capsule->SetCollisionProfileName(TEXT("DeadSpectral"));
        }
    }

    // Start the Dead character in Flying mode (spectral float).
    SetMovementMode(MOVE_Flying);
    MaxFlySpeed = SpectralSpeed;
}

// ─────────────────────────────────────────────────────────────────────────────
// EnterPassThroughMode — call this on the SERVER after energy check
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::EnterPassThroughMode()
{
    if (bPassingThrough) return; // Already in this mode

    bPassingThrough = true;

    // Use MOVE_Custom with sub-mode 0 (EGhostMovementMode::PassThrough).
    // The engine replicates movement mode changes automatically, so clients
    // will see the correct mode without a separate RPC.
    SetMovementMode(MOVE_Custom, (uint8)EGhostMovementMode::PassThrough);

    // Drop collision so the capsule can overlap world geometry.
    SetCapsuleCollision(false);

    // Slow down mid-wall for tension.
    MaxFlySpeed = PassThroughSpeed;

    UE_LOG(LogTemp, Log, TEXT("[GhostMovement] Entering pass-through mode."));
}

// ─────────────────────────────────────────────────────────────────────────────
// ExitPassThroughMode
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::ExitPassThroughMode()
{
    if (!bPassingThrough) return;

    bPassingThrough = false;

    // Return to spectral flying.
    SetMovementMode(MOVE_Flying);

    // Restore world collision.
    SetCapsuleCollision(true);

    MaxFlySpeed = SpectralSpeed;

    UE_LOG(LogTemp, Log, TEXT("[GhostMovement] Exiting pass-through mode."));
}

// ─────────────────────────────────────────────────────────────────────────────
// PhysCustom
//   Called every frame while MovementMode == MOVE_Custom.
//   We handle our PassThrough sub-mode here.
//
//   KEY CONCEPT: In MOVE_Custom we are responsible for ALL position updates.
//   We apply the player's input velocity directly, plus an optional downward
//   drift so the ghost doesn't accidentally fly into the sky.
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    Super::PhysCustom(DeltaTime, Iterations);

    if (CustomMovementMode != (uint8)EGhostMovementMode::PassThrough) return;

    // ── Apply input acceleration ───────────────────────────────────────────
    // CalcVelocity handles acceleration, friction, and braking for us.
    // We pass a braking deceleration so the ghost doesn't slide forever.
    CalcVelocity(DeltaTime, /*Friction=*/0.5f, /*bFluid=*/true,
        BrakingDecelerationFlying);

    // ── Gravitational drift ────────────────────────────────────────────────
    // If the specter isn't actively moving upward, pull them gently down.
    // This prevents floating to the ceiling when the player stops moving.
    if (Velocity.Z > -GravitationalDrift && !bPassingThrough)
    {
        // Only apply outside pass-through so the ghost can freely sink
        // down through floors when intentionally passing through.
        Velocity.Z = FMath::Max(Velocity.Z - GravitationalDrift * DeltaTime,
            -GravitationalDrift);
    }

    // ── Move the character ─────────────────────────────────────────────────
    FVector Delta = Velocity * DeltaTime;
    FHitResult Hit(1.f);

    // SafeMoveUpdatedComponent respects the no-collision state we set on the
    // capsule — when collision is off it just moves freely.
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(),
        /*bSweep=*/false, Hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnMovementModeChanged
//   Called by the engine whenever SetMovementMode() is invoked.
//   We use it as a second safety net to sync speed.
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::OnMovementModeChanged(
    EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    // Sync max speed based on the new mode.
    if (MovementMode == MOVE_Flying)
    {
        MaxFlySpeed = bPassingThrough ? PassThroughSpeed : SpectralSpeed;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Network serialization
//   UCharacterMovementComponent has its own compressed flag system for
//   replicating movement state. We override UpdateFromCompressedFlags to
//   handle our custom state if needed in the future.
//   For now we just call Super — the movement mode is replicated automatically.
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    // Future: read custom bit flags here for predicted ability activation.
}

FNetworkPredictionData_Client* UGhostMovementComponent::GetPredictionData_Client() const
{
    // Use the base class prediction data — our changes don't require a custom
    // prediction struct yet. We'll add one if we implement client-side prediction
    // for the pass-through toggle (Step: networking polish).
    return Super::GetPredictionData_Client();
}

// ─────────────────────────────────────────────────────────────────────────────
// SetCapsuleCollision
//   Three collision states for the Dead capsule:
//
//   SPECTRAL (normal ghost movement):
//     Profile "DeadSpectral" — blocks the SaltBarrier custom channel so the
//     Dead cannot walk through salt lines, but still ignores world geometry
//     (the capsule floats through scenery as expected for a ghost).
//
//   PASS-THROUGH:
//     Profile "NoCollision" — the capsule generates no queries at all.
//     The Dead can move through any geometry including salt barriers.
//     Energy drain during transit is handled by the ward's Tick overlap.
//
//   RESTORE (called on ExitPassThroughMode):
//     Returns to "DeadSpectral" so salt barriers are blocking again.
//
//   COLLISION PROFILE SETUP REQUIRED IN EDITOR:
//   Edit → Project Settings → Collision → Profiles → New:
//   Name               : DeadSpectral
//   CollisionEnabled   : Query and Physics
//   Object Type        : Pawn
//   Channel responses  :
//     WorldStatic      → Ignore   (ghosts float through walls)
//     WorldDynamic     → Ignore
//     Pawn             → Ignore   (ghosts don't collide with players)
//     DeadMovement     → Block    ← the custom channel you created
//     All others       → Ignore
//
//   The SaltBarrier profile on the ward box blocks DeadMovement.
//   DeadSpectral capsule has DeadMovement = Block → they stop at salt lines.
//   NoCollision capsule has nothing → passes through freely.
// ─────────────────────────────────────────────────────────────────────────────

void UGhostMovementComponent::SetCapsuleCollision(bool bEnableCollision)
{
    ACharacter* Owner = GetCharacterOwner();
    if (!Owner) return;

    UCapsuleComponent* Capsule = Owner->GetCapsuleComponent();
    if (!Capsule) return;

    if (bEnableCollision)
    {
        // Return to spectral profile — can be stopped by salt barriers,
        // but still floats through walls and world geometry.
        Capsule->SetCollisionProfileName(TEXT("DeadSpectral"));
    }
    else
    {
        // Full pass-through — no collision whatsoever.
        // Salt wards detect transit via their overlap sphere, not this capsule.
        Capsule->SetCollisionProfileName(TEXT("NoCollision"));
    }
}

float UGhostMovementComponent::GetTargetSpeed() const
{
    return bPassingThrough ? PassThroughSpeed : SpectralSpeed;
}
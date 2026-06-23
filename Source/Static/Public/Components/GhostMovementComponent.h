#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GhostMovementComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EGhostMovementMode
//   We register one custom movement mode on top of Unreal's built-in set.
//   Unreal's EMovementMode already has: Walking, NavWalking, Falling,
//   Swimming, Flying, Custom.
//   We use MOVE_Custom with a sub-mode index so we don't break any built-in
//   movement logic. The sub-mode is stored in CustomMovementMode (a uint8).
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EGhostMovementMode : uint8
{
    // Specter moves normally (floating slightly above ground, no wall pass-through).
    Spectral    UMETA(DisplayName = "Spectral (Normal)"),

    // Specter ignores all world collision — can move through walls and floors.
    PassThrough UMETA(DisplayName = "Pass Through Walls")
};

// ─────────────────────────────────────────────────────────────────────────────
// UGhostMovementComponent
//
//   Replaces the standard UCharacterMovementComponent on ADeadCharacter.
//
//   HOW PASS-THROUGH WORKS:
//   When PassThrough mode is active we:
//   1. Switch to MOVE_Flying (no gravity, no floor constraint).
//   2. Set collision to NoCollision on the owning character's capsule.
//   3. Apply a speed penalty (ghosts slow down mid-wall for tension).
//   On exit we restore collision and switch back to MOVE_Flying (spectral float).
//
//   WHY FLYING FOR NORMAL SPECTER MODE?
//   The Dead don't walk on floors — they drift/float. MOVE_Flying gives free
//   6-DoF movement without gravity, which matches a ghost aesthetic.
//   We add a gentle gravity-like downward drift in PhysCustom to keep
//   the specter grounded near floor level unless explicitly moving upward.
//
//   ENERGY COST:
//   The component itself does NOT check energy — that's ADeadCharacter's job.
//   The character calls TryEnterPassThrough() which checks with SpecterEnergy
//   before activating the mode.
//
//   EDITOR SETUP:
//   In your ADeadCharacter Blueprint, you won't see this in the Add Component
//   list — it replaces the default movement component. This replacement is
//   set up in ADeadCharacter's constructor. You WILL see it in the component
//   panel as "CharacterMovementComponent" (same slot, different class).
//   Tune PassThroughSpeed and SpectralSpeed in the Blueprint Details panel.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API UGhostMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UGhostMovementComponent();

    // ── Overrides ─────────────────────────────────────────────────────────────

    virtual void BeginPlay() override;

    // Called by the engine every frame to run movement physics.
    // We intercept our custom sub-modes here.
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

    // Called when movement mode changes — we use it to swap collision.
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode,
        uint8 PreviousCustomMode) override;

    // Network: serialize our custom mode so clients see it correctly.
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Toggle pass-through mode ON. Call from ADeadCharacter after confirming
     * the specter has enough energy to pay the activation cost.
     * Server-authoritative: only call on the server.
     */
    UFUNCTION(BlueprintCallable, Category = "Ghost Movement")
    void EnterPassThroughMode();

    /**
     * Toggle pass-through mode OFF. Returns the specter to spectral float.
     */
    UFUNCTION(BlueprintCallable, Category = "Ghost Movement")
    void ExitPassThroughMode();

    /** Is the specter currently passing through walls? */
    UFUNCTION(BlueprintPure, Category = "Ghost Movement")
    bool IsPassingThrough() const { return bPassingThrough; }

    // ── Tuning ────────────────────────────────────────────────────────────────

    /** Normal spectral drift speed (cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Movement|Tuning",
        meta = (ClampMin = "100.0"))
    float SpectralSpeed = 500.0f;

    /** Speed while passing through walls (slower = more tension). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Movement|Tuning",
        meta = (ClampMin = "50.0"))
    float PassThroughSpeed = 250.0f;

    /**
     * How strongly the specter drifts back toward the floor when not
     * actively moving upward. 0 = fully free-floating. 200 = stays grounded.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Movement|Tuning",
        meta = (ClampMin = "0.0"))
    float GravitationalDrift = 150.0f;

    /**
     * Energy drained per second while pass-through is active.
     * Charged by ADeadCharacter's Tick via SpecterEnergyComponent.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Movement|Tuning",
        meta = (ClampMin = "0.0"))
    float PassThroughEnergyCostPerSecond = 8.0f;

private:
    // Whether we are currently in pass-through mode.
    // Not replicated directly — the movement mode change replicates automatically
    // via the built-in movement component replication.
    bool bPassingThrough = false;

    // Cache the capsule's original collision profile so we can restore it.
    FName OriginalCollisionProfileName;

    // Helpers
    void SetCapsuleCollision(bool bEnableCollision);
    float GetTargetSpeed() const;
};
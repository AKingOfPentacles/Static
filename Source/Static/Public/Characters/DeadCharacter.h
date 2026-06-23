#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Systems/GamePhaseManager.h"   // EGamePhase
#include "DeadCharacter.generated.h"

// Forward declarations
class USpecterEnergyComponent;
class UGhostMovementComponent;
class UCameraComponent;
class USpringArmComponent;
class ADeadPlayerState;

// ─────────────────────────────────────────────────────────────────────────────
// EDeadAbility
//   Identifies each of the Dead's active abilities.
//   Used as a key in the energy cost table and in the unlock bitmask.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EDeadAbility : uint8
{
    None            UMETA(DisplayName = "None"),
    Whisper         UMETA(DisplayName = "Whisper"),       // Bit 0
    Shiver          UMETA(DisplayName = "Shiver"),        // Bit 1
    Spook           UMETA(DisplayName = "Spook"),         // Bit 2
    PassThrough     UMETA(DisplayName = "Pass Through"),  // Always available
    Manifestation   UMETA(DisplayName = "Manifestation"), // Bit 3 (Phase 2+)
    FullManifest    UMETA(DisplayName = "Full Manifest")  // Bit 4 (Phase 3 only)
};

// ─────────────────────────────────────────────────────────────────────────────
// ADeadCharacter
//
//   The playable pawn for the Dead team.
//
//   KEY DIFFERENCES FROM ALivingCharacter:
//   • Uses UGhostMovementComponent (custom flying + pass-through mode).
//   • No inventory — abilities are intrinsic, not item-based.
//   • Abilities are gated by EGamePhase AND SpecterEnergy.
//   • No physical body visible by default — mesh is hidden unless manifesting.
//   • Third-person (or overhead) camera rather than first-person.
//
//   ABILITY SYSTEM (stub — full implementation in Dead Ability step):
//   Each ability is a method here that:
//   1. Checks phase unlock flag.
//   2. Calls SpecterEnergyComponent::TrySpendEnergy(cost).
//   3. If both pass, executes the effect via Server RPC.
//   The actual fear-raising calls on CardiacRhythmComponent live in the
//   ability execution — the Dead character just initiates and the server applies.
//
//   EDITOR SETUP:
//   1. Create BP_DeadCharacter from this class.
//   2. Do NOT add a UCharacterMovementComponent — it's already replaced by
//      UGhostMovementComponent in the constructor (ObjectInitializer pattern).
//   3. Add a skeletal mesh for the manifestation state — keep it hidden by
//      default (set Hidden in Game = true in the mesh component settings).
//   4. Position the camera above and slightly behind for a "haunting" angle.
//   5. In Project Settings → Input, add:
//      Actions : "AbilityWhisper", "AbilityShiver", "AbilitySpook",
//                "AbilityPassThrough", "AbilityManifest"
//      Axes    : "MoveForward", "MoveRight", "MoveUp", "LookUp", "LookRight"
//      (MoveUp lets the specter rise/sink freely in 3D space.)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ADeadCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    // ── Constructor — uses ObjectInitializer to swap the movement component ──
    //   This is the UE5 pattern for replacing a default subobject.
    //   The movement component MUST be replaced in the constructor.
    explicit ADeadCharacter(const FObjectInitializer& ObjectInitializer);

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Component accessors ───────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Dead")
    USpecterEnergyComponent* GetSpecterEnergyComponent() const { return SpecterEnergyComponent; }

    UFUNCTION(BlueprintPure, Category = "Dead")
    UGhostMovementComponent* GetGhostMovementComponent() const { return GhostMovementComponent; }

    // ── Ability interface (full implementation in Dead Ability step) ──────────

    /**
     * Attempt to activate an ability. Checks phase unlock + energy cost.
     * Returns true if the ability fired successfully.
     * Server-authoritative: client input calls a Server RPC which calls this.
     */
    UFUNCTION(BlueprintCallable, Category = "Dead|Abilities")
    bool TryActivateAbility(EDeadAbility Ability);

    /** Is a given ability currently unlocked for this phase? */
    UFUNCTION(BlueprintPure, Category = "Dead|Abilities")
    bool IsAbilityUnlocked(EDeadAbility Ability) const;

    /** Is the specter currently in pass-through mode? */
    UFUNCTION(BlueprintPure, Category = "Dead|Abilities")
    bool IsPassingThrough() const;

    // ── Phase awareness ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintNativeEvent, Category = "Dead|Phase")
    void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase);
    virtual void OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase);

    // ── Manifestation visibility ──────────────────────────────────────────────

    /**
     * Show or hide the specter's mesh.
     * Called by manifestation abilities — not called directly by input.
     * Replicated via the bIsManifested flag below.
     */
    UFUNCTION(BlueprintCallable, Category = "Dead|Manifestation")
    void SetManifested(bool bManifest);

    UFUNCTION(BlueprintPure, Category = "Dead|Manifestation")
    bool IsManifested() const { return bIsManifested; }

    // ── Burial grounds respawn ────────────────────────────────────────────────

    /**
     * Teleport the specter to the burial grounds spawn point.
     * Called by SpecterEnergyComponent's OnSpecterDepleted delegate.
     * The burial grounds actor reference is set in the Blueprint.
     */
    UFUNCTION(BlueprintCallable, Category = "Dead|Respawn")
    void RespawnAtBurialGrounds();

    /** Reference to the burial grounds actor. Set this in Blueprint or via editor. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Dead|Respawn")
    AActor* BurialGroundsActor = nullptr;

    // ── Energy costs (designer-tunable per ability) ───────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float WhisperEnergyCost = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float ShiverEnergyCost = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float SpookEnergyCost = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float ManifestEnergyCost = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float FullManifestEnergyCost = 50.0f;

    /** Energy drained per second while pass-through is held. Forwarded to GhostMovementComponent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dead|Energy Costs")
    float PassThroughCostPerSecond = 8.0f;

protected:
    // ── Components ────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dead|Components")
    USpecterEnergyComponent* SpecterEnergyComponent;

    /**
     * The ghost movement component. Declared as UGhostMovementComponent* so
     * we can call ghost-specific methods without casting every time.
     * The engine sees it as the standard CharacterMovementComponent slot.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dead|Components")
    UGhostMovementComponent* GhostMovementComponent;

    /** Third-person / overhead camera for the Dead perspective. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dead|Components")
    UCameraComponent* SpectralCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dead|Components")
    USpringArmComponent* CameraSpringArm;

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    /** Whether the specter's mesh is currently visible to the Living. */
    UPROPERTY(ReplicatedUsing = OnRep_Manifested)
    bool bIsManifested = false;

    /** Which abilities are currently unlocked (bitmask, mirrors ADeadPlayerState). */
    UPROPERTY(Replicated)
    int32 UnlockedAbilityFlags = 0;

    UFUNCTION()
    void OnRep_Manifested();

    // ── Input handlers (client-side, fire RPCs) ───────────────────────────────

    void Input_MoveForward(float Value);
    void Input_MoveRight(float Value);
    void Input_MoveUp(float Value);
    void Input_LookUp(float Value);
    void Input_LookRight(float Value);
    void Input_AbilityWhisper();
    void Input_AbilityShiver();
    void Input_AbilitySpook();
    void Input_AbilityPassThroughPressed();
    void Input_AbilityPassThroughReleased();
    void Input_AbilityManifest();

    // ── Server RPCs ───────────────────────────────────────────────────────────

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ActivateAbility(EDeadAbility Ability);
    bool Server_ActivateAbility_Validate(EDeadAbility Ability) { return true; }
    void Server_ActivateAbility_Implementation(EDeadAbility Ability);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetPassThrough(bool bEnable);
    bool Server_SetPassThrough_Validate(bool bEnable) { return true; }
    void Server_SetPassThrough_Implementation(bool bEnable);

    // ── Multicast RPCs — broadcast ability effects to all clients ─────────────
    //   These play cosmetic feedback (sounds, particles) on every machine.
    //   They do NOT apply gameplay effects — that happens server-side first.

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayWhisperEffect();
    void Multicast_PlayWhisperEffect_Implementation();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayShiverEffect();
    void Multicast_PlayShiverEffect_Implementation();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySpookEffect();
    void Multicast_PlaySpookEffect_Implementation();

    // ── Ability execution (server-only internal methods) ──────────────────────

    void Execute_Whisper();
    void Execute_Shiver();
    void Execute_Spook();
    void Execute_Manifestation();
    void Execute_FullManifestation();

    /** Apply fear to all Living players within Radius, up to MaxTargets. */
    void ApplyFearInRadius(float Radius, float FearAmount, int32 MaxTargets = 2);

    // ── Pass-through drain (server tick) ─────────────────────────────────────

    void TickPassThroughEnergy(float DeltaTime);

    // ── Delegate bindings ─────────────────────────────────────────────────────

    void BindComponentDelegates();
    void BindPhaseEvents();

    // ── Phase unlock helpers ──────────────────────────────────────────────────

    void SetAbilityUnlockFlag(EDeadAbility Ability, bool bUnlocked);
    int32 AbilityToBitIndex(EDeadAbility Ability) const;
    float GetEnergyCostForAbility(EDeadAbility Ability) const;

    // ── Typed PlayerState access ──────────────────────────────────────────────

    ADeadPlayerState* GetDeadPlayerState() const;
    

    // ── Server-only state ─────────────────────────────────────────────────────

    bool bPassThroughHeld = false; // Is the pass-through key currently held?
    
protected:
    UFUNCTION() // Obligatoire pour AddDynamic
    void TrackDepletionInPlayerState();

};
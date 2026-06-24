#pragma once

#include "CoreMinimal.h"
#include "Characters/StaticCharacterBase.h"
#include "InputActionValue.h"
#include "Systems/GamePhaseManager.h"
#include "DeadCharacter.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class UAlsCameraComponent;
class USpecterEnergyComponent;
class ADeadPlayerState;

// ─────────────────────────────────────────────────────────────────────────────
// EDeadAbility
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EDeadAbility : uint8
{
    None            UMETA(DisplayName = "None"),
    Whisper         UMETA(DisplayName = "Whisper"),       // Bit 0
    Shiver          UMETA(DisplayName = "Shiver"),        // Bit 1
    Spook           UMETA(DisplayName = "Spook"),         // Bit 2
    Manifestation   UMETA(DisplayName = "Manifestation"), // Bit 3 (Phase 2+)
    FullManifest    UMETA(DisplayName = "Full Manifest")  // Bit 4 (Phase 3 only)
};

// ─────────────────────────────────────────────────────────────────────────────
// ADeadCharacter
//
//   The playable pawn for the Dead team.
//   Inherits standard ALS movement from AStaticCharacterBase — walks exactly
//   like the Living. The ghostly feel comes from:
//   • Being invisible by default (mesh hidden, only shown when manifesting)
//   • Passing through doors via ADoorActor::PassThrough()
//   • Fear abilities (Whisper, Shiver, Spook, Manifestation)
//   • No inventory — abilities are intrinsic, gated by phase and energy
//
//   EDITOR SETUP:
//   1. Create BP_DeadCharacter from this class.
//   2. Assign all UInputAction properties in Blueprint defaults.
//   3. Add Dead input actions to your InputMappingContext asset.
//   4. Set mesh Hidden in Game = true by default.
//   5. Set BurialGroundsActor to BP_BurialGrounds in your level.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ADeadCharacter : public AStaticCharacterBase
{
    GENERATED_BODY()

    // ── ALS camera ────────────────────────────────────────────────────────────
protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Als Character Example")
    TObjectPtr<UAlsCameraComponent> Camera;

    // ── ALS input actions ─────────────────────────────────────────────────────
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputMappingContext> InputMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> LookMouseAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> WalkAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> SwitchShoulderAction;

    // ── Dead ability input actions ────────────────────────────────────────────
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> AbilityWhisperAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> AbilityShiverAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> AbilitySpookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> AbilityManifestAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> InteractAction;

    // ── Look sensitivity ──────────────────────────────────────────────────────
protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Als Character Example",
        Meta = (ClampMin = 0, ForceUnits = "x"))
    float LookUpMouseSensitivity{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Als Character Example",
        Meta = (ClampMin = 0, ForceUnits = "x"))
    float LookRightMouseSensitivity{1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Als Character Example",
        Meta = (ClampMin = 0, ForceUnits = "deg/s"))
    float LookUpRate{90.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Als Character Example",
        Meta = (ClampMin = 0, ForceUnits = "deg/s"))
    float LookRightRate{240.0f};

    // ── Lifecycle ─────────────────────────────────────────────────────────────
public:
    ADeadCharacter();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── ALS overrides ─────────────────────────────────────────────────────────
public:
    virtual void NotifyControllerChanged() override;
    virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo,
        float& Unused, float& VerticalLocation) override;

protected:
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

    // ── Component accessors ───────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintPure, Category = "Dead")
    USpecterEnergyComponent* GetSpecterEnergyComponent() const { return SpecterEnergyComponent; }

    // ── Ability interface ─────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintCallable, Category = "Dead|Abilities")
    bool TryActivateAbility(EDeadAbility Ability);

    UFUNCTION(BlueprintPure, Category = "Dead|Abilities")
    bool IsAbilityUnlocked(EDeadAbility Ability) const;

    // ── Phase awareness ───────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Dead|Phase")
    void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase);
    virtual void OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase);

    // ── Manifestation ─────────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintCallable, Category = "Dead|Manifestation")
    void SetManifested(bool bManifest);

    UFUNCTION(BlueprintPure, Category = "Dead|Manifestation")
    bool IsManifested() const { return bIsManifested; }

    // ── Burial grounds respawn ────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintCallable, Category = "Dead|Respawn")
    void RespawnAtBurialGrounds();

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Dead|Respawn")
    AActor* BurialGroundsActor = nullptr;

    // ── Energy costs ──────────────────────────────────────────────────────────
public:
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

    // ── Components ────────────────────────────────────────────────────────────
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dead|Components")
    USpecterEnergyComponent* SpecterEnergyComponent;

    // ── Replicated state ──────────────────────────────────────────────────────
private:
    UPROPERTY(ReplicatedUsing = OnRep_Manifested)
    bool bIsManifested = false;

    UPROPERTY(Replicated)
    int32 UnlockedAbilityFlags = 0;

    UFUNCTION()
    void OnRep_Manifested();

    // ── ALS input handlers ────────────────────────────────────────────────────
private:
    void Input_OnLookMouse(const FInputActionValue& ActionValue);
    void Input_OnLook(const FInputActionValue& ActionValue);
    void Input_OnMove(const FInputActionValue& ActionValue);
    void Input_OnSprint(const FInputActionValue& ActionValue);
    void Input_OnWalk();
    void Input_OnCrouch();
    void Input_OnSwitchShoulder();

    // ── Ability input handlers ────────────────────────────────────────────────
private:
    void Input_AbilityWhisper();
    void Input_AbilityShiver();
    void Input_AbilitySpook();
    void Input_AbilityManifest();
    void Input_Interact();

    // ── Server RPCs ───────────────────────────────────────────────────────────
private:
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ActivateAbility(EDeadAbility Ability);
    bool Server_ActivateAbility_Validate(EDeadAbility Ability) { return true; }
    void Server_ActivateAbility_Implementation(EDeadAbility Ability);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_Interact(FVector TraceStart, FVector TraceEnd);
    bool Server_Interact_Validate(FVector TraceStart, FVector TraceEnd) { return true; }
    void Server_Interact_Implementation(FVector TraceStart, FVector TraceEnd);

    // ── Multicast RPCs ────────────────────────────────────────────────────────
private:
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayWhisperEffect();
    void Multicast_PlayWhisperEffect_Implementation();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayShiverEffect();
    void Multicast_PlayShiverEffect_Implementation();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySpookEffect();
    void Multicast_PlaySpookEffect_Implementation();

    // ── Ability execution (server-only) ──────────────────────────────────────
private:
    void Execute_Whisper();
    void Execute_Shiver();
    void Execute_Spook();
    void Execute_Manifestation();
    void Execute_FullManifestation();
    void ApplyFearInRadius(float Radius, float FearAmount, int32 MaxTargets = 2);

    // ── Helpers ───────────────────────────────────────────────────────────────
private:
    void BindComponentDelegates();
    void BindPhaseEvents();
    void SetAbilityUnlockFlag(EDeadAbility Ability, bool bUnlocked);
    int32 AbilityToBitIndex(EDeadAbility Ability) const;
    float GetEnergyCostForAbility(EDeadAbility Ability) const;
    ADeadPlayerState* GetDeadPlayerState() const;

    UFUNCTION()
    void HandleSpecterDepleted();
};
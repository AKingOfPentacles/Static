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
    Whisper         UMETA(DisplayName = "Whisper"),
    Shiver          UMETA(DisplayName = "Shiver"),
    Spook           UMETA(DisplayName = "Spook"),
    Manifestation   UMETA(DisplayName = "Manifestation"),
    FullManifest    UMETA(DisplayName = "Full Manifest")
};

// ─────────────────────────────────────────────────────────────────────────────
// ADeadCharacter
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
    virtual void Tick(float DeltaTime) override;
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

    // ── Floating locomotion mode (door pass-through) ──────────────────────────
public:
    /**
     * Enter Floating mode — called by ADoorActor during pass-through.
     * Dead glides smoothly to EntryPosition first, then to ExitPosition.
     * Movement driven by Tick on both server and client — no timer injection.
     * ADoorActor's completion timer calls StopFloating() and restores input.
     */
    UFUNCTION(BlueprintCallable, Category = "Dead|Floating")
    void StartFloating(FVector EntryPosition, FVector ExitPosition,
        FRotator ExitRotation, float Speed = 200.0f);

    /** Exit Floating mode — restores normal ALS locomotion. */
    UFUNCTION(BlueprintCallable, Category = "Dead|Floating")
    void StopFloating();

    UFUNCTION(BlueprintPure, Category = "Dead|Floating")
    bool IsFloating() const { return bIsFloating; }

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

    // ── Floating state ────────────────────────────────────────────────────────
private:
    // Replicated so clients run the same Tick movement logic.
    UPROPERTY(ReplicatedUsing = OnRep_FloatState)
    bool bIsFloating = false;

    UPROPERTY(Replicated)
    FVector FloatWaypoint1 = FVector::ZeroVector; // Entry point

    UPROPERTY(Replicated)
    FVector FloatWaypoint2 = FVector::ZeroVector; // Exit point

    UPROPERTY(Replicated)
    FRotator FloatExitRotation = FRotator::ZeroRotator; // Exit point rotation

    UPROPERTY(Replicated)
    float FloatForwardSpeed = 0.0f;

    // Current waypoint index — not replicated, each machine tracks independently.
    int32 FloatWaypointIndex = 0;

    // Timer handles kept for cleanup only (no movement driven from timers).
    FTimerHandle FloatTimerHandle;

    UFUNCTION()
    void OnRep_FloatState();

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

    // ── Ability execution ─────────────────────────────────────────────────────
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
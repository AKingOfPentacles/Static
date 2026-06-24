#pragma once

#include "CoreMinimal.h"
#include "Characters/StaticCharacterBase.h"
#include "InputActionValue.h"
#include "Items/ItemBase.h"
#include "Systems/GamePhaseManager.h"
#include "LivingCharacter.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class UAlsCameraComponent;
class UCardiacRhythmComponent;
class UInventoryComponent;
class ALivingPlayerState;

// ─────────────────────────────────────────────────────────────────────────────
// ALivingCharacter
//
//   The playable pawn for the Living team.
//   Inherits ALS movement, camera, and animation from AStaticCharacterBase.
//   Adds: cardiac rhythm, inventory, interact/use-item, phase awareness.
//
//   ALS owns: camera (UAlsCameraComponent), movement, input for move/look/sprint.
//   We own : interact, use item, cycle item, cardiac, inventory.
//
//   EDITOR SETUP:
//   In BP_LivingCharacter, assign all UInputAction properties under
//   "Settings|Als Character Example" — same as the ALS example character.
//   Add your Static Mansion input actions (Interact, UseItem, CycleItem)
//   to your InputMappingContext asset.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ALivingCharacter : public AStaticCharacterBase
{
    GENERATED_BODY()

    // ── ALS camera ────────────────────────────────────────────────────────────
protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Als Character Example")
    TObjectPtr<UAlsCameraComponent> Camera;

    // ── ALS input actions — assigned in Blueprint ─────────────────────────────
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
    TObjectPtr<UInputAction> AimAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> RagdollAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> RollAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> RotationModeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> ViewModeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> SwitchShoulderAction;

    // ── Static Mansion input actions — assigned in Blueprint ──────────────────
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> UseItemAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> CycleItemNextAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character Example",
        Meta = (DisplayThumbnail = false))
    TObjectPtr<UInputAction> CycleItemPrevAction;

    // ── ALS look sensitivity ──────────────────────────────────────────────────
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
    ALivingCharacter();

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
    UFUNCTION(BlueprintPure, Category = "Living")
    UCardiacRhythmComponent* GetCardiacComponent() const { return CardiacRhythmComponent; }

    UFUNCTION(BlueprintPure, Category = "Living")
    UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    // ── Gameplay actions ──────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void Interact();

    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void UseCurrentItem();

    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void CycleItemNext();

    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void CycleItemPrev();

    UFUNCTION(BlueprintPure, Category = "Living|Interaction")
    EItemType GetSelectedItemType() const { return SelectedItemType; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Living|Interaction",
        meta = (ClampMin = "50.0"))
    float InteractRange = 200.0f;

    // ── Phase awareness ───────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Living|Phase")
    void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase);
    virtual void OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase);

    // ── Fear callbacks ────────────────────────────────────────────────────────
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Living|Fear")
    void OnHeartPain();
    virtual void OnHeartPain_Implementation();

    UFUNCTION(BlueprintNativeEvent, Category = "Living|Fear")
    void OnFlee();
    virtual void OnFlee_Implementation();

    // ── Components ────────────────────────────────────────────────────────────
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    UCardiacRhythmComponent* CardiacRhythmComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    UInventoryComponent* InventoryComponent;

    // ── Replicated state ──────────────────────────────────────────────────────
private:
    UPROPERTY(ReplicatedUsing = OnRep_SelectedItemType)
    EItemType SelectedItemType = EItemType::None;

    UFUNCTION()
    void OnRep_SelectedItemType();

    // ── ALS input handlers ────────────────────────────────────────────────────
private:
    void Input_OnLookMouse(const FInputActionValue& ActionValue);
    void Input_OnLook(const FInputActionValue& ActionValue);
    void Input_OnMove(const FInputActionValue& ActionValue);
    void Input_OnSprint(const FInputActionValue& ActionValue);
    void Input_OnWalk();
    void Input_OnCrouch();
    void Input_OnAim(const FInputActionValue& ActionValue);
    void Input_OnRagdoll();
    void Input_OnRoll();
    void Input_OnRotationMode();
    void Input_OnViewMode();
    void Input_OnSwitchShoulder();

    // ── Static Mansion input handlers ─────────────────────────────────────────
private:
    void Input_Interact();
    void Input_UseItem();
    void Input_CycleNext();
    void Input_CyclePrev();

    // ── Server RPCs ───────────────────────────────────────────────────────────
private:
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_Interact(FVector TraceStart, FVector TraceEnd);
    bool Server_Interact_Validate(FVector TraceStart, FVector TraceEnd) { return true; }
    void Server_Interact_Implementation(FVector TraceStart, FVector TraceEnd);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UseItem(EItemType ItemType, FVector TraceStart, FVector TraceEnd);
    bool Server_UseItem_Validate(EItemType ItemType, FVector TraceStart, FVector TraceEnd) { return true; }
    void Server_UseItem_Implementation(EItemType ItemType, FVector TraceStart, FVector TraceEnd);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetSelectedItem(EItemType NewItem);
    bool Server_SetSelectedItem_Validate(EItemType NewItem) { return true; }
    void Server_SetSelectedItem_Implementation(EItemType NewItem);

    // ── Helpers ───────────────────────────────────────────────────────────────
private:
    bool GetInteractionTrace(FHitResult& OutHit) const;
    ALivingPlayerState* GetLivingPlayerState() const;
    void BindComponentDelegates();
    void BindPhaseEvents();

    int32 HotbarIndex = 0;
    bool bIsFleeing = false;
};
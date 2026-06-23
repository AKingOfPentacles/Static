#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/ItemBase.h"                 // EItemType
#include "Systems/GamePhaseManager.h"        // EGamePhase
#include "LivingCharacter.generated.h"

// Forward declarations — avoids circular includes and speeds compile times
class UCardiacRhythmComponent;
class UInventoryComponent;
class UCameraComponent;
class USpringArmComponent;
class ALivingPlayerState;

// ─────────────────────────────────────────────────────────────────────────────
// ALivingCharacter
//
//   The playable pawn for the Living team.
//   Uses standard UCharacterMovementComponent (no custom movement needed).
//
//   WHAT THIS CLASS OWNS:
//   • UCardiacRhythmComponent  — fear/health system
//   • UInventoryComponent      — item slots
//   • First-person camera      — standard FPS setup
//   • Interaction logic        — line-trace interact / use-item
//   • Input handling           — Move, Look, Interact, UseItem, CycleItem
//
//   WHAT THIS CLASS DOES NOT OWN:
//   • Ward spawning logic      — that lives in the item subclasses (USaltItem, etc.)
//   • Flashlight bulb actor    — spawned by UFlashlightItem
//   • Phase gating             — components listen to UGamePhaseManager directly
//
//   EDITOR SETUP:
//   1. Create a Blueprint child: right-click Content Browser →
//      Blueprint Class → search ALivingCharacter → name it BP_LivingCharacter.
//   2. Assign a skeletal mesh in the Mesh slot.
//   3. The Camera and SpringArm are created in C++ — you'll see them in the
//      Blueprint component list, just position them.
//   4. In Project Settings → Input, create the following Action/Axis mappings:
//      Actions : "Interact", "UseItem", "CycleItemNext", "CycleItemPrev"
//      Axes    : "MoveForward", "MoveRight", "LookUp", "LookRight"
//      (Step: InputConfig will be explained after this file.)
//   5. Set the GameMode's Default Pawn Class to BP_LivingCharacter for
//      Living players. Dead players use BP_DeadCharacter.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ALivingCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ALivingCharacter();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Component accessors (Blueprint-readable) ──────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Living")
    UCardiacRhythmComponent* GetCardiacComponent() const { return CardiacRhythmComponent; }

    UFUNCTION(BlueprintPure, Category = "Living")
    UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    // ── Interaction ───────────────────────────────────────────────────────────

    /**
     * Perform a context-sensitive interaction (open door, pick up item, etc.).
     * SERVER RPC — called from input binding, executed on server.
     */
    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void Interact();

    /**
     * Use the currently selected inventory item.
     * Fires a line trace to get a world HitResult, then calls
     * InventoryComponent::TryUseItem on the server.
     */
    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void UseCurrentItem();

    /** Cycle to the next item in the inventory hotbar. */
    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void CycleItemNext();

    /** Cycle to the previous item in the inventory hotbar. */
    UFUNCTION(BlueprintCallable, Category = "Living|Interaction")
    void CycleItemPrev();

    /** Currently selected item type (replicated so other clients can see held item). */
    UFUNCTION(BlueprintPure, Category = "Living|Interaction")
    EItemType GetSelectedItemType() const { return SelectedItemType; }

    // ── Interaction trace settings ────────────────────────────────────────────

    /** How far in front of the camera the interaction line trace reaches (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Living|Interaction",
        meta = (ClampMin = "50.0"))
    float InteractRange = 200.0f;

    // ── Phase awareness ───────────────────────────────────────────────────────

    /**
     * Called by UGamePhaseManager::OnPhaseChanged.
     * Living characters may restrict or enable actions based on phase.
     * Bind this in BeginPlay.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Living|Phase")
    void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase);
    virtual void OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase);

    // ── Fear / flee callbacks (wired to CardiacRhythmComponent) ──────────────

    /**
     * Fired when a Heart Pain event occurs.
     * Override in Blueprint to add camera shake, audio sting, vignette flash.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Living|Fear")
    void OnHeartPain();
    virtual void OnHeartPain_Implementation();

    /**
     * Fired when the player is forced to flee (3 Heart Pain events).
     * Override in Blueprint to trigger flee animation, spectate mode, etc.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Living|Fear")
    void OnFlee();
    virtual void OnFlee_Implementation();

protected:
    // ── Components ────────────────────────────────────────────────────────────

    /** Fear / heart rate system. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    UCardiacRhythmComponent* CardiacRhythmComponent;

    /** Item inventory. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    UInventoryComponent* InventoryComponent;

    /** First-person camera. Position this in the Blueprint to sit at eye level. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    UCameraComponent* FirstPersonCamera;

    /**
     * Spring arm for the camera — optional but useful for adding subtle
     * camera lag and collision avoidance later.
     *
     * NOTE: For a pure first-person game you can attach the camera directly
     * to the capsule/mesh without a spring arm. We include it here because
     * it costs nothing and gives designers flexibility.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Living|Components")
    USpringArmComponent* CameraSpringArm;

private:
    // ── Replicated state ──────────────────────────────────────────────────────

    /** Which item slot is currently active. Replicated so others see held items. */
    UPROPERTY(ReplicatedUsing = OnRep_SelectedItemType)
    EItemType SelectedItemType = EItemType::None;

    UFUNCTION()
    void OnRep_SelectedItemType();

    // ── Input handlers (local client only, fire RPCs) ─────────────────────────

    void Input_MoveForward(float Value);
    void Input_MoveRight(float Value);
    void Input_LookUp(float Value);
    void Input_LookRight(float Value);
    void Input_Interact();
    void Input_UseItem();
    void Input_CycleNext();
    void Input_CyclePrev();

    // ── Server RPCs ───────────────────────────────────────────────────────────
    //   Input is LOCAL — the server must validate and execute gameplay.
    //   Each input action goes through a Server_ RPC.

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

    /** Fire a line trace from the camera. Returns true if something was hit. */
    bool GetInteractionTrace(FHitResult& OutHit) const;

    /** Typed access to our PlayerState without a repeated cast. */
    ALivingPlayerState* GetLivingPlayerState() const;

    /** Wire component delegates to our callback functions. */
    void BindComponentDelegates();

    /** Subscribe to the phase manager. */
    void BindPhaseEvents();

    // ── Internal state (server-only) ──────────────────────────────────────────

    /** Index into the inventory array for cycling. */
    int32 HotbarIndex = 0;

    /** True while flee animation/transition is running (suppresses further input). */
    bool bIsFleeing = false;
};
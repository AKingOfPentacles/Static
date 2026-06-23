#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatchFlameActor.generated.h"

class UPointLightComponent;
class UParticleSystemComponent;

// ─────────────────────────────────────────────────────────────────────────────
// AMatchFlameActor
//
//   A small flickering flame spawned when a Living player strikes a match.
//   Attaches to the player's hand socket and burns for FlameDuration seconds
//   before extinguishing automatically.
//
//   This is the primary Phase 2 light source alongside the Flashlight.
//   Unlike the flashlight, matches are:
//   • Consumed on use (one match = one flame, no toggle)
//   • Short-lived (default 30 seconds)
//   • Extinguished if the Dead uses Shiver or Spook nearby (cold-breath effect)
//   • Stackable (players carry up to 5)
//
//   The flame attaches to the character via a socket named "MatchSocket".
//   Add this socket to your ALivingCharacter skeletal mesh at roughly
//   the right-hand position.
//
//   EXTINGUISH ON SCARE:
//   CardiacRhythmComponent broadcasts OnHeartPainEvent. AMatchFlameActor
//   listens to this and calls Extinguish() — a bad scare literally makes
//   your hand shake the flame out. Very cheap to implement, high horror value.
//
//   EDITOR SETUP:
//   1. Create BP_MatchFlame from this class.
//   2. Assign a small fire particle system to FlameParticles.
//   3. Tune FlameDuration (default 30s), LightRadius, LightIntensity.
//   4. Assign to UMatchItem.MatchFlameClass.
//   5. Add a "MatchSocket" socket to your Living character's hand bone.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API AMatchFlameActor : public AActor
{
    GENERATED_BODY()

public:
    AMatchFlameActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Attach to a Living character's hand socket and start the burn timer.
     * Called by UMatchItem::UseItem_Implementation immediately after spawn.
     */
    UFUNCTION(BlueprintCallable, Category = "Match Flame")
    void IgniteOn(AActor* IgniteOwner);

    /**
     * Extinguish the flame early — called on Heart Pain or by the Dead's
     * Shiver ability when within snuff range.
     */
    UFUNCTION(BlueprintCallable, Category = "Match Flame")
    void Extinguish();

    UFUNCTION(BlueprintPure, Category = "Match Flame")
    bool IsBurning() const { return bIsBurning; }

    // ── Configuration ─────────────────────────────────────────────────────────

    /** How long the match burns (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Flame",
        meta = (ClampMin = "5.0"))
    float FlameDuration = 30.0f;

    /** Radius of the point light cast by the flame (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Flame",
        meta = (ClampMin = "50.0"))
    float LightRadius = 300.0f;

    /** Intensity of the point light (candelas). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Flame",
        meta = (ClampMin = "0.0"))
    float LightIntensity = 800.0f;

    /**
     * Name of the socket on the Living character's skeletal mesh to attach to.
     * Default: "MatchSocket" — add this socket to your hand bone in the mesh editor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Flame")
    FName AttachSocketName = FName("MatchSocket");

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Flame|Components")
    UPointLightComponent* FlameLight;

    /**
     * Optional particle system for the flame VFX.
     * Assign a fire particle in Blueprint — works without one (light only).
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match Flame|Components")
    UParticleSystemComponent* FlameParticles;

private:
    UPROPERTY(ReplicatedUsing = OnRep_IsBurning)
    bool bIsBurning = false;

    UFUNCTION()
    void OnRep_IsBurning();

    FTimerHandle BurnTimerHandle;
    FTimerHandle FlickerTimerHandle;

    // Flicker simulation — randomly varies light intensity to simulate a real flame.
    void TickFlicker();

    // Reference to the owner for Heart Pain binding.
    UPROPERTY()
    AActor* FlameOwner = nullptr;

    // Bound to the owner's CardiacRhythmComponent::OnHeartPainEvent.
    UFUNCTION()
    void OnOwnerHeartPain();

    void SetFlameVisible(bool bVisible);
};
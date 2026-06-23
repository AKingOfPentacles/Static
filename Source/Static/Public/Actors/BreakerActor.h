#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Systems/GamePhaseManager.h"
#include "BreakerActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;

// ─────────────────────────────────────────────────────────────────────────────
// ABreakerActor
//
//   The fuse box / breaker panel in the mansion.
//   At the Exploration → Protection transition, the breaker TRIPS automatically
//   (server-side via OnPhaseChanged) cutting all managed lights.
//
//   The Living can reset it via Interact() — but the Dead can trip it again
//   via a tagged interactive prop (Move Object ability, built in a later step).
//
//   MANAGED LIGHTS:
//   Place any number of point/spot lights in the level and add them to the
//   ManagedLights array in the Blueprint Details. The breaker controls their
//   enabled state. This avoids having one global "lights off" that you can't
//   tune per-room.
//
//   EDITOR SETUP:
//   1. Create BP_Breaker from this class.
//   2. Assign a mesh (fuse box prop).
//   3. In the ManagedLights array, reference all ceiling lights in the mansion.
//   4. Set BreakerResetTime if you want the Dead to be able to re-trip it on
//      a cooldown after the Living reset it.
//   5. Tag the actor "Breaker" for Dead ability targeting (Move Object step).
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ABreakerActor : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    ABreakerActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── IInteractable ─────────────────────────────────────────────────────────

    virtual bool Interact_Implementation(
        AActor* Interactor, const FHitResult& HitResult) override;
    virtual FText GetInteractPrompt_Implementation() const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Breaker")
    bool IsTripped() const { return bIsTripped; }

    /** Trip the breaker (cut power). Called automatically on Protection phase. */
    UFUNCTION(BlueprintCallable, Category = "Breaker")
    void TripBreaker();

    /** Reset the breaker (restore power). Called by Living via Interact(). */
    UFUNCTION(BlueprintCallable, Category = "Breaker")
    void ResetBreaker();

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * All lights this breaker controls.
     * Assign in the Blueprint Details panel by dragging level actors here.
     *
     * EDITOR TIP: Use the eyedropper in the array element to pick lights
     * directly from the viewport.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Breaker")
    TArray<AActor*> ManagedLights;

    /** Seconds the Living must hold the interact button to reset the breaker.
     *  0 = instant reset. A short hold (2–3s) adds tension. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breaker",
        meta = (ClampMin = "0.0"))
    float ResetHoldTime = 2.0f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breaker|Components")
    UStaticMeshComponent* BreakerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breaker|Components")
    UBoxComponent* InteractionVolume;

    /** Small indicator light on the breaker panel itself — red when tripped. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breaker|Components")
    UPointLightComponent* IndicatorLight;

private:
    UPROPERTY(ReplicatedUsing = OnRep_IsTripped)
    bool bIsTripped = false;

    UFUNCTION()
    void OnRep_IsTripped();

    UFUNCTION()
    void OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase);

    void SetManagedLightsEnabled(bool bEnabled);
};
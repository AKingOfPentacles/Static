#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UInteractable / IInteractable
//
//   UE5 interfaces come in pairs:
//   • UInteractable  — the UObject boilerplate (never used directly)
//   • IInteractable  — the actual interface you implement and cast to
//
//   HOW TO IMPLEMENT ON AN ACTOR:
//   1. Add IInteractable to the class declaration:
//        class STATIC_API AMyActor : public AActor, public IInteractable
//   2. Override Interact_Implementation in the .cpp.
//   3. That's it — ALivingCharacter's interact trace will find it automatically.
//
//   HOW TO CHECK IN BLUEPRINTS:
//   Right-click an actor reference → "Does Implement Interface" → Interactable.
//   Or call the "Interact" message node directly — Unreal routes it correctly.
// ─────────────────────────────────────────────────────────────────────────────

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
    GENERATED_BODY()
};

class STATIC_API IInteractable
{
    GENERATED_BODY()

public:
    /**
     * Called by ALivingCharacter when the player presses Interact while
     * looking at this actor.
     *
     * Interactor : the Living character who initiated the interaction.
     * HitResult  : the trace result — contains hit location, normal, component.
     *              Use HitResult.Location for placement effects.
     *
     * Returns true if the interaction succeeded (used for audio/VFX feedback).
     * SERVER ONLY — never call on a client directly.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool Interact(AActor* Interactor, const FHitResult& HitResult);

    /**
     * Returns the prompt string shown on the Living player's crosshair HUD
     * when they look at this actor. E.g. "Open door", "Pick up match".
     * Safe to call on clients (read-only).
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractPrompt() const;

    /**
     * Can this actor be interacted with right now?
     * Checked before showing the HUD prompt and before calling Interact().
     * E.g. a door that is locked returns false; a burned-out candle returns false.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(AActor* Interactor) const;
};
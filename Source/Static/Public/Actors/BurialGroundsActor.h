#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interactable.h"
#include "Systems/GamePhaseManager.h"
#include "BurialGroundsActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UDecalComponent;

// ─────────────────────────────────────────────────────────────────────────────
// ABurialGroundsActor
//
//   The ritual site in the mansion where the Living must burn the Pile of Bones
//   to achieve absolution and win the match.
//
//   HOW ABSOLUTION WORKS:
//   1. A Living player must be inside the AbsolutionRadius AND holding the
//      Pile of Bones item AND be in Phase 3 (Confrontation).
//   2. They press UseItem — UBonesItem::UseItem_Implementation calls
//      AttemptAbsolution(Interactor) on this actor.
//   3. AttemptAbsolution validates all conditions, then calls
//      UGamePhaseManager::TriggerLivingVictory().
//
//   Also serves as the Dead character's respawn point (referenced from
//   ADeadCharacter::BurialGroundsActor — set that reference in your level).
//
//   EDITOR SETUP:
//   1. Place one BP_BurialGrounds in the deepest/hardest-to-reach part of the mansion.
//   2. Set AbsolutionRadius to the ritual circle size.
//   3. Assign a floor decal (ritual circle marking).
//   4. In each ADeadCharacter Blueprint instance in the level, assign this
//      actor to the BurialGroundsActor reference slot.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class STATIC_API ABurialGroundsActor : public AActor
{
    GENERATED_BODY()

public:
    ABurialGroundsActor();

    virtual void BeginPlay() override;

    /**
     * Called by UBonesItem::UseItem_Implementation when a Living player
     * uses the Pile of Bones while inside the burial grounds.
     * Validates phase and proximity, then triggers victory if valid.
     * Returns true if absolution succeeded.
     */
    UFUNCTION(BlueprintCallable, Category = "BurialGrounds")
    bool AttemptAbsolution(AActor* Interactor);

    /**
     * Is the given actor currently inside the absolution radius?
     * Called by UBonesItem::CanUse() so the item greys out when not at the site.
     */
    UFUNCTION(BlueprintPure, Category = "BurialGrounds")
    bool IsActorInRange(AActor* Actor) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BurialGrounds",
        meta = (ClampMin = "50.0"))
    float AbsolutionRadius = 150.0f;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BurialGrounds|Components")
    USphereComponent* AbsolutionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BurialGrounds|Components")
    UDecalComponent* GroundsDecal;

private:
    bool bAbsolved = false; // Prevent double-trigger.
};
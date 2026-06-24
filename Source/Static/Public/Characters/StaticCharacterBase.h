#pragma once

#include "CoreMinimal.h"
#include "AlsCharacter.h"
#include "StaticCharacterBase.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// AStaticCharacterBase
//
//   The shared base class for both ALivingCharacter and ADeadCharacter.
//   Sits between AAlsCharacter and our game-specific classes.
//
//   RESPONSIBILITIES:
//   Anything that both the Living AND the Dead share belongs here:
//   • Any future shared gameplay state (team ID, match score, etc.)
//   • Any shared utility methods
//   • The AutoExpandCategories so Blueprint details are readable
//
//   WHAT DOES NOT GO HERE:
//   • Cardiac rhythm (Living only)
//   • Specter energy (Dead only)
//   • Inventory (Living only)
//   • Ghost movement (Dead only)
//   • Any ability or item logic
//
//   ALS INTEGRATION NOTE:
//   AAlsCharacter handles all of the following — do NOT duplicate in subclasses:
//   • Skeletal mesh and animation
//   • Movement (walk, sprint, crouch, roll, ragdoll)
//   • Camera via UAlsCameraComponent
//   • Input via UInputMappingContext + UInputAction
//   • Rotation modes (velocity direction, aiming)
//   Our subclasses simply add their own UInputAction properties and bind them
//   in SetupPlayerInputComponent alongside the ALS bindings.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(AutoExpandCategories = ("Settings|Als Static Character", "State|Als Character Example"))
class STATIC_API AStaticCharacterBase : public AAlsCharacter
{
	GENERATED_BODY()

public:
	// Default constructor — used by ALivingCharacter.
	AStaticCharacterBase();

	// ObjectInitializer constructor — used by ADeadCharacter so it can replace
	// the default movement component slot via ObjectInitializer.SetDefaultSubobjectClass.
	// Without this, the compiler cannot pass FObjectInitializer up the chain.
	explicit AStaticCharacterBase(const FObjectInitializer& ObjectInitializer);
};
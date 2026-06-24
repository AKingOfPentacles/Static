#include "Characters/StaticCharacterBase.h"

AStaticCharacterBase::AStaticCharacterBase()
{
	// AAlsCharacter sets up everything for movement and animation.
}

AStaticCharacterBase::AStaticCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Passes ObjectInitializer up to AAlsCharacter so subclasses can
	// replace default subobjects (e.g. the movement component slot).
}
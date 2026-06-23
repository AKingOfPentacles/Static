#include "Items/ItemBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// UseItem_Implementation
//   The base class does nothing — subclasses override this.
//   We return false here so that an unimplemented item doesn't silently
//   consume itself from the inventory.
// ─────────────────────────────────────────────────────────────────────────────

bool UItemBase::UseItem_Implementation(AActor* User, const FHitResult& HitResult)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[ItemBase] UseItem called on base class for item type %d. Did you forget to override UseItem in your subclass?"),
		(int32)ItemType);
	return false;
}
#include "Player/DeadPlayerState.h"
#include "Net/UnrealNetwork.h"

ADeadPlayerState::ADeadPlayerState()
{
}

void ADeadPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADeadPlayerState, MemorabiliaCollected);
	DOREPLIFETIME(ADeadPlayerState, DepletionCount);
	DOREPLIFETIME(ADeadPlayerState, AbilityUnlockFlags);
}

void ADeadPlayerState::AddMemorabiliaCollected(int32 Amount)
{
	if (!HasAuthority()) return;
	MemorabiliaCollected += Amount;
	UE_LOG(LogTemp, Log, TEXT("[DeadPlayerState] %s collected memorabilia. Total: %d"),
		*GetPlayerName(), MemorabiliaCollected);
}

void ADeadPlayerState::IncrementDepletionCount()
{
	if (!HasAuthority()) return;
	DepletionCount++;
}

void ADeadPlayerState::SetAbilityUnlockFlags(int32 Flags)
{
	if (!HasAuthority()) return;
	AbilityUnlockFlags = Flags;
	UE_LOG(LogTemp, Log, TEXT("[DeadPlayerState] %s ability flags set to: %d"),
		*GetPlayerName(), Flags);
}

void ADeadPlayerState::OnRep_AbilityUnlockFlags()
{
	// Owning client can react — e.g. animate new ability slots appearing in the HUD.
	UE_LOG(LogTemp, Log, TEXT("[DeadPlayerState] OnRep: ability flags updated to %d"),
		AbilityUnlockFlags);
}
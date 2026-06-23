#include "Player/LivingPlayerState.h"
#include "Net/UnrealNetwork.h"

ALivingPlayerState::ALivingPlayerState()
{
}

void ALivingPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// These replicate to ALL clients — spectators can see who has fled, etc.
	DOREPLIFETIME(ALivingPlayerState, HeartPainCount);
	DOREPLIFETIME(ALivingPlayerState, bHasFled);
	DOREPLIFETIME(ALivingPlayerState, bHasAbsolved);
}

void ALivingPlayerState::IncrementHeartPainCount()
{
	if (!HasAuthority()) return;
	HeartPainCount++;
	UE_LOG(LogTemp, Log, TEXT("[LivingPlayerState] %s heart pain count: %d"),
		*GetPlayerName(), HeartPainCount);
}

void ALivingPlayerState::SetFled()
{
	if (!HasAuthority()) return;
	bHasFled = true;
	UE_LOG(LogTemp, Warning, TEXT("[LivingPlayerState] %s has fled!"), *GetPlayerName());
}

void ALivingPlayerState::SetAbsolved()
{
	if (!HasAuthority()) return;
	bHasAbsolved = true;
}

void ALivingPlayerState::OnRep_Fled()
{
	// Clients can react to this — e.g. grey out this player's portrait in the HUD.
	UE_LOG(LogTemp, Log, TEXT("[LivingPlayerState] OnRep: %s has fled."), *GetPlayerName());
}
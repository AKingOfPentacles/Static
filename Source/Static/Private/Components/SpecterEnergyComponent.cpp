#include "Components/SpecterEnergyComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

USpecterEnergyComponent::USpecterEnergyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::BeginPlay()
{
    Super::BeginPlay();

    // Dead players start at zero — they must collect Memorabilia in Phase 1.
    CurrentEnergy = 0.0f;
    bInDepletionPenalty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Energy is private to the owning client — opponents shouldn't see exact values.
    DOREPLIFETIME_CONDITION(USpecterEnergyComponent, CurrentEnergy, COND_OwnerOnly);

    // Penalty flag: replicate to all (spectators, future team HUDs).
    DOREPLIFETIME(USpecterEnergyComponent, bInDepletionPenalty);
}

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent — passive regen (server only)
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // Passive regen only when NOT in depletion penalty.
    // Phase-gating (e.g. "no regen in Phase 1") can be layered here later
    // by checking UGamePhaseManager::GetCurrentPhase().
    if (!bInDepletionPenalty && CurrentEnergy < GetCurrentEnergyCap())
    {
        GainEnergy(PassiveRegenRate * DeltaTime);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TrySpendEnergy
//   Returns true and deducts cost if available. Returns false otherwise.
//   Dead ability classes call this before executing any ability.
// ─────────────────────────────────────────────────────────────────────────────

bool USpecterEnergyComponent::TrySpendEnergy(float Cost)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Cost <= 0.0f) return true; // Free abilities always succeed.

    if (CurrentEnergy < Cost)
    {
        UE_LOG(LogTemp, Log, TEXT("[Specter] Not enough energy. Have: %.1f, Need: %.1f"),
            CurrentEnergy, Cost);
        return false;
    }

    SetEnergy(CurrentEnergy - Cost);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// GainEnergy
//   Clamps to the current cap (MaxEnergy or PenaltyEnergyCap).
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::GainEnergy(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (Amount <= 0.0f) return;

    const float Cap = GetCurrentEnergyCap();
    SetEnergy(FMath::Min(CurrentEnergy + Amount, Cap));
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyDefenseHit
//   Called when the Dead contacts a ward (Salt line, Sage zone, Occult item).
//   DefenseStrength is set by the ward actor (different wards drain differently).
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::ApplyDefenseHit(float DefenseStrength)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bInDepletionPenalty) return; // Already depleted, ignore.

    UE_LOG(LogTemp, Log, TEXT("[Specter] Defense hit! Draining %.1f energy."), DefenseStrength);

    const float NewEnergy = CurrentEnergy - DefenseStrength;

    if (NewEnergy <= 0.0f)
    {
        SetEnergy(0.0f);
        TriggerDepletion();
    }
    else
    {
        SetEnergy(NewEnergy);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HasEnoughEnergy — pure query, safe on any machine
// ─────────────────────────────────────────────────────────────────────────────

bool USpecterEnergyComponent::HasEnoughEnergy(float Cost) const
{
    return CurrentEnergy >= Cost;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetNormalizedEnergy
// ─────────────────────────────────────────────────────────────────────────────

float USpecterEnergyComponent::GetNormalizedEnergy() const
{
    if (MaxEnergy <= 0.0f) return 0.0f;
    return FMath::Clamp(CurrentEnergy / MaxEnergy, 0.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// TriggerDepletion — server only
//   Called when energy hits zero from a defense hit.
//   Starts the penalty timer, fires the Depleted delegate (which should
//   teleport the Dead back to the burial grounds in your GameMode/Character).
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::TriggerDepletion()
{
    bInDepletionPenalty = true;

    UE_LOG(LogTemp, Warning, TEXT("[Specter] Specter depleted! Penalty for %.1fs."),
        DepletionPenaltyDuration);

    OnSpecterDepleted.Broadcast();

    // Arm the recovery timer.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PenaltyTimerHandle,
            this,
            &USpecterEnergyComponent::EndPenalty,
            DepletionPenaltyDuration,
            false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPenalty — server only, called by timer
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::EndPenalty()
{
    bInDepletionPenalty = false;

    UE_LOG(LogTemp, Log, TEXT("[Specter] Penalty ended. Specter can act again."));

    // Grant a small starting energy so the returning specter isn't completely helpless.
    SetEnergy(PenaltyEnergyCap * 0.5f);

    OnSpecterRestored.Broadcast();
}

// ─────────────────────────────────────────────────────────────────────────────
// GetCurrentEnergyCap — returns the active maximum (penalty-aware)
// ─────────────────────────────────────────────────────────────────────────────

float USpecterEnergyComponent::GetCurrentEnergyCap() const
{
    return bInDepletionPenalty ? PenaltyEnergyCap : MaxEnergy;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetEnergy — internal, always use this
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::SetEnergy(float NewValue)
{
    CurrentEnergy = FMath::Clamp(NewValue, 0.0f, GetCurrentEnergyCap());
    Client_NotifyEnergyChanged(GetNormalizedEnergy());
}

// ─────────────────────────────────────────────────────────────────────────────
// Client_NotifyEnergyChanged — runs on owning client for HUD
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::Client_NotifyEnergyChanged_Implementation(float NormalizedValue)
{
    OnEnergyChanged.Broadcast(NormalizedValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// RepNotify callbacks
// ─────────────────────────────────────────────────────────────────────────────

void USpecterEnergyComponent::OnRep_CurrentEnergy()
{
    OnEnergyChanged.Broadcast(GetNormalizedEnergy());
}

void USpecterEnergyComponent::OnRep_Penalty()
{
    if (bInDepletionPenalty)
    {
        OnSpecterDepleted.Broadcast();
    }
    else
    {
        OnSpecterRestored.Broadcast();
    }
}
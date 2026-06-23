#include "Actors/OccultAuraActor.h"
#include "Characters/LivingCharacter.h"
#include "Characters/DeadCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/SphereComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

AOccultAuraActor::AOccultAuraActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    AuraSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AuraSphere"));
    AuraSphere->SetSphereRadius(OccultRadius);
    AuraSphere->SetCollisionProfileName(TEXT("NoCollision")); // Query-only in OnAuraTick
    SetRootComponent(AuraSphere);

    AuraParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("AuraParticles"));
    AuraParticles->SetupAttachment(AuraSphere);
    AuraParticles->bAutoActivate = false;
}

void AOccultAuraActor::BeginPlay()
{
    Super::BeginPlay();
    // Activation happens via ActivateOn() — not automatic on BeginPlay.
}

void AOccultAuraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Always restore the carrier's speed on destruction.
    if (bActive)
    {
        ApplySpeedPenalty(false);
    }
    GetWorldTimerManager().ClearTimer(AuraTickHandle);
    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// ActivateOn
// ─────────────────────────────────────────────────────────────────────────────

void AOccultAuraActor::ActivateOn(ALivingCharacter* Carrier)
{
    if (!Carrier || !HasAuthority()) return;

    AuraCarrier = Carrier;
    bActive = true;

    // Attach to the carrier so it moves with them.
    AttachToActor(Carrier, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    // Apply speed penalty.
    ApplySpeedPenalty(true);

    // Start the 1-second drain tick.
    GetWorldTimerManager().SetTimer(AuraTickHandle, this,
        &AOccultAuraActor::OnAuraTick, 1.0f, true);

    // Show the glow particle.
    AuraParticles->Activate(true);

    UE_LOG(LogTemp, Log, TEXT("[OccultAura] Activated on %s. Radius: %.0fcm."),
        *Carrier->GetName(), OccultRadius);
}

// ─────────────────────────────────────────────────────────────────────────────
// Deactivate
// ─────────────────────────────────────────────────────────────────────────────

void AOccultAuraActor::Deactivate()
{
    if (!bActive) return;
    bActive = false;

    ApplySpeedPenalty(false);
    GetWorldTimerManager().ClearTimer(AuraTickHandle);
    AuraParticles->Deactivate();

    UE_LOG(LogTemp, Log, TEXT("[OccultAura] Deactivated."));

    // Destroy after a brief VFX settle time.
    SetLifeSpan(0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnAuraTick — fires once per second while active
// ─────────────────────────────────────────────────────────────────────────────

void AOccultAuraActor::OnAuraTick()
{
    if (!bActive || !HasAuthority()) return;

    const FVector Origin = GetActorLocation();

    // 1. Drain all Dead within radius.
    for (TActorIterator<ADeadCharacter> It(GetWorld()); It; ++It)
    {
        ADeadCharacter* Dead = *It;
        if (!Dead) continue;

        const float Dist = FVector::Dist(Origin, Dead->GetActorLocation());
        if (Dist > OccultRadius) continue;

        // Scale drain by proximity — closer = more drain. Full drain at 50% radius.
        const float ProximityScale = FMath::Clamp(
            1.0f - (Dist / OccultRadius) + 0.5f, 0.5f, 1.0f);

        if (USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent())
        {
            Energy->ApplyDefenseHit(DrainPerSecond * ProximityScale);
        }
    }

    // 2. Calm the carrier.
    if (AuraCarrier)
    {
        if (UCardiacRhythmComponent* Cardiac = AuraCarrier->GetCardiacComponent())
        {
            Cardiac->ApplyCalm(CalmPerSecond);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplySpeedPenalty
// ─────────────────────────────────────────────────────────────────────────────

void AOccultAuraActor::ApplySpeedPenalty(bool bApply)
{
    if (!AuraCarrier) return;

    UCharacterMovementComponent* Movement =
        AuraCarrier->GetCharacterMovement();
    if (!Movement) return;

    if (bApply)
    {
        OriginalWalkSpeed = Movement->MaxWalkSpeed;
        Movement->MaxWalkSpeed = OriginalWalkSpeed * SpeedPenaltyFactor;
        UE_LOG(LogTemp, Log,
            TEXT("[OccultAura] Walk speed reduced: %.0f → %.0f cm/s."),
            OriginalWalkSpeed, Movement->MaxWalkSpeed);
    }
    else
    {
        if (OriginalWalkSpeed > 0.0f)
        {
            Movement->MaxWalkSpeed = OriginalWalkSpeed;
        }
        UE_LOG(LogTemp, Log,
            TEXT("[OccultAura] Walk speed restored to %.0f cm/s."), Movement->MaxWalkSpeed);
    }
}
#include "Actors/MatchFlameActor.h"
#include "Characters/LivingCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/PointLightComponent.h"
#include "Items/Concrete/ConcreteItems.h"
#include "Particles/ParticleSystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMatchFlameActor::AMatchFlameActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    // The light is the primary gameplay element — particles are cosmetic.
    FlameLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlameLight"));
    FlameLight->SetIntensity(LightIntensity);
    FlameLight->SetAttenuationRadius(LightRadius);
    // Warm amber colour — real match flame.
    FlameLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.1f));
    FlameLight->SetCastShadows(true);
    SetRootComponent(FlameLight);

    FlameParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("FlameParticles"));
    FlameParticles->SetupAttachment(FlameLight);
    // No template set here — designer assigns in Blueprint.
    FlameParticles->bAutoActivate = false;
}

void AMatchFlameActor::BeginPlay()
{
    Super::BeginPlay();
    // Start hidden — IgniteOn() makes it visible after attachment.
    SetFlameVisible(false);
}

void AMatchFlameActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMatchFlameActor, bIsBurning);
}

// ─────────────────────────────────────────────────────────────────────────────
// IgniteOn
//   Attach to the Living character's hand socket and begin burning.
//   Called server-side immediately after spawn by UMatchItem.
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::IgniteOn(AActor* IgniteOwner)
{
    if (!IgniteOwner || !HasAuthority()) return;

    FlameOwner = IgniteOwner;

    // Attach to the character's hand socket so the light moves with them.
    AttachToActor(IgniteOwner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    if (USkeletalMeshComponent* Mesh = IgniteOwner->FindComponentByClass<USkeletalMeshComponent>())
    {
        AttachToComponent(Mesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            AttachSocketName);
    }

    bIsBurning = true;
    OnRep_IsBurning(); // Apply on server immediately.

    // Auto-extinguish when the match burns out.
    GetWorldTimerManager().SetTimer(BurnTimerHandle, this,
        &AMatchFlameActor::Extinguish, FlameDuration, false);

    // Flicker every 0.1s for a realistic flame effect.
    GetWorldTimerManager().SetTimer(FlickerTimerHandle, this,
        &AMatchFlameActor::TickFlicker, 0.1f, true);

    // Bind to the owner's Heart Pain event — a bad scare snuffs the match.
    if (ALivingCharacter* Living = Cast<ALivingCharacter>(Owner))
    {
        if (UCardiacRhythmComponent* Cardiac = Living->GetCardiacComponent())
        {
            Cardiac->OnHeartPainEvent.AddDynamic(
                this, &AMatchFlameActor::OnOwnerHeartPain);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Match] Ignited. Burns for %.0fs."), FlameDuration);
}

// ─────────────────────────────────────────────────────────────────────────────
// Extinguish
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::Extinguish()
{
    if (!bIsBurning) return;
    if (!HasAuthority()) return;

    bIsBurning = false;
    OnRep_IsBurning(); // Apply on server.

    // Clear timers.
    GetWorldTimerManager().ClearTimer(BurnTimerHandle);
    GetWorldTimerManager().ClearTimer(FlickerTimerHandle);

    UE_LOG(LogTemp, Log, TEXT("[Match] Extinguished."));

    // Destroy after a brief moment so the extinguish VFX can play on clients.
    SetLifeSpan(1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnOwnerHeartPain — Heart Pain snuffs the match
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::OnOwnerHeartPain()
{
    UE_LOG(LogTemp, Log,
        TEXT("[Match] Owner suffered Heart Pain — match extinguished by shock!"));
    Extinguish();
}

// ─────────────────────────────────────────────────────────────────────────────
// TickFlicker — subtle intensity variation every 0.1s
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::TickFlicker()
{
    if (!bIsBurning) return;

    // Random intensity variation ±20% of base to simulate a real flame.
    const float Variation = FMath::RandRange(-0.2f, 0.2f);
    FlameLight->SetIntensity(LightIntensity * (1.0f + Variation));
}

// ─────────────────────────────────────────────────────────────────────────────
// SetFlameVisible — show/hide both light and particles
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::SetFlameVisible(bool bVisible)
{
    FlameLight->SetVisibility(bVisible);
    if (bVisible)
        FlameParticles->Activate(true);
    else
        FlameParticles->Deactivate();
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRep_IsBurning — fires on all clients when replication updates
// ─────────────────────────────────────────────────────────────────────────────

void AMatchFlameActor::OnRep_IsBurning()
{
    SetFlameVisible(bIsBurning);
}
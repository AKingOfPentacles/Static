#include "Actors/BreakerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ABreakerActor::ABreakerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    BreakerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BreakerMesh"));
    BreakerMesh->SetupAttachment(Root);
    BreakerMesh->SetCollisionProfileName(TEXT("BlockAll"));

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(Root);
    InteractionVolume->SetBoxExtent(FVector(80.f, 80.f, 100.f));
    InteractionVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    // Small indicator light — green when powered, red when tripped.
    IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
    IndicatorLight->SetupAttachment(BreakerMesh);
    IndicatorLight->SetIntensity(200.f);
    IndicatorLight->SetLightColor(FLinearColor::Green);
    IndicatorLight->SetAttenuationRadius(40.f);
}

void ABreakerActor::BeginPlay()
{
    Super::BeginPlay();

    Tags.AddUnique(FName("Breaker"));

    // Subscribe to phase changes — the Protection phase trips us automatically.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            PM->OnPhaseChanged.AddDynamic(this, &ABreakerActor::OnPhaseChanged);
        }
    }
}

void ABreakerActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreakerActor, bIsTripped);
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase listener — trip on Midnight
// ─────────────────────────────────────────────────────────────────────────────

void ABreakerActor::OnPhaseChanged(EGamePhase NewPhase, EGamePhase OldPhase)
{
    if (!HasAuthority()) return;

    if (NewPhase == EGamePhase::Protection)
    {
        UE_LOG(LogTemp, Log, TEXT("[Breaker] Midnight — tripping the breaker."));
        TripBreaker();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IInteractable
// ─────────────────────────────────────────────────────────────────────────────

bool ABreakerActor::CanInteract_Implementation(AActor* Interactor) const
{
    // Can only reset when tripped.
    return bIsTripped;
}

FText ABreakerActor::GetInteractPrompt_Implementation() const
{
    return bIsTripped
        ? FText::FromString("Reset breaker")
        : FText::FromString("Breaker — power on");
}

bool ABreakerActor::Interact_Implementation(AActor* Interactor, const FHitResult& HitResult)
{
    if (!HasAuthority()) return false;
    if (!bIsTripped) return false;

    // ResetHoldTime > 0 is handled in Blueprint via a "hold to interact" widget.
    // The server just executes the reset when the RPC arrives — the hold timer
    // lives on the client and gates when the RPC is sent.
    ResetBreaker();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TripBreaker / ResetBreaker
// ─────────────────────────────────────────────────────────────────────────────

void ABreakerActor::TripBreaker()
{
    if (!HasAuthority()) return;
    bIsTripped = true;
    OnRep_IsTripped(); // Apply on server.
}

void ABreakerActor::ResetBreaker()
{
    if (!HasAuthority()) return;
    bIsTripped = false;
    OnRep_IsTripped();
    UE_LOG(LogTemp, Log, TEXT("[Breaker] Power restored."));
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRep_IsTripped — runs on all clients
// ─────────────────────────────────────────────────────────────────────────────

void ABreakerActor::OnRep_IsTripped()
{
    // Update the indicator light color.
    IndicatorLight->SetLightColor(bIsTripped ? FLinearColor::Red : FLinearColor::Green);

    // Enable or disable all managed world lights.
    SetManagedLightsEnabled(!bIsTripped);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetManagedLightsEnabled
//   Iterates the designer-assigned light actors and sets their visibility.
//   Works with any actor that has a ULightComponent (PointLight, SpotLight, etc.)
// ─────────────────────────────────────────────────────────────────────────────

void ABreakerActor::SetManagedLightsEnabled(bool bEnabled)
{
    for (AActor* LightActor : ManagedLights)
    {
        if (!LightActor) continue;

        // Find any light component on the actor and toggle it.
        TArray<ULightComponent*> LightComponents;
        LightActor->GetComponents<ULightComponent>(LightComponents);
        for (ULightComponent* LC : LightComponents)
        {
            LC->SetVisibility(bEnabled);
        }
    }
}
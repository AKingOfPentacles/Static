#include "Actors/MemorabiliaActor.h"
#include "Characters/DeadCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Player/DeadPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"

AMemorabiliaActor::AMemorabiliaActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
    SetRootComponent(MeshComponent);

    // Collection sphere is larger than the mesh — Dead players collect by proximity.
    CollectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollectSphere"));
    CollectSphere->SetupAttachment(MeshComponent);
    CollectSphere->SetSphereRadius(80.0f);
    CollectSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AMemorabiliaActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(FName("Memorabilia"));

    if (HasAuthority())
    {
        CollectSphere->OnComponentBeginOverlap.AddDynamic(
            this, &AMemorabiliaActor::OnDeadOverlap);
    }
}

bool AMemorabiliaActor::IsCollectionAllowed() const
{
    if (!bPhase1Only) return true;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            return PM->GetCurrentPhase() == EGamePhase::Exploration;
        }
    }
    return false;
}

void AMemorabiliaActor::OnDeadOverlap(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bCollected) return;
    if (!HasAuthority()) return;
    if (!IsCollectionAllowed()) return;

    ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor);
    if (!Dead) return;

    bCollected = true;

    // Grant energy.
    if (USpecterEnergyComponent* Energy = Dead->GetSpecterEnergyComponent())
    {
        Energy->GainEnergy(EnergyGain);
    }

    // Record in PlayerState.
    if (ADeadPlayerState* PS = Dead->GetPlayerState<ADeadPlayerState>())
    {
        PS->AddMemorabiliaCollected(1);
    }

    UE_LOG(LogTemp, Log, TEXT("[Memorabilia] Collected by %s. +%.0f energy."),
        *Dead->GetName(), EnergyGain);

    // Blueprint can play a dissolve VFX before destroy via OnDestroyed event.
    Destroy();
}
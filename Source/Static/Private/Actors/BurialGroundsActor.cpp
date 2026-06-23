#include "Actors/BurialGroundsActor.h"
#include "Characters/LivingCharacter.h"
#include "Player/LivingPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Components/SphereComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/GameInstance.h"

ABurialGroundsActor::ABurialGroundsActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    AbsolutionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AbsolutionSphere"));
    AbsolutionSphere->SetSphereRadius(AbsolutionRadius);
    AbsolutionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    SetRootComponent(AbsolutionSphere);

    GroundsDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("GroundsDecal"));
    GroundsDecal->SetupAttachment(AbsolutionSphere);
    GroundsDecal->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    GroundsDecal->DecalSize = FVector(4.f, 200.f, 200.f);
}

void ABurialGroundsActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(FName("BurialGrounds"));
    AbsolutionSphere->SetSphereRadius(AbsolutionRadius);
}

bool ABurialGroundsActor::IsActorInRange(AActor* Actor) const
{
    if (!Actor) return false;
    return FVector::Dist(GetActorLocation(), Actor->GetActorLocation()) <= AbsolutionRadius;
}

bool ABurialGroundsActor::AttemptAbsolution(AActor* Interactor)
{
    if (!HasAuthority()) return false;
    if (bAbsolved) return false;

    // Phase check — absolution only valid in Phase 3.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            if (PM->GetCurrentPhase() != EGamePhase::Confrontation)
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[BurialGrounds] Absolution attempted outside Confrontation phase."));
                return false;
            }
        }
    }

    // Proximity check.
    if (!IsActorInRange(Interactor))
    {
        UE_LOG(LogTemp, Log, TEXT("[BurialGrounds] Player not close enough to burial grounds."));
        return false;
    }

    // Mark this player as having absolved.
    if (ALivingCharacter* Living = Cast<ALivingCharacter>(Interactor))
    {
        if (ALivingPlayerState* PS = Living->GetPlayerState<ALivingPlayerState>())
        {
            PS->SetAbsolved();
        }
    }

    bAbsolved = true;

    UE_LOG(LogTemp, Log, TEXT("[BurialGrounds] Absolution complete! Triggering Living victory."));

    // Trigger victory through the phase manager.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            PM->TriggerLivingVictory();
        }
    }

    return true;
}
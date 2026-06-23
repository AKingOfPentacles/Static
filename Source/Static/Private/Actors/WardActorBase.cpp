#include "Actors/WardActorBase.h"
#include "Characters/DeadCharacter.h"
#include "Characters/LivingCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/SphereComponent.h"
#include "Components/DecalComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWardActorBase::AWardActorBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
    OverlapSphere->SetSphereRadius(OverlapRadius);
    OverlapSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    SetRootComponent(OverlapSphere);

    WardDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("WardDecal"));
    WardDecal->SetupAttachment(OverlapSphere);
    // Decal projects downward onto the floor.
    WardDecal->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    WardDecal->DecalSize = FVector(4.f, 100.f, 100.f); // Depth, width, height
}

void AWardActorBase::BeginPlay()
{
    Super::BeginPlay();

    // Update sphere radius from the configurable property.
    OverlapSphere->SetSphereRadius(OverlapRadius);

    // Bind overlap events — only meaningful on the server.
    if (HasAuthority())
    {
        OverlapSphere->OnComponentBeginOverlap.AddDynamic(
            this, &AWardActorBase::OnDeadEntered);
        OverlapSphere->OnComponentEndOverlap.AddDynamic(
            this, &AWardActorBase::OnDeadExited);

        // Arm the self-destruct timer if a duration is set.
        if (Duration > 0.0f)
        {
            GetWorldTimerManager().SetTimer(DurationTimerHandle, this,
                &AWardActorBase::OnDurationExpired, Duration, false);
        }
    }
}

void AWardActorBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // Ward actors replicate their existence — spawn/destroy is automatically
    // replicated for bReplicates=true actors. No extra properties needed here.
}

void AWardActorBase::OnDeadEntered(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (ADeadCharacter* Dead = Cast<ADeadCharacter>(OtherActor))
    {
        ApplyWardEffect(Dead);
    }
}

void AWardActorBase::OnDeadExited(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Base class does nothing on exit — Sage overrides this for continuous drain.
}

void AWardActorBase::ApplyWardEffect(ADeadCharacter* DeadCharacter)
{
    if (!DeadCharacter) return;
    if (USpecterEnergyComponent* Energy = DeadCharacter->GetSpecterEnergyComponent())
    {
        Energy->ApplyDefenseHit(DefenseStrength);
        UE_LOG(LogTemp, Log, TEXT("[Ward] %s hit Dead for %.1f energy."),
            *GetName(), DefenseStrength);
    }
}

void AWardActorBase::OnDurationExpired()
{
    UE_LOG(LogTemp, Log, TEXT("[Ward] %s expired — dissolving."), *GetName());
    Destroy();
}
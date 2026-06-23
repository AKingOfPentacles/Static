#include "Characters/DeadCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Components/GhostMovementComponent.h"
#include "Player/DeadPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator — for finding Living characters
#include "Characters/LivingCharacter.h"
#include "Components/CardiacRhythmComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
//
//   CRITICAL: To replace UCharacterMovementComponent we MUST use the
//   ObjectInitializer pattern. This tells the engine to construct our custom
//   class in the slot normally occupied by UCharacterMovementComponent.
//
//   If you skip this and just assign GhostMovementComponent in the body,
//   you'll have TWO movement components, which will cause undefined behavior.
// ─────────────────────────────────────────────────────────────────────────────

ADeadCharacter::ADeadCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UGhostMovementComponent>(
        ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    // Cache our typed pointer to the movement component.
    // GetCharacterMovement() returns UCharacterMovementComponent* — we cast once here.
    GhostMovementComponent = Cast<UGhostMovementComponent>(GetCharacterMovement());
    // If this is nullptr something went wrong with the ObjectInitializer pattern.
    check(GhostMovementComponent != nullptr);

    // ── Specter energy ────────────────────────────────────────────────────────
    SpecterEnergyComponent = CreateDefaultSubobject<USpecterEnergyComponent>(
        TEXT("SpecterEnergyComponent"));

    // ── Camera — third-person overhead angle ─────────────────────────────────
    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(GetRootComponent());
    // Pull the camera back and up — gives that "haunting surveillance" angle.
    CameraSpringArm->TargetArmLength    = 400.0f;
    CameraSpringArm->SocketOffset       = FVector(0.0f, 0.0f, 100.0f);
    CameraSpringArm->bUsePawnControlRotation = true;
    CameraSpringArm->bDoCollisionTest   = false; // Ghosts pass through walls — camera follows suit

    SpectralCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectralCamera"));
    SpectralCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
    SpectralCamera->bUsePawnControlRotation = false;

    // ── Mesh hidden by default — only visible when manifesting ───────────────
    // GetMesh() returns the skeletal mesh component inherited from ACharacter.
    // We hide it in game; manifestation abilities will toggle visibility.
    GetMesh()->SetHiddenInGame(true);
    // Keep capsule collision off by default — ghost passes through world.
    // GhostMovementComponent::BeginPlay will handle the correct initial state.

    bReplicates = true;
    SetReplicateMovement(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::BeginPlay()
{
    Super::BeginPlay();

    BindComponentDelegates();
    BindPhaseEvents();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — server-only logic for sustained abilities
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) return;

    // Drain energy continuously while pass-through is held.
    if (bPassThroughHeld)
    {
        TickPassThroughEnergy(DeltaTime);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Manifestation state replicates to ALL clients —
    // Living players need to see when the ghost appears.
    DOREPLIFETIME(ADeadCharacter, bIsManifested);

    // Ability flags replicate to all for HUD / spectator display.
    DOREPLIFETIME(ADeadCharacter, UnlockedAbilityFlags);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetupPlayerInputComponent
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Movement axes — note MoveUp for free vertical movement.
    PlayerInputComponent->BindAxis("MoveForward", this, &ADeadCharacter::Input_MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ADeadCharacter::Input_MoveRight);
    PlayerInputComponent->BindAxis("MoveUp",      this, &ADeadCharacter::Input_MoveUp);
    PlayerInputComponent->BindAxis("LookUp",      this, &ADeadCharacter::Input_LookUp);
    PlayerInputComponent->BindAxis("LookRight",   this, &ADeadCharacter::Input_LookRight);

    // Abilities — single press (Whisper, Shiver, Spook, Manifest).
    PlayerInputComponent->BindAction("AbilityWhisper", IE_Pressed, this,
        &ADeadCharacter::Input_AbilityWhisper);
    PlayerInputComponent->BindAction("AbilityShiver",  IE_Pressed, this,
        &ADeadCharacter::Input_AbilityShiver);
    PlayerInputComponent->BindAction("AbilitySpook",   IE_Pressed, this,
        &ADeadCharacter::Input_AbilitySpook);
    PlayerInputComponent->BindAction("AbilityManifest",IE_Pressed, this,
        &ADeadCharacter::Input_AbilityManifest);

    // Pass-through is a HOLD ability — fires on both press and release.
    PlayerInputComponent->BindAction("AbilityPassThrough", IE_Pressed, this,
        &ADeadCharacter::Input_AbilityPassThroughPressed);
    PlayerInputComponent->BindAction("AbilityPassThrough", IE_Released, this,
        &ADeadCharacter::Input_AbilityPassThroughReleased);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input handlers — local client only
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Input_MoveForward(float Value)
{
    if (Value != 0.0f)
        AddMovementInput(GetActorForwardVector(), Value);
}

void ADeadCharacter::Input_MoveRight(float Value)
{
    if (Value != 0.0f)
        AddMovementInput(GetActorRightVector(), Value);
}

void ADeadCharacter::Input_MoveUp(float Value)
{
    // Vertical movement — unique to the Dead. Living can't fly.
    if (Value != 0.0f)
        AddMovementInput(GetActorUpVector(), Value);
}

void ADeadCharacter::Input_LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void ADeadCharacter::Input_LookRight(float Value)
{
    AddControllerYawInput(Value);
}

void ADeadCharacter::Input_AbilityWhisper()
{
    Server_ActivateAbility(EDeadAbility::Whisper);
}

void ADeadCharacter::Input_AbilityShiver()
{
    Server_ActivateAbility(EDeadAbility::Shiver);
}

void ADeadCharacter::Input_AbilitySpook()
{
    Server_ActivateAbility(EDeadAbility::Spook);
}

void ADeadCharacter::Input_AbilityManifest()
{
    Server_ActivateAbility(EDeadAbility::Manifestation);
}

void ADeadCharacter::Input_AbilityPassThroughPressed()
{
    Server_SetPassThrough(true);
}

void ADeadCharacter::Input_AbilityPassThroughReleased()
{
    Server_SetPassThrough(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Server RPCs
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Server_ActivateAbility_Implementation(EDeadAbility Ability)
{
    TryActivateAbility(Ability);
}

void ADeadCharacter::Server_SetPassThrough_Implementation(bool bEnable)
{
    if (!GhostMovementComponent) return;

    if (bEnable)
    {
        // Check the specter has at least 1 second's worth of energy before entering.
        const float MinEntry = PassThroughCostPerSecond;
        if (!SpecterEnergyComponent->HasEnoughEnergy(MinEntry)) return;
        if (SpecterEnergyComponent->IsInPenalty()) return;

        bPassThroughHeld = true;
        GhostMovementComponent->EnterPassThroughMode();
    }
    else
    {
        bPassThroughHeld = false;
        GhostMovementComponent->ExitPassThroughMode();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TryActivateAbility — server only
// ─────────────────────────────────────────────────────────────────────────────

bool ADeadCharacter::TryActivateAbility(EDeadAbility Ability)
{
    if (!HasAuthority()) return false;

    // Gate 1: Phase unlock check.
    if (!IsAbilityUnlocked(Ability))
    {
        UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Ability %d not yet unlocked."),
            (int32)Ability);
        return false;
    }

    // Gate 2: Penalty check — depleted spectres can't use abilities.
    if (SpecterEnergyComponent->IsInPenalty()) return false;

    // Gate 3: Energy cost.
    const float Cost = GetEnergyCostForAbility(Ability);
    if (!SpecterEnergyComponent->TrySpendEnergy(Cost))
    {
        UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Insufficient energy for ability %d."),
            (int32)Ability);
        return false;
    }

    // All gates passed — execute.
    switch (Ability)
    {
        case EDeadAbility::Whisper:       Execute_Whisper();         break;
        case EDeadAbility::Shiver:        Execute_Shiver();          break;
        case EDeadAbility::Spook:         Execute_Spook();           break;
        case EDeadAbility::Manifestation: Execute_Manifestation();   break;
        case EDeadAbility::FullManifest:  Execute_FullManifestation(); break;
        default: break;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Ability executions — server only
//   These are stubs that will be fleshed out in the Dead Ability step.
//   Each one:
//   1. Applies the gameplay effect (fear, physics, etc.)
//   2. Fires a Multicast RPC for cosmetic feedback on all clients.
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Execute_Whisper()
{
    // Low-intensity proximity fear. Works in all phases.
    // Affects Living players within a short radius.
    ApplyFearInRadius(300.0f, 10.0f);
    Multicast_PlayWhisperEffect();

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Whisper executed."));
}

void ADeadCharacter::Execute_Shiver()
{
    // Mid-intensity cold-wave fear. Phase 2+.
    ApplyFearInRadius(500.0f, 20.0f);
    Multicast_PlayShiverEffect();

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Shiver executed."));
}

void ADeadCharacter::Execute_Spook()
{
    // High-intensity jump-scare burst. Phase 2+.
    // Affects a single nearest Living player with a large fear spike.
    ApplyFearInRadius(400.0f, 40.0f, /*MaxTargets=*/1);
    Multicast_PlaySpookEffect();

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Spook executed."));
}

void ADeadCharacter::Execute_Manifestation()
{
    // Brief visual appearance. Phase 2+.
    // The Dead player becomes visible for a short window.
    SetManifested(true);

    // Auto-hide after 3 seconds via a timer.
    FTimerHandle ManifestTimer;
    GetWorldTimerManager().SetTimer(ManifestTimer, FTimerDelegate::CreateLambda([this]()
    {
        SetManifested(false);
    }), 3.0f, false);

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Manifestation executed (3 second window)."));
}

void ADeadCharacter::Execute_FullManifestation()
{
    // Extended visibility. Phase 3 only.
    SetManifested(true);

    FTimerHandle ManifestTimer;
    GetWorldTimerManager().SetTimer(ManifestTimer, FTimerDelegate::CreateLambda([this]()
    {
        SetManifested(false);
    }), 8.0f, false);

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Full Manifestation executed (8 second window)."));
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyFearInRadius
//   Finds Living characters within Radius (cm) and calls AddRhythm on their
//   CardiacRhythmComponent. Stops after MaxTargets have been affected.
//
//   This runs on the server — all fear state mutations are server-authoritative.
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::ApplyFearInRadius(float Radius, float FearAmount, int32 MaxTargets)
{
    if (!GetWorld()) return;

    const FVector Origin = GetActorLocation();
    int32 AffectedCount = 0;

    for (TActorIterator<ALivingCharacter> It(GetWorld()); It; ++It)
    {
        if (AffectedCount >= MaxTargets) break;

        ALivingCharacter* Living = *It;
        if (!Living || Living->IsPendingKillPending()) continue;


        const float Distance = FVector::Dist(Origin, Living->GetActorLocation());
        if (Distance > Radius) continue;

        // Scale fear by distance — closer = more frightening.
        const float ScaledFear = FearAmount * (1.0f - (Distance / Radius));

        if (UCardiacRhythmComponent* Cardiac = Living->GetCardiacComponent())
        {
            Cardiac->AddRhythm(ScaledFear);
        }

        AffectedCount++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Multicast cosmetic RPCs — fire-and-forget, Unreliable is fine for VFX/SFX
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Multicast_PlayWhisperEffect_Implementation()
{
    // Blueprint adds: whisper audio, subtle screen distortion on nearby Living.
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Whisper effect."));
}

void ADeadCharacter::Multicast_PlayShiverEffect_Implementation()
{
    // Blueprint adds: cold breath particle, low-frequency audio.
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Shiver effect."));
}

void ADeadCharacter::Multicast_PlaySpookEffect_Implementation()
{
    // Blueprint adds: jump-scare stinger, camera shake on nearby Living.
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Spook effect."));
}

// ─────────────────────────────────────────────────────────────────────────────
// SetManifested
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::SetManifested(bool bManifest)
{
    if (!HasAuthority()) return;

    bIsManifested = bManifest;
    // OnRep_Manifested will fire on clients via replication.
    // We also call it directly on the server so the server-side mesh is correct.
    OnRep_Manifested();
}

void ADeadCharacter::OnRep_Manifested()
{
    // Show or hide the skeletal mesh based on replication.
    if (GetMesh())
    {
        GetMesh()->SetHiddenInGame(!bIsManifested);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RespawnAtBurialGrounds
//   Called when SpecterEnergyComponent fires OnSpecterDepleted.
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::RespawnAtBurialGrounds()
{
    if (!HasAuthority()) return;
    if (!BurialGroundsActor)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DeadCharacter] RespawnAtBurialGrounds: BurialGroundsActor not set! "
                 "Assign it in the BP_DeadCharacter Details panel."));
        return;
    }

    // Ensure pass-through exits cleanly before teleporting.
    if (GhostMovementComponent && GhostMovementComponent->IsPassingThrough())
    {
        GhostMovementComponent->ExitPassThroughMode();
    }
    bPassThroughHeld = false;

    // Also hide manifestation on respawn.
    SetManifested(false);

    const FVector SpawnLocation = BurialGroundsActor->GetActorLocation()
        + FVector(0.0f, 0.0f, 50.0f); // Offset up to avoid floor overlap
    SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Respawned at burial grounds: %s"),
        *SpawnLocation.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass-through energy drain (called from Tick on server)
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::TickPassThroughEnergy(float DeltaTime)
{
    const float Drain = PassThroughCostPerSecond * DeltaTime;

    // ApplyDefenseHit reuses the "drain and check for depletion" logic.
    // It's semantically the right call even though this isn't a defense hit —
    // the result (drain → possibly deplete) is identical.
    SpecterEnergyComponent->ApplyDefenseHit(Drain);

    // If the specter ran out of energy mid-wall, force exit.
    if (SpecterEnergyComponent->IsInPenalty() && GhostMovementComponent->IsPassingThrough())
    {
        bPassThroughHeld = false;
        GhostMovementComponent->ExitPassThroughMode();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase awareness
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::BindPhaseEvents()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            PM->OnPhaseChanged.AddDynamic(this, &ADeadCharacter::OnPhaseChanged);
        }
    }
}

void ADeadCharacter::OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase)
{
    // Only the server sets unlock flags — they replicate to all clients.
    if (!HasAuthority()) return;

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Phase changed to %d — updating ability unlocks."),
        (int32)NewPhase);

    switch (NewPhase)
    {
        case EGamePhase::Exploration:
            // Phase 1: Whisper only. No fear abilities beyond the gentlest one.
            UnlockedAbilityFlags = 0;
            SetAbilityUnlockFlag(EDeadAbility::Whisper,  true);
            SetAbilityUnlockFlag(EDeadAbility::PassThrough, true); // Always available
            break;

        case EGamePhase::Protection:
            // Phase 2: Unlock Shiver, Spook, and basic Manifestation.
            SetAbilityUnlockFlag(EDeadAbility::Shiver,       true);
            SetAbilityUnlockFlag(EDeadAbility::Spook,        true);
            SetAbilityUnlockFlag(EDeadAbility::Manifestation,true);
            break;

        case EGamePhase::Confrontation:
            // Phase 3: Full Manifestation unlocked. All abilities active.
            SetAbilityUnlockFlag(EDeadAbility::FullManifest, true);
            break;

        case EGamePhase::GameOver:
            // Lock everything.
            UnlockedAbilityFlags = 0;
            break;

        default:
            break;
    }

    // Mirror flags to the PlayerState for persistence and HUD.
    if (ADeadPlayerState* PS = GetDeadPlayerState())
    {
        PS->SetAbilityUnlockFlags(UnlockedAbilityFlags);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Ability unlock helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ADeadCharacter::IsAbilityUnlocked(EDeadAbility Ability) const
{
    if (Ability == EDeadAbility::None) return false;
    const int32 BitIndex = AbilityToBitIndex(Ability);
    if (BitIndex < 0) return false;
    return (UnlockedAbilityFlags & (1 << BitIndex)) != 0;
}

void ADeadCharacter::SetAbilityUnlockFlag(EDeadAbility Ability, bool bUnlocked)
{
    const int32 BitIndex = AbilityToBitIndex(Ability);
    if (BitIndex < 0) return;

    if (bUnlocked)
        UnlockedAbilityFlags |= (1 << BitIndex);
    else
        UnlockedAbilityFlags &= ~(1 << BitIndex);
}

int32 ADeadCharacter::AbilityToBitIndex(EDeadAbility Ability) const
{
    // Map each ability to a stable bit position.
    // These must match the bit positions documented in ADeadPlayerState.
    switch (Ability)
    {
        case EDeadAbility::Whisper:       return 0;
        case EDeadAbility::Shiver:        return 1;
        case EDeadAbility::Spook:         return 2;
        case EDeadAbility::PassThrough:   return 3;
        case EDeadAbility::Manifestation: return 4;
        case EDeadAbility::FullManifest:  return 5;
        default:                          return -1;
    }
}

float ADeadCharacter::GetEnergyCostForAbility(EDeadAbility Ability) const
{
    switch (Ability)
    {
        case EDeadAbility::Whisper:       return WhisperEnergyCost;
        case EDeadAbility::Shiver:        return ShiverEnergyCost;
        case EDeadAbility::Spook:         return SpookEnergyCost;
        case EDeadAbility::Manifestation: return ManifestEnergyCost;
        case EDeadAbility::FullManifest:  return FullManifestEnergyCost;
        case EDeadAbility::PassThrough:   return 0.0f; // Cost is per-second, handled in Tick
        default:                          return 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IsPassingThrough
// ─────────────────────────────────────────────────────────────────────────────

bool ADeadCharacter::IsPassingThrough() const
{
    return GhostMovementComponent && GhostMovementComponent->IsPassingThrough();
}

// ─────────────────────────────────────────────────────────────────────────────
// Delegate bindings
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::BindComponentDelegates()
{
    if (SpecterEnergyComponent)
    {
        // When energy is fully depleted, respawn at burial grounds.
        SpecterEnergyComponent->OnSpecterDepleted.AddDynamic(
            this, &ADeadCharacter::RespawnAtBurialGrounds);

        // Track depletion count in the PlayerState for scoring/analytics.
        
        SpecterEnergyComponent->OnSpecterDepleted.AddDynamic(
            this, &ADeadCharacter::TrackDepletionInPlayerState);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GetDeadPlayerState
// ─────────────────────────────────────────────────────────────────────────────

ADeadPlayerState* ADeadCharacter::GetDeadPlayerState() const
{
    return GetPlayerState<ADeadPlayerState>();
}

void ADeadCharacter::TrackDepletionInPlayerState()
{
    if (ADeadPlayerState* PS = GetDeadPlayerState())
    {
        PS->IncrementDepletionCount();
    }
}

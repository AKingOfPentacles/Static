#include "Characters/DeadCharacter.h"
#include "Components/SpecterEnergyComponent.h"
#include "Interfaces/Interactable.h"
#include "Player/DeadPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Characters/LivingCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ALS includes
#include "AlsCameraComponent.h"
#include "AlsCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor — standard, no ObjectInitializer needed
// ─────────────────────────────────────────────────────────────────────────────

ADeadCharacter::ADeadCharacter()
{
    // ALS requires Tick to be enabled — it updates locomotion state,
    // rotation, camera, and animation every frame via Tick.
    PrimaryActorTick.bCanEverTick = true;

    // ALS camera — same setup as ALivingCharacter.
    Camera = CreateDefaultSubobject<UAlsCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetMesh());
    Camera->SetRelativeRotation_Direct(FRotator{-15.0f, 0.0f, 0.0f});

    // Specter energy resource.
    SpecterEnergyComponent = CreateDefaultSubobject<USpecterEnergyComponent>(
        TEXT("SpecterEnergyComponent"));

    // Invisible by default — visible only when manifesting.
    GetMesh()->SetHiddenInGame(true);

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
// Tick — drives floating movement on BOTH server and client
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsFloating) return;

    const FVector CurrentLoc = GetActorLocation();
    const FVector Target     = (FloatWaypointIndex == 0) ? FloatWaypoint1 : FloatWaypoint2;
    const FVector ToTarget   = Target - CurrentLoc;
    const float   Dist2D     = FVector(ToTarget.X, ToTarget.Y, 0.0f).Size();

    if (Dist2D < 15.0f)
    {
        if (FloatWaypointIndex == 0)
        {
            // Entry reached — advance to exit.
            FloatWaypointIndex = 1;
        }
        else
        {
            // Exit reached — stop immediately from Tick.
            // This eliminates the gap between arrival and the timer firing
            // where ALS would apply its own rotation for a frame or two.
            // The timer in DoorActor becomes a safety net only.
            StopFloating();

            // Restore input — DoorActor can't do this from its timer
            // if StopFloating fires first, so we handle it here too.
            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                PC->ResetIgnoreMoveInput();
                PC->ResetIgnoreLookInput();
            }
        }
        return;
    }

    // Move toward current waypoint.
    const FVector MoveDir = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
    AddMovementInput(MoveDir, 1.0f);

    // Smoothly rotate to face direction of travel.
    const FRotator TargetRot = MoveDir.Rotation();
    const FRotator NewRot    = FMath::RInterpTo(GetActorRotation(), TargetRot,
        DeltaTime, 8.0f);
    SetActorRotation(NewRot);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADeadCharacter, bIsManifested);
    DOREPLIFETIME(ADeadCharacter, UnlockedAbilityFlags);
    // Float state replicated so clients run identical Tick movement.
    DOREPLIFETIME(ADeadCharacter, bIsFloating);
    DOREPLIFETIME(ADeadCharacter, FloatWaypoint1);
    DOREPLIFETIME(ADeadCharacter, FloatWaypoint2);
    DOREPLIFETIME(ADeadCharacter, FloatExitRotation);
    DOREPLIFETIME(ADeadCharacter, FloatForwardSpeed);
}

// ─────────────────────────────────────────────────────────────────────────────
// ALS overrides — identical pattern to ALivingCharacter
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();

    if (const auto* PC = Cast<APlayerController>(GetController()))
    {
        if (auto* InputSubsystem = ULocalPlayer::GetSubsystem<
            UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            InputSubsystem->ClearAllMappings();
            if (InputMappingContext)
            {
                InputSubsystem->AddMappingContext(InputMappingContext, 0);
            }
        }
    }
}

void ADeadCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
    if (Camera)
    {
        Camera->GetViewInfo(ViewInfo);
    }
}

void ADeadCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo,
    float& Unused, float& VerticalLocation)
{
    if (Camera)
    {
        Camera->DisplayDebug(Canvas, DisplayInfo, VerticalLocation);
    }
    Super::DisplayDebug(Canvas, DisplayInfo, Unused, VerticalLocation);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetupPlayerInputComponent
//   Binds ALS movement/look actions AND Dead ability actions.
//   Dead gets sprint, walk, crouch — same as Living — they move normally.
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);

    auto* EnhancedInput = Cast<UEnhancedInputComponent>(Input);
    if (!ensureMsgf(EnhancedInput,
        TEXT("EnhancedInputComponent not found on %s."), *GetName()))
    {
        return;
    }

    // ── ALS movement ──────────────────────────────────────────────────────────
    if (LookMouseAction)
        EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnLookMouse);

    if (LookAction)
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnLook);

    if (MoveAction)
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnMove);

    if (SprintAction)
    {
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnSprint);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed,
            this, &ThisClass::Input_OnSprint);
    }

    if (WalkAction)
        EnhancedInput->BindAction(WalkAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnWalk);

    if (CrouchAction)
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnCrouch);

    if (SwitchShoulderAction)
        EnhancedInput->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_OnSwitchShoulder);

    // ── Dead abilities ─────────────────────────────────────────────────────────
    if (AbilityWhisperAction)
        EnhancedInput->BindAction(AbilityWhisperAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_AbilityWhisper);

    if (AbilityShiverAction)
        EnhancedInput->BindAction(AbilityShiverAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_AbilityShiver);

    if (AbilitySpookAction)
        EnhancedInput->BindAction(AbilitySpookAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_AbilitySpook);

    if (AbilityManifestAction)
        EnhancedInput->BindAction(AbilityManifestAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_AbilityManifest);

    if (InteractAction)
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Triggered,
            this, &ThisClass::Input_Interact);
}

// ─────────────────────────────────────────────────────────────────────────────
// ALS input handlers — identical to LivingCharacter
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Input_OnLookMouse(const FInputActionValue& ActionValue)
{
    const auto Value{ActionValue.Get<FVector2D>()};
    AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
    AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void ADeadCharacter::Input_OnLook(const FInputActionValue& ActionValue)
{
    const auto Value{ActionValue.Get<FVector2D>()};
    AddControllerPitchInput(Value.Y * LookUpRate * GetWorld()->GetDeltaSeconds());
    AddControllerYawInput(Value.X * LookRightRate * GetWorld()->GetDeltaSeconds());
}

void ADeadCharacter::Input_OnMove(const FInputActionValue& ActionValue)
{
    const auto Value{AAlsCharacter::GetViewRotation().RotateVector(
        {ActionValue.Get<FVector2D>().Y, ActionValue.Get<FVector2D>().X, 0.0f})
        .GetSafeNormal()};
    AddMovementInput(FVector{Value.X, Value.Y, 0.0f}.GetSafeNormal());
}

void ADeadCharacter::Input_OnSprint(const FInputActionValue& ActionValue)
{
    SetDesiredGait(ActionValue.Get<bool>()
        ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void ADeadCharacter::Input_OnWalk()
{
    if (GetDesiredGait() == AlsGaitTags::Walking)
        SetDesiredGait(AlsGaitTags::Running);
    else if (GetDesiredGait() == AlsGaitTags::Running)
        SetDesiredGait(AlsGaitTags::Walking);
}

void ADeadCharacter::Input_OnCrouch()
{
    if (GetDesiredStance() == AlsStanceTags::Standing)
        SetDesiredStance(AlsStanceTags::Crouching);
    else
        SetDesiredStance(AlsStanceTags::Standing);
}

void ADeadCharacter::Input_OnSwitchShoulder()
{
    if (Camera)
        Camera->SetRightShoulder(!Camera->IsRightShoulder());
}

// ─────────────────────────────────────────────────────────────────────────────
// Ability input handlers
// ─────────────────────────────────────────────────────────────────────────────

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

void ADeadCharacter::Input_Interact()
{
    // Line trace from eye height forward — DoorActor routes Dead to PassThrough.
    const FVector Start = GetActorLocation() + FVector(0, 0, 60.0f);
    const FVector End   = Start + GetViewRotation().Vector() * 200.0f;
    Server_Interact(Start, End);
}

// ─────────────────────────────────────────────────────────────────────────────
// Server RPCs
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Server_ActivateAbility_Implementation(EDeadAbility Ability)
{
    TryActivateAbility(Ability);
}

void ADeadCharacter::Server_Interact_Implementation(FVector TraceStart, FVector TraceEnd)
{
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

    if (!bHit || !Hit.GetActor()) return;

    AActor* HitActor = Hit.GetActor();
    if (!HitActor->Implements<UInteractable>()) return;
    if (!IInteractable::Execute_CanInteract(HitActor, this)) return;

    IInteractable::Execute_Interact(HitActor, this, Hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// TryActivateAbility — server only
// ─────────────────────────────────────────────────────────────────────────────

bool ADeadCharacter::TryActivateAbility(EDeadAbility Ability)
{
    if (!HasAuthority()) return false;

    if (!IsAbilityUnlocked(Ability))
    {
        UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Ability %d not unlocked."),
            (int32)Ability);
        return false;
    }

    if (SpecterEnergyComponent->IsInPenalty()) return false;

    const float Cost = GetEnergyCostForAbility(Ability);
    if (!SpecterEnergyComponent->TrySpendEnergy(Cost))
    {
        UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Not enough energy for ability %d."),
            (int32)Ability);
        return false;
    }

    switch (Ability)
    {
        case EDeadAbility::Whisper:       Execute_Whisper();           break;
        case EDeadAbility::Shiver:        Execute_Shiver();            break;
        case EDeadAbility::Spook:         Execute_Spook();             break;
        case EDeadAbility::Manifestation: Execute_Manifestation();     break;
        case EDeadAbility::FullManifest:  Execute_FullManifestation(); break;
        default: break;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Ability executions — server only
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Execute_Whisper()
{
    ApplyFearInRadius(300.0f, 10.0f);
    Multicast_PlayWhisperEffect();
}

void ADeadCharacter::Execute_Shiver()
{
    ApplyFearInRadius(500.0f, 20.0f);
    Multicast_PlayShiverEffect();
}

void ADeadCharacter::Execute_Spook()
{
    ApplyFearInRadius(400.0f, 40.0f, 1);
    Multicast_PlaySpookEffect();
}

void ADeadCharacter::Execute_Manifestation()
{
    SetManifested(true);
    FTimerHandle T;
    GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([this]()
    {
        SetManifested(false);
    }), 3.0f, false);
}

void ADeadCharacter::Execute_FullManifestation()
{
    SetManifested(true);
    FTimerHandle T;
    GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([this]()
    {
        SetManifested(false);
    }), 8.0f, false);
}

void ADeadCharacter::ApplyFearInRadius(float Radius, float FearAmount, int32 MaxTargets)
{
    if (!GetWorld()) return;

    const FVector Origin = GetActorLocation();
    int32 Count = 0;
    int32 TotalFound = 0;

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] ApplyFearInRadius: Origin=%s Radius=%.0f Fear=%.1f"),
        *Origin.ToString(), Radius, FearAmount);

    for (TActorIterator<ALivingCharacter> It(GetWorld()); It; ++It)
    {
        if (Count >= MaxTargets) break;

        ALivingCharacter* Living = *It;
        if (!Living || !IsValid(Living)) continue;

        TotalFound++;

        const float Dist = FVector::Dist(Origin, Living->GetActorLocation());

        UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Found Living '%s' at dist=%.0f (radius=%.0f) — %s"),
            *Living->GetName(), Dist, Radius,
            Dist <= Radius ? TEXT("IN RANGE") : TEXT("out of range"));

        if (Dist > Radius) continue;

        const float ScaledFear = FearAmount * (1.0f - (Dist / Radius));

        if (UCardiacRhythmComponent* Cardiac = Living->GetCardiacComponent())
        {
            UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Adding %.1f rhythm to %s. Current: %.1f"),
                ScaledFear, *Living->GetName(), Cardiac->GetNormalizedRhythm() * 100.0f);
            Cardiac->AddRhythm(ScaledFear);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[DeadCharacter] CardiacRhythmComponent is NULL on %s!"),
                *Living->GetName());
        }

        Count++;
    }

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] ApplyFearInRadius done. Living found: %d, Affected: %d"),
        TotalFound, Count);
}

// ─────────────────────────────────────────────────────────────────────────────
// Multicast cosmetic RPCs
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::Multicast_PlayWhisperEffect_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Whisper."));
}

void ADeadCharacter::Multicast_PlayShiverEffect_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Shiver."));
}

void ADeadCharacter::Multicast_PlaySpookEffect_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Multicast: Spook."));
}

// ─────────────────────────────────────────────────────────────────────────────
// SetManifested / OnRep_Manifested
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::SetManifested(bool bManifest)
{
    if (!HasAuthority()) return;
    bIsManifested = bManifest;
    OnRep_Manifested();
}

void ADeadCharacter::OnRep_Manifested()
{
    if (GetMesh())
    {
        GetMesh()->SetHiddenInGame(!bIsManifested);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RespawnAtBurialGrounds
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::RespawnAtBurialGrounds()
{
    if (!HasAuthority()) return;
    if (!BurialGroundsActor)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DeadCharacter] RespawnAtBurialGrounds: BurialGroundsActor not set!"));
        return;
    }

    SetManifested(false);

    const FVector SpawnLoc = BurialGroundsActor->GetActorLocation()
        + FVector(0.0f, 0.0f, 90.0f);
    SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Respawned at burial grounds."));
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
    if (!HasAuthority()) return;

    switch (NewPhase)
    {
        case EGamePhase::Exploration:
            UnlockedAbilityFlags = 0;
            SetAbilityUnlockFlag(EDeadAbility::Whisper, true);
            break;
        case EGamePhase::Protection:
            SetAbilityUnlockFlag(EDeadAbility::Shiver,        true);
            SetAbilityUnlockFlag(EDeadAbility::Spook,         true);
            SetAbilityUnlockFlag(EDeadAbility::Manifestation, true);
            break;
        case EGamePhase::Confrontation:
            SetAbilityUnlockFlag(EDeadAbility::FullManifest, true);
            break;
        case EGamePhase::GameOver:
            UnlockedAbilityFlags = 0;
            break;
        default: break;
    }

    if (ADeadPlayerState* PS = GetDeadPlayerState())
    {
        PS->SetAbilityUnlockFlags(UnlockedAbilityFlags);
    }

    UE_LOG(LogTemp, Log, TEXT("[DeadCharacter] Phase changed to %d — ability flags: %d"),
        (int32)NewPhase, UnlockedAbilityFlags);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ability unlock helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ADeadCharacter::IsAbilityUnlocked(EDeadAbility Ability) const
{
    if (Ability == EDeadAbility::None) return false;
    const int32 Bit = AbilityToBitIndex(Ability);
    if (Bit < 0) return false;
    return (UnlockedAbilityFlags & (1 << Bit)) != 0;
}

void ADeadCharacter::SetAbilityUnlockFlag(EDeadAbility Ability, bool bUnlocked)
{
    const int32 Bit = AbilityToBitIndex(Ability);
    if (Bit < 0) return;
    if (bUnlocked) UnlockedAbilityFlags |=  (1 << Bit);
    else           UnlockedAbilityFlags &= ~(1 << Bit);
}

int32 ADeadCharacter::AbilityToBitIndex(EDeadAbility Ability) const
{
    switch (Ability)
    {
        case EDeadAbility::Whisper:       return 0;
        case EDeadAbility::Shiver:        return 1;
        case EDeadAbility::Spook:         return 2;
        case EDeadAbility::Manifestation: return 3;
        case EDeadAbility::FullManifest:  return 4;
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
        default:                          return 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Delegate bindings
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::BindComponentDelegates()
{
    if (SpecterEnergyComponent)
    {
        SpecterEnergyComponent->OnSpecterDepleted.AddDynamic(
            this, &ADeadCharacter::RespawnAtBurialGrounds);

        SpecterEnergyComponent->OnSpecterDepleted.AddDynamic(
            this, &ADeadCharacter::HandleSpecterDepleted);
    }
}

void ADeadCharacter::HandleSpecterDepleted()
{
    if (ADeadPlayerState* PS = GetDeadPlayerState())
    {
        PS->IncrementDepletionCount();
    }
}

ADeadPlayerState* ADeadCharacter::GetDeadPlayerState() const
{
    return GetPlayerState<ADeadPlayerState>();
}

// ─────────────────────────────────────────────────────────────────────────────
// StartFloating
//   Sets replicated waypoints and float state so BOTH server and client
//   run the same Tick movement logic. No timer injection needed.
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::StartFloating(FVector EntryPosition, FVector ExitPosition,
    FRotator ExitRotation, float Speed)
{
    if (!HasAuthority()) return;
    if (bIsFloating) return;

    FloatWaypoint1     = EntryPosition;
    FloatWaypoint2     = ExitPosition;
    FloatExitRotation  = ExitRotation;
    FloatForwardSpeed  = Speed;
    FloatWaypointIndex = 0;
    bIsFloating        = true;

    SetLocomotionMode(AlsLocomotionModeTags::Floating);

    UE_LOG(LogTemp, Log,
        TEXT("[Dead] StartFloating: Entry=%s Exit=%s ExitRot=%s Speed=%.0f"),
        *EntryPosition.ToString(), *ExitPosition.ToString(),
        *ExitRotation.ToString(), Speed);
}

// ─────────────────────────────────────────────────────────────────────────────
// StopFloating
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::StopFloating()
{
    if (!HasAuthority()) return;
    if (!bIsFloating) return;

    bIsFloating        = false;
    FloatForwardSpeed  = 0.0f;
    FloatWaypointIndex = 0;

    // Snap to exact exit point location and rotation.
    // FloatWaypoint2 is the exit world position set by ADoorActor.
    // FloatExitRotation is the exit point's world rotation set by ADoorActor.
    // if (!FloatWaypoint2.IsZero())
    // {
    //     SetActorLocationAndRotation(FloatWaypoint2, FloatExitRotation,
    //         false, nullptr, ETeleportType::TeleportPhysics);
    // }

    FloatWaypoint1     = FVector::ZeroVector;
    FloatWaypoint2     = FVector::ZeroVector;
    FloatExitRotation  = FRotator::ZeroRotator;

    GetWorldTimerManager().ClearTimer(FloatTimerHandle);

    // Zero velocity so ALS has nothing to rotate toward.
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->Velocity = FVector::ZeroVector;
        Movement->StopMovementImmediately();
    }

    SetLocomotionMode(AlsLocomotionModeTags::Grounded);

    UE_LOG(LogTemp, Log, TEXT("[Dead] StopFloating — placed at exit point."));
}

// ─────────────────────────────────────────────────────────────────────────────
// OnRep_FloatState — fires on clients when bIsFloating changes
// ─────────────────────────────────────────────────────────────────────────────

void ADeadCharacter::OnRep_FloatState()
{
    if (bIsFloating)
    {
        FloatWaypointIndex = 0;
        SetLocomotionMode(AlsLocomotionModeTags::Floating);
    }
    else
    {
        FloatWaypointIndex = 0;

        // Snap to exact exit point — same as server.
        if (!FloatWaypoint2.IsZero())
        {
            SetActorLocationAndRotation(FloatWaypoint2, FloatExitRotation,
                false, nullptr, ETeleportType::TeleportPhysics);
        }

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->Velocity = FVector::ZeroVector;
            Movement->StopMovementImmediately();
        }

        SetLocomotionMode(AlsLocomotionModeTags::Grounded);
    }
}
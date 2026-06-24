#include "Characters/LivingCharacter.h"
#include "Characters/StaticCharacterBase.h"
#include "Interfaces/Interactable.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/InventoryComponent.h"
#include "Player/LivingPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

// ALS includes
#include "AlsCameraComponent.h"
#include "AlsCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
//   ALS creates and owns: mesh, movement component, camera.
//   We only create our gameplay components here.
// ─────────────────────────────────────────────────────────────────────────────

ALivingCharacter::ALivingCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // ALS camera — same pattern as ALS example character.
    Camera = CreateDefaultSubobject<UAlsCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetMesh());
    Camera->SetRelativeRotation_Direct(FRotator{-15.0f, 0.0f, 0.0f});

    // Our gameplay components.
    CardiacRhythmComponent = CreateDefaultSubobject<UCardiacRhythmComponent>(
        TEXT("CardiacRhythmComponent"));

    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(
        TEXT("InventoryComponent"));

    bReplicates = true;
    SetReplicateMovement(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::BeginPlay()
{
    Super::BeginPlay();
    BindComponentDelegates();
    BindPhaseEvents();
}

void ALivingCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALivingCharacter, SelectedItemType);
}

// ─────────────────────────────────────────────────────────────────────────────
// ALS overrides — identical to the ALS example character
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();

    if (const auto* PC = Cast<APlayerController>(GetController()))
    {
        if (auto* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            PC->GetLocalPlayer()))
        {
            InputSubsystem->ClearAllMappings();
            if (InputMappingContext)
            {
                InputSubsystem->AddMappingContext(InputMappingContext, 0);
            }
        }
    }
}

void ALivingCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
    if (Camera)
    {
        Camera->GetViewInfo(ViewInfo);
    }
}

void ALivingCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo,
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
//   Binds BOTH the ALS actions AND our Static Mansion actions.
//   ALS uses Enhanced Input — we follow the same pattern for our actions.
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);

    auto* EnhancedInput = Cast<UEnhancedInputComponent>(Input);
    if (!ensureMsgf(EnhancedInput, TEXT("EnhancedInputComponent not found on %s. "
        "Check Project Settings → Input → Default Input Component Class."), *GetName()))
    {
        return;
    }

    // ── ALS actions ───────────────────────────────────────────────────────────
    if (LookMouseAction)
        EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnLookMouse);

    if (LookAction)
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnLook);

    if (MoveAction)
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnMove);

    if (SprintAction)
    {
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnSprint);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this,
            &ThisClass::Input_OnSprint);
    }

    if (WalkAction)
        EnhancedInput->BindAction(WalkAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnWalk);

    if (CrouchAction)
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnCrouch);

    if (AimAction)
    {
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnAim);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this,
            &ThisClass::Input_OnAim);
    }

    if (RagdollAction)
        EnhancedInput->BindAction(RagdollAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnRagdoll);

    if (RollAction)
        EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnRoll);

    if (RotationModeAction)
        EnhancedInput->BindAction(RotationModeAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnRotationMode);

    if (ViewModeAction)
        EnhancedInput->BindAction(ViewModeAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnViewMode);

    if (SwitchShoulderAction)
        EnhancedInput->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_OnSwitchShoulder);

    // ── Static Mansion actions ─────────────────────────────────────────────────
    // These use the same InputMappingContext — just add them to your IMC asset.
    if (InteractAction)
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_Interact);

    if (UseItemAction)
        EnhancedInput->BindAction(UseItemAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_UseItem);

    if (CycleItemNextAction)
        EnhancedInput->BindAction(CycleItemNextAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_CycleNext);

    if (CycleItemPrevAction)
        EnhancedInput->BindAction(CycleItemPrevAction, ETriggerEvent::Triggered, this,
            &ThisClass::Input_CyclePrev);
}

// ─────────────────────────────────────────────────────────────────────────────
// ALS input implementations — copied from ALS example character
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::Input_OnLookMouse(const FInputActionValue& ActionValue)
{
    const auto Value{ActionValue.Get<FVector2D>()};
    AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
    AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void ALivingCharacter::Input_OnLook(const FInputActionValue& ActionValue)
{
    const auto Value{ActionValue.Get<FVector2D>()};
    AddControllerPitchInput(Value.Y * LookUpRate * GetWorld()->GetDeltaSeconds());
    AddControllerYawInput(Value.X * LookRightRate * GetWorld()->GetDeltaSeconds());
}

void ALivingCharacter::Input_OnMove(const FInputActionValue& ActionValue)
{
    const auto Value{AAlsCharacter::GetViewRotation().RotateVector(
        {ActionValue.Get<FVector2D>().Y, ActionValue.Get<FVector2D>().X, 0.0f}).GetSafeNormal()};

    AddMovementInput(FVector{Value.X, Value.Y, 0.0f}.GetSafeNormal());
}

void ALivingCharacter::Input_OnSprint(const FInputActionValue& ActionValue)
{
    SetDesiredGait(ActionValue.Get<bool>() ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void ALivingCharacter::Input_OnWalk()
{
    if (GetDesiredGait() == AlsGaitTags::Walking)
        SetDesiredGait(AlsGaitTags::Running);
    else if (GetDesiredGait() == AlsGaitTags::Running)
        SetDesiredGait(AlsGaitTags::Walking);
}

void ALivingCharacter::Input_OnCrouch()
{
    if (GetDesiredStance() == AlsStanceTags::Standing)
        SetDesiredStance(AlsStanceTags::Crouching);
    else
        SetDesiredStance(AlsStanceTags::Standing);
}

void ALivingCharacter::Input_OnAim(const FInputActionValue& ActionValue)
{
    SetDesiredAiming(ActionValue.Get<bool>());
}

void ALivingCharacter::Input_OnRagdoll()
{
    if (GetLocomotionMode() == AlsLocomotionModeTags::InAir)
        return;

    if (GetLocomotionAction() == AlsLocomotionActionTags::Ragdolling)
        StopRagdolling();
    else
        StartRagdolling();
}

void ALivingCharacter::Input_OnRoll()
{
    StartRolling(0.5f);
}

void ALivingCharacter::Input_OnRotationMode()
{
    if (GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection)
        SetDesiredRotationMode(AlsRotationModeTags::ViewDirection);
    else if (GetDesiredRotationMode() == AlsRotationModeTags::ViewDirection)
        SetDesiredRotationMode(AlsRotationModeTags::VelocityDirection);
}

void ALivingCharacter::Input_OnViewMode()
{
    if (ViewMode == AlsViewModeTags::ThirdPerson)
        SetViewMode(AlsViewModeTags::FirstPerson);
    else if (ViewMode == AlsViewModeTags::FirstPerson)
        SetViewMode(AlsViewModeTags::ThirdPerson);
}

void ALivingCharacter::Input_OnSwitchShoulder()
{
    if (Camera)
        Camera->SetRightShoulder(!Camera->IsRightShoulder());
}

// ─────────────────────────────────────────────────────────────────────────────
// Static Mansion input handlers
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::Input_Interact()
{
    if (bIsFleeing) return;
    Interact();
}

void ALivingCharacter::Input_UseItem()
{
    if (bIsFleeing) return;
    UseCurrentItem();
}

void ALivingCharacter::Input_CycleNext()
{
    CycleItemNext();
}

void ALivingCharacter::Input_CyclePrev()
{
    CycleItemPrev();
}

// ─────────────────────────────────────────────────────────────────────────────
// Interact
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::Interact()
{
    FHitResult Hit;
    if (GetInteractionTrace(Hit))
    {
        const FVector Start = GetActorLocation() + FVector(0, 0, 60.0f);
        const FVector End   = Start + GetViewRotation().Vector() * InteractRange;
        Server_Interact(Start, End);
    }
}

void ALivingCharacter::Server_Interact_Implementation(FVector TraceStart, FVector TraceEnd)
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

    UE_LOG(LogTemp, Log, TEXT("[LivingCharacter] Interacted with: %s"),
        *HitActor->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// UseCurrentItem
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::UseCurrentItem()
{
    if (SelectedItemType == EItemType::None) return;

    const FVector Start = GetActorLocation() + FVector(0, 0, 60.0f);
    const FVector End   = Start + GetViewRotation().Vector() * InteractRange;
    Server_UseItem(SelectedItemType, Start, End);
}

void ALivingCharacter::Server_UseItem_Implementation(
    EItemType ItemType, FVector TraceStart, FVector TraceEnd)
{
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
    InventoryComponent->TryUseItem(ItemType, Hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hotbar cycling
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::CycleItemNext()
{
    const TArray<FItemData>& Inv = InventoryComponent->GetInventory();
    if (Inv.Num() == 0) return;
    HotbarIndex = (HotbarIndex + 1) % Inv.Num();
    Server_SetSelectedItem(Inv[HotbarIndex].ItemType);
}

void ALivingCharacter::CycleItemPrev()
{
    const TArray<FItemData>& Inv = InventoryComponent->GetInventory();
    if (Inv.Num() == 0) return;
    HotbarIndex = (HotbarIndex - 1 + Inv.Num()) % Inv.Num();
    Server_SetSelectedItem(Inv[HotbarIndex].ItemType);
}

void ALivingCharacter::Server_SetSelectedItem_Implementation(EItemType NewItem)
{
    if (NewItem != EItemType::None && !InventoryComponent->HasItem(NewItem)) return;
    SelectedItemType = NewItem;
}

void ALivingCharacter::OnRep_SelectedItemType()
{
    // Blueprint reacts here to swap held item mesh.
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase awareness
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::BindPhaseEvents()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PM = GI->GetSubsystem<UGamePhaseManager>())
        {
            PM->OnPhaseChanged.AddDynamic(this, &ALivingCharacter::OnPhaseChanged);
        }
    }
}

void ALivingCharacter::OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase)
{
    UE_LOG(LogTemp, Log, TEXT("[LivingCharacter] Phase changed to %d. Adjusting..."),
        (int32)NewPhase);

    switch (NewPhase)
    {
        case EGamePhase::GameOver:
            bIsFleeing = true;
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fear callbacks
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::BindComponentDelegates()
{
    if (CardiacRhythmComponent)
    {
        CardiacRhythmComponent->OnHeartPainEvent.AddDynamic(
            this, &ALivingCharacter::OnHeartPain);
        CardiacRhythmComponent->OnPlayerFlees.AddDynamic(
            this, &ALivingCharacter::OnFlee);
    }
}

void ALivingCharacter::OnHeartPain_Implementation()
{
    if (ALivingPlayerState* PS = GetLivingPlayerState())
    {
        PS->IncrementHeartPainCount();
    }
}

void ALivingCharacter::OnFlee_Implementation()
{
    bIsFleeing = true;
    if (ALivingPlayerState* PS = GetLivingPlayerState())
    {
        PS->SetFled();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ALivingCharacter::GetInteractionTrace(FHitResult& OutHit) const
{
    const FVector Start = GetActorLocation() + FVector(0, 0, 60.0f);
    const FVector End   = Start + GetViewRotation().Vector() * InteractRange;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        OutHit, Start, End, ECC_Visibility, Params);

#if WITH_EDITOR
    DrawDebugLine(GetWorld(), Start, End,
        bHit ? FColor::Green : FColor::Red, false, 0.1f, 0, 1.0f);
#endif

    return bHit;
}

ALivingPlayerState* ALivingCharacter::GetLivingPlayerState() const
{
    return GetPlayerState<ALivingPlayerState>();
}
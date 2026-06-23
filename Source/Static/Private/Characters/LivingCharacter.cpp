#include "Characters/LivingCharacter.h"
#include "Components/CardiacRhythmComponent.h"
#include "Components/InventoryComponent.h"
#include "Player/LivingPlayerState.h"
#include "Systems/GamePhaseManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"    // For debug trace visualization — remove in shipping

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
//   Create all components here. Unreal's component system requires components
//   to be created in the constructor — you cannot safely do it in BeginPlay.
// ─────────────────────────────────────────────────────────────────────────────

ALivingCharacter::ALivingCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── Camera setup ─────────────────────────────────────────────────────────
    // Spring arm attaches to the root (CapsuleComponent).
    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(GetRootComponent());
    CameraSpringArm->TargetArmLength = 0.0f;     // Zero = first-person (camera at attachment point)
    CameraSpringArm->bUsePawnControlRotation = true; // Arm rotates with the controller
    CameraSpringArm->bDoCollisionTest = false;    // Disable for FPS — avoids camera clipping into walls

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
    FirstPersonCamera->bUsePawnControlRotation = false; // Camera uses arm rotation, not its own

    // ── Gameplay components ───────────────────────────────────────────────────
    CardiacRhythmComponent = CreateDefaultSubobject<UCardiacRhythmComponent>(
        TEXT("CardiacRhythmComponent"));

    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(
        TEXT("InventoryComponent"));

    // ── Movement defaults for a human survivor ────────────────────────────────
    GetCharacterMovement()->MaxWalkSpeed = 400.0f;    // Cautious walking pace
    GetCharacterMovement()->MaxWalkSpeedCrouched = 200.0f;
    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

    // Replicate this character to all clients (standard for multiplayer).
    bReplicates = true;
    // Also replicate movement — required for server-authoritative character movement.
    SetReplicateMovement(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Wire delegates so the character reacts to its own components.
    BindComponentDelegates();

    // Subscribe to phase changes so we can lock/unlock abilities per phase.
    BindPhaseEvents();
}

void ALivingCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // Per-frame logic goes here later (e.g. flashlight battery UI update).
    // Avoid heavy work here — prefer event-driven where possible.
}

// ─────────────────────────────────────────────────────────────────────────────
// GetLifetimeReplicatedProps
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // SelectedItemType replicates to everyone so other players can see what
    // you're holding (useful for Dead players knowing what defenses are up).
    DOREPLIFETIME(ALivingCharacter, SelectedItemType);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetupPlayerInputComponent
//   Binds raw input events to our local handler functions.
//   These run CLIENT-SIDE ONLY — they then call Server RPCs for gameplay.
//
//   EDITOR NOTE: The string names here ("MoveForward", "Interact", etc.) must
//   exactly match what you set up in Project Settings → Input.
//   We use Legacy Input here (simple and reliable for a small project).
//   If you want Enhanced Input (UE5's newer system), that's a separate setup
//   we can switch to later without changing the game logic.
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Axis mappings — held input (movement, looking)
    PlayerInputComponent->BindAxis("MoveForward", this, &ALivingCharacter::Input_MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ALivingCharacter::Input_MoveRight);
    PlayerInputComponent->BindAxis("LookUp",      this, &ALivingCharacter::Input_LookUp);
    PlayerInputComponent->BindAxis("LookRight",   this, &ALivingCharacter::Input_LookRight);

    // Action mappings — single press events
    PlayerInputComponent->BindAction("Interact",      IE_Pressed, this, &ALivingCharacter::Input_Interact);
    PlayerInputComponent->BindAction("UseItem",       IE_Pressed, this, &ALivingCharacter::Input_UseItem);
    PlayerInputComponent->BindAction("CycleItemNext", IE_Pressed, this, &ALivingCharacter::Input_CycleNext);
    PlayerInputComponent->BindAction("CycleItemPrev", IE_Pressed, this, &ALivingCharacter::Input_CyclePrev);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input handlers — LOCAL CLIENT ONLY
//   These run immediately on the owning client for responsiveness.
//   Gameplay-affecting actions send an RPC to the server for validation.
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::Input_MoveForward(float Value)
{
    if (bIsFleeing) return; // Suppress movement during flee sequence
    if (Value != 0.0f)
    {
        AddMovementInput(GetActorForwardVector(), Value);
    }
}

void ALivingCharacter::Input_MoveRight(float Value)
{
    if (bIsFleeing) return;
    if (Value != 0.0f)
    {
        AddMovementInput(GetActorRightVector(), Value);
    }
}

void ALivingCharacter::Input_LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void ALivingCharacter::Input_LookRight(float Value)
{
    AddControllerYawInput(Value);
}

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
//   Local client fires a trace and sends result to server.
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::Interact()
{
    FHitResult Hit;
    if (GetInteractionTrace(Hit))
    {
        // Send trace endpoints to server — server re-validates the trace.
        const FVector Start = FirstPersonCamera->GetComponentLocation();
        const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractRange;
        Server_Interact(Start, End);
    }
}

void ALivingCharacter::Server_Interact_Implementation(FVector TraceStart, FVector TraceEnd)
{
    // Server re-traces to prevent cheating (client can't fake hit positions).
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

    if (!bHit || !Hit.GetActor()) return;

    // Check if the hit actor implements our IInteractable interface.
    // We'll add that interface in Step 6 (pickup actors, doors, etc.).
    // For now, just log to confirm the system works end-to-end.
    UE_LOG(LogTemp, Log, TEXT("[LivingCharacter] Server interact hit: %s"),
        *Hit.GetActor()->GetName());

    // TODO (Step 6): Cast to IInteractable and call Interact(this).
}

// ─────────────────────────────────────────────────────────────────────────────
// UseCurrentItem
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::UseCurrentItem()
{
    if (SelectedItemType == EItemType::None) return;

    // Fire a trace for placement items (Salt, Chalk need a surface to place on).
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End   = Start + FirstPersonCamera->GetForwardVector() * InteractRange;

    Server_UseItem(SelectedItemType, Start, End);
}

void ALivingCharacter::Server_UseItem_Implementation(
    EItemType ItemType, FVector TraceStart, FVector TraceEnd)
{
    // Server re-traces.
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

    // Hit may be empty for items that don't need a surface (e.g. Sage burns in place).
    // InventoryComponent::TryUseItem handles that gracefully.
    InventoryComponent->TryUseItem(ItemType, Hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hotbar cycling
//   We cycle through the inventory array by index, wrapping at both ends.
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
    // Validate: only allow setting to items actually in inventory.
    if (NewItem != EItemType::None && !InventoryComponent->HasItem(NewItem))
    {
        return;
    }
    SelectedItemType = NewItem;
}

void ALivingCharacter::OnRep_SelectedItemType()
{
    // All clients can react — e.g. show/hide held item mesh.
    // Blueprint handles the mesh swap via the OnRep event in the event graph.
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase awareness
// ─────────────────────────────────────────────────────────────────────────────

void ALivingCharacter::BindPhaseEvents()
{
    // Get the subsystem — works on both server and client.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGamePhaseManager* PhaseManager = GI->GetSubsystem<UGamePhaseManager>())
        {
            // Lambda so we don't need a separate UFUNCTION for the binding.
            PhaseManager->OnPhaseChanged.AddDynamic(
                this, &ALivingCharacter::OnPhaseChanged);
        }
    }
}

void ALivingCharacter::OnPhaseChanged_Implementation(EGamePhase NewPhase, EGamePhase OldPhase)
{
    UE_LOG(LogTemp, Log, TEXT("[LivingCharacter] Phase changed to %d. Adjusting..."),
        (int32)NewPhase);

    switch (NewPhase)
    {
        case EGamePhase::Exploration:
            // Full movement, all items usable (ward placement not needed yet).
            GetCharacterMovement()->MaxWalkSpeed = 400.0f;
            break;

        case EGamePhase::Protection:
            // Lights go out — player should light a match/flashlight.
            // Movement stays the same; ability to use matches/flashlight unchanged.
            // Blueprint can add screen-darkening effect via OnPhaseChanged event.
            break;

        case EGamePhase::Confrontation:
            // Bones now usable at the burial grounds.
            // All defenses remain active.
            break;

        case EGamePhase::GameOver:
            // Disable all input.
            bIsFleeing = true;
            break;

        default:
            break;
    }

    // Blueprint can further customize this (camera effects, audio stingers, etc.)
    // via the BlueprintNativeEvent override.
}

// ─────────────────────────────────────────────────────────────────────────────
// Fear callbacks (wired to CardiacRhythmComponent delegates)
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
    // Update the persistent PlayerState count.
    if (ALivingPlayerState* PS = GetLivingPlayerState())
    {
        PS->IncrementHeartPainCount();
    }

    UE_LOG(LogTemp, Warning, TEXT("[LivingCharacter] HEART PAIN! Count: %d"),
        GetLivingPlayerState() ? GetLivingPlayerState()->GetHeartPainCount() : -1);

    // Blueprint adds: camera shake, heartbeat audio, red vignette flash.
}

void ALivingCharacter::OnFlee_Implementation()
{
    bIsFleeing = true;

    if (ALivingPlayerState* PS = GetLivingPlayerState())
    {
        PS->SetFled();
    }

    UE_LOG(LogTemp, Warning, TEXT("[LivingCharacter] FLEE triggered. Player: %s"),
        *GetName());

    // Blueprint adds: flee animation, fade to spectator camera, disable HUD.
    // GameMode logic (Step 5) handles switching to spectator.
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ALivingCharacter::GetInteractionTrace(FHitResult& OutHit) const
{
    if (!FirstPersonCamera) return false;

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End   = Start + FirstPersonCamera->GetForwardVector() * InteractRange;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        OutHit, Start, End, ECC_Visibility, Params);

#if WITH_EDITOR
    // Visualize the interaction trace in the editor — very helpful for setup.
    DrawDebugLine(GetWorld(), Start, End,
        bHit ? FColor::Green : FColor::Red,
        false, 0.1f, 0, 1.0f);
#endif

    return bHit;
}

ALivingPlayerState* ALivingCharacter::GetLivingPlayerState() const
{
    return GetPlayerState<ALivingPlayerState>();
}
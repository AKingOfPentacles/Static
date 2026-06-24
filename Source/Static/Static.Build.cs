using UnrealBuildTool;

// ─────────────────────────────────────────────────────────────────────────────
// StaticMansion.Build.cs
//
//   This file tells Unreal Build Tool (UBT) what modules your game depends on.
//   Every time you #include a class from a different module (e.g. Camera,
//   NavigationSystem, AIModule), that module must be listed here or the
//   linker will fail with an "unresolved external symbol" error.
//
//   HOW TO ADD A NEW MODULE:
//   1. Add its name to PublicDependencyModuleNames or PrivateDependencyModuleNames.
//   2. Right-click your .uproject → Generate Visual Studio project files.
//   3. Rebuild.
//
//   Public  = the module is visible to other modules that depend on yours.
//   Private = the module is only visible inside this module's own .cpp files.
//   For a single-module game project, the distinction rarely matters —
//   putting everything in Public is safe.
// ─────────────────────────────────────────────────────────────────────────────

public class Static : ModuleRules
{
    public Static(ReadOnlyTargetRules Target) : base(Target)
    {
        // Use explicit pre-compiled header mode for faster incremental builds.
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            // ── Core engine ────────────────────────────────────────────────
            "Core",             // FString, TArray, TMap, UObject, etc.
            "CoreUObject",      // UClass, UObject, UPROPERTY macros
            "Engine",           // ACharacter, AGameMode, UWorld, UActorComponent, etc.
            "InputCore",        // FKey, EKeys — required for any input binding

            // ── Networking ─────────────────────────────────────────────────
            // These provide the DOREPLIFETIME macros, RPC support, and the
            // replicated movement component infrastructure.
            "NetCore",          // Core networking types
            "OnlineSubsystem",  // Session management (needed even for LAN play)

            // ── Gameplay ───────────────────────────────────────────────────
            "GameplayTasks",    // Required by some movement component internals

            // ── Camera ────────────────────────────────────────────────────
            // UCameraComponent and USpringArmComponent live here.
            "CinematicCamera",  // Optional: adds cinematic camera features

            // ── Physics / collision ────────────────────────────────────────
            // Required for overlap events and collision channel queries.
            "PhysicsCore",

            // ── AI / Navigation ────────────────────────────────────────────
            // Needed for NavAgentProps on CharacterMovementComponent.
            "NavigationSystem",
            "AIModule",
            // ── ALS / Navigation ────────────────────────────────────────────
            // Needed for ALS animation  
            "ALS", "EnhancedInput",
            

        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // ── Enhanced Input ─────────────────────────────────────────────
            // Add this if you switch from Legacy Input to Enhanced Input.
            "ALSCamera"
            // ── Slate / UMG ────────────────────────────────────────────────
            // Only needed if you build C++ widgets. Blueprint UMG doesn't require this.
            // Add when you build HUD widgets in C++.
            // "Slate",
            // "SlateCore",
            // "UMG",
            
        });

        // ── Additional include paths ───────────────────────────────────────
        // Our Public and Private folders are automatically on the include path.
        // If you add subfolders (e.g. Public/Systems, Public/Characters), UBT
        // finds headers there without any extra configuration — the #include
        // path is relative to the Public/ or Private/ root.
        //
        // Example: #include "Systems/GamePhaseManager.h" works because
        // Source/Static/Public/ is already on the search path.
    }
}
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StaticMansionGameData.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UStaticMansionGameData
//
//   Un seul asset DataAsset qui centralise TOUTES les variables de tuning.
//   Créer un Blueprint child (DA_StaticMansionGameData) dans le Content Browser.
//   Le GameMode charge cet asset au démarrage et le distribue aux systèmes.
//
//   EDITOR SETUP:
//   1. Content Browser → right-click → Miscellaneous → Data Asset
//   2. Choisir UStaticMansionGameData comme classe parente
//   3. Nommer DA_StaticMansionGameData
//   4. Assigner dans BP_StaticMansionGameMode → GameData
//   5. Toutes les valeurs modifiables directement dans le Details panel de l'asset
//
//   AVANTAGES:
//   • Un seul endroit pour tous les paramètres de jeu
//   • Pas besoin d'ouvrir les Blueprints des systèmes
//   • Sauvegardable, versionnable, duplicable par preset (DA_GameData_Hard, etc.)
//   • Peut être modifié en cours de PIE pour du live tuning
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class STATIC_API UStaticMansionGameData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ═════════════════════════════════════════════════════════════════════════
    // PHASE DURATIONS
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase Durations",
        meta = (ClampMin = "10.0", ForceUnits = "s",
            ToolTip = "Durée de la Phase 1 (Exploration). Les Dead collectent les mémorabilias."))
    float ExplorationDuration = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase Durations",
        meta = (ClampMin = "10.0", ForceUnits = "s",
            ToolTip = "Durée de la Phase 2 (Protection). Lumières éteintes, abilities de peur débloquées."))
    float ProtectionDuration = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase Durations",
        meta = (ClampMin = "10.0", ForceUnits = "s",
            ToolTip = "Durée de la Phase 3 (Confrontation). Les Living doivent brûler les os avant l'aube."))
    float ConfrontationDuration = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase Durations",
        meta = (ClampMin = "0.0", ForceUnits = "s",
            ToolTip = "Délai d'attente avant le début de la nuit après connexion des joueurs."))
    float PreNightCountdown = 5.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // CARDIAC RHYTHM (Living fear)
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac Rhythm",
        meta = (ClampMin = "1.0",
            ToolTip = "Valeur maximale avant qu'un Heart Pain event se déclenche."))
    float CardiacMaxRhythm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac Rhythm",
        meta = (ClampMin = "0.0", ForceUnits = "x/s",
            ToolTip = "Vitesse de décroissance du rythme quand le joueur est calme."))
    float CardiacDecayRate = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac Rhythm",
        meta = (ClampMin = "0.0", ForceUnits = "s",
            ToolTip = "Secondes après la dernière peur avant que le rythme commence à décroître."))
    float CardiacCalmDelay = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac Rhythm",
        meta = (ClampMin = "0.0",
            ToolTip = "Valeur du rythme après un Heart Pain event. Plus bas = récupération plus lente."))
    float CardiacPostPainReset = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cardiac Rhythm",
        meta = (ClampMin = "1",
            ToolTip = "Nombre de Heart Pain events avant que le Living soit forcé de fuir."))
    int32 CardiacHeartPainLimit = 3;

    // ═════════════════════════════════════════════════════════════════════════
    // SPECTER ENERGY (Dead resource)
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter Energy",
        meta = (ClampMin = "1.0",
            ToolTip = "Énergie maximale du Dead."))
    float SpecterMaxEnergy = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter Energy",
        meta = (ClampMin = "0.0", ForceUnits = "x/s",
            ToolTip = "Regain d'énergie passif par seconde hors pénalité."))
    float SpecterPassiveRegenRate = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter Energy",
        meta = (ClampMin = "0.0", ForceUnits = "s",
            ToolTip = "Durée de la pénalité de respawn après déplétion totale."))
    float SpecterDepletionPenaltyDuration = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter Energy",
        meta = (ClampMin = "0.0",
            ToolTip = "Énergie maximale pendant la pénalité. Empêche d'utiliser des abilities en sortant."))
    float SpecterPenaltyEnergyCap = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specter Energy",
        meta = (ClampMin = "0.0",
            ToolTip = "Énergie de départ. 0 = Dead doit collecter des mémorabilias en Phase 1."))
    float SpecterStartingEnergy = 0.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // DEAD ABILITY COSTS
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Costs",
        meta = (ClampMin = "0.0",
            ToolTip = "Coût en énergie par utilisation de Whisper (Phase 1+)."))
    float WhisperEnergyCost = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Costs",
        meta = (ClampMin = "0.0",
            ToolTip = "Coût en énergie par utilisation de Shiver (Phase 2+)."))
    float ShiverEnergyCost = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Costs",
        meta = (ClampMin = "0.0",
            ToolTip = "Coût en énergie par utilisation de Spook (Phase 2+). Cible unique, forte peur."))
    float SpookEnergyCost = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Costs",
        meta = (ClampMin = "0.0",
            ToolTip = "Coût en énergie par Manifestation (Phase 2+). Rend le Dead visible 3s."))
    float ManifestEnergyCost = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Costs",
        meta = (ClampMin = "0.0",
            ToolTip = "Coût en énergie par Full Manifestation (Phase 3). Visible 8s."))
    float FullManifestEnergyCost = 50.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // FEAR ABILITY RADII & AMOUNTS
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0", ForceUnits = "cm",
            ToolTip = "Rayon de Whisper. Affecte jusqu'à 2 Living proches."))
    float WhisperRadius = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0",
            ToolTip = "Quantité de rythme cardiaque ajouté par Whisper au centre du rayon."))
    float WhisperFearAmount = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float ShiverRadius = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0"))
    float ShiverFearAmount = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0", ForceUnits = "cm",
            ToolTip = "Rayon de Spook. Cible uniquement le Living le plus proche."))
    float SpookRadius = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Fear",
        meta = (ClampMin = "0.0"))
    float SpookFearAmount = 40.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // DOOR PASS-THROUGH
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Pass-Through",
        meta = (ClampMin = "0.0",
            ToolTip = "Énergie dépensée par le Dead pour passer à travers une porte."))
    float DoorPassThroughEnergyCost = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Pass-Through",
        meta = (ClampMin = "50.0", ForceUnits = "cm/s",
            ToolTip = "Vitesse du mouvement automatique à travers la porte."))
    float DoorPassThroughSpeed = 200.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // WARD DEFENSES
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0", ForceUnits = "s",
            ToolTip = "Durée de toutes les wards (Salt, Sage, Chalk) en secondes."))
    float WardDuration = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0",
            ToolTip = "Énergie drainée du Dead au contact initial d'une ward."))
    float WardDefenseStrength = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0", ForceUnits = "x/s",
            ToolTip = "Drain continu de Sage sur le Dead à l'intérieur de la zone."))
    float SageDrainPerSecond = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0", ForceUnits = "x/s",
            ToolTip = "Calme par seconde que Sage applique aux Living à l'intérieur."))
    float SageLivingCalmPerSecond = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0", ForceUnits = "x/s",
            ToolTip = "Drain par seconde de la Salt Ward quand le Dead y transite."))
    float SaltBurnThroughDrainPerSecond = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "1.0", ClampMax = "100.0",
            ToolTip = "Dommage d'intégrité par transit. 100/BreachDamage = nombre de transits avant rupture."))
    float SaltBreachDamageAmount = 34.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.1", ClampMax = "1.0",
            ToolTip = "Facteur de ralentissement du Dead dans le Chalk Circle. 0.5 = moitié de la vitesse."))
    float ChalkSlowFactor = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ward Defenses",
        meta = (ClampMin = "0.0", ForceUnits = "x/s"))
    float ChalkLivingCalmPerSecond = 10.0f;

    // ═════════════════════════════════════════════════════════════════════════
    // ITEMS
    // ═════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "5.0", ForceUnits = "s",
            ToolTip = "Durée d'une allumette avant extinction automatique."))
    float MatchFlameDuration = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "10.0", ForceUnits = "s",
            ToolTip = "Durée totale de la batterie de la lampe de poche."))
    float FlashlightBatteryLife = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "0.0",
            ToolTip = "Énergie gagnée par le Dead en collectant une mémorabilias."))
    float MemorabiliaEnergyGain = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "50.0", ForceUnits = "cm",
            ToolTip = "Distance maximale pour les interactions (ramassage, portes, breaker)."))
    float InteractRange = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "50.0", ForceUnits = "cm",
            ToolTip = "Distance maximale pour déclencher l'absolution aux burial grounds."))
    float AbsolutionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "0.0",
            ToolTip = "Rayon de drain de l'Occult Item en mètres."))
    float OccultAuraRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "0.0", ForceUnits = "x/s"))
    float OccultDrainPerSecond = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items",
        meta = (ClampMin = "0.0", ForceUnits = "x/s"))
    float OccultCalmPerSecond = 8.0f;
};
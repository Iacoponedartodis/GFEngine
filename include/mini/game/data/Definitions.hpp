#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

namespace mini
{

// ── WeaponDef ─────────────────────────────────────────────────────────────
// Statistiche complete di un'arma, caricabili da data/weapons/<id>.json
struct WeaponDef
{
    std::string id;
    std::string name;

    float damage          = 25.0f;
    float fireRate        = 4.5f;   // colpi/sec
    float bulletSpeed     = 25.0f;  // m/s
    float bulletLifetime  = 3.0f;   // sec
    float bulletScale     = 0.12f;

    // Colore proiettile [r, g, b]
    std::array<float,3> bulletColor = {0.3f, 0.65f, 1.0f};

    // Sistema calore
    float heatPerShot     = 0.12f;
    float cooldownRate    = 0.30f;
    float overheatPenalty = 2.0f;

    // Asset (per quando avremo modelli reali)
    std::string meshPath;
    std::string projectileMeshPath;
};

// ── AiProfileDef ──────────────────────────────────────────────────────────
// Configurazione comportamento AI, caricabile da data/ai/<id>.json
struct AiProfileDef
{
    std::string id;

    float sightRange        = 20.0f;
    float fovDeg            = 110.0f;
    float hearingRange      = 12.0f;
    float reactionTime      = 0.4f;
    float aggression        = 0.65f;
    float accuracy          = 0.55f;
    float coverPreference   = 0.75f;
    float retreatHpThresh   = 0.25f;

    float peekDurationMin   = 0.6f;
    float peekDurationMax   = 1.1f;
    float hideDurationMin   = 0.8f;
    float hideDurationMax   = 1.8f;

    float repositionChance  = 0.30f;
    float flankChance       = 0.20f;

    float shootInterval     = 2.5f;   // sec tra un colpo e l'altro
    float patrolSpeed       = 2.5f;   // m/s
    float seekSpeed         = 4.0f;   // m/s

    bool  jumpEnabled       = true;
};

// ── EnemyDef ──────────────────────────────────────────────────────────────
// Definizione completa di un tipo di nemico, da data/enemies/<id>.json
struct EnemyDef
{
    std::string id;
    std::string name;

    std::string meshPath;
    std::string texturePath;
    std::string hitboxProfileId;
    std::string aiProfileId;
    std::string weaponId;

    int team = 2;

    // Stats base
    float hp          = 80.0f;
    float moveSpeed   = 4.0f;

    // Colore visivo (placeholder finché non ci sono texture reali)
    std::array<float,3> color     = {0.7f, 0.1f, 0.1f};
    std::array<float,3> bulletColor = {1.0f, 0.5f, 0.0f};
};

// ── MapDef ────────────────────────────────────────────────────────────────
// Definizione di una mappa, da data/maps/<id>.json
struct MapDef
{
    std::string id;
    std::string name;

    std::string meshPath;
    std::string metadataPath;
    std::string navmeshPath;

    std::array<float,3> spawnTeam1 = {0.f, 0.86f, 8.f};
    std::array<float,3> spawnTeam2 = {0.f, 0.86f, -8.f};

    int   maxTickets  = 10;
    int   enemyCount  = 6;
    std::vector<std::string> enemyTypes = {"grunt"};
};

// ── HitboxProfile ─────────────────────────────────────────────────────────
// Caricato da data/hitboxes/<id>.json dal DefinitionRegistry.
// Condiviso tra tutte le entità dello stesso tipo tramite puntatore.
// NOTA: questo duplica HitZone/HitboxProfile da HitboxComponent.hpp
//       per permettere il caricamento nel Registry senza dipendere dall'ECS.
//       Al momento della creazione dell'entità, il profilo dal Registry
//       viene assegnato al HitboxComponent via pointer.

} // namespace mini
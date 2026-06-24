#pragma once
#include <string>
#include <vector>
#include <array>

namespace mini
{

// ── Fazioni ───────────────────────────────────────────────────────────────
enum class Faction { Neutral = 0, Republic = 1, Separatist = 2 };
inline Faction factionFromString(const std::string& s)
{
    if (s == "republic")   return Faction::Republic;
    if (s == "separatist") return Faction::Separatist;
    return Faction::Neutral;
}
inline const char* factionToString(Faction f)
{
    switch (f) {
    case Faction::Republic:   return "republic";
    case Faction::Separatist: return "separatist";
    default:                  return "neutral";
    }
}
// Array statico per combo UI
inline const char* const* factionNames()
{
    static const char* names[] = { "neutral", "republic", "separatist" };
    return names;
}
inline int factionToIndex(Faction f) { return (int)f; }
inline Faction factionFromIndex(int i) { return (Faction)i; }

// ── AbilityDef ────────────────────────────────────────────────────────────
// data/abilities/<id>.json
struct AbilityDef
{
    std::string id;
    std::string name;
    std::string type;    // "shield" | "roll" | "melee" | "jetpack" | "missile" | "command_aura"
    float param1   = 0.0f;   // es. shield_hp, roll_speed, melee_range
    float param2   = 0.0f;   // es. regen_rate, roll_duration
    float param3   = 0.0f;   // es. regen_delay, cooldown
    float cooldown = 5.0f;
    bool  passive  = false;
};

// ── WeaponDef ─────────────────────────────────────────────────────────────
// data/weapons/<id>.json
struct WeaponDef
{
    std::string id;
    std::string name;
    Faction     faction        = Faction::Neutral;
    float damage              = 25.0f;
    float fireRate            = 4.5f;
    float bulletSpeed         = 25.0f;
    float bulletLifetime      = 3.0f;
    float bulletScale         = 0.12f;
    std::array<float,3> bulletColor = {0.3f, 0.65f, 1.0f};
    float heatPerShot         = 0.12f;
    float cooldownRate        = 0.30f;
    float overheatPenalty     = 2.0f;
    float effectiveRange      = 20.0f;
    float minRange            = 0.0f;

    // Precisione: 0=sempre al centro, 1=massima dispersione
    // Rappresenta il raggio di dispersione base (in gradi o unità angolari)
    float baseSpread          = 0.02f;  // fermo, non in mira
    float adsSpread           = 0.005f; // in mira (ADS)
    float moveSpread          = 0.06f;  // in movimento
    float sprintSpread        = 0.14f;  // in corsa
    float jumpSpread          = 0.20f;  // in aria

    std::string meshPath;
    std::string projectileMeshPath;
};

// ── AiProfileDef ─────────────────────────────────────────────────────────
// data/ai/<id>.json
struct AiProfileDef
{
    std::string id;
    std::string role;    // "infantry" | "heavy" | "sniper" | "elite" | "support"
    float sightRange       = 20.0f;
    float fovDeg           = 110.0f;
    float hearingRange     = 12.0f;
    float reactionTime     = 0.4f;
    float aggression       = 0.65f;
    float accuracy         = 0.55f;
    float coverPreference  = 0.75f;
    float retreatHpThresh  = 0.25f;
    float peekDurationMin  = 0.6f;
    float peekDurationMax  = 1.1f;
    float hideDurationMin  = 0.8f;
    float hideDurationMax  = 1.8f;
    float repositionChance = 0.30f;
    float flankChance      = 0.20f;
    float shootInterval    = 2.5f;
    float patrolSpeed      = 2.5f;
    float seekSpeed        = 4.0f;
    bool  jumpEnabled      = true;
};

// ── EnemyDef ─────────────────────────────────────────────────────────────
// Composizione completa di un'unità nemica. Solo dati, nessuna logica.
// data/enemies/<id>.json
// Nota: mesh/texture/color sono qui direttamente.
//       character_type era un'astrazione prematura senza UI dedicata.
struct EnemyDef
{
    std::string id;
    std::string name;
    Faction     faction   = Faction::Separatist;
    int         team      = 2;

    // Visuale
    std::string meshPath;
    std::string texturePath;
    std::array<float,3> color = {0.70f, 0.60f, 0.45f};

    // Composizione comportamentale
    std::string aiProfileId;
    std::string hitboxProfileId;
    std::vector<std::string> weaponIds;
    std::vector<std::string> abilityIds;

    // Stats
    float hp          = 80.0f;
    float moveSpeed   = 4.0f;
    float damageScale = 1.0f;

    // Colore proiettili
    std::array<float,3> bulletColor = {1.0f, 0.5f, 0.0f};

    [[nodiscard]] const std::string& primaryWeaponId() const
    {
        static const std::string empty;
        return weaponIds.empty() ? empty : weaponIds[0];
    }
    [[nodiscard]] bool hasAbility(const std::string& abilityId) const
    {
        for (auto& a : abilityIds) if (a == abilityId) return true;
        return false;
    }
};

// ── MapDef ────────────────────────────────────────────────────────────────
// data/maps/<id>.json
struct MapDef
{
    std::string id;
    std::string name;
    std::string meshPath;
    std::string metadataPath;
    std::string navmeshPath;
    std::array<float,3> spawnTeam1 = {0.f, 0.86f,  8.f};
    std::array<float,3> spawnTeam2 = {0.f, 0.86f, -8.f};
    int maxTickets = 10;
    int enemyCount = 6;
    int allyCount  = 1;
    std::vector<std::string> enemyTypes;
    std::vector<std::string> allyTypes;
};

// ── PlayerDef ─────────────────────────────────────────────────────────────
// Stat BASE del personaggio giocabile. Non contiene equipaggiamento né
// visuale: armi, armatura e colori vengono scelti nel PreMatch/loadout.
// data/characters/<id>.json
struct PlayerDef
{
    std::string id;
    std::string name;
    std::string description;

    // Stat base (modificabili dall'editor, indipendenti dall'equipaggiamento)
    float hp          = 100.0f;
    float moveSpeed   = 5.0f;
    float jumpHeight  = 1.0f;   // moltiplicatore impulso base
    float sprintMult  = 1.5f;   // moltiplicatore velocità scatto
    float armorRating = 1.0f;   // riduzione danni passiva (1=standard)
};

} // namespace mini
#include "mini/game/data/DefinitionRegistry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace mini
{
using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Helper ────────────────────────────────────────────────────────────────
static std::optional<json> readJson(const fs::path& p)
{
    std::ifstream f(p);
    if (!f.is_open()) { std::cerr << "[Registry] Cannot open: " << p << "\n"; return {}; }
    try { json j; f >> j; return j; }
    catch(const std::exception& e) {
        std::cerr << "[Registry] Errore parsing \"" << p.string() << "\": " << e.what() << "\n";
        return {};
    }
}
static float getf(const json& j, const char* k, float d)
{ return j.contains(k) ? j[k].get<float>() : d; }
static int geti(const json& j, const char* k, int d)
{ return j.contains(k) ? j[k].get<int>() : d; }
static bool getb(const json& j, const char* k, bool d)
{ return j.contains(k) ? j[k].get<bool>() : d; }
static std::string gets(const json& j, const char* k, const std::string& d = "")
{ return j.contains(k) ? j[k].get<std::string>() : d; }
static std::array<float,3> getColor(const json& j, const char* k,
                                     std::array<float,3> d = {1,1,1})
{
    if (!j.contains(k) || !j[k].is_array() || j[k].size() < 3) return d;
    return {j[k][0].get<float>(), j[k][1].get<float>(), j[k][2].get<float>()};
}
static std::vector<std::string> getStrArray(const json& j, const char* k)
{
    std::vector<std::string> out;
    if (j.contains(k) && j[k].is_array())
        for (auto& e : j[k]) out.push_back(e.get<std::string>());
    return out;
}

// ── Loaders ───────────────────────────────────────────────────────────────
void DefinitionRegistry::loadAbilities(const std::string& dir)
{
    fs::path folder = dir + "/abilities";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        AbilityDef a;
        a.id      = gets(*j, "id", entry.path().stem().string());
        a.name    = gets(*j, "name", a.id);
        a.type    = gets(*j, "type");
        a.param1  = getf(*j, "param1", 0);
        a.param2  = getf(*j, "param2", 0);
        a.param3  = getf(*j, "param3", 0);
        a.cooldown= getf(*j, "cooldown", 5);
        a.passive = getb(*j, "passive", false);
        std::cout << "[Registry] Ability: " << a.id << " (" << a.type << ")\n";
        m_abilities[a.id] = std::move(a);
    }
}

void DefinitionRegistry::loadWeapons(const std::string& dir)
{
    fs::path folder = dir + "/weapons";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        WeaponDef w;
        w.id                 = gets(*j, "id", entry.path().stem().string());
        w.name               = gets(*j, "name", w.id);
        w.faction            = factionFromString(gets(*j, "faction"));
        w.damage             = getf(*j, "damage", 25);
        w.fireRate           = getf(*j, "fire_rate", 4.5f);
        w.bulletSpeed        = getf(*j, "bullet_speed", 25);
        w.bulletLifetime     = getf(*j, "bullet_lifetime", 3);
        w.bulletScale        = getf(*j, "bullet_scale", 0.12f);
        w.bulletColor        = getColor(*j, "bullet_color", {0.3f,0.65f,1.0f});
        w.heatPerShot        = getf(*j, "heat_per_shot", 0.12f);
        w.cooldownRate       = getf(*j, "cooldown_rate", 0.3f);
        w.overheatPenalty    = getf(*j, "overheat_penalty", 2);
        w.effectiveRange     = getf(*j, "effective_range", 20);
        w.minRange           = getf(*j, "min_range", 0);
        w.baseSpread         = getf(*j, "spread_base",   0.02f);
        w.adsSpread          = getf(*j, "spread_ads",    0.005f);
        w.moveSpread         = getf(*j, "spread_move",   0.06f);
        w.sprintSpread       = getf(*j, "spread_sprint", 0.14f);
        w.jumpSpread         = getf(*j, "spread_jump",   0.20f);
        w.meshPath           = gets(*j, "mesh");
        w.projectileMeshPath = gets(*j, "projectile_mesh");
        std::cout << "[Registry] Weapon: " << w.id
                  << " [" << factionToString(w.faction) << "]\n";
        m_weapons[w.id] = std::move(w);
    }
}

void DefinitionRegistry::loadAiProfiles(const std::string& dir)
{
    fs::path folder = dir + "/ai";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        AiProfileDef a;
        a.id               = gets(*j, "profile_id", entry.path().stem().string());
        a.role             = gets(*j, "role", "infantry");
        a.sightRange       = getf(*j, "sight_range", 20);
        a.fovDeg           = getf(*j, "fov_deg", 110);
        a.hearingRange     = getf(*j, "hearing_range", 12);
        a.reactionTime     = getf(*j, "reaction_time", 0.4f);
        a.aggression       = getf(*j, "aggression", 0.65f);
        a.accuracy         = getf(*j, "accuracy", 0.55f);
        a.coverPreference  = getf(*j, "cover_preference", 0.75f);
        a.retreatHpThresh  = getf(*j, "retreat_hp_threshold", 0.25f);
        a.peekDurationMin  = getf(*j, "peek_duration_min", 0.6f);
        a.peekDurationMax  = getf(*j, "peek_duration_max", 1.1f);
        a.hideDurationMin  = getf(*j, "hide_duration_min", 0.8f);
        a.hideDurationMax  = getf(*j, "hide_duration_max", 1.8f);
        a.repositionChance = getf(*j, "reposition_chance", 0.3f);
        a.flankChance      = getf(*j, "flank_chance", 0.2f);
        a.shootInterval    = getf(*j, "shoot_interval", 2.5f);
        a.patrolSpeed      = getf(*j, "patrol_speed", 2.5f);
        a.seekSpeed        = getf(*j, "seek_speed", 4);
        a.jumpEnabled      = getb(*j, "jump_enabled", true);
        std::cout << "[Registry] AI Profile: " << a.id << " (role:" << a.role << ")\n";
        m_aiProfiles[a.id] = std::move(a);
    }
}

void DefinitionRegistry::loadEnemies(const std::string& dir)
{
    fs::path folder = dir + "/enemies";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        EnemyDef e;
        e.id              = gets(*j, "id", entry.path().stem().string());
        e.name            = gets(*j, "name", e.id);
        e.faction         = factionFromString(gets(*j, "faction", "separatist"));
        e.team            = geti(*j, "team", 2);
        e.meshPath        = gets(*j, "mesh");
        e.texturePath     = gets(*j, "texture");
        e.color           = getColor(*j, "color", {0.70f,0.60f,0.45f});
        e.aiProfileId     = gets(*j, "ai_profile");
        e.hitboxProfileId = gets(*j, "hitbox_profile");
        e.weaponIds       = getStrArray(*j, "weapons");
        e.abilityIds      = getStrArray(*j, "abilities");
        e.bulletColor     = getColor(*j, "bullet_color", {1.0f,0.5f,0.0f});
        if ((*j).contains("stats")) {
            auto& s = (*j)["stats"];
            e.hp          = getf(s, "hp", 80);
            e.moveSpeed   = getf(s, "move_speed", 4);
            e.damageScale = getf(s, "damage_scale", 1);
        }
        // Retrocompatibilità: campo "weapon" singolo
        if (e.weaponIds.empty() && (*j).contains("weapon"))
            e.weaponIds.push_back((*j)["weapon"].get<std::string>());
        std::cout << "[Registry] Enemy: " << e.id
                  << " (weapons:" << e.weaponIds.size()
                  << " abilities:" << e.abilityIds.size() << ")\n";
        m_enemies[e.id] = std::move(e);
    }
}

void DefinitionRegistry::loadMaps(const std::string& dir)
{
    fs::path folder = dir + "/maps";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        MapDef m;
        m.id           = gets(*j, "id", entry.path().stem().string());
        m.name         = gets(*j, "name", m.id);
        m.meshPath     = gets(*j, "mesh");
        m.metadataPath = gets(*j, "metadata");
        m.maxTickets   = geti(*j, "max_tickets", 10);
        m.enemyCount   = geti(*j, "enemy_count", 6);
        m.allyCount    = geti(*j, "ally_count",  1);
        if ((*j).contains("spawn_team1") && (*j)["spawn_team1"].size() >= 3)
            m.spawnTeam1 = {(*j)["spawn_team1"][0],(*j)["spawn_team1"][1],(*j)["spawn_team1"][2]};
        if ((*j).contains("spawn_team2") && (*j)["spawn_team2"].size() >= 3)
            m.spawnTeam2 = {(*j)["spawn_team2"][0],(*j)["spawn_team2"][1],(*j)["spawn_team2"][2]};
        m.enemyTypes   = getStrArray(*j, "enemy_types");
        m.allyTypes    = getStrArray(*j, "ally_types");
        std::cout << "[Registry] Map: " << m.id << "\n";
        m_maps[m.id] = std::move(m);
    }
}

void DefinitionRegistry::loadHitboxProfiles(const std::string& dir)
{
    fs::path folder = dir + "/hitboxes";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        HitboxProfile p;
        p.profileId = gets(*j, "profile_id", entry.path().stem().string());
        if ((*j).contains("zones") && (*j)["zones"].is_array())
            for (auto& z : (*j)["zones"])
            {
                HitZone hz;
                hz.name               = gets(z, "name", "unknown");
                hz.damageMultiplier   = getf(z, "damage_multiplier", 1);
                hz.debugVisible       = getb(z, "debug_visible", true);
                if (z.contains("offset") && z["offset"].size() >= 3)
                    hz.offset = {z["offset"][0], z["offset"][1], z["offset"][2]};
                if (z.contains("half_extents") && z["half_extents"].size() >= 3)
                    hz.halfExtents = {z["half_extents"][0], z["half_extents"][1], z["half_extents"][2]};
                p.zones.push_back(std::move(hz));
            }
        std::cout << "[Registry] Hitbox: " << p.profileId
                  << " (" << p.zones.size() << " zone)\n";
        m_hitboxProfiles[p.profileId] = std::move(p);
    }
}

// Carica alleati da data/allies/ — stessa struttura EnemyDef ma team=1
void DefinitionRegistry::loadAllies(const std::string& dir)
{
    fs::path folder = dir + "/allies";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        EnemyDef e;
        e.id              = gets(*j, "id", entry.path().stem().string());
        e.name            = gets(*j, "name", e.id);
        e.faction         = factionFromString(gets(*j, "faction", "republic"));
        e.team            = 1; // sempre alleati
        e.meshPath        = gets(*j, "mesh");
        e.texturePath     = gets(*j, "texture");
        e.color           = getColor(*j, "color", {0.25f,0.45f,1.0f});
        e.aiProfileId     = gets(*j, "ai_profile");
        e.hitboxProfileId = gets(*j, "hitbox_profile");
        e.weaponIds       = getStrArray(*j, "weapons");
        e.abilityIds      = getStrArray(*j, "abilities");
        e.bulletColor     = getColor(*j, "bullet_color", {0.30f,0.60f,1.0f});
        if ((*j).contains("stats")) {
            auto& s = (*j)["stats"];
            e.hp          = getf(s, "hp", 60);
            e.moveSpeed   = getf(s, "move_speed", 1.8f);
            e.damageScale = getf(s, "damage_scale", 1);
        }
        if (e.weaponIds.empty() && (*j).contains("weapon"))
            e.weaponIds.push_back((*j)["weapon"].get<std::string>());
        std::cout << "[Registry] Ally: " << e.id << "\n";
        m_allies[e.id] = std::move(e);
    }
}

void DefinitionRegistry::loadPlayerDefs(const std::string& dir)
{
    fs::path folder = dir + "/characters";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        PlayerDef p;
        p.id          = gets(*j, "id", entry.path().stem().string());
        p.name        = gets(*j, "name", p.id);
        p.description = gets(*j, "description");
        if ((*j).contains("stats")) {
            auto& s = (*j)["stats"];
            p.hp          = getf(s, "hp",           100.0f);
            p.moveSpeed   = getf(s, "move_speed",     5.0f);
            p.jumpHeight  = getf(s, "jump_height",    1.0f);
            p.sprintMult  = getf(s, "sprint_mult",    1.5f);
            p.armorRating = getf(s, "armor_rating",   1.0f);
        }
        std::cout << "[Registry] PlayerDef: " << p.id << "\n";
        m_playerDefs[p.id] = std::move(p);
    }
}

void DefinitionRegistry::loadAll(const std::string& dataRoot)
{
    m_abilities.clear();
    m_weapons.clear();
    m_aiProfiles.clear();
    m_enemies.clear();
    m_allies.clear();
    m_maps.clear();
    m_hitboxProfiles.clear();
    m_playerDefs.clear();
    m_loaded = false;

    std::cout << "[Registry] Caricamento definizioni da '" << dataRoot << "'...\n";

    loadAbilities(dataRoot);
    loadWeapons(dataRoot);
    loadAiProfiles(dataRoot);
    loadHitboxProfiles(dataRoot);
    loadEnemies(dataRoot);
    loadAllies(dataRoot);
    loadMaps(dataRoot);
    loadPlayerDefs(dataRoot);

    m_loaded = true;
    std::cout << "[Registry] " << m_weapons.size() << " armi, "
              << m_enemies.size() << " nemici, "
              << m_allies.size() << " alleati, "
              << m_abilities.size() << " abilità, "
              << m_aiProfiles.size() << " profili AI, "
              << m_hitboxProfiles.size() << " hitbox, "
              << m_playerDefs.size() << " preset personaggio.\n";
}

// ── Getters ───────────────────────────────────────────────────────────────
#define GETTER(map, type, name) \
const type* DefinitionRegistry::name(const std::string& id) const { \
    auto it = map.find(id); \
    if (it == map.end()) { std::cerr << "[Registry] " #type " non trovato: " << id << "\n"; return nullptr; } \
    return &it->second; }

GETTER(m_abilities,      AbilityDef,       getAbility)
GETTER(m_weapons,        WeaponDef,        getWeapon)
GETTER(m_aiProfiles,     AiProfileDef,     getAiProfile)
GETTER(m_enemies,        EnemyDef,         getEnemy)
GETTER(m_allies,         EnemyDef,         getAlly)
GETTER(m_maps,           MapDef,           getMap)
GETTER(m_hitboxProfiles, HitboxProfile,    getHitboxProfile)
GETTER(m_playerDefs,     PlayerDef,        getPlayerDef)
#undef GETTER

} // namespace mini
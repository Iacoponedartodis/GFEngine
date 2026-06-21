#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace mini
{

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Helper: leggi un file JSON, ritorna oggetto json o null ───────────────
static std::optional<json> readJson(const fs::path& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "[Registry] Impossibile aprire: " << path << std::endl;
        return std::nullopt;
    }
    try
    {
        json j;
        f >> j;
        return j;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Registry] Errore parsing " << path << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

static float getf(const json& j, const char* k, float def)
{ return j.contains(k) ? j[k].get<float>() : def; }
static int   geti(const json& j, const char* k, int def)
{ return j.contains(k) ? j[k].get<int>() : def; }
static std::string gets(const json& j, const char* k, const std::string& def = "")
{ return j.contains(k) ? j[k].get<std::string>() : def; }
static std::array<float,3> getColor(const json& j, const char* k,
                                     std::array<float,3> def = {1,1,1})
{
    if (!j.contains(k) || !j[k].is_array() || j[k].size() < 3) return def;
    return {j[k][0].get<float>(), j[k][1].get<float>(), j[k][2].get<float>()};
}

// ── Loaders ───────────────────────────────────────────────────────────────

void DefinitionRegistry::loadWeapons(const std::string& dir)
{
    const fs::path folder = dir + "/weapons";
    if (!fs::exists(folder)) return;

    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto jOpt = readJson(entry.path());
        if (!jOpt) continue;
        const auto& j = *jOpt;

        WeaponDef w;
        w.id              = gets(j, "id", entry.path().stem().string());
        w.name            = gets(j, "name", w.id);
        w.damage          = getf(j, "damage",           25.0f);
        w.fireRate        = getf(j, "fire_rate",         4.5f);
        w.bulletSpeed     = getf(j, "bullet_speed",     25.0f);
        w.bulletLifetime  = getf(j, "bullet_lifetime",   3.0f);
        w.bulletScale     = getf(j, "bullet_scale",      0.12f);
        w.bulletColor     = getColor(j, "bullet_color",  {0.3f, 0.65f, 1.0f});
        w.heatPerShot     = getf(j, "heat_per_shot",     0.12f);
        w.cooldownRate    = getf(j, "cooldown_rate",     0.30f);
        w.overheatPenalty = getf(j, "overheat_penalty",  2.0f);
        w.meshPath            = gets(j, "mesh");
        w.projectileMeshPath  = gets(j, "projectile_mesh");

        std::cout << "[Registry] Weapon: " << w.id << " (" << w.name << ")" << std::endl;
        m_weapons[w.id] = std::move(w);
    }
}

void DefinitionRegistry::loadEnemies(const std::string& dir)
{
    const fs::path folder = dir + "/enemies";
    if (!fs::exists(folder)) return;

    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto jOpt = readJson(entry.path());
        if (!jOpt) continue;
        const auto& j = *jOpt;

        EnemyDef e;
        e.id              = gets(j, "id", entry.path().stem().string());
        e.name            = gets(j, "name", e.id);
        e.meshPath        = gets(j, "mesh");
        e.texturePath     = gets(j, "texture");
        e.hitboxProfileId = gets(j, "hitbox_profile");
        e.aiProfileId     = gets(j, "ai_profile");
        e.weaponId        = gets(j, "weapon");
        e.team            = geti(j, "team", 2);
        e.color           = getColor(j, "color",        {0.7f, 0.1f, 0.1f});
        e.bulletColor     = getColor(j, "bullet_color", {1.0f, 0.5f, 0.0f});

        if (j.contains("stats"))
        {
            const auto& s = j["stats"];
            e.hp        = getf(s, "hp",         80.0f);
            e.moveSpeed = getf(s, "move_speed",  4.0f);
        }

        std::cout << "[Registry] Enemy: " << e.id << " (" << e.name << ")" << std::endl;
        m_enemies[e.id] = std::move(e);
    }
}

void DefinitionRegistry::loadMaps(const std::string& dir)
{
    const fs::path folder = dir + "/maps";
    if (!fs::exists(folder)) return;

    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto jOpt = readJson(entry.path());
        if (!jOpt) continue;
        const auto& j = *jOpt;

        MapDef m;
        m.id           = gets(j, "id", entry.path().stem().string());
        m.name         = gets(j, "name", m.id);
        m.meshPath     = gets(j, "mesh");
        m.metadataPath = gets(j, "metadata");
        m.navmeshPath  = gets(j, "navmesh");
        m.maxTickets   = geti(j, "max_tickets", 10);
        m.enemyCount   = geti(j, "enemy_count",  6);

        if (j.contains("spawn_team1") && j["spawn_team1"].size() >= 3)
            m.spawnTeam1 = {j["spawn_team1"][0], j["spawn_team1"][1], j["spawn_team1"][2]};
        if (j.contains("spawn_team2") && j["spawn_team2"].size() >= 3)
            m.spawnTeam2 = {j["spawn_team2"][0], j["spawn_team2"][1], j["spawn_team2"][2]};
        if (j.contains("enemy_types"))
            for (auto& t : j["enemy_types"])
                m.enemyTypes.push_back(t.get<std::string>());

        std::cout << "[Registry] Map: " << m.id << " (" << m.name << ")" << std::endl;
        m_maps[m.id] = std::move(m);
    }
}

void DefinitionRegistry::loadAiProfiles(const std::string& dir)
{
    const fs::path folder = dir + "/ai";
    if (!fs::exists(folder)) return;

    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto jOpt = readJson(entry.path());
        if (!jOpt) continue;
        const auto& j = *jOpt;

        AiProfileDef a;
        a.id              = gets(j, "profile_id", entry.path().stem().string());
        a.sightRange      = getf(j, "sight_range",          20.0f);
        a.fovDeg          = getf(j, "fov_deg",             110.0f);
        a.hearingRange    = getf(j, "hearing_range",        12.0f);
        a.reactionTime    = getf(j, "reaction_time",         0.4f);
        a.aggression      = getf(j, "aggression",            0.65f);
        a.accuracy        = getf(j, "accuracy",              0.55f);
        a.coverPreference = getf(j, "cover_preference",      0.75f);
        a.retreatHpThresh = getf(j, "retreat_hp_threshold",  0.25f);
        a.peekDurationMin = getf(j, "peek_duration_min",     0.6f);
        a.peekDurationMax = getf(j, "peek_duration_max",     1.1f);
        a.hideDurationMin = getf(j, "hide_duration_min",     0.8f);
        a.hideDurationMax = getf(j, "hide_duration_max",     1.8f);
        a.repositionChance= getf(j, "reposition_chance",     0.3f);
        a.flankChance     = getf(j, "flank_chance",          0.2f);
        a.shootInterval   = getf(j, "shoot_interval",        2.5f);
        a.patrolSpeed     = getf(j, "patrol_speed",          2.5f);
        a.seekSpeed       = getf(j, "seek_speed",            4.0f);
        a.jumpEnabled     = j.contains("jump_enabled") ? j["jump_enabled"].get<bool>() : true;

        std::cout << "[Registry] AI Profile: " << a.id << std::endl;
        m_aiProfiles[a.id] = std::move(a);
    }
}


void DefinitionRegistry::loadHitboxProfiles(const std::string& dir)
{
    const fs::path folder = dir + "/hitboxes";
    if (!fs::exists(folder)) return;

    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto jOpt = readJson(entry.path());
        if (!jOpt) continue;
        const auto& j = *jOpt;

        HitboxProfile profile;
        profile.profileId = gets(j, "profile_id", entry.path().stem().string());

        if (j.contains("zones") && j["zones"].is_array())
        {
            for (const auto& zj : j["zones"])
            {
                HitZone zone;
                zone.name = gets(zj, "name", "unknown");
                zone.damageMultiplier = getf(zj, "damage_multiplier", 1.0f);
                zone.debugVisible     = zj.contains("debug_visible") ?
                                        zj["debug_visible"].get<bool>() : true;

                if (zj.contains("offset") && zj["offset"].size() >= 3)
                    zone.offset = {zj["offset"][0], zj["offset"][1], zj["offset"][2]};
                if (zj.contains("half_extents") && zj["half_extents"].size() >= 3)
                    zone.halfExtents = {zj["half_extents"][0],
                                        zj["half_extents"][1],
                                        zj["half_extents"][2]};
                profile.zones.push_back(std::move(zone));
            }
        }

        std::cout << "[Registry] Hitbox: " << profile.profileId
                  << " (" << profile.zones.size() << " zone)" << std::endl;
        m_hitboxProfiles[profile.profileId] = std::move(profile);
    }
}

void DefinitionRegistry::loadAll(const std::string& dataRoot)
{
    m_weapons.clear(); m_enemies.clear();
    m_maps.clear();    m_aiProfiles.clear();
    m_loaded = false;

    std::cout << "[Registry] Caricamento definizioni da '" << dataRoot << "'..." << std::endl;
    loadWeapons(dataRoot);
    loadEnemies(dataRoot);
    loadMaps(dataRoot);
    loadAiProfiles(dataRoot);
    loadHitboxProfiles(dataRoot);

    m_loaded = true;
    std::cout << "[Registry] " << m_weapons.size() << " armi, "
              << m_enemies.size() << " nemici, "
              << m_maps.size() << " mappe, "
              << m_aiProfiles.size() << " profili AI, "
              << m_hitboxProfiles.size() << " profili hitbox." << std::endl;
}

const WeaponDef* DefinitionRegistry::getWeapon(const std::string& id) const
{
    auto it = m_weapons.find(id);
    if (it == m_weapons.end())
    { std::cerr << "[Registry] Weapon non trovata: " << id << std::endl; return nullptr; }
    return &it->second;
}
const EnemyDef* DefinitionRegistry::getEnemy(const std::string& id) const
{
    auto it = m_enemies.find(id);
    if (it == m_enemies.end())
    { std::cerr << "[Registry] Enemy non trovato: " << id << std::endl; return nullptr; }
    return &it->second;
}
const MapDef* DefinitionRegistry::getMap(const std::string& id) const
{
    auto it = m_maps.find(id);
    if (it == m_maps.end())
    { std::cerr << "[Registry] Map non trovata: " << id << std::endl; return nullptr; }
    return &it->second;
}
const HitboxProfile* DefinitionRegistry::getHitboxProfile(const std::string& id) const
{
    auto it = m_hitboxProfiles.find(id);
    if (it == m_hitboxProfiles.end())
    { std::cerr << "[Registry] HitboxProfile non trovato: " << id << std::endl; return nullptr; }
    return &it->second;
}

const AiProfileDef* DefinitionRegistry::getAiProfile(const std::string& id) const
{
    auto it = m_aiProfiles.find(id);
    if (it == m_aiProfiles.end())
    { std::cerr << "[Registry] AI Profile non trovato: " << id << std::endl; return nullptr; }
    return &it->second;
}

} // namespace mini
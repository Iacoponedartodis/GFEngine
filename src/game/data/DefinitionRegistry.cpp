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
        // ADR-001: id = SOLO filename stem; l'eventuale campo in-file è
        // ignorato (un id stantio registrava la definizione sotto la chiave
        // sbagliata rompendo le cross-ref in silenzio — KI #21).
        a.id      = entry.path().stem().string();
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
        w.id                 = entry.path().stem().string();   // ADR-001 (KI #21)
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
        w.meshScale          = getf(*j, "mesh_scale", 0.8f);
        w.meshRotX           = getf(*j, "mesh_rot_x", 0.0f);
        w.meshRotY           = getf(*j, "mesh_rot_y", 0.0f);
        if ((*j).contains("attach_points") && (*j)["attach_points"].is_object())
        {
            auto& ap = (*j)["attach_points"];
            auto readPt = [&](const char* k, std::array<float,3>& out) {
                if (ap.contains(k) && ap[k].is_array() && ap[k].size() >= 3)
                    out = {ap[k][0].get<float>(), ap[k][1].get<float>(), ap[k][2].get<float>()};
            };
            readPt("grip",       w.gripAttach);
            readPt("right_hand", w.gripAttach);  // right_hand ha precedenza
            readPt("muzzle",     w.muzzleAttach);
        }
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
        a.id               = entry.path().stem().string();   // ADR-001 (KI #21)
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

// Parser CONDIVISO nemici/alleati (Todo A7): stessa struttura EnemyDef,
// cambiano solo team e i default di fazione/colore/stats. Prima erano due
// funzioni duplicate al 95% che derivavano a ogni nuovo campo.
static EnemyDef parseUnitDef(const nlohmann::json& j, const fs::path& path, int team)
{
    const bool ally = (team == 1);
    EnemyDef e;
    e.id      = path.stem().string();   // ADR-001 (KI #21): SOLO filename stem
    e.name    = gets(j, "name", e.id);
    e.faction = factionFromString(gets(j, "faction",
                                       ally ? "republic" : "separatist"));
    e.team    = ally ? 1 : geti(j, "team", 2);
    e.meshPath    = gets(j, "mesh");
    e.texturePath = gets(j, "texture");
    e.color       = getColor(j, "color",
        ally ? std::array<float,3>{0.25f,0.45f,1.0f}
             : std::array<float,3>{0.70f,0.60f,0.45f});
    e.meshRotX  = getf(j, "mesh_rot_x", 0.0f);
    e.meshRotY  = getf(j, "mesh_rot_y", 0.0f);
    e.meshScale = getf(j, "mesh_scale", 1.0f);
    if (j.contains("attach_points") && j["attach_points"].is_object())
    {
        for (auto& [apName, apVal] : j["attach_points"].items())
        {
            if (!apVal.is_array() || apVal.size() < 3) continue;
            std::array<float,3> pt = {apVal[0].get<float>(),
                                      apVal[1].get<float>(),
                                      apVal[2].get<float>()};
            e.attachPoints[apName] = pt;
            if (apName == "foot") e.footAttach = pt;
        }
    }
    if (j.contains("weapon_display") && j["weapon_display"].is_object())
    {
        auto& wd = j["weapon_display"];
        e.weaponDisplay.weaponId = gets(wd, "id");
        e.weaponDisplay.hand     = gets(wd, "hand", "right_hand");
        e.weaponDisplay.scale    = getf(wd, "scale", 1.0f);
        if (wd.contains("rot") && wd["rot"].size() >= 3)
            e.weaponDisplay.rot = {wd["rot"][0].get<float>(), wd["rot"][1].get<float>(), wd["rot"][2].get<float>()};
        if (wd.contains("offset") && wd["offset"].size() >= 3)
            e.weaponDisplay.offset = {wd["offset"][0].get<float>(), wd["offset"][1].get<float>(), wd["offset"][2].get<float>()};
    }
    e.aiProfileId     = gets(j, "ai_profile");
    e.hitboxProfileId = gets(j, "hitbox_profile");
    e.weaponIds       = getStrArray(j, "weapons");
    e.abilityIds      = getStrArray(j, "abilities");
    e.bulletColor     = getColor(j, "bullet_color",
        ally ? std::array<float,3>{0.30f,0.60f,1.0f}
             : std::array<float,3>{1.0f,0.5f,0.0f});
    if (j.contains("stats")) {
        auto& s = j["stats"];
        e.hp          = getf(s, "hp",         ally ? 60.0f : 80.0f);
        e.moveSpeed   = getf(s, "move_speed", ally ? 1.8f  : 4.0f);
        e.damageScale = getf(s, "damage_scale", 1);
    }
    // Retrocompatibilità: campo "weapon" singolo
    if (e.weaponIds.empty() && j.contains("weapon"))
        e.weaponIds.push_back(j["weapon"].get<std::string>());
    return e;
}

void DefinitionRegistry::loadEnemies(const std::string& dir)
{
    fs::path folder = dir + "/enemies";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        EnemyDef e = parseUnitDef(*j, entry.path(), 2);
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
        m.id           = entry.path().stem().string();   // ADR-001 (KI #21)
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
        if ((*j).contains("geometry") && (*j)["geometry"].is_array())
        {
            for (auto& gb : (*j)["geometry"])
            {
                MapGeometryBox box;
                box.x  = getf(gb, "x",  0.0f);
                box.y  = getf(gb, "y",  0.0f);
                box.z  = getf(gb, "z",  0.0f);
                box.ry = getf(gb, "ry", 0.0f);
                box.sx = getf(gb, "sx", 2.0f);
                box.sy = getf(gb, "sy", 2.0f);
                box.sz = getf(gb, "sz", 2.0f);
                box.r  = getf(gb, "r",  0.35f);
                box.g  = getf(gb, "g",  0.32f);
                box.b  = getf(gb, "b",  0.28f);
                box.collider = getb(gb, "collider", true);
                m.geometry.push_back(box);
            }
        }
        if ((*j).contains("command_posts") && (*j)["command_posts"].is_array())
        {
            for (auto& cp : (*j)["command_posts"])
            {
                CommandPostDef p;
                p.label       = gets(cp, "label", "Post");
                p.x           = getf(cp, "x", 0.0f);
                p.y           = getf(cp, "y", 0.0f);
                p.z           = getf(cp, "z", 0.0f);
                p.radius      = getf(cp, "radius", 4.0f);
                p.initialTeam = geti(cp, "team", 0);
                p.captureTime = getf(cp, "capture_time", 8.0f);
                m.commandPosts.push_back(p);
            }
        }
        // ── Map Metadata (15_MapMetadata): opzionali, additivi ─────────
        if ((*j).contains("cover_points") && (*j)["cover_points"].is_array())
        {
            for (auto& cp : (*j)["cover_points"])
            {
                CoverPointDef c;
                c.x         = getf(cp, "x", 0.0f);
                c.y         = getf(cp, "y", 0.0f);
                c.z         = getf(cp, "z", 0.0f);
                c.facingDeg = getf(cp, "facing_deg", 0.0f);
                c.height    = getf(cp, "height", 1.0f);
                m.coverPoints.push_back(c);
            }
        }
        if ((*j).contains("patrol_routes") && (*j)["patrol_routes"].is_array())
        {
            for (auto& pr : (*j)["patrol_routes"])
            {
                PatrolRouteDef r;
                r.id = gets(pr, "id", "route");
                if (pr.contains("points") && pr["points"].is_array())
                    for (auto& pt : pr["points"])
                        if (pt.is_array() && pt.size() >= 3)
                            r.points.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
                m.patrolRoutes.push_back(std::move(r));
            }
        }
        if ((*j).contains("danger_zones") && (*j)["danger_zones"].is_array())
        {
            for (auto& dz : (*j)["danger_zones"])
            {
                DangerZoneDef d;
                d.x           = getf(dz, "x", 0.0f);
                d.y           = getf(dz, "y", 0.0f);
                d.z           = getf(dz, "z", 0.0f);
                d.radius      = getf(dz, "radius", 4.0f);
                d.dangerLevel = getf(dz, "danger_level", 0.5f);
                m.dangerZones.push_back(d);
            }
        }

        // Veicoli in mappa (19_Vehicles, Fase A)
        if ((*j).contains("vehicle_spawns") && (*j)["vehicle_spawns"].is_array())
        {
            for (auto& vs : (*j)["vehicle_spawns"])
            {
                VehicleSpawnDef v;
                v.vehicleId = gets(vs, "vehicle_id");
                v.x  = getf(vs, "x", 0.0f);
                v.z  = getf(vs, "z", 0.0f);
                v.ry = getf(vs, "ry", 0.0f);
                if (!v.vehicleId.empty()) m.vehicleSpawns.push_back(std::move(v));
            }
        }

        std::cout << "[Registry] Map: " << m.id
                  << " (geometry: " << m.geometry.size() << " box, "
                  << m.commandPosts.size() << " command post, "
                  << m.coverPoints.size() << " cover, "
                  << m.patrolRoutes.size() << " route, "
                  << m.dangerZones.size() << " danger)\n";
        m_maps[m.id] = std::move(m);
    }
}

// ── Veicoli (19_Vehicles, Fase A) ─────────────────────────────────────────
void DefinitionRegistry::loadVehicles(const std::string& dir)
{
    fs::path folder = dir + "/vehicles";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        VehicleDef v;
        v.id          = entry.path().stem().string();   // id = filename (ADR-001)
        v.name        = gets(*j, "name", v.id);
        v.hp          = getf(*j, "hp", 150.0f);
        v.maxSpeed    = getf(*j, "max_speed", 12.0f);
        v.accel       = getf(*j, "accel", 10.0f);
        v.turnRateDeg = getf(*j, "turn_rate_deg", 90.0f);
        v.meshPath    = gets(*j, "mesh");
        v.meshScale   = getf(*j, "mesh_scale", 1.0f);
        v.meshRotX    = getf(*j, "mesh_rot_x", 0.0f);
        v.meshRotY    = getf(*j, "mesh_rot_y", 0.0f);
        v.meshOffsetY = getf(*j, "mesh_offset_y", 0.0f);
        v.halfX       = getf(*j, "half_x", 0.7f);
        v.halfY       = getf(*j, "half_y", 0.5f);
        v.halfZ       = getf(*j, "half_z", 1.2f);
        // Volume di danno (0 = usa il box di collisione)
        v.hitOffsetY  = getf(*j, "hit_offset_y", 0.0f);
        v.hitHalfX    = getf(*j, "hit_half_x", 0.0f);
        v.hitHalfY    = getf(*j, "hit_half_y", 0.0f);
        v.hitHalfZ    = getf(*j, "hit_half_z", 0.0f);
        if ((*j).contains("color") && (*j)["color"].size() >= 3)
            v.color = {(*j)["color"][0], (*j)["color"][1], (*j)["color"][2]};
        std::cout << "[Registry] Vehicle: " << v.id << "\n";
        m_vehicles[v.id] = std::move(v);
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
        p.profileId = entry.path().stem().string();   // ADR-001 (KI #21)
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
                hz.boneName = z.value("bone", std::string(""));
                if (z.contains("rotation") && z["rotation"].size() >= 3)
                    hz.eulerDeg = {z["rotation"][0], z["rotation"][1], z["rotation"][2]};
                p.zones.push_back(std::move(hz));
            }
        std::cout << "[Registry] Hitbox: " << p.profileId
                  << " (" << p.zones.size() << " zone)\n";
        m_hitboxProfiles[p.profileId] = std::move(p);
    }
}

// Carica alleati da data/allies/ — stesso parser dei nemici, team=1 (A7)
void DefinitionRegistry::loadAllies(const std::string& dir)
{
    fs::path folder = dir + "/allies";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        EnemyDef e = parseUnitDef(*j, entry.path(), 1);
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
        p.id          = entry.path().stem().string();   // ADR-001 (KI #21)
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
    m_vehicles.clear();
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
    loadVehicles(dataRoot);

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
GETTER(m_vehicles,       VehicleDef,       getVehicle)
GETTER(m_hitboxProfiles, HitboxProfile,    getHitboxProfile)
GETTER(m_playerDefs,     PlayerDef,        getPlayerDef)
#undef GETTER

} // namespace mini
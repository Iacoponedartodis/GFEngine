#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/ai/WorldIntel.hpp"   // grafo dei link tattici (ADR-032)
#include <nlohmann/json.hpp>
#include <chrono>
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
// ── Campi fantasma (24_ContentValidation, ADR-018) ───────────────────────
// Registra le chiavi presenti nel JSON che NESSUN loader legge. È il modo in cui
// un refuso degrada in silenzio: "fire_rat": 4.5 non fallisce — l'arma prende il
// default e il sintomo appare lontano dalla causa.
//
// LIMITE, da conoscere: cattura le chiavi che il loader IGNORA, non i campi che
// il loader legge ma nessun sistema consuma (min_range, fov_deg, hearing_range —
// la lista di KI #25). Quelli sono un problema di CODICE, non di dati: nessun gate
// che guardi il registry può vederli. Vedi la nota in 24_ContentValidation.
static void noteUnknownKeys(const json& j, const std::string& owner,
                            std::initializer_list<const char*> known,
                            std::unordered_map<std::string, std::vector<std::string>>& out)
{
    if (!j.is_object()) return;
    for (auto it = j.begin(); it != j.end(); ++it)
    {
        bool found = false;
        for (const char* k : known)
            if (it.key() == k) { found = true; break; }
        if (!found) out[owner].push_back(it.key());
    }
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
        noteUnknownKeys(*j, "abilities/" + a.id + ".json",
            {"name","type","param1","param2","param3","cooldown","passive",
             "description"}, m_unknownKeys);
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
        w.adsFov             = getf(*j, "ads_fov", 35.0f);
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
        // Posa in mano (KI #49): la scala/rotazione/offset dell'arma quando è
        // impugnata. Vive sull'ARMA, non sull'entità, così vale per chiunque.
        w.handScale = getf(*j, "hand_scale", 0.0f);   // 0 = non autorata → fallback
        if ((*j).contains("hand_rot") && (*j)["hand_rot"].size() >= 3)
            w.handRot = {(*j)["hand_rot"][0].get<float>(), (*j)["hand_rot"][1].get<float>(),
                         (*j)["hand_rot"][2].get<float>()};
        if ((*j).contains("hand_offset") && (*j)["hand_offset"].size() >= 3)
            w.handOffset = {(*j)["hand_offset"][0].get<float>(), (*j)["hand_offset"][1].get<float>(),
                            (*j)["hand_offset"][2].get<float>()};
        noteUnknownKeys(*j, "weapons/" + w.id + ".json",
            {"name","faction","damage","fire_rate","bullet_speed","bullet_lifetime",
             "bullet_scale","bullet_color","heat_per_shot","cooldown_rate",
             "overheat_penalty","effective_range","min_range","ads_fov","spread_base",
             "spread_ads","spread_move","spread_sprint","spread_jump","mesh",
             "projectile_mesh","mesh_scale","mesh_rot_x","mesh_rot_y",
             "attach_points","hand_scale","hand_rot","hand_offset",
             "description"}, m_unknownKeys);
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
        a.huntTimeout      = getf(*j, "hunt_timeout", 20.0f);
        a.jumpEnabled      = getb(*j, "jump_enabled", true);
        noteUnknownKeys(*j, "ai/" + a.id + ".json",
            {"role","sight_range","fov_deg","hearing_range","reaction_time",
             "aggression","accuracy","cover_preference","retreat_hp_threshold",
             "peek_duration_min","peek_duration_max","hide_duration_min",
             "hide_duration_max","reposition_chance","flank_chance","shoot_interval",
             "patrol_speed","seek_speed","hunt_timeout","jump_enabled","name","description"},
            m_unknownKeys);
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
    e.classId         = gets(j, "class");   // ADR-022: la classe fornisce loadout+comportamento
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

// Chiavi lette da parseUnitDef: tenerle QUI accanto al parser è ciò che impedisce
// alla lista di divergere dal codice che legge davvero (ADR-018).
static const std::initializer_list<const char*> kUnitKeys = {
    "name","faction","team","color","texture","mesh","mesh_scale","mesh_rot_x",
    "mesh_rot_y","attach_points","offset","rot","stats","weapon","weapons",
    "weapon_display","abilities","ai_profile","hitbox_profile","bullet_color",
    "class","description"
};

void DefinitionRegistry::loadEnemies(const std::string& dir)
{
    fs::path folder = dir + "/enemies";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        EnemyDef e = parseUnitDef(*j, entry.path(), 2);
        noteUnknownKeys(*j, "enemies/" + e.id + ".json", kUnitKeys, m_unknownKeys);
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
        // Multi-spawn (opzionale): array di punti [x,y,z] per distribuire le unità AI.
        for (const char* key : {"spawn_points_team1", "spawn_points_team2"})
        {
            if (!(*j).contains(key) || !(*j)[key].is_array()) continue;
            auto& out = (std::string(key).back() == '1') ? m.spawnPointsTeam1
                                                          : m.spawnPointsTeam2;
            for (auto& pt : (*j)[key])
                if (pt.is_array() && pt.size() >= 3)
                    out.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
        }
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

        // Bersagli strategici distruttibili (doc 25, DestroyTarget)
        if ((*j).contains("strategic_targets") && (*j)["strategic_targets"].is_array())
        {
            for (auto& st : (*j)["strategic_targets"])
            {
                StrategicTargetDef t;
                t.label     = gets(st, "label", "Bersaglio");
                t.x         = getf(st, "x", 0.0f);
                t.z         = getf(st, "z", 0.0f);
                t.y         = getf(st, "y", 0.0f);   // altezza sopra il suolo (0 = a terra)
                t.ry        = getf(st, "ry", 0.0f);
                t.hp        = getf(st, "hp", 300.0f);
                t.team      = geti(st, "team", 2);
                if (t.team != 1 && t.team != 2) t.team = 2;
                // Ruolo (doc 34): whitelist. Un valore ignoto degraderebbe in
                // silenzio a torre non-funzionante → si riporta a "generic".
                // Il valore si conserva GREZZO: un refuso normalizzato qui in
                // silenzio farebbe credere all'autore di aver messo una torre
                // che non esiste. Il runtime tratta comunque ogni valore ignoto
                // come "generic" (confronta ==), e il gate ADR-018 lo segnala.
                t.role      = gets(st, "role", "generic");
                // Valore tattico (doc 35). engage_radius 0 = mai ingaggiata di
                // iniziativa: è il default, il sistema resta inerte finché non
                // lo si autora.
                const float pr = getf(st, "priority", 0.5f);
                t.priority     = pr < 0.0f ? 0.0f : (pr > 1.0f ? 1.0f : pr);
                t.engageRadius = getf(st, "engage_radius", 0.0f);
                if (t.engageRadius < 0.0f) t.engageRadius = 0.0f;
                t.meshPath  = gets(st, "mesh");
                t.meshScale = getf(st, "mesh_scale", 1.0f);
                t.halfX     = getf(st, "half_x", 0.0f);
                t.halfY     = getf(st, "half_y", 0.0f);
                t.halfZ     = getf(st, "half_z", 0.0f);
                if (st.contains("color") && st["color"].size() >= 3)
                    t.color = {st["color"][0], st["color"][1], st["color"][2]};
                m.strategicTargets.push_back(t);
            }
        }
        // ── Posizioni tattiche (ADR-030) ───────────────────────────────
        // Chiave nuova `tactical_positions` + MIGRAZIONE TRASPARENTE delle due
        // legacy (`cover_points`, `tactical_points`): le mappe non ancora
        // convertite continuano a funzionare senza toccarle. L'editor, salvando,
        // scrive la chiave nuova e cancella le legacy.
        auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
        // importance NON è [0,1] ma un PESO tattico relativo (≥0, senza tetto): più
        // alto = più importante. L'autore lo grada (es. 0.5–6) per creare peso
        // tattico; i consumatori lo usano linearmente. Prima un clamp01 lo schiacciava
        // a 1.0 azzerando la gradazione (KI #81). Solo floor a 0 (no valori negativi).
        auto nonneg = [](float v) { return v < 0.0f ? 0.0f : v; };
        auto readPos = [&](const nlohmann::json& p, const char* roleKey,
                           const char* defRole) {
            TacticalPositionDef t;
            t.x          = getf(p, "x", 0.0f);
            t.y          = getf(p, "y", 0.0f);
            t.z          = getf(p, "z", 0.0f);
            t.facingDeg  = getf(p, "facing_deg", 0.0f);
            t.role       = gets(p, roleKey);
            if (t.role.empty()) t.role = defRole;
            t.height     = getf(p, "height", 1.0f);
            t.protection = clamp01(getf(p, "protection", 0.5f));
            t.canShoot   = p.contains("can_shoot") ? (bool)p["can_shoot"] : true;
            t.importance = nonneg(getf(p, "importance", 0.5f));   // peso tattico, non [0,1] (KI #81)
            t.radius     = getf(p, "radius", 4.0f);
            // Settore di tiro (ADR-031): default ampio → le posizioni già
            // autorate restano utilizzabili senza ri-autorarle.
            t.fireArcDeg = getf(p, "fire_arc_deg", 120.0f);
            if (t.fireArcDeg < 5.0f)   t.fireArcDeg = 5.0f;
            if (t.fireArcDeg > 360.0f) t.fireArcDeg = 360.0f;
            t.fireRange  = getf(p, "fire_range", 25.0f);
            if (t.fireRange < 1.0f) t.fireRange = 1.0f;
            m.tacticalPositions.push_back(t);
        };

        if ((*j).contains("tactical_positions") && (*j)["tactical_positions"].is_array())
            for (auto& p : (*j)["tactical_positions"]) readPos(p, "role", "cover");
        // Legacy: una copertura è una posizione con role "cover".
        if ((*j).contains("cover_points") && (*j)["cover_points"].is_array())
            for (auto& p : (*j)["cover_points"]) readPos(p, "role", "cover");
        // Legacy: il vecchio `type` diventa `role`; non riparavano (protection 0)
        // salvo che il dato lo dica.
        if ((*j).contains("tactical_points") && (*j)["tactical_points"].is_array())
            for (auto& p : (*j)["tactical_points"])
            {
                readPos(p, "type", "vantage");
                if (!p.contains("protection")) m.tacticalPositions.back().protection = 0.0f;
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

        // Comandante strategico (ADR-024, doc 32) — uno per mappa, opzionale.
        if ((*j).contains("commander") && (*j)["commander"].is_object())
        {
            auto& c = (*j)["commander"];
            m.commander.unit = gets(c, "unit");
            m.commander.x    = getf(c, "x", 0.0f);
            m.commander.z    = getf(c, "z", 0.0f);
            m.commander.leashRadius = getf(c, "leash_radius", 0.0f);
            if (m.commander.leashRadius < 0.0f) m.commander.leashRadius = 0.0f;
        }

        // LIMITE VOLUTO: solo le chiavi di primo livello. Le sotto-strutture
        // (geometry, command_posts, cover_points, patrol_routes, danger_zones,
        // vehicle_spawns) hanno ognuna il proprio set e non sono ancora coperte:
        // un refuso DENTRO un box di geometry passa tuttora liscio. Vedi 08 KI #40.
        noteUnknownKeys(*j, "maps/" + m.id + ".json",
            {"name","mesh","metadata","max_tickets","enemy_count","ally_count",
             "spawn_team1","spawn_team2","spawn_points_team1","spawn_points_team2",
             "enemy_types","ally_types","geometry",
             "command_posts","strategic_targets","cover_points","patrol_routes",
             "danger_zones","vehicle_spawns","tactical_positions","tactical_points",
             "sectors","commander","description"}, m_unknownKeys);

        // Settori / Combat Areas (ADR-034): autorati, opzionali.
        if ((*j).contains("sectors") && (*j)["sectors"].is_array())
        {
            for (auto& s : (*j)["sectors"])
            {
                SectorDef sec;
                sec.label      = gets(s, "label");
                if (sec.label.empty()) sec.label = "Settore";
                sec.x          = getf(s, "x", 0.0f);
                sec.z          = getf(s, "z", 0.0f);
                sec.radius     = getf(s, "radius", 12.0f);
                if (sec.radius < 1.0f) sec.radius = 1.0f;
                sec.importance = nonneg(getf(s, "importance", 0.5f));   // peso tattico, non [0,1] (KI #81)
                m.sectors.push_back(sec);
            }
        }

        // Grafo "chi copre chi" (ADR-032): derivato dalle posizioni autorate +
        // geometria. Si calcola QUI, una volta al load, così a runtime le AI lo
        // leggono e basta — è la scelta "meccaniche pesanti precalcolate nel mondo".
        {
            const auto t0 = std::chrono::steady_clock::now();
            worldintel::buildTacticalLinks(m);
            const auto ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - t0).count();
            size_t links = 0;
            for (const auto& v : m.positionCovers) links += v.size();
            std::cout << "[Registry]   link tattici: " << links << " su "
                      << m.tacticalPositions.size() << " posizioni ("
                      << ms << " ms)\n";
        }

        std::cout << "[Registry] Map: " << m.id
                  << " (geometry: " << m.geometry.size() << " box, "
                  << m.commandPosts.size() << " command post, "
                  << m.patrolRoutes.size() << " route, "
                  << m.dangerZones.size() << " danger, "
                  << m.tacticalPositions.size() << " posizioni tattiche)\n";
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
        noteUnknownKeys(*j, "vehicles/" + v.id + ".json",
            {"name","hp","max_speed","accel","turn_rate_deg","mesh","mesh_scale",
             "mesh_rot_x","mesh_rot_y","mesh_offset_y","half_x","half_y","half_z",
             "hit_offset_y","hit_half_x","hit_half_y","hit_half_z","color",
             "description"}, m_unknownKeys);
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
                // Prima del move: dopo std::move(hz) il nome non e' piu' leggibile.
                noteUnknownKeys(z, "hitboxes/" + p.profileId + ".json (zona '"
                                   + hz.name + "')",
                    {"name","damage_multiplier","debug_visible","offset",
                     "half_extents","bone","rotation"}, m_unknownKeys);
                p.zones.push_back(std::move(hz));
            }
        noteUnknownKeys(*j, "hitboxes/" + p.profileId + ".json",
            {"zones"}, m_unknownKeys);
        std::cout << "[Registry] Hitbox: " << p.profileId
                  << " (" << p.zones.size() << " zone)\n";
        m_hitboxProfiles[p.profileId] = std::move(p);
    }

    // Profilo SINTETICO per i bersagli strategici (DestroyTarget): una struttura
    // è grande, il fallback sferico (0.7 m) la renderebbe quasi impossibile da
    // colpire. Box ~3×4×3 m attorno alla base. Owned dal registry come gli altri.
    // Nome con "__" per non collidere con un file autorato.
    if (!m_hitboxProfiles.count("__strategic_target"))
    {
        HitboxProfile st;
        st.profileId = "__strategic_target";
        HitZone z;
        z.name        = "struttura";
        // Cubo unitario: il test hitbox scala offset/extents per lo scale
        // dell'entità e aggiunge lo STESSO meshOffsetY del rendering. Con offset 0
        // ed extents 0.5 la hitbox coincide ESATTAMENTE col cubo di fallback
        // (visibile == colpibile). Prima era un box gigante a +4 → il "cubo
        // volante con hitbox sfasata" del playtest.
        z.offset      = {0.0f, 0.0f, 0.0f};
        z.halfExtents = {0.5f, 0.5f, 0.5f};
        z.damageMultiplier = 1.0f;
        z.debugVisible = false;
        st.zones.push_back(z);
        m_hitboxProfiles["__strategic_target"] = std::move(st);
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
        noteUnknownKeys(*j, "allies/" + e.id + ".json", kUnitKeys, m_unknownKeys);
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
            p.sprintMult  = getf(s, "sprint_mult",   1.65f);   // = comportamento storico
            p.armorRating = getf(s, "armor_rating",   1.0f);
        }
        noteUnknownKeys(*j, "characters/" + p.id + ".json",
            {"name","description","stats"}, m_unknownKeys);
        std::cout << "[Registry] PlayerDef: " << p.id << "\n";
        m_playerDefs[p.id] = std::move(p);
    }
}

// ── Obiettivi e missioni (ADR-019, doc 25) ───────────────────────────────
namespace {

ObjectiveType parseObjectiveType(const std::string& s)
{
    if (s == "reach_area")             return ObjectiveType::ReachArea;
    if (s == "eliminate_target")       return ObjectiveType::EliminateTarget;
    if (s == "hold_area_for_duration") return ObjectiveType::HoldAreaForDuration;
    if (s == "capture_zone")           return ObjectiveType::CaptureZone;
    if (s == "defend_zone")            return ObjectiveType::DefendZone;
    if (s == "destroy_target")         return ObjectiveType::DestroyTarget;
    if (s == "escort_entity")          return ObjectiveType::EscortEntity;
    if (s == "survive_wave")           return ObjectiveType::SurviveWave;
    if (s == "interact_hack")          return ObjectiveType::InteractHack;
    return ObjectiveType::ReachArea;
}

ObjectiveTier parseTier(const std::string& s)
{
    if (s == "primary")   return ObjectiveTier::Primary;
    if (s == "strategic") return ObjectiveTier::Strategic;
    return ObjectiveTier::Tactical;
}

// Un tipo sconosciuto NON diventa un default silenzioso: resta None e il gate
// ADR-018 lo respinge — un refuso qui produrrebbe un obiettivo che sembra avere
// una conseguenza e invece non ne ha nessuna.
ConsequenceType parseConsequenceType(const std::string& s)
{
    if (s == "block_enemy_reinforcements") return ConsequenceType::BlockEnemyReinforcements;
    if (s == "enemy_accuracy")             return ConsequenceType::EnemyAccuracy;
    if (s == "ally_reinforcements")        return ConsequenceType::AllyReinforcements;
    if (s == "unlock_spawn")               return ConsequenceType::UnlockSpawn;
    return ConsequenceType::None;
}

std::vector<ConsequenceDef> parseConsequences(const json& j, const char* key)
{
    std::vector<ConsequenceDef> out;
    if (!j.contains(key) || !j[key].is_array()) return out;
    for (const auto& e : j[key])
    {
        if (!e.is_object()) continue;
        ConsequenceDef c;
        c.type   = parseConsequenceType(gets(e, "type"));
        c.value  = getf(e, "value", 0.0f);
        c.target = gets(e, "target");
        out.push_back(std::move(c));
    }
    return out;
}

ActivationType parseActivation(const std::string& s)
{
    if (s == "after_objective") return ActivationType::AfterObjective;
    if (s == "after_time")      return ActivationType::AfterTime;
    return ActivationType::Immediate;
}

// Ritorna false se la stringa non è una regola nota: un typo in un JSON non
// deve diventare silenziosamente "AllPrimaryComplete" (doc 24: i dati sbagliati
// vanno respinti, non interpretati).
bool parseMissionRule(const std::string& s, MissionRule& out)
{
    if (s == "all_primary_complete") { out = MissionRule::AllPrimaryComplete; return true; }
    if (s == "any_primary_complete") { out = MissionRule::AnyPrimaryComplete; return true; }
    if (s == "any_primary_failed")   { out = MissionRule::AnyPrimaryFailed;   return true; }
    if (s == "time_limit")           { out = MissionRule::TimeLimit;          return true; }
    return false;
}

} // namespace

void DefinitionRegistry::loadCommanders(const std::string& dir)
{
    fs::path folder = dir + "/commanders";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        CommanderDef c;
        c.id                = entry.path().stem().string();   // ADR-001
        c.name              = gets(*j, "name", c.id);
        c.baseEntity        = gets(*j, "base_entity");
        c.selfDefenseWeapon = gets(*j, "self_defense_weapon");
        c.aiProfile         = gets(*j, "ai_profile");
        c.abilities         = getStrArray(*j, "abilities");
        c.hp                = getf(*j, "hp", 120.0f);
        c.speedMult         = getf(*j, "speed_mult", 0.9f);
        c.meshScale         = getf(*j, "mesh_scale", 1.0f);
        c.colorMult         = getColor(*j, "color_mult", {1.0f, 1.0f, 1.0f});
        c.team              = geti(*j, "team", 2);
        if (c.team != 1 && c.team != 2) c.team = 2;
        noteUnknownKeys(*j, "commanders/" + c.id + ".json",
            {"name","base_entity","self_defense_weapon","ai_profile","abilities",
             "hp","speed_mult","mesh_scale","color_mult","team","description"}, m_unknownKeys);
        std::cout << "[Registry] Commander: " << c.id << "\n";
        m_commanders[c.id] = std::move(c);
    }
}

void DefinitionRegistry::loadClasses(const std::string& dir)
{
    fs::path folder = dir + "/classes";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        ClassDef c;
        c.id                = entry.path().stem().string();   // ADR-001
        c.name              = gets(*j, "name", c.id);
        c.primaryWeaponId   = gets(*j, "primary_weapon");
        c.secondaryWeaponId = gets(*j, "secondary_weapon");
        c.aiProfileId       = gets(*j, "ai_profile");   // ADR-022: il comportamento
        c.role              = gets(*j, "role");
        c.abilityIds        = getStrArray(*j, "abilities");
        c.baseEntityId      = gets(*j, "base_entity");        // ADR-023: il corpo
        c.hpMult            = getf(*j, "hp_mult",     1.0f);
        c.speedMult         = getf(*j, "speed_mult",  1.0f);
        c.damageMult        = getf(*j, "damage_mult", 1.0f);
        c.colorMult         = getColor(*j, "color_mult", {1.0f, 1.0f, 1.0f});
        noteUnknownKeys(*j, "classes/" + c.id + ".json",
            {"name","primary_weapon","secondary_weapon","abilities","role",
             "ai_profile","base_entity","hp_mult","speed_mult","damage_mult",
             "color_mult","description"}, m_unknownKeys);
        std::cout << "[Registry] Class: " << c.id << "\n";
        m_classes[c.id] = std::move(c);
    }
}

void DefinitionRegistry::loadObjectives(const std::string& dir)
{
    fs::path folder = dir + "/objectives";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        ObjectiveDef o;
        o.id   = entry.path().stem().string();   // ADR-001: mai dal JSON
        o.name = gets(*j, "name", o.id);
        o.type = parseObjectiveType(gets(*j, "type", "reach_area"));
        o.tier = parseTier(gets(*j, "tier", "tactical"));
        if ((*j).contains("target"))
        {
            auto& t = (*j)["target"];
            o.x = getf(t, "x", 0.0f); o.y = getf(t, "y", 0.0f); o.z = getf(t, "z", 0.0f);
            o.radius      = getf(t, "radius", 5.0f);
            o.actorTeam   = geti(t, "actor_team",  1);
            o.targetTeam  = geti(t, "target_team", 2);
            o.count       = geti(t, "count", 1);
            o.holdSeconds = getf(t, "hold_seconds", 10.0f);
            o.targetPost  = gets(t, "post");   // CaptureZone/DefendZone (ADR-009)
            o.targetStructure = gets(t, "structure");   // DestroyTarget (doc 25)
        }
        if ((*j).contains("activation"))
        {
            auto& a = (*j)["activation"];
            o.activation          = parseActivation(gets(a, "type", "immediate"));
            o.activationObjective = gets(a, "objective");
            o.activationTime      = getf(a, "time", 0.0f);
        }
        o.onSuccess = parseConsequences(*j, "on_success");   // doc 25
        o.onFailure = parseConsequences(*j, "on_failure");
        o.timeLimit = getf(*j, "time_limit", 0.0f);
        o.reward    = geti(*j, "reward", 0);
        if ((*j).contains("linked_objectives"))
            for (auto& l : (*j)["linked_objectives"])
                if (l.is_string()) o.linkedObjectives.push_back(l.get<std::string>());
        noteUnknownKeys(*j, "objectives/" + o.id + ".json",
            {"name","type","tier","target","activation","time_limit","reward",
             "linked_objectives","description","on_success","on_failure"}, m_unknownKeys);
        std::cout << "[Registry] Objective: " << o.id << "\n";
        m_objectives[o.id] = std::move(o);
    }
}

void DefinitionRegistry::loadMissions(const std::string& dir)
{
    fs::path folder = dir + "/missions";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        auto j = readJson(entry.path()); if (!j) continue;
        MissionDef m;
        m.id       = entry.path().stem().string();   // ADR-001
        m.name     = gets(*j, "name", m.id);
        m.briefing = gets(*j, "briefing");
        m.mapId    = gets(*j, "map");
        m.modeId   = gets(*j, "mode", "conquest");
        if ((*j).contains("primary_objectives"))
            for (auto& o : (*j)["primary_objectives"])
                if (o.is_string()) m.primaryObjectives.push_back(o.get<std::string>());
        if ((*j).contains("optional_objectives"))
            for (auto& o : (*j)["optional_objectives"])
                if (o.is_string()) m.optionalObjectives.push_back(o.get<std::string>());
        // Regole obbligatorie: si registra se erano PRESENTI e VALIDE — il gate
        // a runtime rifiuta la missione, qui non si inventano default.
        if ((*j).contains("success_rules"))
            m.hasSuccessRule = parseMissionRule(gets((*j)["success_rules"], "rule"),
                                                m.successRule);
        if ((*j).contains("failure_rules"))
        {
            auto& f = (*j)["failure_rules"];
            m.hasFailureRule   = parseMissionRule(gets(f, "rule"), m.failureRule);
            m.failureTimeLimit = getf(f, "time_limit", 0.0f);
        noteUnknownKeys(*j, "missions/" + m.id + ".json",
            {"name","briefing","map","mode","primary_objectives","optional_objectives",
             "success_rules","failure_rules","reward_profile","persistence_policy"},
            m_unknownKeys);
        }
        std::cout << "[Registry] Mission: " << m.id << "\n";
        m_missions[m.id] = std::move(m);
    }
}

void DefinitionRegistry::loadAll(const std::string& dataRoot)
{
    m_classes.clear();
    m_objectives.clear();
    m_missions.clear();
    m_unknownKeys.clear();
    m_abilities.clear();
    m_weapons.clear();
    m_aiProfiles.clear();
    m_enemies.clear();
    m_allies.clear();
    m_maps.clear();
    m_hitboxProfiles.clear();
    m_playerDefs.clear();
    m_vehicles.clear();
    m_commanders.clear();
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
    loadClasses(dataRoot);
    loadCommanders(dataRoot);   // ADR-044: dopo classi/entità (riusa baseEntity)
    loadObjectives(dataRoot);
    loadMissions(dataRoot);

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
GETTER(m_classes,        ClassDef,         getClass)
GETTER(m_commanders,     CommanderDef,     getCommander)
GETTER(m_objectives,     ObjectiveDef,     getObjective)
GETTER(m_missions,       MissionDef,       getMission)
#undef GETTER

} // namespace mini
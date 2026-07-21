// MapEditor.cpp
// Modulo GFEditor per la modifica visuale della geometria delle mappe.
// Layout: [lista box | viewport 3D | pannello proprietà]
// Salva/carica da data/maps/<id>.json, campo "geometry".

#include "util/DataPath.hpp"
#include "modules/MapEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/UiWidgets.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
// Esposizione mostrata al designer (ADR-033): si riusa la STESSA funzione del
// runtime invece di duplicarne la regola nell'editor.
#include "mini/game/data/Definitions.hpp"
#include "mini/game/ai/WorldIntel.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor
{

// ── Helpers ───────────────────────────────────────────────────────────────────
// R8 chiuso: unica risoluzione in util/DataPath. Questo modulo era una delle
// quattro copie col controllo DEBOLE (solo "la cartella esiste"): ora usa quello
// forte (`data/weapons`) come tutti gli altri.
static std::string getDataDir() { return editor::datapath::root(); }

// ── Ctor ─────────────────────────────────────────────────────────────────────
MapEditor::MapEditor()
{
    // Id veicoli per il combo degli spawn (id = filename stem, ADR-001)
    {
        std::error_code ec;
        fs::path folder = fs::path(getDataDir()) / "vehicles";
        if (fs::exists(folder, ec))
            for (auto& entry : fs::directory_iterator(folder, ec))
                if (entry.path().extension() == ".json")
                    m_vehicleIds.push_back(entry.path().stem().string());
        std::sort(m_vehicleIds.begin(), m_vehicleIds.end());
    }
    // Classi per il combo del comandante (id = filename stem, ADR-001/041)
    {
        std::error_code ec;
        fs::path folder = fs::path(getDataDir()) / "classes";
        if (fs::exists(folder, ec))
            for (auto& entry : fs::directory_iterator(folder, ec))
                if (entry.path().extension() == ".json")
                    m_classIds.push_back(entry.path().stem().string());
        std::sort(m_classIds.begin(), m_classIds.end());
    }

    loadMaps();
    if (!m_mapList.empty())
        loadMap(m_mapList[0].id);
}

// ── tick ─────────────────────────────────────────────────────────────────────
void MapEditor::tick(float dt)
{
    m_viewport.tick(dt);

    // Gizmo Sposta: applica lo spostamento all'elemento selezionato.
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.x += delta.x; b.y += delta.y; b.z += delta.z;
        }
        else if (m_selBox == -2)
        { m_spawnTeam1[0]+=delta.x; m_spawnTeam1[1]+=delta.y; m_spawnTeam1[2]+=delta.z; }
        else if (m_selBox == -3)
        { m_spawnTeam2[0]+=delta.x; m_spawnTeam2[1]+=delta.y; m_spawnTeam2[2]+=delta.z; }
        else if (m_selBox <= -10 && m_selBox > -100
                 && (-10 - m_selBox) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - m_selBox];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (m_selBox <= -200 && m_selBox > -300
                 && (-200 - m_selBox) < (int)m_dangers.size())
        {
            auto& d = m_dangers[-200 - m_selBox];
            d.x += delta.x; d.y += delta.y; d.z += delta.z;
        }
        else if (m_selBox <= -300 && m_selBox > -400
                 && (-300 - m_selBox) < (int)m_routes.size())
        {
            auto& r = m_routes[-300 - m_selBox];
            if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
            {
                r.points[m_selRoutePt][0] += delta.x;
                r.points[m_selRoutePt][1] += delta.y;
                r.points[m_selRoutePt][2] += delta.z;
            }
        }
        else if (m_selBox <= -400 && m_selBox > -500
                 && (-400 - m_selBox) < (int)m_vehSpawns.size())
        {
            auto& v = m_vehSpawns[-400 - m_selBox];
            v.x += delta.x; v.z += delta.z;
        }
        else if (m_selBox <= -500 && m_selBox > -1000
                 && (-500 - m_selBox) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - m_selBox];
            t.x += delta.x; t.z += delta.z;
        }
        else if (m_selBox <= -2000 && (-2000 - m_selBox) < (int)m_sectors.size())   // ADR-034
        {
            auto& s = m_sectors[-2000 - m_selBox];
            s.x += delta.x; s.z += delta.z;
        }
        else if (m_selBox <= -1000 && m_selBox > -2000
                 && (-1000 - m_selBox) < (int)m_positions.size())   // ADR-030
        {
            auto& p = m_positions[-1000 - m_selBox];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (m_selBox == kSelCommander && m_commander.exists)   // ADR-041
        {
            m_commander.x += delta.x; m_commander.z += delta.z;
        }
        m_dirty = true;
        updateViewport();
    }

    // Gizmo Ruota (solo asse Y): box mappa (ry), cover point (facing), veicolo (ry).
    // ADR-025: i marker metadata con un campo di orientamento sono ruotabili.
    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta))
    {
        auto wrap = [](float a) { while (a > 180.0f) a -= 360.0f; while (a < -180.0f) a += 360.0f; return a; };
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.ry = wrap(b.ry + rotDelta.y);
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -400 && m_selBox > -500
                 && (-400 - m_selBox) < (int)m_vehSpawns.size())
        {
            auto& v = m_vehSpawns[-400 - m_selBox];
            v.ry = wrap(v.ry + rotDelta.y);
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -500 && m_selBox > -1000
                 && (-500 - m_selBox) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - m_selBox];
            t.ry = wrap(t.ry + rotDelta.y);
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -1000 && m_selBox > -2000
                 && (-1000 - m_selBox) < (int)m_positions.size())   // ADR-030
        {
            auto& p = m_positions[-1000 - m_selBox];
            p.facing = wrap(p.facing + rotDelta.y);
            m_dirty = true; updateViewport();
        }
    }

    // Gizmo Scala: box (dimensioni per asse), post/danger (raggio uniforme).
    // ADR-025: i marker metadata con un raggio sono scalabili (delta.x → radius).
    glm::vec3 scaleDelta;
    if (m_viewport.popGizmoScaleDelta(scaleDelta))
    {
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.sx += scaleDelta.x; if (b.sx < 0.1f) b.sx = 0.1f;
            b.sy += scaleDelta.y; if (b.sy < 0.1f) b.sy = 0.1f;
            b.sz += scaleDelta.z; if (b.sz < 0.1f) b.sz = 0.1f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -10 && m_selBox > -100
                 && (-10 - m_selBox) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - m_selBox];
            p.radius += scaleDelta.x; if (p.radius < 0.5f) p.radius = 0.5f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -200 && m_selBox > -300
                 && (-200 - m_selBox) < (int)m_dangers.size())
        {
            auto& d = m_dangers[-200 - m_selBox];
            d.radius += scaleDelta.x; if (d.radius < 0.5f) d.radius = 0.5f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -2000 && (-2000 - m_selBox) < (int)m_sectors.size())   // ADR-034
        {
            auto& s = m_sectors[-2000 - m_selBox];
            s.radius += scaleDelta.x; if (s.radius < 2.0f) s.radius = 2.0f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -500 && m_selBox > -1000
                 && (-500 - m_selBox) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - m_selBox];
            t.scale += scaleDelta.x * 0.4f; if (t.scale < 0.2f) t.scale = 0.2f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox == kSelCommander && m_commander.exists)   // ADR-041: raggio leash
        {
            m_commander.leashRadius += scaleDelta.x;
            if (m_commander.leashRadius < 0.0f) m_commander.leashRadius = 0.0f;
            m_dirty = true; updateViewport();
        }
    }
}

// ── snap ─────────────────────────────────────────────────────────────────────
float MapEditor::snap(float v) const
{
    if (m_gridSnap <= 0.0f) return v;
    return std::round(v / m_gridSnap) * m_gridSnap;
}

// ── loadMaps ─────────────────────────────────────────────────────────────────
void MapEditor::loadMaps()
{
    m_mapList.clear();
    fs::path folder = getDataDir() + "/maps";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        json j;
        try { f >> j; } catch (...) { continue; }
        MapEntry me;
        me.id   = j.value("id",   entry.path().stem().string());
        me.path = entry.path().string();
        m_mapList.push_back(me);
    }
}

// ── loadMap ──────────────────────────────────────────────────────────────────
void MapEditor::loadMap(const std::string& id)
{
    auto it = std::find_if(m_mapList.begin(), m_mapList.end(),
                           [&](const MapEntry& e){ return e.id == id; });
    if (it == m_mapList.end()) return;

    std::ifstream f(it->path);
    if (!f) return;
    json j;
    try { f >> j; } catch (...) { return; }

    m_mapId      = id;
    m_mapJsonPath = it->path;
    m_boxes.clear();
    m_posts.clear();
    m_positions.clear();
    m_sectors.clear();
    m_dangers.clear();
    m_routes.clear();
    m_vehSpawns.clear();
    m_targets.clear();
    m_selRoutePt = 0;
    m_selBox = -1;

    if (j.contains("spawn_team1") && j["spawn_team1"].size() >= 3)
        m_spawnTeam1 = {j["spawn_team1"][0], j["spawn_team1"][1], j["spawn_team1"][2]};
    if (j.contains("spawn_team2") && j["spawn_team2"].size() >= 3)
        m_spawnTeam2 = {j["spawn_team2"][0], j["spawn_team2"][1], j["spawn_team2"][2]};

    // Comandante strategico (ADR-024/041): uno per mappa, campo `commander`.
    m_commander = CommanderEntry{};
    if (j.contains("commander") && j["commander"].is_object())
    {
        auto& c = j["commander"];
        m_commander.exists      = true;
        m_commander.unit        = c.value("unit", std::string());
        m_commander.x           = c.value("x", 0.0f);
        m_commander.z           = c.value("z", 0.0f);
        m_commander.leashRadius = c.value("leash_radius", 0.0f);
    }

    if (j.contains("geometry") && j["geometry"].is_array())
    {
        for (auto& gb : j["geometry"])
        {
            BoxEntry b;
            b.x  = gb.value("x",  0.f);
            b.y  = gb.value("y",  0.f);
            b.z  = gb.value("z",  0.f);
            b.ry = gb.value("ry", 0.f);
            b.sx = gb.value("sx", 2.f);
            b.sy = gb.value("sy", 2.f);
            b.sz = gb.value("sz", 2.f);
            b.r  = gb.value("r",  0.35f);
            b.g  = gb.value("g",  0.32f);
            b.b  = gb.value("b",  0.28f);
            b.isCollider = gb.value("collider", true);

            std::string type  = gb.value("type",  std::string("wall"));
            std::string label = gb.value("label", std::string(""));
            std::strncpy(b.type,  type.c_str(),  sizeof(b.type)  - 1);
            std::strncpy(b.label, label.c_str(), sizeof(b.label) - 1);

            m_boxes.push_back(b);
        }
    }

    if (j.contains("command_posts") && j["command_posts"].is_array())
    {
        for (auto& cp : j["command_posts"])
        {
            PostEntry p;
            std::string lbl = cp.value("label", std::string("Post"));
            std::strncpy(p.label, lbl.c_str(), sizeof(p.label) - 1);
            p.x           = cp.value("x", 0.f);
            p.y           = cp.value("y", 0.f);
            p.z           = cp.value("z", 0.f);
            p.radius      = cp.value("radius", 4.f);
            p.team        = cp.value("team", 0);
            p.captureTime = cp.value("capture_time", 8.f);
            m_posts.push_back(p);
        }
    }

    // Bersagli strategici (doc 25, DestroyTarget)
    if (j.contains("strategic_targets") && j["strategic_targets"].is_array())
    {
        for (auto& st : j["strategic_targets"])
        {
            TargetEntry t;
            std::string lbl = st.value("label", std::string("Bersaglio"));
            std::strncpy(t.label, lbl.c_str(), sizeof(t.label) - 1);
            t.x  = st.value("x", 0.f);
            t.z  = st.value("z", 0.f);
            t.hp = st.value("hp", 300.f);
            t.ry    = st.value("ry", 0.f);
            t.team  = st.value("team", 2);
            t.scale = st.value("mesh_scale", 1.f);
            t.halfX = st.value("half_x", 0.f);
            t.halfY = st.value("half_y", 0.f);
            t.halfZ = st.value("half_z", 0.f);
            // Ruolo (doc 34): "comms" = torre di comunicazione della sua fazione.
            const std::string rl = st.value("role", std::string("generic"));
            t.role = (rl == "comms") ? 1 : (rl == "control") ? 2 : 0;
            t.priority     = st.value("priority", 0.5f);       // doc 35
            t.engageRadius = st.value("engage_radius", 0.0f);
            m_targets.push_back(t);
        }
    }

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    // Posizioni tattiche (ADR-030): chiave nuova + MIGRAZIONE delle due legacy.
    // Salvando si riscrive solo `tactical_positions` e le legacy spariscono.
    auto readPos = [&](const nlohmann::json& p, const char* roleKey,
                       const char* defRole, float defProtection)
    {
        PositionEntry e;
        e.x          = p.value("x", 0.f);
        e.y          = p.value("y", 0.5f);
        e.z          = p.value("z", 0.f);
        e.facing     = p.value("facing_deg", 0.f);
        e.role       = p.value(roleKey, std::string(defRole));
        if (e.role.empty()) e.role = defRole;
        e.height     = p.value("height", 1.f);
        e.protection = p.value("protection", defProtection);
        e.canShoot   = p.value("can_shoot", true);
        e.importance = p.value("importance", 0.5f);
        e.radius     = p.value("radius", 4.f);
        e.fireArc    = p.value("fire_arc_deg", 120.f);   // ADR-031
        e.fireRange  = p.value("fire_range", 25.f);
        m_positions.push_back(e);
    };
    if (j.contains("tactical_positions") && j["tactical_positions"].is_array())
        for (auto& p : j["tactical_positions"]) readPos(p, "role", "cover", 0.5f);
    if (j.contains("cover_points") && j["cover_points"].is_array())
        for (auto& p : j["cover_points"]) readPos(p, "role", "cover", 0.5f);
    if (j.contains("tactical_points") && j["tactical_points"].is_array())
        for (auto& p : j["tactical_points"]) readPos(p, "type", "vantage", 0.0f);

    if (j.contains("sectors") && j["sectors"].is_array())   // ADR-034
        for (auto& s : j["sectors"])
        {
            SectorEntry e;
            e.label      = s.value("label", std::string("Settore"));
            e.x          = s.value("x", 0.f);
            e.z          = s.value("z", 0.f);
            e.radius     = s.value("radius", 12.f);
            e.importance = s.value("importance", 0.5f);
            m_sectors.push_back(e);
        }
    if (j.contains("danger_zones") && j["danger_zones"].is_array())
    {
        for (auto& dz : j["danger_zones"])
        {
            DangerEntry d;
            d.x      = dz.value("x", 0.f);
            d.y      = dz.value("y", 0.f);
            d.z      = dz.value("z", 0.f);
            d.radius = dz.value("radius", 4.f);
            d.level  = dz.value("danger_level", 0.5f);
            m_dangers.push_back(d);
        }
    }
    if (j.contains("patrol_routes") && j["patrol_routes"].is_array())
    {
        for (auto& pr : j["patrol_routes"])
        {
            RouteEntry r;
            std::string rid = pr.value("id", std::string("route"));
            std::strncpy(r.id, rid.c_str(), sizeof(r.id) - 1);
            if (pr.contains("points") && pr["points"].is_array())
                for (auto& pt : pr["points"])
                    if (pt.is_array() && pt.size() >= 3)
                        r.points.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
            m_routes.push_back(std::move(r));
        }
    }

    if (j.contains("vehicle_spawns") && j["vehicle_spawns"].is_array())
    {
        for (auto& vs : j["vehicle_spawns"])
        {
            VehicleSpawnEntry v;
            v.vehicleId = vs.value("vehicle_id", std::string(""));
            v.x  = vs.value("x", 0.f);
            v.z  = vs.value("z", 0.f);
            v.ry = vs.value("ry", 0.f);
            m_vehSpawns.push_back(std::move(v));
        }
    }

    m_dirty = false;
    updateViewport();
}

// ── saveMap ───────────────────────────────────────────────────────────────────
bool MapEditor::saveMap()
{
    if (m_mapJsonPath.empty()) return false;

    // saveJsonRMW (ADR-010): unico canale di scrittura JSON dell'editor.
    return editor::jsonsave::saveJsonRMW(m_mapJsonPath, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["spawn_team1"] = {m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]};
    j["spawn_team2"] = {m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]};

    json geom = json::array();
    for (const auto& b : m_boxes)
    {
        json gb;
        gb["x"]        = b.x;  gb["y"]  = b.y;  gb["z"]  = b.z;
        gb["ry"]       = b.ry;
        gb["sx"]       = b.sx; gb["sy"] = b.sy; gb["sz"] = b.sz;
        gb["r"]        = b.r;  gb["g"]  = b.g;  gb["b"]  = b.b;
        gb["type"]     = b.type;
        gb["label"]    = b.label;
        gb["collider"] = b.isCollider;
        geom.push_back(gb);
    }
    j["geometry"] = geom;

    json postsArr = json::array();
    for (const auto& p : m_posts)
    {
        json cp;
        cp["label"]        = p.label;
        cp["x"] = p.x;  cp["y"] = p.y;  cp["z"] = p.z;
        cp["radius"]       = p.radius;
        cp["team"]         = p.team;
        cp["capture_time"] = p.captureTime;
        postsArr.push_back(cp);
    }
    j["command_posts"] = postsArr;

    // Comandante strategico (ADR-024/041): uno per mappa. Se non è autorato si
    // RIMUOVE il campo (RMW: non lasciare un residuo che spawnerebbe comunque).
    if (m_commander.exists && !m_commander.unit.empty())
    {
        json c;
        c["unit"] = m_commander.unit;
        c["x"] = m_commander.x;  c["z"] = m_commander.z;
        c["leash_radius"] = m_commander.leashRadius;
        j["commander"] = c;
    }
    else j.erase("commander");

    // Bersagli strategici (doc 25, DestroyTarget)
    json targetsArr = json::array();
    for (const auto& t : m_targets)
    {
        json st;
        st["label"] = t.label;
        st["x"] = t.x;  st["z"] = t.z;
        st["hp"] = t.hp;
        st["ry"]         = t.ry;
        st["team"]       = t.team;
        st["mesh_scale"] = t.scale;
        st["half_x"] = t.halfX;  st["half_y"] = t.halfY;  st["half_z"] = t.halfZ;
        st["role"]   = (t.role == 1) ? "comms"            // doc 34
                     : (t.role == 2) ? "control"          // doc 36
                                     : "generic";
        st["priority"]      = t.priority;                 // doc 35
        st["engage_radius"] = t.engageRadius;
        targetsArr.push_back(st);
    }
    j["strategic_targets"] = targetsArr;

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    // Posizioni tattiche unificate (ADR-030). Si scrive SOLO la chiave nuova e si
    // cancellano le legacy: aprire+salvare una mappa la migra definitivamente.
    json posArr = json::array();
    for (const auto& p : m_positions)
    {
        json o;
        o["x"] = p.x;  o["y"] = p.y;  o["z"] = p.z;
        o["facing_deg"] = p.facing;
        o["role"]       = p.role;
        o["height"]     = p.height;
        o["protection"] = p.protection;
        o["can_shoot"]  = p.canShoot;
        o["importance"] = p.importance;
        o["radius"]     = p.radius;
        o["fire_arc_deg"] = p.fireArc;     // ADR-031
        o["fire_range"]   = p.fireRange;
        posArr.push_back(o);
    }
    j["tactical_positions"] = posArr;
    j.erase("cover_points");
    j.erase("tactical_points");

    json secArr = json::array();   // ADR-034
    for (const auto& s : m_sectors)
    {
        json o;
        o["label"]      = s.label;
        o["x"] = s.x;  o["z"] = s.z;
        o["radius"]     = s.radius;
        o["importance"] = s.importance;
        secArr.push_back(o);
    }
    j["sectors"] = secArr;

    json dangerArr = json::array();
    for (const auto& d : m_dangers)
    {
        json dz;
        dz["x"] = d.x;  dz["y"] = d.y;  dz["z"] = d.z;
        dz["radius"]       = d.radius;
        dz["danger_level"] = d.level;
        dangerArr.push_back(dz);
    }
    j["danger_zones"] = dangerArr;

    json routeArr = json::array();
    for (const auto& r : m_routes)
    {
        json pr;
        pr["id"] = r.id;
        json pts = json::array();
        for (const auto& pt : r.points)
            pts.push_back({pt[0], pt[1], pt[2]});
        pr["points"] = pts;
        routeArr.push_back(pr);
    }
    j["patrol_routes"] = routeArr;

    json vehArr = json::array();
    for (const auto& v : m_vehSpawns)
    {
        json vs;
        vs["vehicle_id"] = v.vehicleId;
        vs["x"] = v.x;  vs["z"] = v.z;  vs["ry"] = v.ry;
        vehArr.push_back(vs);
    }
    j["vehicle_spawns"] = vehArr;

    m_dirty = false;
    return true;
    });
}

// ── addBox ────────────────────────────────────────────────────────────────────
void MapEditor::addBox()
{
    BoxEntry b;
    // Posiziona al centro della scena
    b.x = 0; b.y = 1.0f; b.z = 0;
    std::strncpy(b.type, "wall", sizeof(b.type) - 1);
    std::strncpy(b.label, "Nuovo Box", sizeof(b.label) - 1);
    m_boxes.push_back(b);
    m_selBox = (int)m_boxes.size() - 1;
    m_dirty  = true;
    updateViewport();
}

// ── duplicateBox ─────────────────────────────────────────────────────────────
void MapEditor::duplicateBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    BoxEntry b = m_boxes[idx];
    b.x += 1.0f;
    m_boxes.insert(m_boxes.begin() + idx + 1, b);
    m_selBox = idx + 1;
    m_dirty  = true;
    updateViewport();
}

// ── deleteBox ─────────────────────────────────────────────────────────────────
void MapEditor::deleteBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    m_boxes.erase(m_boxes.begin() + idx);
    m_selBox = std::min(m_selBox, (int)m_boxes.size() - 1);
    m_dirty  = true;
    updateViewport();
}

// ── recomputeExposure (ADR-033) ───────────────────────────────────────────────
// Costruisce un MapDef temporaneo dai dati dell'editor e chiama la STESSA funzione
// del runtime: la regola dell'esposizione esiste in un posto solo. Si ricalcola a
// ogni modifica (updateViewport), non a ogni frame.
void MapEditor::recomputeExposure()
{
    mini::MapDef tmp;
    tmp.geometry.reserve(m_boxes.size());
    for (const auto& b : m_boxes)
    {
        mini::MapGeometryBox g;
        g.x = b.x; g.y = b.y; g.z = b.z; g.ry = b.ry;
        g.sx = b.sx; g.sy = b.sy; g.sz = b.sz;
        g.collider = b.isCollider;
        tmp.geometry.push_back(g);
    }
    tmp.tacticalPositions.reserve(m_positions.size());
    for (const auto& p : m_positions)
    {
        mini::TacticalPositionDef t;
        t.x = p.x; t.y = p.y; t.z = p.z;
        t.facingDeg = p.facing; t.role = p.role;
        t.height = p.height; t.protection = p.protection; t.canShoot = p.canShoot;
        t.importance = p.importance; t.radius = p.radius;
        t.fireArcDeg = p.fireArc; t.fireRange = p.fireRange;
        tmp.tacticalPositions.push_back(t);
    }
    mini::worldintel::buildTacticalLinks(tmp);
    m_exposure = tmp.positionExposure;
}

// ── updateViewport ────────────────────────────────────────────────────────────
void MapEditor::updateViewport()
{
    recomputeExposure();   // ADR-033: tenuta in pari con le posizioni
    std::vector<FreeCameraViewport::MapBoxDraw> draws;
    draws.reserve(m_boxes.size() + 2);

    // Tipi "floor" visualizzati diversamente se showNavmesh
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        FreeCameraViewport::MapBoxDraw d;
        d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
        d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
        d.selected = (i == m_selBox);
        d.pickId = i;

        bool isFloor = (std::string(b.type) == "floor");

        if (m_showNavmesh && isFloor) {
            // Overlay verde per area navigabile
            d.r = 0.10f; d.g = 0.90f; d.b = 0.30f;
        } else {
            d.r = b.r; d.g = b.g; d.b = b.b;
        }

        draws.push_back(d);
    }

    // Spawn team1 (blu) come croce
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam1[0]; s.y = m_spawnTeam1[1]; s.z = m_spawnTeam1[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 0.20f; s.g = 0.50f; s.b = 1.00f;
        s.selected = (m_selBox == -2);
        s.pickId = -2;
        draws.push_back(s);
    }
    // Spawn team2 (rosso)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam2[0]; s.y = m_spawnTeam2[1]; s.z = m_spawnTeam2[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 1.00f; s.g = 0.20f; s.b = 0.20f;
        s.selected = (m_selBox == -3);
        s.pickId = -3;
        draws.push_back(s);
    }

    // Command post: palo alto + area di cattura, colorati per team
    for (int i = 0; i < (int)m_posts.size(); ++i)
    {
        const auto& p = m_posts[i];
        float r = 0.75f, g = 0.75f, b = 0.75f;
        if (p.team == 1) { r = 0.25f; g = 0.50f; b = 1.00f; }
        if (p.team == 2) { r = 1.00f; g = 0.25f; b = 0.25f; }
        const bool sel = (m_selBox == -10 - i);

        FreeCameraViewport::MapBoxDraw pole;
        pole.x = p.x; pole.y = p.y + 1.5f; pole.z = p.z; pole.ry = 0;
        pole.sx = 0.3f; pole.sy = 3.0f; pole.sz = 0.3f;
        pole.r = r; pole.g = g; pole.b = b;
        pole.selected = sel;
        pole.pickId = -10 - i;
        draws.push_back(pole);

        FreeCameraViewport::MapBoxDraw area;
        area.x = p.x; area.y = p.y + 0.05f; area.z = p.z; area.ry = 0;
        area.sx = p.radius * 2.0f; area.sy = 0.05f; area.sz = p.radius * 2.0f;
        area.r = r * 0.6f; area.g = g * 0.6f; area.b = b * 0.6f;
        area.selected = sel;
        area.pickId = -10 - i;
        draws.push_back(area);
    }

    // Comandante strategico (ADR-041): palo viola alto + disco del raggio di
    // leash (l'area da cui non esce). Se leash 0 il disco non si disegna (fermo).
    if (m_commander.exists)
    {
        const bool sel = (m_selBox == kSelCommander);
        FreeCameraViewport::MapBoxDraw pole;
        pole.x = m_commander.x; pole.y = 1.6f; pole.z = m_commander.z; pole.ry = 0;
        pole.sx = 0.5f; pole.sy = 3.2f; pole.sz = 0.5f;
        pole.r = 0.65f; pole.g = 0.25f; pole.b = 0.85f;   // viola = comando
        pole.selected = sel;
        pole.pickId = kSelCommander;
        draws.push_back(pole);

        if (m_commander.leashRadius > 0.01f)
        {
            FreeCameraViewport::MapBoxDraw area;
            area.x = m_commander.x; area.y = 0.06f; area.z = m_commander.z; area.ry = 0;
            area.sx = m_commander.leashRadius * 2.0f; area.sy = 0.05f;
            area.sz = m_commander.leashRadius * 2.0f;
            area.r = 0.40f; area.g = 0.16f; area.b = 0.55f;
            area.selected = sel;
            area.pickId = kSelCommander;
            draws.push_back(area);
        }
    }

    // Bersagli strategici: box arancione, con la STESSA scala e rotazione che
    // avranno in gioco. Prima erano disegnati con `ry = 0` e lato fisso 2.5:
    // ruotare o scalare cambiava il dato ma non si vedeva nulla, quindi
    // sembravano "non funzionare" (segnalato dall'utente).
    for (int i = 0; i < (int)m_targets.size(); ++i)
    {
        const auto& t = m_targets[i];
        const float sc = 2.5f * ((t.scale > 0.0001f) ? t.scale : 1.0f);
        FreeCameraViewport::MapBoxDraw s;
        s.x = t.x; s.y = sc * 0.5f; s.z = t.z; s.ry = t.ry;
        s.sx = sc; s.sy = sc; s.sz = sc;
        s.r = 0.85f; s.g = 0.55f; s.b = 0.15f;
        s.selected = (m_selBox == -500 - i);
        s.pickId = -500 - i;
        draws.push_back(s);
    }

    // ── Posizioni tattiche (ADR-030) ─────────────────────────────────────
    // Un solo marker per tutte: colore dal RUOLO, altezza dalla copertura.
    // Chi ripara (protection > 0) è una lastra alta `height` (si vede cosa
    // copre); chi non ripara è un pilastro sottile. Naso = fronte; disco = raggio
    // d'influenza (solo per i ruoli d'area). pickId = -1000 - i.
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        const auto& p = m_positions[i];
        const bool sel = (m_selBox == -1000 - i);
        float r = 0.15f, g = 0.85f, b = 0.70f;                       // cover (verde-acqua)
        if      (p.role == "vantage")     { r = 0.2f;  g = 0.8f;  b = 0.9f;  }
        else if (p.role == "defensive")   { r = 0.9f;  g = 0.4f;  b = 0.2f;  }
        else if (p.role == "chokepoint")  { r = 0.7f;  g = 0.3f;  b = 0.9f;  }
        else if (p.role == "observation") { r = 0.9f;  g = 0.85f; b = 0.2f;  }

        const bool shields = (p.protection > 0.0f);
        const float h = shields ? p.height : 1.2f;

        FreeCameraViewport::MapBoxDraw body;
        body.x = p.x; body.y = p.y + h * 0.5f; body.z = p.z; body.ry = p.facing;
        body.sx = shields ? 0.9f : 0.4f; body.sy = h; body.sz = shields ? 0.25f : 0.4f;
        body.r = r; body.g = g; body.b = b;
        body.selected = sel; body.pickId = -1000 - i;
        draws.push_back(body);

        const float fr = glm::radians(p.facing);
        FreeCameraViewport::MapBoxDraw nose;
        nose.x = p.x + std::sin(fr) * 0.5f; nose.y = p.y + h * 0.5f;
        nose.z = p.z + std::cos(fr) * 0.5f;
        nose.ry = p.facing; nose.sx = 0.2f; nose.sy = 0.2f; nose.sz = 0.5f;
        nose.r = r * 0.6f; nose.g = g * 0.6f; nose.b = b * 0.6f;
        nose.selected = sel; nose.pickId = -1000 - i;
        draws.push_back(nose);

        if (p.role == "defensive" || p.role == "chokepoint")
        {
            FreeCameraViewport::MapBoxDraw disc;
            disc.x = p.x; disc.y = p.y - 0.4f; disc.z = p.z; disc.ry = 0;
            disc.sx = p.radius * 2.0f; disc.sy = 0.03f; disc.sz = p.radius * 2.0f;
            disc.r = r; disc.g = g; disc.b = b;
            disc.selected = sel; disc.pickId = -1000 - i;
            draws.push_back(disc);
        }

        // Settore di tiro (ADR-031): i due bordi dell'arco, come raggi lunghi
        // quanto la gittata. SOLO sulla posizione selezionata — con 60 posizioni
        // disegnarli tutti renderebbe il viewport illeggibile. Senza vederlo il
        // settore non è autorabile con cura, ed è il dato più delicato.
        if (sel && p.canShoot)
        {
            const float halfArc = p.fireArc * 0.5f;
            const float len = p.fireRange;
            for (int s = 0; s < 2; ++s)
            {
                const float a = glm::radians(p.facing + (s == 0 ? -halfArc : halfArc));
                FreeCameraViewport::MapBoxDraw ray;
                ray.x = p.x + std::sin(a) * len * 0.5f;   // centro del raggio
                ray.y = p.y + 0.15f;
                ray.z = p.z + std::cos(a) * len * 0.5f;
                ray.ry = p.facing + (s == 0 ? -halfArc : halfArc);
                ray.sx = 0.10f; ray.sy = 0.06f; ray.sz = len;
                ray.r = 1.0f; ray.g = 0.85f; ray.b = 0.25f;   // giallo: linea di tiro
                ray.selected = false;                          // non ri-evidenziare
                ray.pickId = -1000 - i;
                draws.push_back(ray);
            }
        }
    }

    // Settore (ADR-034): disco ampio e tenue, colore per importanza. È l'area su
    // cui ragiona il comandante. pickId = -2000 - i.
    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        const auto& s = m_sectors[i];
        const bool sel = (m_selBox == -2000 - i);
        FreeCameraViewport::MapBoxDraw area;
        area.x = s.x; area.y = 0.02f; area.z = s.z; area.ry = 0;
        area.sx = s.radius * 2.0f; area.sy = 0.02f; area.sz = s.radius * 2.0f;
        area.r = 0.35f + s.importance * 0.5f;   // più importante = più acceso
        area.g = 0.30f; area.b = 0.75f;
        area.selected = sel; area.pickId = -2000 - i;
        draws.push_back(area);
    }

    // Danger zone: disco arancione (più rosso quanto più pericoloso)
    for (int i = 0; i < (int)m_dangers.size(); ++i)
    {
        const auto& d = m_dangers[i];
        const bool sel = (m_selBox == -200 - i);
        FreeCameraViewport::MapBoxDraw area;
        area.x = d.x; area.y = d.y + 0.03f; area.z = d.z; area.ry = 0;
        area.sx = d.radius * 2.0f; area.sy = 0.04f; area.sz = d.radius * 2.0f;
        area.r = 0.9f; area.g = 0.55f - d.level * 0.45f; area.b = 0.10f;
        area.selected = sel;
        area.pickId = -200 - i;
        draws.push_back(area);
    }

    // Spawn veicoli: box arancio a misura di speeder + freccia direzione
    for (int i = 0; i < (int)m_vehSpawns.size(); ++i)
    {
        const auto& v = m_vehSpawns[i];
        const bool sel = (m_selBox == -400 - i);

        FreeCameraViewport::MapBoxDraw body;
        body.x = v.x; body.y = 0.6f; body.z = v.z; body.ry = v.ry;
        body.sx = 1.0f; body.sy = 1.0f; body.sz = 2.6f;
        body.r = 0.95f; body.g = 0.60f; body.b = 0.15f;
        body.selected = sel;
        body.pickId = -400 - i;
        draws.push_back(body);

        const float vr = glm::radians(v.ry);
        FreeCameraViewport::MapBoxDraw nose;
        nose.x = v.x + std::sin(vr) * 1.6f;
        nose.y = 0.6f;
        nose.z = v.z + std::cos(vr) * 1.6f;
        nose.ry = v.ry;
        nose.sx = 0.3f; nose.sy = 0.3f; nose.sz = 0.6f;
        nose.r = 0.8f; nose.g = 0.45f; nose.b = 0.1f;
        nose.selected = sel;
        nose.pickId = -400 - i;
        draws.push_back(nose);
    }

    // Patrol route: pilastrino viola per punto (quello attivo più alto)
    for (int ri = 0; ri < (int)m_routes.size(); ++ri)
    {
        const auto& r = m_routes[ri];
        const bool routeSel = (m_selBox == -300 - ri);
        for (int pi = 0; pi < (int)r.points.size(); ++pi)
        {
            const bool activePt = routeSel && (pi == m_selRoutePt);
            FreeCameraViewport::MapBoxDraw wp;
            wp.x = r.points[pi][0];
            wp.y = r.points[pi][1] + (activePt ? 0.9f : 0.5f);
            wp.z = r.points[pi][2];
            wp.ry = 0;
            wp.sx = 0.3f; wp.sy = activePt ? 1.8f : 1.0f; wp.sz = 0.3f;
            wp.r = 0.65f; wp.g = 0.35f; wp.b = 0.95f;
            wp.selected = activePt;
            wp.pickId = -300 - ri;
            draws.push_back(wp);
        }
    }

    m_viewport.setMapBoxes(draws);

    // Gizmo sull'elemento selezionato (box o spawn point).
    // I box mappa ruotano solo attorno a Y; gli spawn: solo Sposta.
    if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
    {
        const auto& b = m_boxes[m_selBox];
        m_viewport.setGizmoTarget({b.x, b.y, b.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);
        m_viewport.setGizmoCanRotateScale(true, true);
    }
    else if (m_selBox == -2)
    {
        m_viewport.setGizmoTarget({m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox == -3)
    {
        m_viewport.setGizmoTarget({m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox == kSelCommander && m_commander.exists)   // ADR-041
    {
        m_viewport.setGizmoTarget({m_commander.x, 1.6f, m_commander.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio di leash
    }
    else if (m_selBox <= -10 && m_selBox > -100
             && (-10 - m_selBox) < (int)m_posts.size())
    {
        const auto& p = m_posts[-10 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y, p.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio (ADR-025)
    }
    else if (m_selBox <= -200 && m_selBox > -300
             && (-200 - m_selBox) < (int)m_dangers.size())
    {
        const auto& d = m_dangers[-200 - m_selBox];
        m_viewport.setGizmoTarget({d.x, d.y, d.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio (ADR-025)
    }
    else if (m_selBox <= -300 && m_selBox > -400
             && (-300 - m_selBox) < (int)m_routes.size())
    {
        const auto& r = m_routes[-300 - m_selBox];
        if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            const auto& pt = r.points[m_selRoutePt];
            m_viewport.setGizmoTarget({pt[0], pt[1], pt[2]}, true);
            m_viewport.setGizmoCanRotateScale(false, false);
        }
        else
            m_viewport.setGizmoTarget({0,0,0}, false);
    }
    else if (m_selBox <= -400 && m_selBox > -500
             && (-400 - m_selBox) < (int)m_vehSpawns.size())
    {
        const auto& v = m_vehSpawns[-400 - m_selBox];
        m_viewport.setGizmoTarget({v.x, 0.6f, v.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // orientamento veicolo (ry)
        m_viewport.setGizmoCanRotateScale(true, false);   // ruota → ry (ADR-025)
    }
    else if (m_selBox <= -500 && m_selBox > -1000
             && (-500 - m_selBox) < (int)m_targets.size())
    {
        const auto& t = m_targets[-500 - m_selBox];
        m_viewport.setGizmoTarget({t.x, 1.25f, t.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // orientamento struttura
        m_viewport.setGizmoCanRotateScale(true, true);    // ruota → ry, scala → scale
    }
    else if (m_selBox <= -1000 && m_selBox > -2000
             && (-1000 - m_selBox) < (int)m_positions.size())   // ADR-030
    {
        const auto& p = m_positions[-1000 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y, p.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // fronte della posizione
        m_viewport.setGizmoCanRotateScale(true, false);
    }
    else if (m_selBox <= -2000 && (-2000 - m_selBox) < (int)m_sectors.size())   // ADR-034
    {
        const auto& s = m_sectors[-2000 - m_selBox];
        m_viewport.setGizmoTarget({s.x, 0.5f, s.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio del settore
    }
    else
        m_viewport.setGizmoTarget({0,0,0}, false);
}

// ── draw ─────────────────────────────────────────────────────────────────────
void MapEditor::draw()
{
    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;

    drawToolbar();

    float toolbarH = ImGui::GetItemRectSize().y + ImGui::GetStyle().ItemSpacing.y;
    float remaining = totalH - toolbarH - 4.0f;

    // Pannelli ridimensionabili: lista e proprietà con grip sul bordo destro;
    // il viewport prende lo spazio residuo.
    static float s_propW = 260.0f;

    ImGui::BeginChild("##map_panels", ImVec2(totalW, remaining), ImGuiChildFlags_None);

    // ── Lista box ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_list", ImVec2(200, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawBoxList(ImGui::GetContentRegionAvail().x, remaining);
    ImGui::EndChild();
    const float listW = ImGui::GetItemRectSize().x;

    ImGui::SameLine();

    float vpW = totalW - listW - s_propW - ImGui::GetStyle().ItemSpacing.x * 2;
    if (vpW < 120.0f) vpW = 120.0f;

    // ── Viewport 3D ──────────────────────────────────────────────────────
    ImGui::BeginChild("##map_vp", ImVec2(vpW, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(vpW, remaining);
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Proprietà ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_props", ImVec2(s_propW, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawProperties(ImGui::GetContentRegionAvail().x, remaining);
    ImGui::EndChild();
    s_propW = ImGui::GetItemRectSize().x;

    ImGui::EndChild();
}

// ── drawToolbar ───────────────────────────────────────────────────────────────
void MapEditor::drawToolbar()
{
    // Selettore mappa
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("##mapsel", m_mapId.empty() ? "-- nessuna --" : m_mapId.c_str()))
    {
        for (auto& me : m_mapList)
        {
            bool sel = (me.id == m_mapId);
            if (ImGui::Selectable(me.id.c_str(), sel))
                loadMap(me.id);
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mappa corrente");

    ImGui::SameLine();
    if (m_dirty) ImGui::TextColored({1.0f,0.7f,0.2f,1.0f}, "*");
    else         ImGui::TextDisabled(" ");
    ImGui::SameLine();

    if (ImGui::Button("Salva")) {
        if (saveMap()) ImGui::OpenPopup("##saved_ok");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Salva geometry in JSON");

    ImGui::SameLine();
    if (ImGui::Button("+ Box"))         addBox();
    ImGui::SameLine();
    if (ImGui::Button("Duplica") && m_selBox >= 0) duplicateBox(m_selBox);
    ImGui::SameLine();
    if (ImGui::Button("Elimina") && m_selBox >= 0) {
        ImGui::OpenPopup("##del_confirm");
    }

    // ── Rinomina mappa (ADR-010) ─────────────────────────────────────────
    if (!m_mapId.empty())
    {
        ImGui::SameLine(0, 16);
        static char renameBuf[64] = "";
        static std::string renameErr;
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("##mrename", renameBuf, sizeof(renameBuf));
        ImGui::SameLine();
        if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
        {
            int refs = 0;
            renameErr = editor::rename::renameDefinition(
                getDataDir() + "/", editor::rename::Category::Map,
                m_mapId, renameBuf, &refs);
            if (renameErr.empty())
            {
                std::string newId = renameBuf;
                renameBuf[0] = '\0';
                loadMaps();
                loadMap(newId);
            }
        }
        if (!renameErr.empty())
        { ImGui::SameLine(); ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str()); }
    }

    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 16);

    ImGui::TextUnformatted("Snap:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    const float snapValues[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f};
    const char* snapLabels[] = {"Off","0.25","0.5","1.0","2.0"};
    int snapIdx = 2;
    for (int i = 0; i < 5; ++i) if (m_gridSnap == snapValues[i]) { snapIdx = i; break; }
    if (ImGui::BeginCombo("##snap", snapLabels[snapIdx], ImGuiComboFlags_NoArrowButton))
    {
        for (int i = 0; i < 5; ++i) {
            bool s = (i == snapIdx);
            if (ImGui::Selectable(snapLabels[i], s)) m_gridSnap = snapValues[i];
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 16);
    ImGui::Checkbox("Area navigabile", &m_showNavmesh);
    if (m_showNavmesh) { updateViewport(); } // aggiorna colori floor

    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 16);
    // Modalità gizmo, PER TIPO di selezione. Prima Ruota/Scala erano abilitati
    // solo sui box della geometria (`boxSel, boxSel`): su un bersaglio strategico
    // i pulsanti restavano grigi e la modalità ricadeva su Sposta, quindi ruota e
    // scala risultavano semplicemente assenti (segnalato dall'utente).
    // Ogni tipo dichiara cosa sa fare, coerentemente con i gizmo gestiti in tick().
    bool canRot = false, canSca = false;
    if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
    { canRot = true;  canSca = true; }                       // box: ry + dimensioni
    else if (m_selBox <= -500 && m_selBox > -1000)
    { canRot = true;  canSca = true; }                       // bersaglio: ry + scala
    else if (m_selBox <= -400 && m_selBox > -500)
    { canRot = true;  canSca = false; }                      // spawn veicolo: solo ry
    else if (m_selBox <= -1000 && m_selBox > -2000)
    { canRot = true;  canSca = false; }                      // posizione tattica: facing
    else if (m_selBox <= -2000)
    { canRot = false; canSca = true; }                       // settore: raggio
    else if (m_selBox <= -10 && m_selBox > -100)
    { canRot = false; canSca = true; }                       // command post: raggio
    else if (m_selBox <= -200 && m_selBox > -300)
    { canRot = false; canSca = true; }                       // danger zone: raggio
    else if (m_selBox == kSelCommander)
    { canRot = false; canSca = true; }                       // comandante: raggio leash
    editor::ui::gizmoModeBar(m_viewport, canRot, canSca);

    // Popups
    if (ImGui::BeginPopup("##saved_ok")) {
        ImGui::TextColored({0.4f,1.0f,0.4f,1.0f}, "Salvato!");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("##del_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Eliminare il box selezionato?");
        if (ImGui::Button("Sì")) { deleteBox(m_selBox); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("No")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ── drawBoxList ───────────────────────────────────────────────────────────────
void MapEditor::drawBoxList(float /*panelW*/, float /*panelH*/)
{
    ImGui::TextDisabled("Box (%d)", (int)m_boxes.size());
    ImGui::Separator();

    const char* typeIcons[] = {"[F]","[W]","[P]","[C]","[D]"};
    auto typeIcon = [&](const char* t) -> const char* {
        if (std::strcmp(t,"floor")  == 0) return typeIcons[0];
        if (std::strcmp(t,"wall")   == 0) return typeIcons[1];
        if (std::strcmp(t,"platform")==0) return typeIcons[2];
        if (std::strcmp(t,"cover")  == 0) return typeIcons[3];
        return typeIcons[4];
    };

    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        char buf[128];
        const char* name = (b.label[0] != '\0') ? b.label : "(nessun nome)";
        std::snprintf(buf, sizeof(buf), "%s %s##box%d", typeIcon(b.type), name, i);

        bool sel = (i == m_selBox);
        if (ImGui::Selectable(buf, sel))
        {
            m_selBox = i;
            updateViewport();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("tipo: %s\npos: (%.1f, %.1f, %.1f)\ndim: %.1fx%.1fx%.1f",
                              b.type, b.x, b.y, b.z, b.sx, b.sy, b.sz);
        }
    }

    ImGui::Separator();
    // Spawn points
    ImGui::TextDisabled("Spawn");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    bool spawnT1 = (m_selBox == -2);
    if (ImGui::Selectable("[T1] Spawn Alleati", spawnT1)) { m_selBox = -2; updateViewport(); }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    bool spawnT2 = (m_selBox == -3);
    if (ImGui::Selectable("[T2] Spawn Nemici", spawnT2)) { m_selBox = -3; updateViewport(); }
    ImGui::PopStyleColor();

    // ── Comandante strategico (ADR-041): uno per mappa ───────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Comando");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.45f, 0.95f, 1.0f));
    if (m_commander.exists)
    {
        bool selC = (m_selBox == kSelCommander);
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[CMD] %s##cmd",
                      m_commander.unit.empty() ? "(nessuna classe)"
                                               : m_commander.unit.c_str());
        if (ImGui::Selectable(lbl, selC)) { m_selBox = kSelCommander; updateViewport(); }
    }
    else if (ImGui::SmallButton("+ Comandante (Droide Tattico)"))
    {
        m_commander.exists = true;
        if (!m_classIds.empty()) m_commander.unit = m_classIds.front();
        m_commander.x = m_spawnTeam2[0]; m_commander.z = m_spawnTeam2[2];
        m_commander.leashRadius = 6.0f;
        m_selBox = kSelCommander;
        m_dirty = true; updateViewport();
    }
    ImGui::PopStyleColor();

    // ── Command post ─────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Command Post (%d)", (int)m_posts.size());
    for (int i = 0; i < (int)m_posts.size(); ++i)
    {
        const auto& p = m_posts[i];
        ImVec4 col = (p.team == 1) ? ImVec4(0.4f,0.7f,1.0f,1.0f)
                   : (p.team == 2) ? ImVec4(1.0f,0.4f,0.4f,1.0f)
                                   : ImVec4(0.8f,0.8f,0.8f,1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[CP] %s##cp%d", p.label, i);
        bool sel = (m_selBox == -10 - i);
        if (ImGui::Selectable(lbl, sel)) { m_selBox = -10 - i; updateViewport(); }
        ImGui::PopStyleColor();
    }
    if (ImGui::SmallButton("+ Post"))
    {
        PostEntry p;
        std::snprintf(p.label, sizeof(p.label), "Post %d", (int)m_posts.size() + 1);
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
        m_dirty = true;
        updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Rimuovi##cp") && m_selBox <= -10 && m_selBox > -100)
    {
        int i = -10 - m_selBox;
        if (i >= 0 && i < (int)m_posts.size())
        {
            m_posts.erase(m_posts.begin() + i);
            m_selBox = -1;
            m_dirty = true;
            updateViewport();
        }
    }

    // ── Bersagli strategici (doc 25, DestroyTarget) ──────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Bersagli strategici (%d)", (int)m_targets.size());
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
    for (int i = 0; i < (int)m_targets.size(); ++i)
    {
        char lbl[96];
        // [COM] rende leggibile a colpo d'occhio quali strutture alimentano la
        // rete di comunicazione (doc 34), e di quale fazione.
        std::snprintf(lbl, sizeof(lbl), "%s %s (T%d)##tg%d",
                      m_targets[i].role == 1 ? "[COM]"
                    : m_targets[i].role == 2 ? "[CTRL]" : "[BG]",
                      m_targets[i].label, m_targets[i].team, i);
        bool sel = (m_selBox == -500 - i);
        if (ImGui::Selectable(lbl, sel)) { m_selBox = -500 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Bersaglio"))
    {
        TargetEntry t;
        std::snprintf(t.label, sizeof(t.label), "Bersaglio %d", (int)m_targets.size() + 1);
        m_targets.push_back(t);
        m_selBox = -500 - ((int)m_targets.size() - 1);
        m_dirty = true;
        updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Rimuovi##tg") && m_selBox <= -500)
    {
        int i = -500 - m_selBox;
        if (i >= 0 && i < (int)m_targets.size())
        {
            m_targets.erase(m_targets.begin() + i);
            m_selBox = -1;
            m_dirty = true;
            updateViewport();
        }
    }

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Metadata AI");

    // Posizioni tattiche (ADR-030): una sola lista, il ruolo è nell'etichetta.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.75f, 1.0f));
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        char lbl[72];
        std::snprintf(lbl, sizeof(lbl), "[%s] %d##tpos%d",
                      m_positions[i].role.c_str(), i + 1, i);
        if (ImGui::Selectable(lbl, m_selBox == -1000 - i))
        { m_selBox = -1000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Posizione"))
    {
        m_positions.push_back({});
        m_selBox = -1000 - ((int)m_positions.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##tpos") && m_selBox <= -1000)
    {
        int i = -1000 - m_selBox;
        if (i >= 0 && i < (int)m_positions.size())
        { m_positions.erase(m_positions.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Settori / Combat Areas (ADR-034): il livello su cui ragiona il comandante.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.55f, 0.95f, 1.0f));
    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        char lbl[72];
        std::snprintf(lbl, sizeof(lbl), "[SET] %s##sec%d", m_sectors[i].label.c_str(), i);
        if (ImGui::Selectable(lbl, m_selBox == -2000 - i))
        { m_selBox = -2000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Settore"))
    {
        m_sectors.push_back({});
        m_selBox = -2000 - ((int)m_sectors.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##sec") && m_selBox <= -2000)
    {
        int i = -2000 - m_selBox;
        if (i >= 0 && i < (int)m_sectors.size())
        { m_sectors.erase(m_sectors.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Danger zone
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.2f, 1.0f));
    for (int i = 0; i < (int)m_dangers.size(); ++i)
    {
        char lbl[48];
        std::snprintf(lbl, sizeof(lbl), "[DZ] Pericolo %d##dz%d", i + 1, i);
        if (ImGui::Selectable(lbl, m_selBox == -200 - i))
        { m_selBox = -200 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Pericolo"))
    {
        m_dangers.push_back({});
        m_selBox = -200 - ((int)m_dangers.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##dz") && m_selBox <= -200 && m_selBox > -300)
    {
        int i = -200 - m_selBox;
        if (i >= 0 && i < (int)m_dangers.size())
        { m_dangers.erase(m_dangers.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Patrol route
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.5f, 1.0f, 1.0f));
    for (int i = 0; i < (int)m_routes.size(); ++i)
    {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "[PR] %s (%d pt)##pr%d",
                      m_routes[i].id, (int)m_routes[i].points.size(), i);
        if (ImGui::Selectable(lbl, m_selBox == -300 - i))
        { m_selBox = -300 - i; m_selRoutePt = 0; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Percorso"))
    {
        RouteEntry r;
        std::snprintf(r.id, sizeof(r.id), "route_%d", (int)m_routes.size() + 1);
        r.points.push_back({0.f, 0.5f, 0.f});
        r.points.push_back({4.f, 0.5f, 0.f});
        m_routes.push_back(std::move(r));
        m_selBox = -300 - ((int)m_routes.size() - 1);
        m_selRoutePt = 0;
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##pr") && m_selBox <= -300 && m_selBox > -400)
    {
        int i = -300 - m_selBox;
        if (i >= 0 && i < (int)m_routes.size())
        { m_routes.erase(m_routes.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Spawn veicoli (19_Vehicles Fase B)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.65f, 0.25f, 1.0f));
    for (int i = 0; i < (int)m_vehSpawns.size(); ++i)
    {
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[VS] %s##vs%d",
                      m_vehSpawns[i].vehicleId.empty()
                          ? "(scegli veicolo)" : m_vehSpawns[i].vehicleId.c_str(), i);
        if (ImGui::Selectable(lbl, m_selBox == -400 - i))
        { m_selBox = -400 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Veicolo"))
    {
        VehicleSpawnEntry v;
        if (!m_vehicleIds.empty()) v.vehicleId = m_vehicleIds[0];
        m_vehSpawns.push_back(std::move(v));
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##vs") && m_selBox <= -400)
    {
        int i = -400 - m_selBox;
        if (i >= 0 && i < (int)m_vehSpawns.size())
        { m_vehSpawns.erase(m_vehSpawns.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }
}

// ── drawProperties ────────────────────────────────────────────────────────────
void MapEditor::drawProperties(float panelW, float /*panelH*/)
{
    float sliderW = panelW - 16.0f;

    // ── Comandante strategico (ADR-024/041) ──────────────────────────────
    if (m_selBox == kSelCommander)
    {
        if (!m_commander.exists) { ImGui::TextDisabled("Seleziona un elemento."); return; }
        ImGui::TextColored({0.75f,0.45f,0.95f,1.0f}, "Comandante strategico");
        ImGui::TextDisabled("Uno per mappa. NON e' nel roster: e' un obiettivo vivente\n"
                            "(come le torri). Non combatte, si difende soltanto.");
        ImGui::Separator();
        bool changed = false;

        // Classe dal registry (dropdown, mai testo libero — regola di progetto).
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Classe##cmd",
                              m_commander.unit.empty() ? "-- scegli --"
                                                       : m_commander.unit.c_str()))
        {
            for (const auto& cid : m_classIds)
                if (ImGui::Selectable(cid.c_str(), m_commander.unit == cid))
                { m_commander.unit = cid; changed = true; }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Deve avere un'ability 'command' per dirigere i droidi\n"
                            "(il gate di validazione lo verifica).");

        ImGui::TextDisabled("Posizione (retrovie, al sicuro)");
        changed |= editor::ui::sliderRow("X##cmd", m_commander.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z##cmd", m_commander.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);

        ImGui::TextDisabled("Raggio di movimento (leash)");
        changed |= editor::ui::sliderRow("Raggio (m)##cmd", m_commander.leashRadius,
                                         0.f, 20.f, 0.5f, "%.1f m");
        if (m_commander.leashRadius <= 0.01f)
            ImGui::TextDisabled("0 = FERMO sul posto (si gira e spara soltanto).");
        else
            ImGui::TextDisabled("Area circolare da cui NON esce: si muove al suo\n"
                                "interno per coprirsi, mai fuori. Gizmo Scala = raggio.");

        if (ImGui::SmallButton("Rimuovi comandante"))
        { m_commander = CommanderEntry{}; m_selBox = -1; m_dirty = true; updateViewport(); return; }

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox == -2 || m_selBox == -3)
    {
        // Spawn point
        auto& sp = (m_selBox == -2) ? m_spawnTeam1 : m_spawnTeam2;
        const char* teamName = (m_selBox == -2) ? "Spawn Alleati (T1)" : "Spawn Nemici (T2)";
        ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "%s", teamName);
        ImGui::Separator();
        bool changed = false;
        changed |= editor::ui::sliderRow("X", sp[0], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", sp[1],   0.f,  5.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", sp[2], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Settore selezionato (ADR-034) ────────────────────────────────────
    if (m_selBox <= -2000)
    {
        int si = -2000 - m_selBox;
        if (si < 0 || si >= (int)m_sectors.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& s = m_sectors[si];
        ImGui::TextColored({0.7f,0.55f,0.95f,1.0f}, "Settore %d", si + 1);
        ImGui::Separator();
        bool changed = false;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s", s.label.c_str());
        if (editor::ui::textRow("Nome", buf, sizeof(buf))) { s.label = buf; changed = true; }

        ImGui::TextDisabled("Area");
        changed |= editor::ui::sliderRow("X", s.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", s.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Raggio", s.radius, 2.f, 40.f, 0.5f, "%.1f");

        ImGui::TextDisabled("Valore strategico");
        changed |= editor::ui::sliderRow("Importanza", s.importance, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled("Il Droide Tattico sceglie l'obiettivo fra i settori,\n"
                            "pesando importanza e quanto la zona e' contesa.");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Posizione tattica selezionata (ADR-030) ──────────────────────────
    // Un solo pannello: il RUOLO descrive, i campi dicono cosa la posizione SA
    // fare. Le sezioni si adattano al ruolo per non mostrare campi inutili.
    if (m_selBox <= -1000 && m_selBox > -2000)
    {
        int pi = -1000 - m_selBox;
        if (pi < 0 || pi >= (int)m_positions.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& p = m_positions[pi];
        ImGui::TextColored({0.3f,0.9f,0.75f,1.0f}, "Posizione tattica %d", pi + 1);
        ImGui::Separator();
        bool changed = false;

        static const char* kRoles[] = {"cover", "vantage", "defensive",
                                       "chokepoint", "observation"};
        int roleIdx = 0;
        for (int k = 0; k < 5; ++k) if (p.role == kRoles[k]) { roleIdx = k; break; }
        if (ImGui::Combo("Ruolo", &roleIdx, kRoles, 5)) { p.role = kRoles[roleIdx]; changed = true; }
        ImGui::TextDisabled(roleIdx == 0 ? "Riparo da cui combattere"
                          : roleIdx == 1 ? "Sopraelevato / dominante"
                          : roleIdx == 2 ? "Posizione da tenere"
                          : roleIdx == 3 ? "Strettoia / ingresso da presidiare"
                                         : "Punto d'osservazione");

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", p.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", p.y, -2.f, 10.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", p.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Fronte", p.facing, -180.f, 180.f, 1.f, "%.0f");

        ImGui::TextDisabled("Riparo");
        changed |= editor::ui::sliderRow("Protezione", p.protection, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled(p.protection <= 0.0f  ? "Non ripara: non e' una copertura"
                          : p.protection >= 0.75f ? "Ottima: l'AI la preferisce"
                          : p.protection <= 0.35f ? "Scarsa: ripiego"
                                                  : "Media");
        if (p.protection > 0.0f)
        {
            changed |= editor::ui::sliderRow("Altezza", p.height, 0.4f, 3.0f, 0.05f, "%.2f");
            ImGui::TextDisabled(p.height < 1.2f ? "Bassa: l'AI fara' peek-over"
                                                : "Alta: l'AI fara' peek-around");
        }
        changed |= ImGui::Checkbox("Si puo' fare fuoco da qui", &p.canShoot);

        // Settore di tiro (ADR-031): cosa questa posizione BATTE. È ciò che
        // permette all'AI di venirci per ATTACCARE, non solo per nascondersi.
        if (p.canShoot)
        {
            ImGui::TextDisabled("Settore di tiro (giallo nel viewport)");
            changed |= editor::ui::sliderRow("Ampiezza", p.fireArc, 10.f, 360.f, 5.f, "%.0f");
            changed |= editor::ui::sliderRow("Gittata", p.fireRange, 3.f, 60.f, 1.f, "%.0f");
            ImGui::TextDisabled(p.fireArc <= 60.f  ? "Stretto: posizione specializzata"
                              : p.fireArc >= 240.f ? "Molto ampio: copre quasi ovunque"
                                                   : "Medio");
        }

        // Esposizione (ADR-033): DERIVATA, sola lettura. Non si autora, ma vederla
        // guida l'authoring — un punto molto esposto è una cattiva posizione di tiro.
        if (pi < (int)m_exposure.size())
        {
            const float ex = m_exposure[pi];
            ImGui::TextDisabled("Esposizione (calcolata): %.0f%%", ex * 100.0f);
            ImGui::TextDisabled(ex >= 0.5f ? "Molto allo scoperto: battuta da meta' mappa"
                              : ex <= 0.15f ? "Riparata: pochi angoli di tiro la battono"
                                            : "Media");
        }

        ImGui::TextDisabled("Tattica");
        changed |= editor::ui::sliderRow("Importanza", p.importance, 0.f, 1.f, 0.05f, "%.2f");
        if (p.role == "defensive" || p.role == "chokepoint")
            changed |= editor::ui::sliderRow("Raggio", p.radius, 0.5f, 20.f, 0.1f, "%.2f");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Danger zone selezionata (15_MapMetadata) ─────────────────────────
    if (m_selBox <= -200 && m_selBox > -300)
    {
        int di = -200 - m_selBox;
        if (di < 0 || di >= (int)m_dangers.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& d = m_dangers[di];
        ImGui::TextColored({0.95f,0.55f,0.2f,1.0f}, "Danger Zone %d", di + 1);
        ImGui::Separator();
        bool changed = false;
        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", d.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", d.y, -2.f, 10.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", d.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        ImGui::TextDisabled("Area");
        changed |= editor::ui::sliderRow("Raggio", d.radius, 0.5f, 30.f, 0.1f, "%.1f");
        changed |= editor::ui::sliderRow("Pericolo", d.level, 0.f, 1.f, 0.01f, "%.2f");
        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Spawn veicolo selezionato (19_Vehicles Fase B) ───────────────────
    // ── Bersaglio strategico (DestroyTarget) — PRIMA dei veicoli: -500 <= -400 ──
    if (m_selBox <= -500)
    {
        int ti = -500 - m_selBox;
        if (ti < 0 || ti >= (int)m_targets.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& t = m_targets[ti];
        ImGui::TextColored({0.9f,0.6f,0.2f,1.0f}, "Bersaglio strategico");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Label##tg", t.label, sizeof(t.label))) changed = true;
        ImGui::TextDisabled("La label e' il nome referenziato dall'obiettivo destroy_target.");
        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X##tg", t.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z##tg", t.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Rotazione##tg", t.ry, -180.f, 180.f, 1.f, "%.0f");
        changed |= editor::ui::sliderRow("Scala##tg", t.scale, 0.2f, 8.f, 0.05f, "%.2f");

        ImGui::TextDisabled("Fazione");
        int teamIdx = (t.team == 1) ? 0 : 1;
        static const char* kTeams[] = {"Repubblica (cloni)", "Separatisti (droidi)"};
        if (ImGui::Combo("Team##tg", &teamIdx, kTeams, 2))
        { t.team = (teamIdx == 0) ? 1 : 2; changed = true; }
        ImGui::TextDisabled("Serve per dare a ogni fazione le PROPRIE strutture\n"
                            "(torre comunicazioni/controllo).");

        ImGui::TextDisabled("Ruolo (doc 34/36)");
        static const char* kRoles[] = {"Generico (solo bersaglio)",
                                       "Torre di comunicazione",
                                       "Torre di controllo"};
        if (ImGui::Combo("Ruolo##tg", &t.role, kRoles, 3)) changed = true;
        if (t.role == 1)
            ImGui::TextDisabled("Finche' e' viva la sua fazione comunica bene. Se cade,\n"
                                "informazioni/ordini/rinforzi RALLENTANO - mai bloccati.");
        else if (t.role == 2)
            ImGui::TextDisabled("Da' visione d'insieme alla SUA fazione: SEGNALA i posti\n"
                                "che contano (settori contesi, strutture nemiche).\n"
                                "NON da' ordini e non manda nessuno in un punto preciso:\n"
                                "e' ogni soldato a scegliere quale segnale seguire.");

        ImGui::TextDisabled("Valore tattico per l'AI (doc 35)");
        changed |= editor::ui::sliderRow("Priorita'##tg", t.priority, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled("Quanto la fazione AVVERSARIA vuole distruggerla.\n"
                            "E' il peso con cui il comando la confronta coi settori.");
        changed |= editor::ui::sliderRow("Raggio ingaggio (m)##tg", t.engageRadius,
                                         0.f, 60.f, 1.f, "%.0f m");
        if (t.engageRadius <= 0.0f)
            ImGui::TextDisabled("0 = MAI ingaggiata di iniziativa: resta affare del\n"
                                "giocatore e del comando. E' il default.");
        else if (t.engageRadius < 3.0f)
            ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.0f},
                               "Troppo piccolo: nessuna AI si trovera' mai cosi'\n"
                               "vicino. Indicativamente servono 15-30 m.");
        else
            ImGui::TextDisabled("Un'unita' avversaria entro questo raggio la attacca\n"
                                "di iniziativa, ma SOLO se non ha bersagli-unita':\n"
                                "una struttura non spara, viene sempre dopo.\n"
                                "Riferimento: la mappa firebase e' 50x40 m.");

        ImGui::TextDisabled("Resistenza");
        changed |= editor::ui::sliderRow("HP##tg", t.hp, 10.f, 2000.f, 10.f, "%.0f");

        ImGui::TextDisabled("Collisione (0 = ricavata dalla scala)");
        changed |= editor::ui::sliderRow("Semiasse X##tg", t.halfX, 0.f, 10.f, 0.1f, "%.2f");
        changed |= editor::ui::sliderRow("Semiasse Y##tg", t.halfY, 0.f, 10.f, 0.1f, "%.2f");
        changed |= editor::ui::sliderRow("Semiasse Z##tg", t.halfZ, 0.f, 10.f, 0.1f, "%.2f");
        ImGui::TextDisabled("Struttura statica colpibile e SOLIDA: prima AI e giocatore\n"
                            "ci passavano attraverso (mancava il collider).");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox <= -400)
    {
        int vi = -400 - m_selBox;
        if (vi < 0 || vi >= (int)m_vehSpawns.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& v = m_vehSpawns[vi];
        ImGui::TextColored({0.95f,0.65f,0.25f,1.0f}, "Spawn Veicolo %d", vi + 1);
        ImGui::Separator();
        bool changed = false;

        // Veicolo dal registry (dropdown, mai testo libero)
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Veicolo##vsid",
                              v.vehicleId.empty() ? "-- scegli --"
                                                  : v.vehicleId.c_str()))
        {
            for (const auto& vid : m_vehicleIds)
                if (ImGui::Selectable(vid.c_str(), v.vehicleId == vid))
                { v.vehicleId = vid; changed = true; }
            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", v.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", v.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        ImGui::TextDisabled("Orientamento");
        changed |= editor::ui::sliderRow("Yaw°", v.ry, -180.f, 180.f, 1.f, "%.0f");
        ImGui::TextDisabled("Il runtime decollide lo spawn e appoggia al suolo.");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Patrol route selezionata (15_MapMetadata) ────────────────────────
    if (m_selBox <= -300)
    {
        int ri = -300 - m_selBox;
        if (ri < 0 || ri >= (int)m_routes.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& r = m_routes[ri];
        ImGui::TextColored({0.75f,0.5f,1.0f,1.0f}, "Percorso pattuglia");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome##prid", r.id, sizeof(r.id))) changed = true;

        ImGui::TextDisabled("Punti (%d)", (int)r.points.size());
        for (int pi = 0; pi < (int)r.points.size(); ++pi)
        {
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "Punto %d##rp%d", pi + 1, pi);
            if (ImGui::Selectable(lbl, pi == m_selRoutePt))
            { m_selRoutePt = pi; updateViewport(); }
        }
        if (ImGui::SmallButton("+ Punto"))
        {
            // Nuovo punto accanto all'ultimo (o all'origine)
            std::array<float,3> np = r.points.empty()
                ? std::array<float,3>{0.f, 0.5f, 0.f}
                : std::array<float,3>{r.points.back()[0] + 2.f,
                                      r.points.back()[1],
                                      r.points.back()[2]};
            r.points.push_back(np);
            m_selRoutePt = (int)r.points.size() - 1;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("- Punto") && !r.points.empty()
            && m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            r.points.erase(r.points.begin() + m_selRoutePt);
            m_selRoutePt = std::max(0, m_selRoutePt - 1);
            changed = true;
        }

        if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            auto& pt = r.points[m_selRoutePt];
            ImGui::TextDisabled("Posizione punto %d", m_selRoutePt + 1);
            changed |= editor::ui::sliderRow("X", pt[0], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
            changed |= editor::ui::sliderRow("Y", pt[1], -2.f, 10.f, 0.05f, "%.2f", 18.0f);
            changed |= editor::ui::sliderRow("Z", pt[2], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        }

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Command post selezionato ─────────────────────────────────────────
    if (m_selBox <= -10)
    {
        int pi = -10 - m_selBox;
        if (pi < 0 || pi >= (int)m_posts.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& p = m_posts[pi];
        ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "Command Post");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome##cpl", p.label, sizeof(p.label))) changed = true;

        const char* teams[] = {"Neutrale", "Alleati (T1)", "Nemici (T2)"};
        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::comboRow("Team iniziale##cpt", p.team, teams, 3)) changed = true;

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", p.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", p.y, -2.f, 10.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", p.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);

        ImGui::TextDisabled("Cattura");
        changed |= editor::ui::sliderRow("Raggio", p.radius, 1.f, 20.f, 0.1f, "%.1f");
        changed |= editor::ui::sliderRow("Tempo (s)", p.captureTime, 1.f, 30.f, 0.5f, "%.1f");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox < 0 || m_selBox >= (int)m_boxes.size())
    {
        ImGui::TextDisabled("Seleziona un box.");
        return;
    }

    auto& b = m_boxes[m_selBox];
    ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "Box %d", m_selBox);
    if (b.label[0]) ImGui::SameLine(), ImGui::TextDisabled(" - %s", b.label);
    ImGui::Separator();

    bool changed = false;

    // Etichetta
    ImGui::SetNextItemWidth(sliderW);
    if (editor::ui::textRow("Etichetta", b.label, sizeof(b.label)))
        changed = true;

    // Tipo
    ImGui::SetNextItemWidth(sliderW);
    const char* types[] = {"floor","wall","platform","cover","decoration"};
    int typeIdx = 1;
    for (int i = 0; i < 5; ++i) if (std::strcmp(b.type, types[i]) == 0) { typeIdx = i; break; }
    if (ImGui::BeginCombo("Tipo", types[typeIdx]))
    {
        for (int i = 0; i < 5; ++i) {
            bool s = (i == typeIdx);
            if (ImGui::Selectable(types[i], s)) {
                std::strncpy(b.type, types[i], sizeof(b.type) - 1);
                changed = true;
            }
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Checkbox("Collider", &b.isCollider)) changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Posizione");
    const float posSpeed = m_gridSnap > 0 ? m_gridSnap : 0.1f;
    if (editor::ui::sliderRow("X", b.x, -60.f, 60.f, posSpeed, "%.2f", 18.0f))
        { b.x = snap(b.x); changed = true; }
    if (editor::ui::sliderRow("Y", b.y, -2.f, 10.f, posSpeed * 0.5f, "%.2f", 18.0f))
        { b.y = snap(b.y); changed = true; }
    if (editor::ui::sliderRow("Z", b.z, -60.f, 60.f, posSpeed, "%.2f", 18.0f))
        { b.z = snap(b.z); changed = true; }

    ImGui::TextDisabled("Rotazione");
    if (editor::ui::sliderRow("Y°", b.ry, -180.f, 180.f, 1.0f, "%.1f", 18.0f))
        changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Dimensioni");
    if (editor::ui::sliderRow("W", b.sx, 0.1f, 120.f, posSpeed, "%.2f", 18.0f))
        { b.sx = snap(b.sx); if (b.sx < 0.1f) b.sx = 0.1f; changed = true; }
    if (editor::ui::sliderRow("H", b.sy, 0.1f, 20.f, posSpeed * 0.5f, "%.2f", 18.0f))
        { b.sy = snap(b.sy); if (b.sy < 0.1f) b.sy = 0.1f; changed = true; }
    if (editor::ui::sliderRow("D", b.sz, 0.1f, 120.f, posSpeed, "%.2f", 18.0f))
        { b.sz = snap(b.sz); if (b.sz < 0.1f) b.sz = 0.1f; changed = true; }

    ImGui::Separator();
    ImGui::TextDisabled("Colore");
    float col[3] = {b.r, b.g, b.b};
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::ColorEdit3("##boxcol", col,
                          ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_Float))
    {
        b.r = col[0]; b.g = col[1]; b.b = col[2];
        changed = true;
    }

    if (changed) { m_dirty = true; updateViewport(); }
}

// ── drawViewport ─────────────────────────────────────────────────────────────
void MapEditor::drawViewport(float vpW, float vpH)
{
    m_viewport.draw(false);

    // Selezione dal viewport (ray-picking): un click su un oggetto lo seleziona
    // esattamente come cliccarlo nella lista. Il pickId assegnato in
    // updateViewport È già il codice di m_selBox, quindi basta assegnarlo.
    int picked = 0;
    if (m_viewport.popClickedMapBox(picked))
    {
        m_selBox = picked;
        if (m_selBox <= -300 && m_selBox > -400) m_selRoutePt = 0;   // route: primo punto attivo
        updateViewport();   // rinfresca evidenziazione + bersaglio del gizmo
    }
    (void)vpW; (void)vpH;
}

} // namespace editor

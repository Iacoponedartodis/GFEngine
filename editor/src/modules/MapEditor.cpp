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
// Salute tattica (doc 41 B4): STESSE regole del gate `--validate`, mai una copia.
#include "mini/game/data/ContentValidation.hpp"
// Stesse costanti del runtime per la visuale verticale (KI #83): l'editor deve
// misurare con lo stesso modello con cui il gioco combatte, non con uno suo.
#include "mini/core/GameConfig.hpp"

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
    // Prefab (ADR-048): caricati col loader del RUNTIME, non con un parser dell'editor
    // — un secondo parser divergerebbe al primo campo aggiunto.
    {
        m_prefabReg.loadPrefabs(getDataDir());
        for (const auto& [id, def] : m_prefabReg.prefabs()) m_prefabIds.push_back(id);
        std::sort(m_prefabIds.begin(), m_prefabIds.end());
    }
    // CommanderDef per il combo del comandante (ADR-044: fuori dalle classi)
    {
        std::error_code ec;
        fs::path folder = fs::path(getDataDir()) / "commanders";
        if (fs::exists(folder, ec))
            for (auto& entry : fs::directory_iterator(folder, ec))
                if (entry.path().extension() == ".json")
                    m_commanderIds.push_back(entry.path().stem().string());
        std::sort(m_commanderIds.begin(), m_commanderIds.end());
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
            t.x += delta.x; t.y += delta.y; t.z += delta.z;
            if (t.y < 0.0f) t.y = 0.0f;   // non sotto il suolo
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
        else if (m_selBox <= -3000 && m_selBox > -3100
                 && (-3000 - m_selBox) < (int)m_spawnPoints1.size())   // multi-spawn team1
        {
            auto& p = m_spawnPoints1[-3000 - m_selBox];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (m_selBox <= -3100 && m_selBox > -3200
                 && (-3100 - m_selBox) < (int)m_spawnPoints2.size())   // multi-spawn team2
        {
            auto& p = m_spawnPoints2[-3100 - m_selBox];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
        {   // Istanza di prefab (ADR-048): si sposta l'ISTANZA, il contenuto la segue.
            auto& p = m_prefabInsts[-4000 - m_selBox];
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
        else if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
        {   // Istanza di prefab (ADR-048): ruota tutto il contenuto con sé.
            auto& p = m_prefabInsts[-4000 - m_selBox];
            p.ry = wrap(p.ry + rotDelta.y);
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
        me.id   = entry.path().stem().string();   // ADR-001: MAI dal contenuto (KI #21/#84)
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
    // Il nome visualizzato NON si carica più in un campo editabile: è l'id (2026-08-02).
    // Esisteva una casella separata che lo cambiava senza rinominare il file, creando
    // due nomi divergenti per la stessa mappa.
    m_boxes.clear();
    m_posts.clear();
    m_positions.clear();
    m_sectors.clear();
    m_dangers.clear();
    m_routes.clear();
    m_vehSpawns.clear();
    m_targets.clear();
    m_spawnPoints1.clear();
    m_spawnPoints2.clear();
    m_prefabInsts.clear();
    m_selRoutePt = 0;
    m_selBox = -1;

    // Istanze di prefab (ADR-048): si carica il RIFERIMENTO, non i dati espansi —
    // quelli li rigenera il motore al load, così aggiornare il prefab aggiorna tutte
    // le istanze e non possono diventare stale.
    if (j.contains("prefabs") && j["prefabs"].is_array())
        for (auto& pi : j["prefabs"])
        {
            if (!pi.contains("id")) continue;
            PrefabInstEntry e;
            e.id = pi["id"].get<std::string>();
            e.x  = pi.value("x", 0.0f); e.y = pi.value("y", 0.0f);
            e.z  = pi.value("z", 0.0f); e.ry = pi.value("ry", 0.0f);
            m_prefabInsts.push_back(std::move(e));
        }

    if (j.contains("spawn_team1") && j["spawn_team1"].size() >= 3)
        m_spawnTeam1 = {j["spawn_team1"][0], j["spawn_team1"][1], j["spawn_team1"][2]};
    if (j.contains("spawn_team2") && j["spawn_team2"].size() >= 3)
        m_spawnTeam2 = {j["spawn_team2"][0], j["spawn_team2"][1], j["spawn_team2"][2]};
    // Multi-spawn (opzionale): array di punti [x,y,z] per fazione.
    for (const char* key : {"spawn_points_team1", "spawn_points_team2"})
    {
        if (!j.contains(key) || !j[key].is_array()) continue;
        auto& out = (std::string(key).back() == '1') ? m_spawnPoints1 : m_spawnPoints2;
        for (auto& pt : j[key])
            if (pt.is_array() && pt.size() >= 3)
                out.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
    }

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
            t.y  = st.value("y", 0.f);   // altezza sopra il suolo (0 = a terra)
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
    // Nome visualizzato: se vuoto usa l'id, così il campo non resta un residuo
    // vecchio (era la causa del "il rename non cambia il nome", 2026-07-21).
    // UN SOLO NOME (2026-08-02): il nome visualizzato è l'id. Non esiste più un modo
    // separato di cambiarlo — si rinomina, e cambia ovunque. Se un file vecchio aveva
    // un `name` divergente, il primo salvataggio lo riallinea.
    j["name"] = m_mapId;
    j["spawn_team1"] = {m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]};
    j["spawn_team2"] = {m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]};
    // Multi-spawn: scrivi l'array se ci sono punti, altrimenti RIMUOVI il campo (RMW:
    // niente residuo che distribuirebbe comunque le AI).
    auto writeSpawnPts = [&](const char* key, const std::vector<std::array<float,3>>& pts) {
        if (pts.empty()) { j.erase(key); return; }
        json arr = json::array();
        for (const auto& p : pts) arr.push_back({p[0], p[1], p[2]});
        j[key] = arr;
    };
    writeSpawnPts("spawn_points_team1", m_spawnPoints1);
    writeSpawnPts("spawn_points_team2", m_spawnPoints2);

    // Istanze di prefab (ADR-048): si scrivono SOLO i riferimenti. I box e le posizioni
    // che ne derivano NON vanno mai salvati: sono dati derivati, e scriverli
    // significherebbe congelare una copia che diverge appena il prefab cambia.
    if (m_prefabInsts.empty()) j.erase("prefabs");
    else
    {
        json arr = json::array();
        for (const auto& p : m_prefabInsts)
            arr.push_back({{"id", p.id}, {"x", p.x}, {"y", p.y}, {"z", p.z}, {"ry", p.ry}});
        j["prefabs"] = arr;
    }

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
        st["y"] = t.y;                  // altezza sopra il suolo (0 = a terra)
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
    // Nasce davanti alla camera (dove stai guardando), non al centro mappa.
    const glm::vec3 fp = m_viewport.groundFocusPoint();
    b.x = snap(fp.x); b.y = 1.0f; b.z = snap(fp.z);
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

// ── duplicateSelected (F4, doc 39) ────────────────────────────────────────────
// Duplica l'elemento selezionato QUALUNQUE sia il tipo, copiando TUTTI i campi
// autorati (ruolo/arco/gittata di una posizione, raggio di un settore, ecc.):
// l'authoring dei metadata era laborioso perché ogni nuovo elemento partiva dai
// default e andava ri-regolato. Ora si autora una volta e si duplica in serie.
// Copia spostata di +2 in XZ per non sovrapporre. Spawn e comandante (unici) no.
void MapEditor::duplicateSelected()
{
    const float off = 2.0f;
    if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
    { duplicateBox(m_selBox); return; }
    else if (m_selBox <= -10 && m_selBox > -100)
    {
        int i = -10 - m_selBox;
        if (i < 0 || i >= (int)m_posts.size()) return;
        PostEntry p = m_posts[i]; p.x += off; p.z += off;
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
    }
    else if (m_selBox <= -200 && m_selBox > -300)
    {
        int i = -200 - m_selBox;
        if (i < 0 || i >= (int)m_dangers.size()) return;
        DangerEntry d = m_dangers[i]; d.x += off; d.z += off;
        m_dangers.push_back(d);
        m_selBox = -200 - ((int)m_dangers.size() - 1);
    }
    else if (m_selBox <= -300 && m_selBox > -400)
    {
        int i = -300 - m_selBox;
        if (i < 0 || i >= (int)m_routes.size()) return;
        RouteEntry r = m_routes[i];
        std::snprintf(r.id, sizeof(r.id), "route_%d", (int)m_routes.size() + 1);
        for (auto& pt : r.points) { pt[0] += off; pt[2] += off; }
        m_routes.push_back(std::move(r));
        m_selBox = -300 - ((int)m_routes.size() - 1);
        m_selRoutePt = 0;
    }
    else if (m_selBox <= -400 && m_selBox > -500)
    {
        int i = -400 - m_selBox;
        if (i < 0 || i >= (int)m_vehSpawns.size()) return;
        VehicleSpawnEntry v = m_vehSpawns[i]; v.x += off; v.z += off;
        m_vehSpawns.push_back(v);
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
    }
    else if (m_selBox <= -500 && m_selBox > -1000)
    {
        int i = -500 - m_selBox;
        if (i < 0 || i >= (int)m_targets.size()) return;
        TargetEntry t = m_targets[i]; t.x += off; t.z += off;
        m_targets.push_back(t);
        m_selBox = -500 - ((int)m_targets.size() - 1);
    }
    else if (m_selBox <= -1000 && m_selBox > -2000)
    {
        int i = -1000 - m_selBox;
        if (i < 0 || i >= (int)m_positions.size()) return;
        PositionEntry p = m_positions[i]; p.x += off; p.z += off;
        m_positions.push_back(p);
        m_selBox = -1000 - ((int)m_positions.size() - 1);
    }
    else if (m_selBox <= -2000)
    {
        int i = -2000 - m_selBox;
        if (i < 0 || i >= (int)m_sectors.size()) return;
        SectorEntry s = m_sectors[i]; s.x += off; s.z += off;
        m_sectors.push_back(s);
        m_selBox = -2000 - ((int)m_sectors.size() - 1);
    }
    else return;   // spawn team1/2 e comandante: unici, non duplicabili

    m_dirty = true;
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

// ── savePrefabFromZone (ADR-048) ─────────────────────────────────────────────
// Promuove ad ASSET ciò che è stato costruito nella mappa: prende i box e le posizioni
// tattiche entro un raggio dal focus del viewport, li converte in coordinate LOCALI e
// scrive `data/prefabs/<id>.json`. È il flusso naturale dell'authoring — si costruisce
// il pezzo dove lo si vede, poi lo si rende riusabile — e senza di esso il sistema
// prefab sarebbe monco: si potevano piazzare solo asset scritti a mano nel JSON.
// Cosa finirebbe nel prefab ORA. UNA sola funzione, usata sia dall'anteprima nel
// viewport sia dal salvataggio: se fossero due, l'utente vedrebbe evidenziata una cosa
// e ne otterrebbe un'altra — il tipo di divergenza che questo progetto paga sempre caro.
// Regola: il RAGGIO fa la selezione grossolana, i ritocchi manuali (Shift+click) la
// correggono; una volta toccato a mano, comanda la lista manuale.
void MapEditor::prefabZoneCollect(std::vector<int>& boxes,
                                  std::vector<int>& positions) const
{
    boxes.clear(); positions.clear();
    if (m_prefabPickManual)
    {
        boxes     = m_prefabPickBoxes;
        positions = m_prefabPickPositions;
        return;
    }
    const float r2 = m_newPrefabRadius * m_newPrefabRadius;
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const float dx = m_boxes[i].x - m_prefabZoneX, dz = m_boxes[i].z - m_prefabZoneZ;
        if (dx*dx + dz*dz <= r2) boxes.push_back(i);
    }
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        const float dx = m_positions[i].x - m_prefabZoneX, dz = m_positions[i].z - m_prefabZoneZ;
        if (dx*dx + dz*dz <= r2) positions.push_back(i);
    }
}

bool MapEditor::savePrefabFromZone(std::string& err)
{
    const std::string id = m_newPrefabId;
    if (id.empty()) { err = "Serve un nome (id = nome del file, ADR-001)."; return false; }
    for (char c : id)
        if (!(std::isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-'))
        { err = "Usa lettere, numeri, spazio, _ o -"; return false; }
    if (m_prefabReg.getPrefab(id))
    { err = "Esiste gia' un prefab con questo nome."; return false; }

    // Centro CONGELATO all'apertura del popup, non il focus corrente: se seguisse la
    // telecamera, ciò che era evidenziato cambierebbe mentre si compila il nome.
    const glm::vec3 c{m_prefabZoneX, 0.0f, m_prefabZoneZ};

    // STESSA selezione mostrata nel viewport (raggio + ritocchi manuali).
    std::vector<int> boxIdx, posIdx;
    prefabZoneCollect(boxIdx, posIdx);

    nlohmann::json coll = nlohmann::json::array();
    nlohmann::json tact = nlohmann::json::array();
    for (int i : boxIdx)
    {
        const auto& b = m_boxes[i];
        coll.push_back({{"x", b.x - c.x}, {"y", b.y}, {"z", b.z - c.z}, {"ry", b.ry},
                        {"sx", b.sx}, {"sy", b.sy}, {"sz", b.sz},
                        {"collider", b.isCollider},
                        {"r", b.r}, {"g", b.g}, {"b", b.b}});
    }
    for (int i : posIdx)
    {
        const auto& p = m_positions[i];
        tact.push_back({{"x", p.x - c.x}, {"y", p.y}, {"z", p.z - c.z},
                        {"facing_deg", p.facing}, {"role", p.role},
                        {"height", p.height}, {"protection", p.protection},
                        {"can_shoot", p.canShoot}, {"importance", p.importance},
                        {"radius", p.radius},
                        {"fire_arc_deg", p.fireArc}, {"fire_range", p.fireRange}});
    }
    if (coll.empty() && tact.empty())
    { err = "Nessun elemento selezionato."; return false; }

    nlohmann::json pj;
    pj["name"]      = id;
    pj["collision"] = coll;
    pj["tactical"]  = tact;

    const std::string path = (fs::path(getDataDir()) / "prefabs" / (id + ".json")).string();
    std::error_code ec;
    fs::create_directories(fs::path(getDataDir()) / "prefabs", ec);
    if (!editor::jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) { j = pj; return true; }))
    { err = "Scrittura fallita: " + path; return false; }

    // Ricarica il registry: il nuovo prefab dev'essere subito piazzabile.
    m_prefabReg.loadPrefabs(getDataDir());
    m_prefabIds.clear();
    for (const auto& [pid, def] : m_prefabReg.prefabs()) m_prefabIds.push_back(pid);
    std::sort(m_prefabIds.begin(), m_prefabIds.end());

    if (m_newPrefabConsume)
    {
        // Gli elementi assorbiti escono dalla mappa e tornano come ISTANZA: così non
        // restano duplicati (una copia "cotta" nella mappa + una dal prefab) che poi
        // divergerebbero. Si rimuove dal fondo per non invalidare gli indici.
        for (auto it = boxIdx.rbegin(); it != boxIdx.rend(); ++it)
            m_boxes.erase(m_boxes.begin() + *it);
        for (auto it = posIdx.rbegin(); it != posIdx.rend(); ++it)
            m_positions.erase(m_positions.begin() + *it);
        PrefabInstEntry inst;
        inst.id = id; inst.x = c.x; inst.y = 0.0f; inst.z = c.z; inst.ry = 0.0f;
        m_prefabInsts.push_back(inst);
        m_selBox = -4000 - ((int)m_prefabInsts.size() - 1);
    }
    m_newPrefabId[0] = '\0';
    m_prefabZoneMode = false;         // esce dalla modalità selezione
    m_prefabPickManual = false;
    m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
    m_dirty = true;
    updateViewport();
    return true;
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

    // ── VISUALE VERTICALE (KI #83) ───────────────────────────────────────────
    // Quante posizioni a QUOTA DIVERSA vede ciascuna posizione, con lo stesso modello
    // di combattimento del runtime: origine agli OCCHI, bersaglio al CORPO
    // ([[combat-los-eye-height]]), e la stessa `hasLineOfFire`.
    // A cosa serve: è stato misurato che l'AI ingaggia tutto ciò che vede a quota
    // diversa e spara — il collo di bottiglia è che quasi non VEDE, perché le
    // piattaforme piene occludono verso il basso oltre il proprio bordo. Questo numero
    // rende quel limite visibile QUI, mentre si autora, invece che a tentativi in
    // partita: 0 = posizione elevata inutile per battere chi sta sotto.
    // La LOS si calcola solo sulle coppie cross-quota (il filtro sul dislivello viene
    // PRIMA), quindi il costo resta una frazione di quello dei link.
    // Copertura dall'alto (doc 41 B3): stessa funzione del runtime, calcolata sul
    // MapDef temporaneo → l'editor e il gioco dicono la stessa cosa.
    m_overhead.assign(m_positions.size(), 0);
    for (size_t i = 0; i < tmp.tacticalPositions.size() && i < m_overhead.size(); ++i)
    {
        const auto& p = tmp.tacticalPositions[i];
        m_overhead[i] = mini::worldintel::hasOverheadCover(
            tmp, p.x, p.y, p.z, mini::config::OVERHEAD_PROBE_HEIGHT) ? 1 : 0;
    }

    m_vertSight.assign(m_positions.size(), 0);
    m_vertPairs.assign(m_positions.size(), 0);
    for (size_t i = 0; i < tmp.tacticalPositions.size(); ++i)
    {
        const auto& a = tmp.tacticalPositions[i];
        for (size_t j = 0; j < tmp.tacticalPositions.size(); ++j)
        {
            if (i == j) continue;
            const auto& b = tmp.tacticalPositions[j];
            if (std::fabs(a.y - b.y) <= mini::config::VERTICAL_ENGAGE_DY) continue;
            ++m_vertPairs[i];
            if (mini::worldintel::hasLineOfFire(
                    tmp, a.x, a.y + mini::config::COMBAT_EYE_HEIGHT, a.z,
                         b.x, b.y + mini::config::AI_HALF_Y,          b.z))
                ++m_vertSight[i];
        }
    }

    // ── SALUTE TATTICA (doc 41 B4) ───────────────────────────────────────────
    // Aggrega in un elenco unico i difetti che prima si scoprivano solo ispezionando
    // un elemento per volta. Non inventa regole nuove: usa i dati derivati già
    // calcolati qui sopra (grafo, esposizione, visuale verticale) + due controlli di
    // coerenza. Ogni voce porta il codice di selezione → è cliccabile.
    // Regole CONDIVISE col gate `--validate` (ADR-018): stanno in ContentValidation,
    // mai duplicate qui — due copie divergerebbero al primo ritocco di soglia.
    // I settori vanno copiati nel MapDef temporaneo, altrimenti la regola "settore
    // senza posizioni" non avrebbe nulla su cui lavorare.
    tmp.sectors.reserve(m_sectors.size());
    for (const auto& s : m_sectors)
    {
        mini::SectorDef sd;
        sd.label = s.label; sd.x = s.x; sd.z = s.z;
        sd.radius = s.radius; sd.importance = s.importance;
        tmp.sectors.push_back(sd);
    }
    m_issues.clear();
    for (const auto& d : mini::analyzeTacticalHealth(tmp))
    {
        // Codice di selezione per banda: geometria = indice diretto in `m_boxes`
        // (tmp.geometry è costruito da lì, stesso ordine), posizioni −1000, settori −2000.
        const int sel =
              (d.target == mini::TacticalDefect::Target::Position) ? (-1000 - d.index)
            : (d.target == mini::TacticalDefect::Target::Geometry) ? d.index
            :                                                        (-2000 - d.index);
        m_issues.push_back({sel, d.severity, (int)d.kind, d.text});
    }
    // Difetto specifico dell'EDITOR: la visuale verticale è un dato che vive qui
    // (m_vertSight/m_vertPairs), quindi la regola resta qui — è l'unica.
    for (size_t i = 0; i < m_vertPairs.size() && i < m_vertSight.size(); ++i)
        if (m_vertPairs[i] > 0 && m_vertSight[i] == 0)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "[%s %d] cieca verso le altre quote (0/%d)",
                          i < tmp.tacticalPositions.size()
                              ? tmp.tacticalPositions[i].role.c_str() : "pos",
                          (int)i + 1, m_vertPairs[i]);
            m_issues.push_back({-1000 - (int)i, 1,
                                (int)mini::TacticalDefect::Kind::BlindVertical, buf});
        }
    std::stable_sort(m_issues.begin(), m_issues.end(),
                     [](const TacticalIssue& a, const TacticalIssue& b) { return a.sev > b.sev; });
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
    // Punti multi-spawn: croci più piccole, azzurro (team1) / arancio (team2).
    for (int i = 0; i < (int)m_spawnPoints1.size(); ++i)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnPoints1[i][0]; s.y = m_spawnPoints1[i][1]; s.z = m_spawnPoints1[i][2];
        s.ry = 0; s.sx = 0.5f; s.sy = 1.0f; s.sz = 0.5f;
        s.r = 0.40f; s.g = 0.70f; s.b = 1.00f;
        s.selected = (m_selBox == -3000 - i);
        s.pickId = -3000 - i;
        draws.push_back(s);
    }
    for (int i = 0; i < (int)m_spawnPoints2.size(); ++i)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnPoints2[i][0]; s.y = m_spawnPoints2[i][1]; s.z = m_spawnPoints2[i][2];
        s.ry = 0; s.sx = 0.5f; s.sy = 1.0f; s.sz = 0.5f;
        s.r = 1.00f; s.g = 0.55f; s.b = 0.30f;
        s.selected = (m_selBox == -3100 - i);
        s.pickId = -3100 - i;
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
        // Base a `t.y` sopra il suolo (0 = a terra); +mezza altezza per centrare il box.
        s.x = t.x; s.y = t.y + sc * 0.5f; s.z = t.z; s.ry = t.ry;
        s.sx = sc; s.sy = sc; s.sz = sc;
        s.r = 0.85f; s.g = 0.55f; s.b = 0.15f;
        s.selected = (m_selBox == -500 - i);
        s.pickId = -500 - i;
        draws.push_back(s);
    }

    // ── Creazione prefab: RAGGIO e SELEZIONE visibili (2026-08-02) ───────
    // Prima il raggio era invisibile e si doveva indovinare cosa sarebbe finito nel
    // prefab. Ora si vede il disco della zona e gli elementi presi sono evidenziati:
    // la stessa selezione che verrà salvata (`prefabZoneCollect`, una sola verità).
    if (m_prefabZoneMode)
    {
        FreeCameraViewport::MapBoxDraw disc;
        disc.x = m_prefabZoneX; disc.y = 0.04f; disc.z = m_prefabZoneZ; disc.ry = 0.0f;
        disc.sx = m_newPrefabRadius * 2.0f; disc.sy = 0.03f;
        disc.sz = m_newPrefabRadius * 2.0f;
        disc.r = 0.35f; disc.g = 0.85f; disc.b = 0.95f;   // ciano = zona di raccolta
        disc.selected = false; disc.pickId = FreeCameraViewport::MapBoxDraw::kNoPick;
        draws.push_back(disc);

        std::vector<int> selBoxes, selPos;
        prefabZoneCollect(selBoxes, selPos);
        // Marker sopra ogni elemento incluso: si legge a colpo d'occhio COSA entra,
        // anche quando gli oggetti sono uno dietro l'altro.
        for (int i : selBoxes)
        {
            if (i < 0 || i >= (int)m_boxes.size()) continue;
            const auto& b = m_boxes[i];
            FreeCameraViewport::MapBoxDraw m;
            m.x = b.x; m.y = b.y + b.sy * 0.5f + 0.45f; m.z = b.z; m.ry = 45.0f;
            m.sx = 0.4f; m.sy = 0.4f; m.sz = 0.4f;
            m.r = 0.35f; m.g = 0.95f; m.b = 1.0f;
            m.selected = false; m.pickId = i;   // cliccabile per il toggle Shift+click
            draws.push_back(m);
        }
        for (int i : selPos)
        {
            if (i < 0 || i >= (int)m_positions.size()) continue;
            const auto& p = m_positions[i];
            FreeCameraViewport::MapBoxDraw m;
            m.x = p.x; m.y = p.y + 1.9f; m.z = p.z; m.ry = 45.0f;
            m.sx = 0.35f; m.sy = 0.35f; m.sz = 0.35f;
            m.r = 0.35f; m.g = 0.95f; m.b = 1.0f;
            m.selected = false; m.pickId = -1000 - i;
            draws.push_back(m);
        }
    }

    // ── Istanze di PREFAB (ADR-048): anteprima ───────────────────────────
    // Si disegnano i box del prefab con la STESSA trasformazione dell'espansione del
    // motore (rotazione attorno a Y + traslazione), così ciò che si vede nell'editor è
    // ciò che esisterà in partita. Tinta distinta: sono contenuto DERIVATO, non box
    // della mappa — non si editano qui, si edita l'istanza.
    for (int i = 0; i < (int)m_prefabInsts.size(); ++i)
    {
        const auto& inst = m_prefabInsts[i];
        const mini::PrefabDef* pf = m_prefabReg.getPrefab(inst.id);
        const bool sel = (m_selBox == -4000 - i);
        if (!pf)
        {   // Riferimento rotto: marker rosso, così si vede invece di sparire in silenzio.
            FreeCameraViewport::MapBoxDraw bad;
            bad.x = inst.x; bad.y = inst.y + 1.0f; bad.z = inst.z;
            bad.sx = 1.0f; bad.sy = 2.0f; bad.sz = 1.0f;
            bad.r = 0.95f; bad.g = 0.15f; bad.b = 0.15f;
            bad.selected = sel; bad.pickId = -4000 - i;
            draws.push_back(bad);
            continue;
        }
        const float rad = inst.ry * 3.14159265f / 180.0f;
        const float cs = std::cos(rad), sn = std::sin(rad);
        for (const auto& b : pf->collision)
        {
            FreeCameraViewport::MapBoxDraw d;
            d.x  = inst.x + b.x * cs + b.z * sn;
            d.z  = inst.z - b.x * sn + b.z * cs;
            d.y  = inst.y + b.y;
            d.ry = b.ry + inst.ry;
            d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
            d.r = 0.55f; d.g = 0.45f; d.b = 0.75f;   // viola: contenuto da prefab
            d.selected = sel; d.pickId = -4000 - i;
            draws.push_back(d);
        }
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

        // Segnale di posizione CIECA sopra/sotto (KI #83): tacca rossa sospesa. Si
        // disegna SOLO dove il difetto esiste davvero — ci sono altre quote in giro
        // (vertPairs > 0) ma da qui non se ne batte nessuna. Serve a trovarle a colpo
        // d'occhio, senza selezionarle una per una: il colore del corpo resta quello
        // del RUOLO, che non va perso.
        if (i < (int)m_vertSight.size() && i < (int)m_vertPairs.size()
            && m_vertPairs[i] > 0 && m_vertSight[i] == 0)
        {
            FreeCameraViewport::MapBoxDraw blind;
            blind.x = p.x; blind.y = p.y + h + 0.55f; blind.z = p.z;
            blind.ry = 45.0f;   // rombo: si distingue dai marker quadrati
            blind.sx = 0.34f; blind.sy = 0.34f; blind.sz = 0.34f;
            blind.r = 0.95f; blind.g = 0.25f; blind.b = 0.20f;
            blind.selected = sel; blind.pickId = -1000 - i;
            draws.push_back(blind);
        }

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
    else if (m_selBox <= -3000 && m_selBox > -3100
             && (-3000 - m_selBox) < (int)m_spawnPoints1.size())   // multi-spawn team1
    {
        const auto& p = m_spawnPoints1[-3000 - m_selBox];
        m_viewport.setGizmoTarget({p[0], p[1], p[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox <= -3100 && m_selBox > -3200
             && (-3100 - m_selBox) < (int)m_spawnPoints2.size())   // multi-spawn team2
    {
        const auto& p = m_spawnPoints2[-3100 - m_selBox];
        m_viewport.setGizmoTarget({p[0], p[1], p[2]}, true);
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
        m_viewport.setGizmoTarget({t.x, t.y + 1.25f, t.z}, true);
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
    // Istanza di PREFAB (ADR-048): PRIMA del ramo settori, che usa `<= -2000` e
    // catturerebbe anche -4000. Sposta e RUOTA (la scala no: scalare un prefab
    // deformerebbe le posizioni tattiche che porta con sé).
    else if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
    {
        const auto& p = m_prefabInsts[-4000 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y + 1.0f, p.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // solo yaw
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

    // ── Maniglia di ridimensionamento del pannello destro ────────────────
    // Splitter ESPLICITO invece di `ChildFlags_ResizeX`: quel flag mette il grip sul
    // bordo DESTRO del child, che qui coincide col bordo della finestra — una volta
    // stretto il pannello non c'era più nulla da afferrare e non si riallargava
    // (segnalato dall'utente). Con una maniglia a SINISTRA il gesto è simmetrico.
    ImGui::InvisibleButton("##propsplit", ImVec2(6.0f, remaining));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
        s_propW -= ImGui::GetIO().MouseDelta.x;   // trascinando a sinistra si ALLARGA
    // Clamp: mai sotto il minimo leggibile, mai oltre metà finestra → non può
    // "incastrarsi" in uno stato da cui non si torna indietro.
    const float propMax = (totalW > 400.0f) ? totalW * 0.5f : 200.0f;
    if (s_propW < 180.0f)    s_propW = 180.0f;
    if (s_propW > propMax)   s_propW = propMax;
    ImGui::SameLine();

    // ── Proprietà ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_props", ImVec2(s_propW, 0), ImGuiChildFlags_Borders);
    drawProperties(ImGui::GetContentRegionAvail().x, remaining);
    ImGui::EndChild();

    ImGui::EndChild();
}

// ── drawToolbar ───────────────────────────────────────────────────────────────
void MapEditor::drawToolbar()
{
    // Selettore mappa. La CREAZIONE di una nuova mappa sta in coda a questa lista
    // (voce "＋ Nuova mappa…" → popup di conferma), non come pulsante sciolto sulla
    // toolbar: la barra era satura e tagliava comandi ([[ui-no-clipping-use-dropdowns]]).
    bool openNewMapPopup = false;
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
        ImGui::Separator();
        if (ImGui::Selectable("+ Nuova mappa..."))
            openNewMapPopup = true;
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mappa corrente (in coda: crea una mappa nuova)");
    if (openNewMapPopup) ImGui::OpenPopup("Nuova mappa");

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
    if (ImGui::Button("Duplica")) duplicateSelected();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Duplica l'elemento selezionato (box, posizione, settore,\n"
                          "pericolo, bersaglio, post, percorso, veicolo) con TUTTI i\n"
                          "suoi valori. Autora una volta, posane una serie.");
    ImGui::SameLine();
    if (ImGui::Button("Elimina") && m_selBox >= 0) {
        ImGui::OpenPopup("##del_confirm");
    }

    // ── Rinomina mappa: UN SOLO nome, UN SOLO comando (ADR-010) ──────────
    // Prima c'erano DUE caselle di testo affiancate con semantiche diverse: una
    // cambiava il NOME VISUALIZZATO (campo `name`), l'altra faceva il RENAME vero
    // (file + cross-reference). Due modi di "cambiare nome" con effetti diversi sono
    // una trappola, e occupavano permanentemente la toolbar (segnalato dall'utente
    // 2026-08-02). Ora: **un pulsante, un popup**, come Nuova mappa/Elimina — e il
    // rename allinea filename, id e nome visualizzato, così **il nome è uno solo,
    // uguale da qualunque parte lo si guardi**.
    if (!m_mapId.empty())
    {
        ImGui::SameLine(0, 16);
        static char renameBuf[64] = "";
        static std::string renameErr;
        if (ImGui::Button("Rinomina..."))
        {
            std::snprintf(renameBuf, sizeof(renameBuf), "%s", m_mapId.c_str());
            renameErr.clear();
            ImGui::OpenPopup("Rinomina mappa");
        }
        if (ImGui::BeginPopup("Rinomina mappa"))
        {
            ImGui::TextDisabled("Il nome cambia ovunque: file, elenco e partita.");
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("##mrename", renameBuf, sizeof(renameBuf));
            if (!renameErr.empty())
                ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
            if (ImGui::Button("Rinomina", {110,0}) && renameBuf[0] != '\0'
                && m_mapId != renameBuf)
            {
                int refs = 0;
                renameErr = editor::rename::renameDefinition(
                    getDataDir() + "/", editor::rename::Category::Map,
                    m_mapId, renameBuf, &refs);
                if (renameErr.empty())
                {
                    const std::string newId = renameBuf;
                    // Il nome VISUALIZZATO segue l'id: è ciò che rende il nome unico.
                    editor::jsonsave::saveJsonRMW(
                        getDataDir() + "/maps/" + newId + ".json",
                        [&](nlohmann::json& j) { j["name"] = newId; return true; });
                    loadMaps();
                    loadMap(newId);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110,0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
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
    {
        bool solid = m_viewport.showSolid();
        if (ImGui::Checkbox("Solido", &solid)) m_viewport.setShowSolid(solid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Facce piene oltre al wireframe: rende visibili le\n"
                              "superfici (muri, piattaforme, cover). Off = solo linee.");
    }

    // NB: i pulsanti modalità gizmo (Sposta/Ruota/Scala) NON stanno più qui: sono
    // l'overlay in alto a sinistra della viewport (FreeCameraViewport::drawGizmoOverlay),
    // che appare quando selezioni un oggetto. Erano un duplicato che saturava la
    // toolbar ([[ui-no-clipping-use-dropdowns]]). Le capacità ruota/scala per tipo di
    // selezione le imposta updateViewport() via setGizmoCanRotateScale, ogni frame.

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

    // ── Nuova mappa (id = nome file, ADR-001) ─────────────────────────────
    // Aperto dalla voce "＋ Nuova mappa…" in coda alla combo. Nominare un file
    // NUOVO è l'eccezione legittima alla regola "dropdown-only" (Todo, ADR-010).
    if (ImGui::BeginPopupModal("Nuova mappa", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char newMapId[64] = "";
        static std::string newMapErr;
        ImGui::TextUnformatted("Nome della nuova mappa (= nome file):");
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool entered = ImGui::InputText("##newmapid", newMapId, sizeof(newMapId),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (!newMapErr.empty())
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", newMapErr.c_str());

        auto tryCreate = [&]() -> bool {
            if (newMapId[0] == '\0') { newMapErr = "nome vuoto"; return false; }
            const std::string id = newMapId;
            // id = filename stem: gli id mappa ammettono spazi ("Training Ground"),
            // ma non i separatori di percorso/caratteri illegali (no traversal).
            const bool badChar = id.find_first_of("/\\:*?\"<>|") != std::string::npos
                                 || id.front() == '.' || id.back() == ' ';
            const std::string path = getDataDir() + "/maps/" + id + ".json";
            if (badChar)            { newMapErr = "nome non valido"; return false; }
            if (fs::exists(path))   { newMapErr = "esiste gia'";     return false; }
            // JSON minimo VALIDO e GIOCABILE: un pavimento (senza, niente navmesh →
            // unità nel vuoto, ContentValidation lo rifiuta) e i due spawn. Muri,
            // metadata, roster e comandante si autorano dopo.
            editor::jsonsave::saveJsonRMW(path, [&](json& j) {
                j["name"]        = id;
                j["spawn_team1"] = {0.0f, 0.86f,  8.0f};
                j["spawn_team2"] = {0.0f, 0.86f, -8.0f};
                json floor;
                floor["type"]  = "floor";  floor["label"] = "Pavimento";
                floor["x"]  = 0.0f;  floor["y"]  = -0.1f;  floor["z"]  = 0.0f;
                floor["ry"] = 0.0f;
                floor["sx"] = 50.0f; floor["sy"] = 0.4f;   floor["sz"] = 40.0f;
                floor["r"]  = 0.40f; floor["g"]  = 0.36f;  floor["b"]  = 0.30f;
                floor["collider"] = true;
                j["geometry"] = json::array({floor});
                return true;
            });
            loadMaps();
            loadMap(id);            // passa subito alla mappa nuova
            return true;
        };

        if ((ImGui::Button("Conferma", {120,0}) || entered) && tryCreate())
        { newMapId[0] = '\0'; newMapErr.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120,0}))
        { newMapId[0] = '\0'; newMapErr.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// ── drawBoxList ───────────────────────────────────────────────────────────────
void MapEditor::drawBoxList(float /*panelW*/, float /*panelH*/)
{
    // ── SALUTE TATTICA (doc 41 B4) ───────────────────────────────────────
    // In CIMA e chiuso di default: si vede subito SE la mappa ha problemi senza che
    // rubi spazio quando non ne ha. Prima questi controlli esistevano ma andavano
    // cercati un elemento per volta — impraticabile oltre le poche decine di posizioni.
    {
        int problems = 0;
        for (const auto& is : m_issues) if (is.sev == 1) ++problems;
        const int warns = (int)m_issues.size() - problems;

        if (m_issues.empty())
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), "Salute tattica: OK");
        else
        {
            char hdr[96];
            std::snprintf(hdr, sizeof(hdr), "Salute tattica: %d problemi, %d avvisi###salute",
                          problems, warns);
            ImGui::PushStyleColor(ImGuiCol_Text, problems > 0 ? ImVec4(0.95f, 0.55f, 0.35f, 1.0f)
                                                              : ImVec4(0.90f, 0.85f, 0.40f, 1.0f));
            const bool open = ImGui::CollapsingHeader(hdr);
            ImGui::PopStyleColor();
            if (open)
            {
                ImGui::TextDisabled("Clicca una voce per selezionare l'elemento.");
                // RAGGRUPPATO PER TIPO (richiesta utente 2026-08-02): un elenco lungo e
                // indifferenziato si smette di leggere. Ogni categoria è una tendina
                // richiudibile, così le famiglie intenzionali per QUESTA mappa (es. i
                // settori di solo transito) si chiudono una volta e non disturbano più,
                // senza doverle disattivare — restano lì se un giorno servono.
                ImGui::BeginChild("##issues", ImVec2(0, 220), true);
                for (int kind = 0; kind < (int)mini::TacticalDefect::Kind::Count; ++kind)
                {
                    int nProb = 0, nWarn = 0;
                    for (const auto& is : m_issues)
                        if (is.kind == kind) { if (is.sev == 1) ++nProb; else ++nWarn; }
                    if (nProb + nWarn == 0) continue;   // categoria vuota → non si mostra

                    char khdr[128];
                    std::snprintf(khdr, sizeof(khdr), "%s (%d)###k%d",
                                  mini::tacticalDefectKindName((mini::TacticalDefect::Kind)kind),
                                  nProb + nWarn, kind);
                    // I gruppi con PROBLEMI si aprono da soli; quelli di soli avvisi
                    // restano chiusi: si vede subito cosa merita attenzione.
                    if (nProb > 0) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        nProb > 0 ? ImVec4(0.95f, 0.60f, 0.50f, 1.0f)
                                  : ImVec4(0.85f, 0.82f, 0.55f, 1.0f));
                    const bool kopen = ImGui::TreeNode(khdr);
                    ImGui::PopStyleColor();
                    if (!kopen) continue;
                    for (int k = 0; k < (int)m_issues.size(); ++k)
                    {
                        const auto& is = m_issues[k];
                        if (is.kind != kind) continue;
                        char lbl[192];
                        std::snprintf(lbl, sizeof(lbl), "%s %s##iss%d",
                                      is.sev == 1 ? "!" : "-", is.text.c_str(), k);
                        ImGui::PushStyleColor(ImGuiCol_Text,
                            is.sev == 1 ? ImVec4(0.95f, 0.60f, 0.50f, 1.0f)
                                        : ImVec4(0.85f, 0.82f, 0.55f, 1.0f));
                        if (ImGui::Selectable(lbl, m_selBox == is.sel))
                        { m_selBox = is.sel; updateViewport(); }
                        ImGui::PopStyleColor();
                    }
                    ImGui::TreePop();
                }
                ImGui::EndChild();
            }
        }
        ImGui::Separator();
    }

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

    // Punti multi-spawn AGGIUNTIVI: le AI si distribuiscono su questi + lo spawn
    // principale della fazione. Selezionabili dal viewport, spostabili col gizmo.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
    for (int i = 0; i < (int)m_spawnPoints1.size(); ++i)
    {
        char lbl[48]; std::snprintf(lbl, sizeof(lbl), "  [T1] punto #%d##sp1_%d", i, i);
        if (ImGui::Selectable(lbl, m_selBox == -3000 - i)) { m_selBox = -3000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.30f, 1.0f));
    for (int i = 0; i < (int)m_spawnPoints2.size(); ++i)
    {
        char lbl[48]; std::snprintf(lbl, sizeof(lbl), "  [T2] punto #%d##sp2_%d", i, i);
        if (ImGui::Selectable(lbl, m_selBox == -3100 - i)) { m_selBox = -3100 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ punto T1"))
    { const glm::vec3 fp = m_viewport.groundFocusPoint();
      m_spawnPoints1.push_back({fp.x, 0.86f, fp.z});
      m_selBox = -3000 - ((int)m_spawnPoints1.size()-1); m_dirty = true; updateViewport(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ punto T2"))
    { const glm::vec3 fp = m_viewport.groundFocusPoint();
      m_spawnPoints2.push_back({fp.x, 0.86f, fp.z});
      m_selBox = -3100 - ((int)m_spawnPoints2.size()-1); m_dirty = true; updateViewport(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##sp"))
    {
        if (m_selBox <= -3000 && m_selBox > -3100)
        { int i = -3000 - m_selBox; if (i >= 0 && i < (int)m_spawnPoints1.size())
          { m_spawnPoints1.erase(m_spawnPoints1.begin()+i); m_selBox = -1; m_dirty = true; updateViewport(); } }
        else if (m_selBox <= -3100 && m_selBox > -3200)
        { int i = -3100 - m_selBox; if (i >= 0 && i < (int)m_spawnPoints2.size())
          { m_spawnPoints2.erase(m_spawnPoints2.begin()+i); m_selBox = -1; m_dirty = true; updateViewport(); } }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Rimuove il punto multi-spawn selezionato.\n"
                          "Le AI si distribuiscono sui punti + lo spawn principale.");

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
        if (!m_commanderIds.empty()) m_commander.unit = m_commanderIds.front();
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        m_commander.x = fp.x; m_commander.z = fp.z;
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        p.x = fp.x; p.z = fp.z;
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        t.x = fp.x; t.z = fp.z;
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

    // Riepilogo VISUALE VERTICALE (KI #83): quante posizioni, pur avendo altre quote
    // in giro, non ne battono NESSUNA. È lo stato di salute verticale della mappa a
    // colpo d'occhio — prima lo si scopriva solo giocando. Le cieche hanno un rombo
    // rosso sopra nel viewport; qui si vede subito se sono un caso isolato o la norma.
    {
        int blind = 0, withPairs = 0;
        for (size_t i = 0; i < m_vertPairs.size(); ++i)
            if (m_vertPairs[i] > 0)
            { ++withPairs; if (i < m_vertSight.size() && m_vertSight[i] == 0) ++blind; }
        if (withPairs > 0)
        {
            if (blind > 0)
                ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f),
                                   "Verticale: %d/%d cieche", blind, withPairs);
            else
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f),
                                   "Verticale: tutte battono altre quote");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Posizioni che NON vedono nessuna quota diversa.\n"
                                  "Rombo rosso nel viewport. Rimedi: avvicinare al bordo,\n"
                                  "abbassare i parapetti, o accettare che il piano alto\n"
                                  "domini solo le lunghe distanze.");
        }
    }

    // ── PREFAB (ADR-048) ─────────────────────────────────────────────────
    // Piazzare un prefab porta con sé collisione E posizioni tattiche già pensate:
    // è il modo di autorare mappe profonde senza piazzare mille posizioni a mano.
    ImGui::Separator();
    ImGui::TextDisabled("Prefab (%d)", (int)m_prefabInsts.size());

    // CREAZIONE da zona: si costruisce il pezzo nella mappa e lo si promuove ad asset.
    // Senza questo si potevano solo piazzare prefab scritti a mano nel JSON.
    {
        static std::string prefabErr;
        if (ImGui::SmallButton("Crea prefab da zona..."))
        {
            prefabErr.clear();
            // Centro CONGELATO all'apertura: se seguisse la telecamera, la selezione
            // cambierebbe sotto gli occhi mentre si scrive il nome.
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            m_prefabZoneX = fp.x; m_prefabZoneZ = fp.z;
            m_prefabZoneMode = true; m_prefabPickManual = false;
            m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
            updateViewport();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Prende box e posizioni tattiche entro un raggio dal punto\n"
                              "inquadrato e li salva come asset riusabile.");
        // Pannello INLINE, non un popup: un popup ImGui si chiude al primo click fuori,
        // quindi cliccare nel viewport per rifinire la selezione lo faceva sparire e il
        // Shift+click arrivava a modalità già spenta (segnalato dall'utente 2026-08-02).
        // Un'azione che richiede di cliccare nella SCENA non può vivere in un popup.
        if (m_prefabZoneMode)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Zona in CIANO nel viewport; gli elementi presi hanno un rombo.");
            ImGui::TextDisabled("Shift/Ctrl+click su un elemento per aggiungerlo o toglierlo.");
            std::vector<int> selB, selP;
            prefabZoneCollect(selB, selP);
            ImGui::TextColored({0.35f,0.95f,1.0f,1.0f}, "Selezionati: %d box, %d posizioni%s",
                               (int)selB.size(), (int)selP.size(),
                               m_prefabPickManual ? "  (ritoccato a mano)" : "");
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputText("Nome", m_newPrefabId, sizeof(m_newPrefabId));
            // Il raggio agisce solo finché non si ritocca a mano: dopo comanda la lista.
            ImGui::BeginDisabled(m_prefabPickManual);
            if (editor::ui::sliderRow("Raggio", m_newPrefabRadius, 1.f, 40.f, 0.5f, "%.1f m"))
                updateViewport();
            ImGui::EndDisabled();
            if (m_prefabPickManual && ImGui::SmallButton("Torna al raggio"))
            { m_prefabPickManual = false; updateViewport(); }
            ImGui::Checkbox("Sostituisci in mappa con un'istanza", &m_newPrefabConsume);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Consigliato: evita di avere due copie (una fissa nella\n"
                                  "mappa e una dal prefab) che poi divergono.");
            if (!prefabErr.empty())
                ImGui::TextColored({0.95f,0.35f,0.30f,1.0f}, "%s", prefabErr.c_str());
            if (ImGui::Button("Crea", {110,0}))
                savePrefabFromZone(prefabErr);   // esce da sé dalla modalità se riesce
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110,0}))
            {
                prefabErr.clear();
                m_prefabZoneMode = false; m_prefabPickManual = false;
                m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
                updateViewport();
            }
            ImGui::Separator();
        }
    }

    if (m_prefabIds.empty())
        ImGui::TextDisabled("nessun prefab in data/prefabs/");
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.62f, 0.95f, 1.0f));
        for (int i = 0; i < (int)m_prefabInsts.size(); ++i)
        {
            const auto& p = m_prefabInsts[i];
            const bool broken = (m_prefabReg.getPrefab(p.id) == nullptr);
            char lbl[96];
            std::snprintf(lbl, sizeof(lbl), "%s%s##pfi%d",
                          broken ? "! " : "", p.id.c_str(), i);
            if (ImGui::Selectable(lbl, m_selBox == -4000 - i))
            { m_selBox = -4000 - i; updateViewport(); }
            if (broken && ImGui::IsItemHovered())
                ImGui::SetTooltip("Prefab '%s' non trovato in data/prefabs/", p.id.c_str());
        }
        ImGui::PopStyleColor();

        if (m_prefabPick >= (int)m_prefabIds.size()) m_prefabPick = 0;
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##pfsel", m_prefabIds[m_prefabPick].c_str()))
        {
            for (int k = 0; k < (int)m_prefabIds.size(); ++k)
                if (ImGui::Selectable(m_prefabIds[k].c_str(), m_prefabPick == k))
                    m_prefabPick = k;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Piazza"))
        {
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            PrefabInstEntry e;
            e.id = m_prefabIds[m_prefabPick];
            e.x = fp.x; e.y = 0.0f; e.z = fp.z;
            m_prefabInsts.push_back(e);
            m_selBox = -4000 - ((int)m_prefabInsts.size() - 1);
            m_dirty = true; updateViewport();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("-##pfi") && m_selBox <= -4000)
        {
            const int i = -4000 - m_selBox;
            if (i >= 0 && i < (int)m_prefabInsts.size())
            { m_prefabInsts.erase(m_prefabInsts.begin() + i); m_selBox = -1;
              m_dirty = true; updateViewport(); }
        }
    }

    ImGui::Separator();
    // Posizioni tattiche (ADR-030): una sola lista, il ruolo è nell'etichetta.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.75f, 1.0f));
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        // Marcatore "cieca" anche in lista: si trova la posizione difettosa senza
        // doverle aprire una a una.
        const bool blindPos = i < (int)m_vertPairs.size() && m_vertPairs[i] > 0
                           && i < (int)m_vertSight.size() && m_vertSight[i] == 0;
        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "%s[%s] %d##tpos%d",
                      blindPos ? "! " : "", m_positions[i].role.c_str(), i + 1, i);
        if (ImGui::Selectable(lbl, m_selBox == -1000 - i))
        { m_selBox = -1000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Posizione"))
    {
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        PositionEntry e; e.x = fp.x; e.z = fp.z;
        m_positions.push_back(e);
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        SectorEntry s; s.x = fp.x; s.z = fp.z;
        m_sectors.push_back(s);
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        DangerEntry d; d.x = fp.x; d.z = fp.z;
        m_dangers.push_back(d);
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        r.points.push_back({fp.x,        0.5f, fp.z});
        r.points.push_back({fp.x + 4.0f, 0.5f, fp.z});
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
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        v.x = fp.x; v.z = fp.z;
        m_vehSpawns.push_back(std::move(v));
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##vs") && m_selBox <= -400 && m_selBox > -500)
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

        // CommanderDef dal registry (dropdown, mai testo libero — ADR-044).
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Tipo##cmd",
                              m_commander.unit.empty() ? "-- scegli --"
                                                       : m_commander.unit.c_str()))
        {
            for (const auto& cid : m_commanderIds)
                if (ImGui::Selectable(cid.c_str(), m_commander.unit == cid))
                { m_commander.unit = cid; changed = true; }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Definizione da data/commanders/ (non e' piu' una classe,\n"
                            "ADR-044). Non combatte: si difende soltanto.");

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

    // ── Istanza di PREFAB selezionata (ADR-048) ──────────────────────────
    // PRIMA del ramo settori: quello usa `<= -2000` come catch-all e catturerebbe
    // anche i codici dei prefab (-4000).
    if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
    {
        auto& inst = m_prefabInsts[-4000 - m_selBox];
        const mini::PrefabDef* pf = m_prefabReg.getPrefab(inst.id);
        ImGui::TextColored({0.72f,0.62f,0.95f,1.0f}, "Prefab: %s", inst.id.c_str());
        ImGui::Separator();
        if (!pf)
            ImGui::TextColored({0.95f,0.35f,0.30f,1.0f},
                               "NON TROVATO in data/prefabs/ — l'istanza non produrra' nulla");
        else
            ImGui::TextDisabled("%d box di collisione, %d posizioni tattiche",
                                (int)pf->collision.size(), (int)pf->tactical.size());
        bool changed = false;
        changed |= editor::ui::dragRow("X", inst.x, 0.1f, -500.f, 500.f, "%.2f");
        changed |= editor::ui::dragRow("Y", inst.y, 0.1f, -100.f, 100.f, "%.2f");
        changed |= editor::ui::dragRow("Z", inst.z, 0.1f, -500.f, 500.f, "%.2f");
        changed |= editor::ui::sliderRow("Rotazione", inst.ry, 0.f, 360.f, 5.f, "%.0f");
        ImGui::TextDisabled("Contenuto e posizioni tattiche sono DERIVATI dal prefab:");
        ImGui::TextDisabled("si modificano nel file del prefab, valgono per ogni istanza.");
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

        // Copertura dall'alto (doc 41 B3): DERIVATA, sola lettura. Utile a capire se una
        // posizione è al riparo da tiro/lanci dall'alto e se sta sotto una struttura.
        if (pi < (int)m_overhead.size())
            ImGui::TextDisabled(m_overhead[pi] ? "Coperta dall'alto: si' (sotto una struttura)"
                                               : "Coperta dall'alto: no (cielo aperto)");

        // Visuale VERTICALE (KI #83): DERIVATA, sola lettura. Misurato in partita che
        // l'AI ingaggia tutto cio' che vede a quota diversa e spara: se qui c'e' 0, il
        // problema non e' l'AI ma la geometria — da qui non si battera' MAI nessuno
        // sopra/sotto, per quanto la posizione sia elevata.
        if (pi < (int)m_vertSight.size() && pi < (int)m_vertPairs.size())
        {
            const int vs = m_vertSight[pi], vp = m_vertPairs[pi];
            if (vp == 0)
                ImGui::TextDisabled("Visuale verticale: n/d (nessuna posizione ad altra quota)");
            else if (vs == 0)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f),
                                   "Visuale verticale: 0 / %d — CIECA sopra/sotto", vp);
                ImGui::TextDisabled("Da qui non si battera' MAI un'altra quota:");
                ImGui::TextDisabled("avvicinala al BORDO, o abbassa il parapetto.");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f),
                                   "Visuale verticale: %d / %d (%.0f%%)",
                                   vs, vp, 100.0f * (float)vs / (float)vp);
                ImGui::TextDisabled("Utile al combattimento verticale (ponti/rialzati).");
            }
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
        changed |= editor::ui::sliderRow("Y (altezza)##tg", t.y, 0.f, 30.f, 0.1f, "%.2f m", 18.0f);
        ImGui::TextDisabled("Altezza sopra il suolo: 0 = a terra, >0 = alzata (es. su\n"
                            "una piattaforma). La struttura e' statica: resta dove la metti.");
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
        // In modalità creazione prefab, Shift/Ctrl+click NON cambia la selezione: la
        // RIFINISCE (aggiunge/toglie l'elemento), come nei file manager e nei software
        // 3D. Il raggio fa la presa grossolana, il click il lavoro di precisione.
        const bool refine = m_prefabZoneMode
                          && (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl);
        if (refine)
        {
            // Al primo ritocco si "congela" la selezione corrente del raggio, così si
            // parte da ciò che si sta già vedendo invece che da una lista vuota.
            if (!m_prefabPickManual)
            {
                prefabZoneCollect(m_prefabPickBoxes, m_prefabPickPositions);
                m_prefabPickManual = true;
            }
            auto toggle = [](std::vector<int>& v, int idx) {
                auto it = std::find(v.begin(), v.end(), idx);
                if (it != v.end()) v.erase(it); else v.push_back(idx);
            };
            if (picked >= 0)                                   // box
                toggle(m_prefabPickBoxes, picked);
            else if (picked <= -1000 && picked > -2000)         // posizione tattica
                toggle(m_prefabPickPositions, -1000 - picked);
            updateViewport();
        }
        else
        {
            m_selBox = picked;
            if (m_selBox <= -300 && m_selBox > -400) m_selRoutePt = 0;   // route: primo punto
            updateViewport();   // rinfresca evidenziazione + bersaglio del gizmo
        }
    }
    (void)vpW; (void)vpH;
}

} // namespace editor

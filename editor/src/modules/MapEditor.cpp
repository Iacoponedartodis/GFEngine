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
    m_editorClock += dt;   // orologio per la coalescenza dell'undo

    // Gizmo Sposta: applica lo spostamento a TUTTA la selezione (G3).
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        // Un trascinamento produce un delta per frame: `pushUndo` li fonde in una
        // sola voce (stessa etichetta entro 0,6 s), così "annulla" riporta all'inizio
        // del gesto e non indietro di un pixel.
        pushUndo("gizmo");
        for (int code : selectionCodes()) applyMove(code, delta);
        m_dirty = true;
        updateViewport();
    }

    applyGizmoRotateScale();
}

// ── Modello di SELEZIONE MULTIPLA (G3) ───────────────────────────────────────
// Un solo insieme di codici. Il PRIMARIO è l'ultimo della lista: è ciò che il
// pannello proprietà mostra, e con un solo elemento tutto si comporta come prima.
int MapEditor::primaryCode() const
{
    if (!m_multiSel.empty()) return m_multiSel.back();
    if (m_selStruct >= 0) return -6000 - m_selStruct;
    return m_selBox;
}

std::vector<int> MapEditor::selectionCodes() const
{
    if (!m_multiSel.empty()) return m_multiSel;
    const int c = primaryCode();
    if (c == -1) return {};
    return { c };
}

void MapEditor::setSelection(int code, bool additive)
{
    if (additive)
    {
        // Ctrl+click: aggiunge o toglie, come in qualunque gestore di file. Se
        // l'insieme è vuoto si parte da ciò che era già selezionato, altrimenti
        // il primo Ctrl+click perderebbe la selezione corrente.
        if (m_multiSel.empty() && primaryCode() != -1)
            m_multiSel.push_back(primaryCode());
        auto it = std::find(m_multiSel.begin(), m_multiSel.end(), code);
        if (it != m_multiSel.end()) m_multiSel.erase(it);
        else                        m_multiSel.push_back(code);
    }
    else
    {
        m_multiSel.clear();
        if (code != -1) m_multiSel.push_back(code);
    }
    // Il primario governa il pannello proprietà: si tiene allineato all'ultimo.
    const int p = m_multiSel.empty() ? -1 : m_multiSel.back();
    if (p <= -6000) { m_selStruct = -6000 - p; m_selBox = -1; }
    else            { m_selStruct = -1;        m_selBox = p;  }
    if (m_selBox <= -300 && m_selBox > -400) m_selRoutePt = 0;
    updateViewport();
}

// Posizione di un elemento, se ne ha una. Serve alla rotazione di gruppo (per il
// baricentro) e all'evidenziazione.
bool MapEditor::codePosition(int code, glm::vec3& out) const
{
    auto set = [&](float x, float y, float z) { out = {x, y, z}; return true; };
    if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
    { const auto& s = m_structures[-6000 - code]; return set(s.x, s.y, s.z); }
    if (code >= 0 && code < (int)m_boxes.size())
    { const auto& b = m_boxes[code]; return set(b.x, b.y, b.z); }
    if (code == -2) return set(m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]);
    if (code == -3) return set(m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]);
    if (code <= -10 && code > -100 && (-10 - code) < (int)m_posts.size())
    { const auto& p = m_posts[-10 - code]; return set(p.x, p.y, p.z); }
    if (code <= -200 && code > -300 && (-200 - code) < (int)m_dangers.size())
    { const auto& d = m_dangers[-200 - code]; return set(d.x, d.y, d.z); }
    if (code <= -400 && code > -500 && (-400 - code) < (int)m_vehSpawns.size())
    { const auto& v = m_vehSpawns[-400 - code]; return set(v.x, 0.0f, v.z); }
    if (code <= -500 && code > -1000 && (-500 - code) < (int)m_targets.size())
    { const auto& t = m_targets[-500 - code]; return set(t.x, t.y, t.z); }
    if (code <= -1000 && code > -2000 && (-1000 - code) < (int)m_positions.size())
    { const auto& p = m_positions[-1000 - code]; return set(p.x, p.y, p.z); }
    if (code <= -2000 && code > -3000 && (-2000 - code) < (int)m_sectors.size())
    { const auto& s = m_sectors[-2000 - code]; return set(s.x, 0.0f, s.z); }
    if (code <= -4000 && code > -5000 && (-4000 - code) < (int)m_prefabInsts.size())
    { const auto& p = m_prefabInsts[-4000 - code]; return set(p.x, p.y, p.z); }
    return false;
}

// Orientamento di un elemento, se ne ha uno (nullptr = non ruotabile).
float* MapEditor::codeYaw(int code)
{
    if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        return &m_structures[-6000 - code].ry;
    if (code >= 0 && code < (int)m_boxes.size())
        return &m_boxes[code].ry;
    if (code <= -400 && code > -500 && (-400 - code) < (int)m_vehSpawns.size())
        return &m_vehSpawns[-400 - code].ry;
    if (code <= -500 && code > -1000 && (-500 - code) < (int)m_targets.size())
        return &m_targets[-500 - code].ry;
    if (code <= -1000 && code > -2000 && (-1000 - code) < (int)m_positions.size())
        return &m_positions[-1000 - code].facing;
    if (code <= -4000 && code > -5000 && (-4000 - code) < (int)m_prefabInsts.size())
        return &m_prefabInsts[-4000 - code].ry;
    return nullptr;
}

// Elimina TUTTI gli elementi selezionati. Si raccolgono gli indici per contenitore
// e si cancella in ordine DECRESCENTE: cancellare in avanti invaliderebbe gli indici
// successivi e si finirebbe per eliminare l'elemento sbagliato.
void MapEditor::deleteSelection()
{
    const auto codes = selectionCodes();
    if (codes.empty()) return;
    pushUndo("elimina selezione");

    std::vector<int> boxes, structs, positions, posts, dangers, targets,
                     sectors, vehicles, prefabs;
    for (int c : codes)
    {
        if      (c >= 0)                          boxes.push_back(c);
        else if (c <= -6000)                      structs.push_back(-6000 - c);
        else if (c <= -4000 && c > -5000)         prefabs.push_back(-4000 - c);
        else if (c <= -2000 && c > -3000)         sectors.push_back(-2000 - c);
        else if (c <= -1000 && c > -2000)         positions.push_back(-1000 - c);
        else if (c <= -500  && c > -1000)         targets.push_back(-500 - c);
        else if (c <= -400  && c > -500)          vehicles.push_back(-400 - c);
        else if (c <= -200  && c > -300)          dangers.push_back(-200 - c);
        else if (c <= -10   && c > -100)          posts.push_back(-10 - c);
        // spawn, route e comandante restano fuori: sono singoli o liste di punti,
        // e "eliminarli in blocco" non è un'operazione sensata.
    }
    auto eraseAll = [](auto& vec, std::vector<int>& idx) {
        std::sort(idx.begin(), idx.end(), std::greater<int>());
        idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
        for (int i : idx) if (i >= 0 && i < (int)vec.size()) vec.erase(vec.begin() + i);
    };
    eraseAll(m_boxes, boxes);            eraseAll(m_structures, structs);
    eraseAll(m_prefabInsts, prefabs);    eraseAll(m_sectors, sectors);
    eraseAll(m_positions, positions);    eraseAll(m_targets, targets);
    eraseAll(m_vehSpawns, vehicles);     eraseAll(m_dangers, dangers);
    eraseAll(m_posts, posts);

    m_multiSel.clear();
    m_selBox = -1; m_selStruct = -1;
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Sposta di `delta` l'elemento identificato dal CODICE di selezione.
// Estratta dalla catena di `tick` perché ora serve a più chiamanti (uno per elemento
// selezionato): con la selezione multipla la stessa logica andava ripetuta, ed è il
// tipo di duplicazione che diverge al primo tipo aggiunto.
void MapEditor::applyMove(int code, const glm::vec3& delta)
{
    {
        // Le STRUTTURE si spostano come tutto il resto (ADR-053). Il gizmo muove la
        // RICETTA e i box si rigenerano: è il motivo per cui una scala si sposta
        // intera, gradini compresi, invece che uno alla volta.
        if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        {
            auto& s = m_structures[-6000 - code];
            s.x += delta.x; s.y += delta.y; s.z += delta.z;
            rebuildStructurePreview();
        }
        else if (code >= 0 && code < (int)m_boxes.size())
        {
            auto& b = m_boxes[code];
            b.x += delta.x; b.y += delta.y; b.z += delta.z;
        }
        else if (code == -2)
        { m_spawnTeam1[0]+=delta.x; m_spawnTeam1[1]+=delta.y; m_spawnTeam1[2]+=delta.z; }
        else if (code == -3)
        { m_spawnTeam2[0]+=delta.x; m_spawnTeam2[1]+=delta.y; m_spawnTeam2[2]+=delta.z; }
        else if (code <= -10 && code > -100
                 && (-10 - code) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code <= -200 && code > -300
                 && (-200 - code) < (int)m_dangers.size())
        {
            auto& d = m_dangers[-200 - code];
            d.x += delta.x; d.y += delta.y; d.z += delta.z;
        }
        else if (code <= -300 && code > -400
                 && (-300 - code) < (int)m_routes.size())
        {
            auto& r = m_routes[-300 - code];
            if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
            {
                r.points[m_selRoutePt][0] += delta.x;
                r.points[m_selRoutePt][1] += delta.y;
                r.points[m_selRoutePt][2] += delta.z;
            }
        }
        else if (code <= -400 && code > -500
                 && (-400 - code) < (int)m_vehSpawns.size())
        {
            auto& v = m_vehSpawns[-400 - code];
            v.x += delta.x; v.z += delta.z;
        }
        else if (code <= -500 && code > -1000
                 && (-500 - code) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - code];
            t.x += delta.x; t.y += delta.y; t.z += delta.z;
            if (t.y < 0.0f) t.y = 0.0f;   // non sotto il suolo
        }
        else if (code <= -2000 && (-2000 - code) < (int)m_sectors.size())   // ADR-034
        {
            auto& s = m_sectors[-2000 - code];
            s.x += delta.x; s.z += delta.z;
        }
        else if (code <= -1000 && code > -2000
                 && (-1000 - code) < (int)m_positions.size())   // ADR-030
        {
            auto& p = m_positions[-1000 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code <= -3000 && code > -3100
                 && (-3000 - code) < (int)m_spawnPoints1.size())   // multi-spawn team1
        {
            auto& p = m_spawnPoints1[-3000 - code];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (code <= -3100 && code > -3200
                 && (-3100 - code) < (int)m_spawnPoints2.size())   // multi-spawn team2
        {
            auto& p = m_spawnPoints2[-3100 - code];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (code <= -4000 && (-4000 - code) < (int)m_prefabInsts.size())
        {   // Istanza di prefab (ADR-048): si sposta l'ISTANZA, il contenuto la segue.
            auto& p = m_prefabInsts[-4000 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code == kSelCommander && m_commander.exists)   // ADR-041
        {
            m_commander.x += delta.x; m_commander.z += delta.z;
        }
    }
}

// Rotazione e scala del gizmo. Restano legate all'elemento PRIMARIO (l'ultimo
// selezionato): ruotare o scalare un gruppo eterogeneo non ha un significato unico —
// un raggio, un'altezza e un `facing` non si scalano allo stesso modo. La rotazione
// DI GRUPPO esiste ed è gestita a parte, sotto, per i soli elementi con posizione.
void MapEditor::applyGizmoRotateScale()
{
    // Gizmo Ruota (solo asse Y): box mappa (ry), cover point (facing), veicolo (ry).
    // ADR-025: i marker metadata con un campo di orientamento sono ruotabili.
    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta))
    {
        auto wrap = [](float a) { while (a > 180.0f) a -= 360.0f; while (a < -180.0f) a += 360.0f; return a; };
        // ── ROTAZIONE DI GRUPPO (G3) ──────────────────────────────────────
        // Con più elementi selezionati la rotazione deve far ORBITARE ognuno
        // attorno al baricentro comune, oltre a girarlo su sé stesso. Applicare
        // solo il proprio yaw farebbe "girare sul posto" ogni pezzo, ed è
        // esattamente ciò che NON serve quando si ruota un edificio.
        if (selectionCodes().size() > 1)
        {
            pushUndo("gizmo-rot-gruppo");
            glm::vec3 c(0.0f); int n = 0;
            for (int code : selectionCodes())
            { glm::vec3 p; if (codePosition(code, p)) { c += p; ++n; } }
            if (n > 0)
            {
                c /= (float)n;
                const float rad = rotDelta.y * 3.14159265f / 180.0f;
                const float cs = std::cos(rad), sn = std::sin(rad);
                for (int code : selectionCodes())
                {
                    glm::vec3 p;
                    if (!codePosition(code, p)) continue;
                    const float dx = p.x - c.x, dz = p.z - c.z;
                    // Stessa convenzione di rotazione dei prefab e delle strutture.
                    const glm::vec3 moved = { c.x + dx * cs + dz * sn - p.x, 0.0f,
                                              c.z - dx * sn + dz * cs - p.z };
                    applyMove(code, moved);
                    if (float* yaw = codeYaw(code)) *yaw = wrap(*yaw + rotDelta.y);
                }
                rebuildStructurePreview();
                m_dirty = true; updateViewport();
            }
            return;
        }
        // Struttura: ruota la RICETTA, e con lei la direzione di salita. Una scala
        // ruotata resta una scala — è il vantaggio di orientare l'intento invece
        // che quindici box.
        if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
        {
            auto& s = m_structures[m_selStruct];
            s.ry = wrap(s.ry + rotDelta.y);
            pushUndo("gizmo-rot");
            m_dirty = true; rebuildStructurePreview(); updateViewport();
        }
        else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
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
        // Struttura: la scala del gizmo agisce sui PARAMETRI che hanno senso per il
        // tipo, non su un box. X = larghezza (scala/rampa) o lunghezza (muro),
        // Y = dislivello o altezza, Z = profondità della piattaforma.
        // I GRADINI NON SI ROMPONO: allargare una scala non tocca l'alzata, e
        // alzarla aggiunge gradini invece di renderli più ripidi.
        if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
        {
            auto& s = m_structures[m_selStruct];
            auto grow = [](float& v, float d, float lo) { v += d; if (v < lo) v = lo; };
            switch (s.kind)
            {
            case mini::StructureKind::Stair:
            case mini::StructureKind::Ramp:
                grow(s.width, scaleDelta.x, 0.5f);
                grow(s.rise,  scaleDelta.y, 0.1f);
                grow(s.tread, scaleDelta.z, mini::mapmetrics::STAIR_TREAD);
                break;
            case mini::StructureKind::Wall:
                grow(s.length,    scaleDelta.x, 0.5f);
                grow(s.height,    scaleDelta.y, 0.2f);
                grow(s.thickness, scaleDelta.z, 0.05f);
                break;
            case mini::StructureKind::Platform:
                grow(s.sizeX, scaleDelta.x, 1.0f);
                grow(s.y,     scaleDelta.y, s.baseY + 0.1f);
                grow(s.sizeZ, scaleDelta.z, 1.0f);
                break;
            }
            pushUndo("gizmo-scale");
            m_dirty = true; rebuildStructurePreview(); updateViewport();
        }
        else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
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

    // Primitive parametriche (ADR-053): si legge la RICETTA. I box che ne derivano
    // si rigenerano subito sotto e non stanno in `m_boxes` — non vanno né salvati né
    // modificati a mano, altrimenti si perde il legame con la ricetta.
    m_structures.clear();
    m_selStruct = -1;
    if (j.contains("structures") && j["structures"].is_array())
    {
        for (auto& s : j["structures"])
        {
            mini::StructureDef d;
            d.kind  = mini::mapstructures::parseKind(s.value("kind", std::string("stair")));
            d.label = s.value("label", std::string(""));
            d.x = s.value("x", 0.f);  d.y = s.value("y", 0.f);  d.z = s.value("z", 0.f);
            d.ry        = s.value("ry", 0.f);
            d.rise      = s.value("rise", 2.f);
            d.width     = s.value("width", 2.f);
            d.riser     = s.value("riser", 0.f);
            d.tread     = s.value("tread", 0.f);
            d.length    = s.value("length", 4.f);
            d.height    = s.value("height", 0.f);
            d.thickness = s.value("thickness", 0.0f);
            d.sizeX     = s.value("size_x", 6.f);
            d.sizeZ     = s.value("size_z", 6.f);
            d.baseY     = s.value("base_y", 0.f);
            d.openW     = s.value("open_w", 0.f);
            d.openH     = s.value("open_h", 0.f);
            d.openSill  = s.value("open_sill", 0.f);
            d.openOff   = s.value("open_off", 0.f);
            d.flightRise= s.value("flight_rise", 0.f);
            d.spacing   = s.value("spacing", 0.f);
            d.ceiling   = s.value("ceiling", false);
            d.railing   = s.value("railing", false);
            if (s.contains("access") && s["access"].is_array())
                for (size_t i = 0; i < 4 && i < s["access"].size(); ++i)
                    d.access[i] = s["access"][i].get<bool>();
            d.color[0] = s.value("r", 0.35f);
            d.color[1] = s.value("g", 0.32f);
            d.color[2] = s.value("b", 0.28f);
            m_structures.push_back(d);
        }
    }

    if (j.contains("geometry") && j["geometry"].is_array())
    {
        for (auto& gb : j["geometry"])
        {
            // I box derivati da una primitiva non si salvano MAI (ADR-033/053).
            // Se ne trovo uno in un file scritto male, lo scarto: altrimenti al
            // prossimo salvataggio verrebbe congelato accanto a quello rigenerato.
            if (gb.value("from_structure", false)) continue;
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
    // La pila di undo appartiene al DOCUMENTO: tenerla fra due mappe diverse
    // significherebbe poter "annullare" la mappa A dentro la mappa B.
    m_undo.clear();
    m_redo.clear();
    m_lastUndoTag.clear();
    rebuildStructurePreview();   // ADR-053: i box derivati esistono solo qui
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

    // Primitive parametriche (ADR-053): si scrive SOLO la ricetta, mai i box che ne
    // derivano — stessa disciplina dei prefab poco sopra.
    if (m_structures.empty()) j.erase("structures");
    else
    {
        json arr = json::array();
        for (const auto& s : m_structures)
        {
            json o;
            o["kind"]  = mini::mapstructures::kindName(s.kind);
            o["label"] = s.label;
            o["x"] = s.x;  o["y"] = s.y;  o["z"] = s.z;  o["ry"] = s.ry;
            o["rise"] = s.rise;  o["width"] = s.width;
            o["riser"] = s.riser;  o["tread"] = s.tread;
            o["length"] = s.length;  o["height"] = s.height;  o["thickness"] = s.thickness;
            o["size_x"] = s.sizeX;  o["size_z"] = s.sizeZ;  o["base_y"] = s.baseY;
            o["open_w"] = s.openW;  o["open_h"] = s.openH;
            o["open_sill"] = s.openSill;  o["open_off"] = s.openOff;
            o["flight_rise"] = s.flightRise;  o["spacing"] = s.spacing;
            o["ceiling"] = s.ceiling;  o["railing"] = s.railing;
            o["access"] = json::array({s.access[0], s.access[1], s.access[2], s.access[3]});
            o["r"] = s.color[0];  o["g"] = s.color[1];  o["b"] = s.color[2];
            arr.push_back(o);
        }
        j["structures"] = arr;
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
    pushUndo("addBox");
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
void MapEditor::duplicateOne(int code)
{
    const float off = 2.0f;
    if (code >= 0 && code < (int)m_boxes.size())
    { duplicateBox(code); return; }
    else if (code <= -10 && code > -100)
    {
        int i = -10 - code;
        if (i < 0 || i >= (int)m_posts.size()) return;
        PostEntry p = m_posts[i]; p.x += off; p.z += off;
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
    }
    else if (code <= -200 && code > -300)
    {
        int i = -200 - code;
        if (i < 0 || i >= (int)m_dangers.size()) return;
        DangerEntry d = m_dangers[i]; d.x += off; d.z += off;
        m_dangers.push_back(d);
        m_selBox = -200 - ((int)m_dangers.size() - 1);
    }
    else if (code <= -300 && code > -400)
    {
        int i = -300 - code;
        if (i < 0 || i >= (int)m_routes.size()) return;
        RouteEntry r = m_routes[i];
        std::snprintf(r.id, sizeof(r.id), "route_%d", (int)m_routes.size() + 1);
        for (auto& pt : r.points) { pt[0] += off; pt[2] += off; }
        m_routes.push_back(std::move(r));
        m_selBox = -300 - ((int)m_routes.size() - 1);
        m_selRoutePt = 0;
    }
    else if (code <= -400 && code > -500)
    {
        int i = -400 - code;
        if (i < 0 || i >= (int)m_vehSpawns.size()) return;
        VehicleSpawnEntry v = m_vehSpawns[i]; v.x += off; v.z += off;
        m_vehSpawns.push_back(v);
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
    }
    else if (code <= -500 && code > -1000)
    {
        int i = -500 - code;
        if (i < 0 || i >= (int)m_targets.size()) return;
        TargetEntry t = m_targets[i]; t.x += off; t.z += off;
        m_targets.push_back(t);
        m_selBox = -500 - ((int)m_targets.size() - 1);
    }
    else if (code <= -1000 && code > -2000)
    {
        int i = -1000 - code;
        if (i < 0 || i >= (int)m_positions.size()) return;
        PositionEntry p = m_positions[i]; p.x += off; p.z += off;
        m_positions.push_back(p);
        m_selBox = -1000 - ((int)m_positions.size() - 1);
    }
    else if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
    {   // Struttura parametrica: si duplica la RICETTA, i box si rigenerano.
        mini::StructureDef s = m_structures[-6000 - code];
        s.x += off; s.z += off;
        m_structures.push_back(s);
        m_selStruct = (int)m_structures.size() - 1;
        rebuildStructurePreview();
    }
    else if (code <= -2000 && code > -3000)
    {
        int i = -2000 - code;
        if (i < 0 || i >= (int)m_sectors.size()) return;
        SectorEntry s = m_sectors[i]; s.x += off; s.z += off;
        m_sectors.push_back(s);
        m_selBox = -2000 - ((int)m_sectors.size() - 1);
    }
    else return;   // spawn team1/2 e comandante: unici, non duplicabili

    m_dirty = true;
}

// ── ARRAY: N copie con offset progressivo (doc 47 E4) ────────────────────────
// La differenza con "Duplica" ripetuto è che l'offset è **progressivo**: la copia
// i-esima sta a i × offset dall'originale, quindi una fila resta allineata invece
// di accumulare l'errore di dodici trascinamenti a mano.
void MapEditor::makeArray()
{
    const auto codes = selectionCodes();
    if (codes.empty() || m_arrayCount < 1) return;
    pushUndo("array");

    for (int k = 1; k <= m_arrayCount; ++k)
    {
        const glm::vec3 off = { m_arrayOff[0] * (float)k,
                                m_arrayOff[1] * (float)k,
                                m_arrayOff[2] * (float)k };
        const float yaw = m_arrayYawStep * (float)k;
        for (int c : codes)
        {
            // `duplicateOne` mette la copia a +2/+2 di default: la si riporta
            // sull'originale e poi si applica l'offset voluto, così l'unico
            // spostamento è quello dichiarato.
            const int beforeBoxes  = (int)m_boxes.size();
            const int beforeStruct = (int)m_structures.size();
            duplicateOne(c);
            int newCode = -1;
            if ((int)m_boxes.size() > beforeBoxes)          newCode = (int)m_boxes.size() - 1;
            else if ((int)m_structures.size() > beforeStruct) newCode = -6000 - ((int)m_structures.size() - 1);
            else                                             newCode = m_selBox;
            glm::vec3 src, dst;
            if (codePosition(c, src) && codePosition(newCode, dst))
                applyMove(newCode, src + off - dst);
            if (yaw != 0.0f) if (float* y = codeYaw(newCode)) *y += yaw;
        }
    }
    m_multiSel.clear();
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

bool MapEditor::filtersActive() const
{
    for (bool b : m_showType) if (!b) return true;
    return m_hideAboveY < 999.0f || !m_showStructures;
}

// Duplica TUTTA la selezione (G3). Le copie si accodano, quindi gli indici degli
// elementi già selezionati restano validi mentre si itera.
void MapEditor::duplicateSelected()
{
    const auto codes = selectionCodes();
    if (codes.empty()) return;
    pushUndo("duplica");
    for (int c : codes) duplicateOne(c);
    // La selezione NON segue le copie: restare sugli originali renderebbe il
    // secondo "duplica" una copia della copia, che non è mai ciò che si vuole.
    // Si tiene il primario sull'ultima copia creata, come nel caso singolo.
    m_multiSel.clear();
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// ── deleteBox ─────────────────────────────────────────────────────────────────
void MapEditor::deleteBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    pushUndo("elimina");
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
    // Stesso punto usato dall'eliminazione: una via sola per stare allineati al disco.
    reloadPrefabAssets();

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

// ── Ricarica degli asset prefab ───────────────────────────────────────────────
// Un punto solo. Funziona perché dal 2026-08-05 `loadPrefabs` AZZERA il proprio
// contenitore: prima sommava al vecchio stato, quindi un prefab cancellato dal
// disco restava nel menu e si poteva ancora piazzare fino al riavvio dell'editor.
// Il comando di eliminazione funzionava: non era autoritativo il ricaricamento.
void MapEditor::reloadPrefabAssets()
{
    m_prefabReg.loadPrefabs(getDataDir());
    m_prefabIds.clear();
    for (const auto& [pid, def] : m_prefabReg.prefabs()) m_prefabIds.push_back(pid);
    std::sort(m_prefabIds.begin(), m_prefabIds.end());
    if (m_prefabIds.empty()) m_prefabIds.push_back("");   // il combo vuole una voce
    if (m_prefabPick >= (int)m_prefabIds.size()) m_prefabPick = 0;
    updateViewport();   // le istanze di un prefab sparito diventano subito "rotte"
}

// ── UNDO / REDO a snapshot (doc 47 E1) ────────────────────────────────────────
MapEditor::Snapshot MapEditor::captureState() const
{
    Snapshot s;
    s.boxes = m_boxes;           s.posts       = m_posts;
    s.positions = m_positions;   s.sectors     = m_sectors;
    s.dangers = m_dangers;       s.routes      = m_routes;
    s.vehSpawns = m_vehSpawns;   s.targets     = m_targets;
    s.prefabInsts = m_prefabInsts;
    s.structures = m_structures; s.commander   = m_commander;
    s.spawnTeam1 = m_spawnTeam1; s.spawnTeam2  = m_spawnTeam2;
    s.spawnPoints1 = m_spawnPoints1; s.spawnPoints2 = m_spawnPoints2;
    return s;
}

void MapEditor::applyState(const Snapshot& s)
{
    m_boxes = s.boxes;           m_posts       = s.posts;
    m_positions = s.positions;   m_sectors     = s.sectors;
    m_dangers = s.dangers;       m_routes      = s.routes;
    m_vehSpawns = s.vehSpawns;   m_targets     = s.targets;
    m_prefabInsts = s.prefabInsts;
    m_structures = s.structures; m_commander   = s.commander;
    m_spawnTeam1 = s.spawnTeam1; m_spawnTeam2  = s.spawnTeam2;
    m_spawnPoints1 = s.spawnPoints1; m_spawnPoints2 = s.spawnPoints2;
    // La selezione può puntare a un elemento che non esiste più: azzerarla è più
    // sicuro che provare a rimapparla (un indice sbagliato fa modificare il box
    // sbagliato, che è peggio di perdere la selezione).
    m_selBox    = -1;
    m_selStruct = -1;
    m_dirty     = true;
    rebuildStructurePreview();
    updateViewport();
}

void MapEditor::pushUndo(const char* tag)
{
    // Coalescenza: un trascinamento di gizmo genera una modifica per frame. Senza
    // raggruppamento, un solo gesto riempirebbe la pila e "annulla" tornerebbe
    // indietro di un pixel per volta — inutile.
    if (m_lastUndoTag == tag && (m_editorClock - m_lastUndoTime) < 0.6f)
    {
        m_lastUndoTime = m_editorClock;
        return;
    }
    m_lastUndoTag  = tag;
    m_lastUndoTime = m_editorClock;

    m_undo.push_back(captureState());
    if (m_undo.size() > kUndoDepth) m_undo.erase(m_undo.begin());
    m_redo.clear();   // una nuova azione taglia il ramo di ripristino
}

void MapEditor::doUndo()
{
    if (m_undo.empty()) return;
    m_redo.push_back(captureState());
    const Snapshot s = m_undo.back();
    m_undo.pop_back();
    applyState(s);
    m_lastUndoTag.clear();   // il prossimo push non si fonde con quello annullato
}

void MapEditor::doRedo()
{
    if (m_redo.empty()) return;
    m_undo.push_back(captureState());
    const Snapshot s = m_redo.back();
    m_redo.pop_back();
    applyState(s);
    m_lastUndoTag.clear();
}

// ── Primitive parametriche (ADR-053) ──────────────────────────────────────────
// L'anteprima usa `mapstructures::expand`, LA STESSA funzione che il motore chiama
// al load. Non è pigrizia: due espansioni separate divergerebbero al primo campo
// aggiunto, e l'editor mostrerebbe una scala diversa da quella che si gioca.
void MapEditor::rebuildStructurePreview()
{
    m_structPreview.clear();
    for (const auto& s : m_structures)
        mini::mapstructures::expand(s, m_structPreview);
}

void MapEditor::addStructure(mini::StructureKind kind)
{
    pushUndo("addStructure");
    mini::StructureDef d;
    d.kind = kind;
    // Nasce davanti alla camera come gli altri elementi, e con i valori NORMATIVI
    // (doc 47 §4.3): il default è già a norma, non un punto di partenza da correggere.
    const glm::vec3 fp = m_viewport.groundFocusPoint();
    d.x = snap(fp.x);  d.z = snap(fp.z);  d.y = 0.0f;
    switch (kind)
    {
        case mini::StructureKind::Stair:
            d.label = "Scala";  d.rise = 2.0f;
            d.width = mini::mapmetrics::STAIR_MIN_WIDTH;  break;
        case mini::StructureKind::Ramp:
            d.label = "Rampa";  d.rise = 2.0f;
            d.width = mini::mapmetrics::CORRIDOR_MIN;     break;
        case mini::StructureKind::Wall:
            d.label = "Muro";   d.length = 6.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.thickness = 0.0f;                          break;
        case mini::StructureKind::Platform:
            d.label = "Piattaforma";  d.sizeX = 8.0f; d.sizeZ = 8.0f;
            d.y = 3.0f; d.baseY = 0.0f;
            d.access[0] = true;                          break;
        case mini::StructureKind::Switchback:
            d.label = "Scala doppia";  d.rise = 6.0f;
            d.width = mini::mapmetrics::STAIR_MIN_WIDTH;
            d.flightRise = 0.0f;                         break;
        case mini::StructureKind::Doorway:
            d.label = "Porta";  d.length = 6.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.openW = mini::mapmetrics::DOOR_WIDTH;
            d.openH = mini::mapmetrics::DOOR_HEIGHT;     break;
        case mini::StructureKind::Room:
            d.label = "Stanza";  d.sizeX = 10.0f; d.sizeZ = 8.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.openW = mini::mapmetrics::DOOR_WIDTH;
            d.openH = mini::mapmetrics::DOOR_HEIGHT;
            d.access[0] = true;                          break;
        case mini::StructureKind::Catwalk:
            d.label = "Passerella";  d.length = 12.0f;
            d.width = mini::mapmetrics::CORRIDOR_MIN;
            d.y = 4.0f;  d.railing = true;               break;
        case mini::StructureKind::Barricade:
            d.label = "Barricata";  d.length = 12.0f;
            d.width = 2.0f;  d.spacing = 1.5f;
            d.height = mini::mapmetrics::COVER_LOW;      break;
    }
    m_structures.push_back(d);
    m_selStruct = (int)m_structures.size() - 1;
    m_selBox    = -1;
    rebuildStructurePreview();
    updateViewport();
}

// ── updateViewport ────────────────────────────────────────────────────────────
void MapEditor::updateViewport(bool recomputeDerived)
{
    if (recomputeDerived) recomputeExposure();   // ADR-033: tenuta in pari con le posizioni
    std::vector<FreeCameraViewport::MapBoxDraw> draws;
    draws.reserve(m_boxes.size() + 2);

    // Selezione in corso per la creazione di un prefab: si raccoglie PRIMA del ciclo
    // dei box, perché ora gli elementi inclusi si mostrano COLORANDO il box stesso
    // invece di appoggiarci sopra un rombo. Il rombo funzionava ma è un oggetto in
    // più da leggere: colorare la cosa selezionata è come si comportano tutti gli
    // editor 3D, e non aggiunge geometria alla scena.
    std::vector<int> zoneBoxes, zonePositions;
    if (m_prefabZoneMode) prefabZoneCollect(zoneBoxes, zonePositions);
    auto inZone = [&](int idx) {
        return std::find(zoneBoxes.begin(), zoneBoxes.end(), idx) != zoneBoxes.end();
    };

    // ── Difetti per box (G7) ──────────────────────────────────────────────
    // −1 = nessun difetto, 0 = avviso, 1 = problema. Costruito da `m_issues`, che
    // viene da `analyzeTacticalHealth`: la stessa funzione del gate `--validate`.
    // Una seconda analisi "solo per l'editor" prima o poi darebbe un verdetto
    // diverso da quello del gioco, ed è il difetto che ci è costato di più
    // (changelog 77: due verità sullo stesso mondo).
    std::vector<int> boxDefect(m_boxes.size(), -1);
    if (m_showDefects)
        for (const auto& is : m_issues)
            if (is.sel >= 0 && is.sel < (int)boxDefect.size())
                boxDefect[is.sel] = std::max(boxDefect[is.sel], is.sev);

    // Seziona un box alla quota di taglio. `false` = sta tutto sopra, non si disegna.
    auto cutBox = [&](FreeCameraViewport::MapBoxDraw& d) -> bool {
        if (m_hideAboveY > 999.0f) return true;              // nessun taglio
        const float base = d.y - d.sy * 0.5f;
        const float top  = d.y + d.sy * 0.5f;
        if (base >= m_hideAboveY) return false;              // interamente sopra
        if (top  <= m_hideAboveY) return true;               // interamente sotto
        d.sy = m_hideAboveY - base;                          // a cavallo: si tronca
        d.y  = base + d.sy * 0.5f;
        return true;
    };

    // Tipi "floor" visualizzati diversamente se showNavmesh
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        // FILTRI DI VISIBILITÀ (E5): solo visivi, non toccano i dati.
        {
            const int ti = (std::strcmp(b.type,"floor")==0)    ? 0
                         : (std::strcmp(b.type,"wall")==0)     ? 1
                         : (std::strcmp(b.type,"platform")==0) ? 2
                         : (std::strcmp(b.type,"cover")==0)    ? 3 : 4;
            if (!m_showType[ti]) continue;
        }
        FreeCameraViewport::MapBoxDraw d;
        d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
        d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
        // ── SEZIONE alla quota di taglio ──────────────────────────────────
        // Non "nascondi ciò che ha la base sopra": quella regola lasciava in piedi
        // ogni muro che parte da terra, quindi muovendo lo slider non si vedeva
        // tagliare nulla. Qui il box viene **sezionato**: sopra la quota sparisce,
        // a cavallo viene disegnato solo fino al piano. È una vista in sezione vera,
        // e costa solo due sottrazioni — i DATI non si toccano.
        if (!cutBox(d)) continue;
        // Selezione multipla (G3): tutti gli elementi dell'insieme si evidenziano,
        // non solo il primario — altrimenti non si vede cosa si sta per spostare.
        d.selected = (i == m_selBox)
            || std::find(m_multiSel.begin(), m_multiSel.end(), i) != m_multiSel.end();
        d.pickId = i;

        bool isFloor = (std::string(b.type) == "floor");

        if (m_prefabZoneMode && inZone(i)) {
            // Incluso nel prefab: ciano pieno. Si legge a colpo d'occhio COSA entra,
            // senza rombi sospesi sopra gli oggetti.
            d.r = 0.30f; d.g = 0.90f; d.b = 1.00f;
        } else if (!m_prefabZoneMode && boxDefect[i] >= 0) {
            // DIFETTO (G7): rosso = problema (il navmesh non ci sale, nessuno ci
            // arriva), ambra = avviso. Si vede mentre costruisci, non dopo aver
            // salvato: è la differenza fra correggere un box e rifare una zona.
            if (boxDefect[i] >= 1) { d.r = 0.95f; d.g = 0.22f; d.b = 0.18f; }
            else                   { d.r = 0.95f; d.g = 0.72f; d.b = 0.20f; }
        } else if (m_prefabZoneMode) {
            // Escluso: smorzato, così il contrasto con gli inclusi è netto.
            d.r = b.r * 0.45f; d.g = b.g * 0.45f; d.b = b.b * 0.45f;
        } else if (m_showNavmesh && isFloor) {
            // Overlay verde per area navigabile
            d.r = 0.10f; d.g = 0.90f; d.b = 0.30f;
        } else {
            d.r = b.r; d.g = b.g; d.b = b.b;
        }

        draws.push_back(d);
    }

    // ── Box DERIVATI dalle primitive (ADR-053) ────────────────────────────
    // Non sono in `m_boxes` e non si salvano: si vedono, non si toccano. Il pickId
    // negativo li mappa sulla RICETTA che li ha generati, così cliccare un gradino
    // seleziona la scala — che è l'unica cosa modificabile.
    // Si rigenera per struttura, così si sa QUALE ricetta ha prodotto ogni box e la
    // selezionata si può evidenziare tutta insieme.
    for (int si = 0; si < (int)m_structures.size() && m_showStructures; ++si)
    {
        std::vector<mini::MapGeometryBox> own;
        mini::mapstructures::expand(m_structures[si], own);
        const bool selected = (si == m_selStruct)
            || std::find(m_multiSel.begin(), m_multiSel.end(), -6000 - si) != m_multiSel.end();
        for (const auto& b : own)
        {
            FreeCameraViewport::MapBoxDraw d;
            d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
            d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
            if (!cutBox(d)) continue;   // i gradini di una scala si sezionano come tutto
            d.selected = selected;
            // Cliccare un GRADINO seleziona la SCALA: i box derivati non esistono
            // come entità modificabili, quindi il pickId rimanda alla ricetta.
            // Codice -6000-i, coerente con gli altri intervalli negativi.
            d.pickId = -6000 - si;
            if (selected)
            {   // Tutta la struttura si accende: una scala si seleziona intera.
                d.r = 1.00f; d.g = 0.78f; d.b = 0.35f;
            }
            else
            {   // Tinta più fredda: si distingue a colpo d'occhio ciò che è generato
                // da ciò che è stato messo a mano.
                d.r = b.r * 0.85f; d.g = b.g * 0.95f; d.b = b.b * 1.15f;
                if (d.b > 1.0f) d.b = 1.0f;
            }
            draws.push_back(d);
        }
    }

    // NOTA: nessuna lastra a segnare il piano di taglio. Un primo tentativo la
    // disegnava (2026-08-05) ed era controproducente — diventava essa stessa un
    // tetto che copriva la vista, cioè l'opposto di ciò per cui si taglia. Il piano
    // si vede da sé: è la quota a cui la geometria risulta sezionata.

    // ── FIGURA DI SCALA (E6) ──────────────────────────────────────────────
    // L'errore più comune del blockout sono gli sbagli di SCALA, e il rimedio
    // raccomandato è banale: avere sotto gli occhi una figura di riferimento.
    // Si disegnano DUE sagome affiancate — l'unità di oggi e il gigante da 2,40 ×
    // 1,20 su cui sono dimensionate le metriche — così si vede subito se una porta
    // o un corridoio reggono anche il caso peggiore.
    if (m_showScaleFigure)
    {
        const glm::vec3 fp = { m_scaleFigX, m_scaleFigY, m_scaleFigZ };
        auto figure = [&](float dx, float w, float h, float r, float g, float b) {
            FreeCameraViewport::MapBoxDraw f;
            f.x = fp.x + dx; f.z = fp.z; f.ry = 0.0f;
            f.sy = h;  f.y = fp.y + h * 0.5f;
            f.sx = w;  f.sz = w * 0.6f;
            f.r = r; f.g = g; f.b = b;
            f.selected = false;
            f.pickId = FreeCameraViewport::MapBoxDraw::kNoPick;
            draws.push_back(f);
        };
        figure(-0.9f, 0.80f, 2.00f, 0.30f, 0.85f, 0.45f);   // clone/B1: ~2,0 m
        figure( 0.9f, mini::mapmetrics::REF_UNIT_WIDTH,
                      mini::mapmetrics::REF_UNIT_HEIGHT, 0.95f, 0.65f, 0.25f);  // gigante
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

        // I BOX inclusi non hanno più un rombo sopra: sono colorati di ciano nel
        // ciclo principale (vedi sopra). Restano i marker per le POSIZIONI TATTICHE,
        // che non hanno un volume da colorare — sono punti.
        const std::vector<int>& selPos = zonePositions;
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
    // SELEZIONE MULTIPLA (G3): il gizmo si mette al BARICENTRO del gruppo, che è
    // il punto attorno a cui la rotazione fa orbitare gli elementi. Scala
    // disattivata: su un gruppo eterogeneo non ha un significato unico.
    if (m_multiSel.size() > 1)
    {
        glm::vec3 c(0.0f); int n = 0;
        for (int code : m_multiSel)
        { glm::vec3 p; if (codePosition(code, p)) { c += p; ++n; } }
        if (n > 0)
        {
            m_viewport.setGizmoTarget(c / (float)n, true);
            m_viewport.setGizmoRotAxes(false, true, false);
            m_viewport.setGizmoCanRotateScale(true, false);
        }
        else m_viewport.setGizmoTarget({0,0,0}, false);
    }
    // Struttura selezionata: il gizmo agisce sulla RICETTA (ADR-053). Sposta,
    // ruota e scala sono tutti e tre attivi — la scala però tocca i PARAMETRI del
    // tipo, non un box, così i gradini non si possono rompere.
    else if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
    {
        const auto& s = m_structures[m_selStruct];
        m_viewport.setGizmoTarget({s.x, s.y, s.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);
        m_viewport.setGizmoCanRotateScale(true, true);
    }
    else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
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
    // ── Aggancio unico dell'undo per TUTTE le modifiche da widget ─────────
    // Invece di infilare un `pushUndo` accanto a ogni DragFloat (decine di punti,
    // e il primo che si dimentica è un'operazione non annullabile), si osserva
    // quando un widget diventa attivo: si fotografa lo stato PRIMA che l'utente
    // cominci a trascinare, e lo si consegna alla pila solo se qualcosa è davvero
    // cambiato. Un intero trascinamento diventa una sola voce di undo.
    {
        static bool s_wasActive = false;
        static Snapshot s_before;
        static bool s_dirtyBefore = false;
        const bool active = ImGui::IsAnyItemActive();
        if (active && !s_wasActive)
        {
            s_before      = captureState();
            s_dirtyBefore = m_dirty;
        }
        else if (!active && s_wasActive && m_dirty && !s_dirtyBefore)
        {
            m_undo.push_back(s_before);
            if (m_undo.size() > kUndoDepth) m_undo.erase(m_undo.begin());
            m_redo.clear();
        }
        s_wasActive = active;
    }

    // Scorciatoie: Ctrl+Z / Ctrl+Y (e Ctrl+Shift+Z, che molti si aspettano).
    if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (ImGui::GetIO().KeyShift) doRedo(); else doUndo();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) doRedo();
        // Ctrl+A: INTERRUTTORE seleziona-tutto / deseleziona-tutto.
        // Un tasto solo per i due versi dello stesso gesto — Esc da solo non è
        // affidabile qui (Windows se lo prende in certe combinazioni), e un pulsante
        // "Deseleziona" separato aggiungeva un secondo modo di fare la stessa cosa.
        else if (ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            const size_t total = m_boxes.size() + m_structures.size();
            if (m_multiSel.size() >= total && total > 0)
            {
                setSelection(-1, false);   // già tutto selezionato → si azzera
            }
            else
            {
                m_multiSel.clear();
                for (int i = 0; i < (int)m_boxes.size(); ++i) m_multiSel.push_back(i);
                for (int i = 0; i < (int)m_structures.size(); ++i) m_multiSel.push_back(-6000 - i);
                if (!m_multiSel.empty())
                {
                    const int last = m_multiSel.back();
                    m_selBox    = last >= 0 ? last : -1;
                    m_selStruct = last <= -6000 ? -6000 - last : -1;
                }
                updateViewport();
            }
        }
    }

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
    // ── Annulla / Ripristina (doc 47 E1) ──────────────────────────────────
    ImGui::BeginDisabled(m_undo.empty());
    if (ImGui::Button("Annulla")) doUndo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+Z — annulla l'ultima modifica (%d in memoria)",
                          (int)m_undo.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redo.empty());
    if (ImGui::Button("Ripristina")) doRedo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+Y o Ctrl+Shift+Z (%d in memoria)", (int)m_redo.size());
    ImGui::SameLine();

    if (ImGui::Button("+ Box"))         addBox();
    ImGui::SameLine();
    // ── Primitive parametriche (ADR-053) ──────────────────────────────────
    // In un DROPDOWN e non in fila: la barra ha già otto controlli e aggiungerne
    // quattro la farebbe tagliare — regola d'uso dell'editor confermata dall'utente.
    if (ImGui::Button("+ Struttura")) ImGui::OpenPopup("##addstruct");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Forme che si autorano come RICETTA, non come box.\n"
                          "Dichiari \"da qui, salendo di 3 m, larga 4\" e i gradini\n"
                          "li genera il motore rispettando lo scalino massimo:\n"
                          "un'alzata sbagliata diventa impossibile da disegnare.");
    if (ImGui::BeginPopup("##addstruct"))
    {
        // Raggruppate per COSA SERVONO, non per ordine di implementazione: si cerca
        // "come salgo" o "come chiudo uno spazio", non "la quarta primitiva".
        ImGui::TextDisabled("Salire");
        if (ImGui::MenuItem("Scala"))  addStructure(mini::StructureKind::Stair);
        if (ImGui::MenuItem("Rampa"))  addStructure(mini::StructureKind::Ramp);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Alzata fine: dolce, adatta anche ai veicoli.");

        ImGui::Separator();
        ImGui::TextDisabled("Chiudere e aprire");
        if (ImGui::MenuItem("Muro"))   addStructure(mini::StructureKind::Wall);
        if (ImGui::MenuItem("Muro con apertura"))
            addStructure(mini::StructureKind::Doorway);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Porta o finestra. Stipiti e architrave li mette lui,\n"
                              "con le misure normative. Con il parapetto (sill) diventa\n"
                              "una finestra, e il parapetto e' COPERTURA vera.");
        if (ImGui::MenuItem("Stanza (guscio)"))
            addStructure(mini::StructureKind::Room);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pavimento + 4 muri + soffitto opzionale, con una porta\n"
                              "per ogni lato dichiarato: un interno non nasce senza\n"
                              "vie d'ingresso.");

        ImGui::Separator();
        ImGui::TextDisabled("Terreno tattico");
        if (ImGui::MenuItem("Piattaforma con accessi"))
            addStructure(mini::StructureKind::Platform);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dichiara da quali lati ci si sale: le scale nascono\n"
                              "con lei. Non puo' venire irraggiungibile.");
        if (ImGui::MenuItem("Passerella"))
            addStructure(mini::StructureKind::Catwalk);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Un corridoio IN QUOTA: domina il piano di sotto.\n"
                              "Parapetti opzionali — riparano, ma tolgono la visuale\n"
                              "verso il basso (KI #83).");
        if (ImGui::MenuItem("Linea di coperture"))
            addStructure(mini::StructureKind::Barricade);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Barricata a intervalli. Emette box di tipo `cover`,\n"
                              "che e' cio' che la derivazione dei metadata cerca.");
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplica")) duplicateSelected();
    ImGui::SameLine();
    // ── ARRAY (E4) ────────────────────────────────────────────────────────
    // In un popup e non in fila: la barra è già affollata, e questi sono quattro
    // campi che si usano insieme una volta ogni tanto.
    if (ImGui::Button("Serie...")) ImGui::OpenPopup("##array");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("N copie con offset PROGRESSIVO: la copia i-esima sta a\n"
                          "i × offset dall'originale, quindi la fila resta allineata\n"
                          "invece di accumulare l'errore di N trascinamenti a mano.\n"
                          "Agisce su tutta la selezione.");
    if (ImGui::BeginPopup("##array"))
    {
        const int n = (int)selectionCodes().size();
        if (n == 0) ImGui::TextDisabled("Seleziona qualcosa prima.");
        else
        {
            ImGui::TextDisabled("%d element%s selezionat%s", n, n == 1 ? "o" : "i",
                                n == 1 ? "o" : "i");
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragInt("Copie", &m_arrayCount, 0.2f, 1, 200);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat3("Offset", m_arrayOff, 0.1f);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat("Rotazione", &m_arrayYawStep, 1.0f, -180.0f, 180.0f, "%.0f°/copia");
            ImGui::TextDisabled("Totale: %d nuovi elementi", m_arrayCount * n);
            ImGui::Separator();
            if (ImGui::Button("Crea", {110, 0})) { makeArray(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // ── Stato di salute SEMPRE VISIBILE (G7) ──────────────────────────────
    // Il conteggio stava solo dentro un pannello che bisognava aprire. Qui è
    // sott'occhio mentre si costruisce: se un'operazione introduce un problema, il
    // numero sale nello stesso istante.
    {
        int nProb = 0, nWarn = 0;
        for (const auto& is : m_issues) { if (is.sev >= 1) ++nProb; else ++nWarn; }
        ImGui::SameLine();
        if (nProb > 0)
            ImGui::TextColored({0.95f, 0.35f, 0.30f, 1.0f}, "%d problemi", nProb);
        else if (nWarn > 0)
            ImGui::TextColored({0.90f, 0.75f, 0.35f, 1.0f}, "%d avvisi", nWarn);
        else
            ImGui::TextColored({0.45f, 0.85f, 0.50f, 1.0f}, "nessun difetto");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Salute tattica della mappa, ricalcolata a ogni modifica.\n"
                              "%d problemi, %d avvisi. I box colpevoli sono colorati nel\n"
                              "viewport; l'elenco completo è nel pannello a sinistra.",
                              nProb, nWarn);
    }

    ImGui::SameLine();
    // ── VISTA: filtri + figura di scala (E5/E6) ───────────────────────────
    const bool filtered = filtersActive();
    if (filtered) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.35f, 1.0f));
    if (ImGui::Button(filtered ? "Vista *" : "Vista")) ImGui::OpenPopup("##vista");
    if (filtered) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Nasconde per tipo o per quota. Solo visivo: non tocca i dati.\n"
                          "L'asterisco ricorda che qualcosa è nascosto.");
    if (ImGui::BeginPopup("##vista"))
    {
        // Ogni cambio ridisegna SUBITO. Prima nulla richiamava il ridisegno, quindi
        // le spunte cambiavano lo stato e non si vedeva nulla finché non si toccava
        // altro: il filtro sembrava non funzionare. `false` = non ricalcolare i dati
        // derivati, è solo vista.
        bool viewChanged = false;
        const char* tn[5] = { "Pavimenti", "Muri", "Piattaforme", "Coperture", "Decorazioni" };
        for (int i = 0; i < 5; ++i)
            if (ImGui::Checkbox(tn[i], &m_showType[i])) viewChanged = true;
        if (ImGui::Checkbox("Strutture (scale, rampe...)", &m_showStructures))
            viewChanged = true;
        ImGui::Separator();

        // Estensione VERA della mappa in quota: uno slider da -5 a 1000 sarebbe
        // inutilizzabile, e senza estremi sensati non si trova la quota giusta.
        float mapLo = 0.0f, mapHi = 8.0f;
        for (const auto& b : m_boxes)
        {
            mapLo = std::min(mapLo, b.y - b.sy * 0.5f);
            mapHi = std::max(mapHi, b.y + b.sy * 0.5f);
        }
        mapHi += 1.0f;
        const bool cutting = (m_hideAboveY < mapHi);
        float cut = cutting ? m_hideAboveY : mapHi;

        ImGui::SetNextItemWidth(200.0f);
        // Slider per trovare la quota trascinando (il taglio si vede mentre si
        // muove), Ctrl+click per digitarla precisa: ImGui dà entrambe le cose con
        // un controllo solo.
        if (ImGui::SliderFloat("Taglia sopra", &cut, mapLo, mapHi, "%.2f m"))
        { m_hideAboveY = (cut >= mapHi - 0.001f) ? 1000.0f : cut; viewChanged = true; }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("VISTA IN SEZIONE: sopra questa quota la geometria sparisce,\n"
                              "e ciò che sta a cavallo viene tagliato al piano. È il modo\n"
                              "di guardare dentro un edificio dall'alto.\n"
                              "Ctrl+click per digitare un valore esatto.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Tutto##nocut")) { m_hideAboveY = 1000.0f; viewChanged = true; }
        if (cutting)
            ImGui::TextDisabled("taglio a %.2f m (mappa %.1f → %.1f)", m_hideAboveY, mapLo, mapHi - 1.0f);
        else
            ImGui::TextDisabled("nessun taglio");
        ImGui::Separator();
        if (ImGui::Checkbox("Evidenzia i difetti", &m_showDefects)) viewChanged = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Colora nel viewport i box con difetti: ROSSO = problema\n"
                              "(il navmesh non ci sale, nessuno ci arriva), AMBRA = avviso.\n"
                              "Stessa analisi del gate --validate, mostrata mentre costruisci.");
        if (ImGui::Checkbox("Figura di scala", &m_showScaleFigure))
        {
            if (m_showScaleFigure)
            {
                const glm::vec3 fp = m_viewport.groundFocusPoint();
                m_scaleFigX = fp.x; m_scaleFigY = fp.y; m_scaleFigZ = fp.z;
            }
            viewChanged = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Due sagome dove stai guardando ORA: l'unità di oggi\n"
                              "(2,0 m) e il gigante di riferimento (2,40 × 1,20) su cui\n"
                              "sono dimensionate le metriche. Restano dove le piazzi.");
        if (m_showScaleFigure && ImGui::SmallButton("Riposiziona qui"))
        {
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            m_scaleFigX = fp.x; m_scaleFigY = fp.y; m_scaleFigZ = fp.z;
            viewChanged = true;
        }
        ImGui::Separator();
        if (ImGui::Button("Mostra tutto", {150, 0}))
        {
            for (bool& b : m_showType) b = true;
            m_showStructures = true;
            m_hideAboveY = 1000.0f;
            viewChanged = true;
        }
        if (viewChanged) updateViewport(/*recomputeDerived=*/false);
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Duplica l'elemento selezionato (box, posizione, settore,\n"
                          "pericolo, bersaglio, post, percorso, veicolo) con TUTTI i\n"
                          "suoi valori. Autora una volta, posane una serie.");
    ImGui::SameLine();
    // Elimina TUTTA la selezione (G3), non solo il box primario.
    if (ImGui::Button("Elimina") && !selectionCodes().empty()) {
        ImGui::OpenPopup("##del_confirm");
    }
    if (ImGui::IsItemHovered() && selectionCodes().size() > 1)
        ImGui::SetTooltip("Elimina i %d elementi selezionati.",
                          (int)selectionCodes().size());

    // Indicatore della selezione multipla: senza, non si sa quanti elementi si sta
    // per spostare o eliminare finché non è troppo tardi.
    if (m_multiSel.size() > 1)
    {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.78f, 0.35f, 1.0f},
                           "%d selezionati", (int)m_multiSel.size());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ctrl+click aggiunge o toglie un elemento.\n"
                              "Ctrl+A seleziona tutto, e ripremuto deseleziona.");
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
        const int n = (int)selectionCodes().size();
        if (n > 1) ImGui::Text("Eliminare i %d elementi selezionati?", n);
        else       ImGui::Text("Eliminare l'elemento selezionato?");
        if (ImGui::Button("Sì")) { deleteSelection(); ImGui::CloseCurrentPopup(); }
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

    // ── Le STRUTTURE per prime (ADR-053) ──────────────────────────────────
    // Stanno sopra i box perché sono l'unità di lavoro: una scala da 15 gradini è
    // UNA riga qui, non quindici nella lista dei box.
    if (!m_structures.empty())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        char shdr[64];
        std::snprintf(shdr, sizeof(shdr), "Strutture (%d)###structs", (int)m_structures.size());
        if (ImGui::CollapsingHeader(shdr))
        {
            for (int i = 0; i < (int)m_structures.size(); ++i)
            {
                const auto& s = m_structures[i];
                char sb[160];
                const char* nm = s.label.empty() ? "(senza nome)" : s.label.c_str();
                // Il conto dei box generati è l'informazione che serve davvero:
                // dice quanto costa quella riga.
                std::vector<mini::MapGeometryBox> tmpb;
                mini::mapstructures::expand(s, tmpb);
                std::snprintf(sb, sizeof(sb), "%s  [%s, %d box]##st%d",
                              nm, mini::mapstructures::kindName(s.kind), (int)tmpb.size(), i);
                const int code = -6000 - i;
                const bool ssel = (m_selStruct == i)
                    || std::find(m_multiSel.begin(), m_multiSel.end(), code) != m_multiSel.end();
                if (ImGui::Selectable(sb, ssel))
                    setSelection(code, ImGui::GetIO().KeyCtrl);
            }
        }
    }

    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        char buf[128];
        const char* name = (b.label[0] != '\0') ? b.label : "(nessun nome)";
        std::snprintf(buf, sizeof(buf), "%s %s##box%d", typeIcon(b.type), name, i);

        const bool sel = (i == m_selBox)
            || std::find(m_multiSel.begin(), m_multiSel.end(), i) != m_multiSel.end();
        if (ImGui::Selectable(buf, sel))
            setSelection(i, ImGui::GetIO().KeyCtrl);
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

        // ── ELIMINA L'ASSET (non l'istanza) ───────────────────────────────
        // Su una riga PROPRIA sotto il menu a tendina, non stretto fra "+" e "-":
        // quelli aggiungono e tolgono un'ISTANZA in questa mappa, questo cancella
        // l'asset dal disco. Metterli in fila accostava due gesti con conseguenze
        // molto diverse. Agisce sul prefab scelto nel menu qui sopra.
        if (!m_prefabIds.empty() && !m_prefabIds[m_prefabPick].empty())
        {
            if (ImGui::SmallButton("Elimina asset...")) ImGui::OpenPopup("##delprefab");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Cancella data/prefabs/%s.json dal disco.",
                                  m_prefabIds[m_prefabPick].c_str());
            if (ImGui::BeginPopup("##delprefab"))
            {
                const std::string pid = m_prefabIds[m_prefabPick];
                int used = 0;
                for (const auto& pi : m_prefabInsts) if (pi.id == pid) ++used;
                ImGui::Text("Eliminare l'asset '%s'?", pid.c_str());
                if (used > 0)
                    ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                       "%d istanze in questa mappa resterebbero rotte.", used);
                else
                    ImGui::TextDisabled("Non è usato in questa mappa.");
                ImGui::Separator();
                if (ImGui::Button("Elimina", {110, 0}))
                {
                    std::error_code ec;
                    fs::remove(fs::path(getDataDir()) / "prefabs" / (pid + ".json"), ec);
                    // Ricarica AUTOMATICA: dal 2026-08-05 `loadPrefabs` azzera il
                    // proprio contenitore, quindi rileggere il disco è autoritativo
                    // e il prefab sparisce subito dal menu — niente riavvio, e
                    // nessun pulsante "ricarica" da ricordarsi di premere.
                    reloadPrefabAssets();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
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

    // ── SELEZIONE MULTIPLA (G3) ───────────────────────────────────────────
    // Non si mostrano i campi di un elemento solo: sarebbe ambiguo su quale dei
    // dieci si sta agendo. Si mostra COSA c'è dentro e cosa si può farci.
    if (m_multiSel.size() > 1)
    {
        int nBox = 0, nStruct = 0, nAltro = 0;
        for (int c : m_multiSel)
        {
            if      (c >= 0)      ++nBox;
            else if (c <= -6000)  ++nStruct;
            else                  ++nAltro;
        }
        ImGui::TextColored({1.0f, 0.78f, 0.35f, 1.0f},
                           "%d elementi selezionati", (int)m_multiSel.size());
        ImGui::Separator();
        if (nBox)    ImGui::BulletText("%d box", nBox);
        if (nStruct) ImGui::BulletText("%d strutture", nStruct);
        if (nAltro)  ImGui::BulletText("%d altri elementi", nAltro);
        ImGui::Separator();
        ImGui::TextDisabled("Sposta e ruota agiscono su TUTTI (la rotazione fa\n"
                            "orbitare attorno al baricentro). Elimina e Duplica\n"
                            "pure. La SCALA resta al singolo: su un gruppo misto\n"
                            "un raggio e un'altezza non si scalano allo stesso modo.");

        // ── RIGHELLO (doc 47 E6) ──────────────────────────────────────────
        // Con ESATTAMENTE due elementi selezionati si misura la distanza fra loro.
        // Non serve uno strumento a parte: la selezione multipla è già il gesto
        // giusto, e il confronto con le metriche normative dice subito se un
        // corridoio o una porta stanno nei limiti.
        if (m_multiSel.size() == 2)
        {
            glm::vec3 a, b;
            if (codePosition(m_multiSel[0], a) && codePosition(m_multiSel[1], b))
            {
                const glm::vec3 d = b - a;
                const float horiz = std::sqrt(d.x * d.x + d.z * d.z);
                ImGui::Separator();
                ImGui::TextColored({0.55f, 0.85f, 1.0f, 1.0f}, "Distanza");
                ImGui::Text("%.2f m in pianta, %.2f m totali", horiz, glm::length(d));
                ImGui::TextDisabled("ΔX %.2f   ΔY %.2f   ΔZ %.2f", d.x, d.y, d.z);
                if (std::fabs(d.y) > 0.01f)
                {
                    const float rise = std::fabs(d.y);
                    if (rise > mini::config::STEP_HEIGHT)
                        ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                            "Dislivello %.2f m: oltre lo scalino (%.2f). Servono %d gradini.",
                            rise, mini::config::STEP_HEIGHT,
                            mini::mapmetrics::stepsFor(rise));
                }
            }
        }
        return;
    }

    // ── PRIMITIVA PARAMETRICA selezionata (ADR-053) ───────────────────────
    if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
    {
        auto& s = m_structures[m_selStruct];
        bool changed = false;
        using SK = mini::StructureKind;
        const bool isFlight = (s.kind == SK::Stair || s.kind == SK::Ramp
                            || s.kind == SK::Switchback);
        const bool isWallish = (s.kind == SK::Wall || s.kind == SK::Doorway);
        const bool hasOpening = (s.kind == SK::Doorway || s.kind == SK::Room);

        ImGui::TextColored({0.55f, 0.80f, 1.00f, 1.0f}, "Struttura parametrica");
        ImGui::TextDisabled("Si autora la RICETTA. I box li genera il motore, e non\n"
                            "si salvano: cambiano da soli se cambi i parametri.");
        ImGui::Separator();

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.label.c_str());
        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf)))
        { s.label = nameBuf; changed = true; }

        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Tipo", mini::mapstructures::kindLabel(s.kind)))
        {
            // Switchback assente di proposito: vedi la nota in MapStructures.hpp —
            // non produce ancora torri percorribili in modo affidabile.
            const mini::StructureKind kinds[8] = {
                mini::StructureKind::Stair,      mini::StructureKind::Ramp,
                mini::StructureKind::Wall,       mini::StructureKind::Doorway,
                mini::StructureKind::Room,       mini::StructureKind::Platform,
                mini::StructureKind::Catwalk,    mini::StructureKind::Barricade };
            for (auto k : kinds)
                if (ImGui::Selectable(mini::mapstructures::kindLabel(k), s.kind == k))
                { s.kind = k; changed = true; }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Posizione");
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("X##st", &s.x, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Y##st", &s.y, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Z##st", &s.z, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Rotazione##st", &s.ry, 1.0f, -360.0f, 360.0f, "%.0f°"))
            changed = true;

        ImGui::Separator();
        if (isFlight)
        {
            ImGui::TextDisabled("Salita");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Dislivello##st", &s.rise, 0.1f, 0.1f, 40.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Larghezza##st", &s.width, 0.1f, 0.5f, 20.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Alzata##st", &s.riser, 0.01f, 0.0f,
                                 mini::config::STEP_HEIGHT, "%.2f m (0 = normativa)"))
                changed = true;
            // La PEDATA allunga o accorcia la scala senza toccare i gradini: il
            // numero di gradini dipende solo dal dislivello, quindi questa leva
            // cambia quanto spazio occupa e quanto è dolce, e non può romperla.
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Pedata##st", &s.tread, 0.01f, 0.0f, 3.0f,
                                 "%.2f m (0 = normativa)"))
                changed = true;
            // I numeri che contano per progettare.
            const float run  = mini::mapstructures::flightRun(s);
            const float used = mini::mapstructures::effectiveRiser(s);
            const float trd  = mini::mapstructures::effectiveTread(s);
            const float slope = (trd > 0.001f)
                ? std::atan(used / trd) * 57.2957795f : 0.0f;
            ImGui::TextDisabled("%d gradini da %.2f × %.2f m — pendenza %.0f°",
                                mini::mapmetrics::stepsFor(s.rise, used), used, trd, slope);
            if (s.kind != SK::Switchback)
                ImGui::TextDisabled("occupa %.2f m in pianta", run);
            if (slope > 35.5f)
                ImGui::TextColored({1.0f, 0.80f, 0.40f, 1.0f},
                                   "Piu' ripida di una scala vera (30-35°): allunga la pedata.");
            if (s.kind == SK::Switchback)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Dislivello per rampa##st", &s.flightRise, 0.1f, 0.0f, 10.0f,
                                     "%.2f m (0 = 3,00, un piano)")) changed = true;
                // I numeri che servono a progettare la torre: quante rampe e, soprattutto,
                // che l'INGOMBRO NON CAMBIA con l'altezza — è il punto di questa primitiva.
                const float maxF = (s.flightRise > 0.1f) ? s.flightRise : 3.0f;
                int nf = (int)std::ceil(s.rise / maxF); if (nf < 2) nf = 2;
                const float rF = s.rise / (float)nf;
                const float R  = (float)mini::mapmetrics::stepsFor(rF, used) * trd;
                const float wid = (s.width > 0.1f) ? s.width : mini::mapmetrics::STAIR_MIN_WIDTH;
                ImGui::TextDisabled("%d rampe da %.2f m — pianta %.1f × %.1f m",
                                    nf, rF, wid * 2.0f, R + wid);
                const float straight = (float)mini::mapmetrics::stepsFor(s.rise, used) * trd;
                ImGui::TextDisabled("diritta occuperebbe %.1f m di sviluppo", straight);
            }
            if (s.riser > mini::config::STEP_HEIGHT - 0.001f && s.riser > 0.0f)
                ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                                   "Alzata limitata a %.2f: oltre non ci si sale.",
                                   mini::config::STEP_HEIGHT);
        }
        else if (isWallish || s.kind == SK::Catwalk || s.kind == SK::Barricade)
        {
            ImGui::TextDisabled("%s", mini::mapstructures::kindLabel(s.kind));
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lunghezza##st", &s.length, 0.1f, 0.5f, 200.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##st", &s.height, 0.1f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            if (s.kind == SK::Catwalk)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Larghezza##st", &s.width, 0.1f, 0.5f, 20.0f, "%.2f m"))
                    changed = true;
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Quota impalcato##st", &s.y, 0.1f, 0.0f, 40.0f, "%.2f m"))
                    changed = true;
                if (ImGui::Checkbox("Parapetti", &s.railing)) changed = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Riparano chi ci sta sopra, MA tolgono la visuale\n"
                                      "verso il basso: e' il difetto KI #83 (posizione cieca\n"
                                      "verso le altre quote). Da mettere con criterio.");
                if (s.width < mini::mapmetrics::CORRIDOR_MIN - 0.01f)
                    ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                        "Sotto il corridoio minimo (%.2f m).", mini::mapmetrics::CORRIDOR_MIN);
            }
            else if (s.kind == SK::Barricade)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Lungh. elemento##st", &s.width, 0.1f, 0.3f, 20.0f, "%.2f m"))
                    changed = true;
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Varco##st", &s.spacing, 0.1f, 0.0f, 20.0f,
                                     "%.2f m (0 = continua)")) changed = true;
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Spessore##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                     "%.2f m (0 = 0,60)")) changed = true;
                ImGui::TextDisabled("Bassa %.2f = ci si spara sopra · alta %.2f = ripara in piedi",
                                    mini::mapmetrics::COVER_LOW, mini::mapmetrics::COVER_HIGH);
            }
            else
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Spessore##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                     "%.2f m (0 = normativo)")) changed = true;
            }
        }
        else if (s.kind == SK::Room)
        {
            ImGui::TextDisabled("Stanza");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato X##st", &s.sizeX, 0.1f, 2.0f, 80.0f, "%.2f m")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato Z##st", &s.sizeZ, 0.1f, 2.0f, 80.0f, "%.2f m")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##st", &s.height, 0.1f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Spessore muri##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                 "%.2f m (0 = normativo)")) changed = true;
            if (ImGui::Checkbox("Soffitto", &s.ceiling)) changed = true;
            ImGui::Separator();
            ImGui::TextDisabled("Aperture — su quali lati");
            const char* sideName[4] = { "Lato -Z", "Lato +Z", "Lato -X", "Lato +X" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::Checkbox(sideName[i], &s.access[i])) changed = true;
            if (!s.access[0] && !s.access[1] && !s.access[2] && !s.access[3])
                ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                   "Nessuna apertura: stanza sigillata.");
            const float lh = (s.height > 0.01f) ? s.height : mini::mapmetrics::WALL_HEIGHT;
            if (lh < mini::mapmetrics::CEILING_MIN - 0.01f)
                ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                    "Altezza sotto il minimo al coperto (%.2f m): il gigante non ci sta.",
                    mini::mapmetrics::CEILING_MIN);
        }
        else
        {
            ImGui::TextDisabled("Piattaforma");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Quota ripiano##st", &s.y, 0.1f, 0.0f, 40.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato X##st", &s.sizeX, 0.1f, 1.0f, 60.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato Z##st", &s.sizeZ, 0.1f, 1.0f, 60.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Quota di partenza##st", &s.baseY, 0.1f, -10.0f, 40.0f, "%.2f m"))
                changed = true;

            ImGui::Separator();
            ImGui::TextDisabled("Accessi — da quali lati ci si sale");
            const char* sideName[4] = { "Lato -Z", "Lato +Z", "Lato -X", "Lato +X" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::Checkbox(sideName[i], &s.access[i])) changed = true;
            if (!s.access[0] && !s.access[1] && !s.access[2] && !s.access[3]
                && (s.y - s.baseY) > mini::config::STEP_HEIGHT)
                ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                   "Nessun accesso: ci si sale solo volando.");
        }

        // ── APERTURA: comune a "muro con apertura" e alle porte della stanza ──
        if (hasOpening)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Apertura");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Larghezza##op", &s.openW, 0.05f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##op", &s.openH, 0.05f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Parapetto##op", &s.openSill, 0.05f, 0.0f, 5.0f,
                                 "%.2f m (0 = porta)")) changed = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Sopra zero diventa una FINESTRA, e il parapetto sotto\n"
                                  "e' copertura vera: ci si ripara dietro e si spara sopra.\n"
                                  "Bassa %.2f · alta %.2f",
                                  mini::mapmetrics::COVER_LOW, mini::mapmetrics::COVER_HIGH);
            if (s.kind == SK::Doorway)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Scostamento##op", &s.openOff, 0.05f, -50.0f, 50.0f, "%.2f m"))
                    changed = true;
            }
            const float ew = (s.openW > 0.01f) ? s.openW : mini::mapmetrics::DOOR_WIDTH;
            const float eh = (s.openH > 0.01f) ? s.openH : mini::mapmetrics::DOOR_HEIGHT;
            if (s.openSill <= 0.01f)
            {
                if (ew < mini::mapmetrics::DOOR_WIDTH - 0.01f)
                    ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                        "Piu' stretta della porta normativa (%.2f m).", mini::mapmetrics::DOOR_WIDTH);
                if (eh < mini::mapmetrics::REF_UNIT_HEIGHT + 0.01f)
                    ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                        "Il gigante (%.2f m) non ci passa.", mini::mapmetrics::REF_UNIT_HEIGHT);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Elimina struttura"))
        {
            m_structures.erase(m_structures.begin() + m_selStruct);
            m_selStruct = -1;
            changed = true;
        }

        if (changed)
        {
            m_dirty = true;
            rebuildStructurePreview();
            updateViewport();
        }
        return;
    }

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
            // Ctrl+click AGGIUNGE/TOGLIE dalla selezione (G3), click normale
            // sostituisce. Fuori dalla modalità prefab, dove Ctrl ha già un altro
            // significato — per questo il ramo `refine` viene prima.
            setSelection(picked, ImGui::GetIO().KeyCtrl);
        }
    }
    (void)vpW; (void)vpH;
}

} // namespace editor

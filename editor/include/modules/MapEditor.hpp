#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include <string>
#include <vector>
#include <array>

namespace editor
{

// ── MapEditor ─────────────────────────────────────────────────────────────────
// Modulo per modificare la geometria delle mappe (box collider/rendering,
// spawn point, anteprima area navigabile) e salvarla in JSON.
class MapEditor
{
public:
    MapEditor();
    ~MapEditor() = default;

    void tick(float dt);
    void draw();
    // Rilascio cattura mouse al cambio modulo (FreeCameraViewport::releaseMouseCapture).
    void releaseMouseCapture() { m_viewport.releaseMouseCapture(); }

private:
    // ── Working data ─────────────────────────────────────────────────────
    struct BoxEntry {
        float x=0, y=0, z=0, ry=0;
        float sx=2, sy=2, sz=2;
        float r=0.35f, g=0.32f, b=0.28f;
        char  type[16]  = "wall";
        char  label[64] = "";
        bool  isCollider = true;
    };

    std::string m_mapId;        // id mappa corrente (= filename stem, ADR-001)
    std::string m_mapJsonPath;  // percorso file JSON
    char m_mapDisplayName[64] = "";  // campo `name`: nome MOSTRATO in partita/sandbox

    // Command post (ADR-009): autorati qui, letti dal runtime via MapDef.
    struct PostEntry {
        char  label[64] = "Post";
        float x=0, y=0, z=0;
        float radius = 4.0f;
        int   team   = 0;       // 0 neutrale, 1 alleati, 2 nemici
        float captureTime = 8.0f;
    };

    // ── Map Metadata (15_MapMetadata): hint spaziali per l'AI ────────────
    // Posizione tattica unificata (ADR-030): sostituisce CoverEntry+TacticalEntry.
    struct PositionEntry { float x=0, y=0.5f, z=0; float facing=0;
                           std::string role="cover"; float height=1.0f;
                           float protection=0.5f; bool canShoot=true;
                           float importance=0.5f; float radius=4.0f;
                           float fireArc=120.0f; float fireRange=25.0f; };   // ADR-031
    struct DangerEntry { float x=0, y=0,    z=0; float radius=4.0f; float level=0.5f; };
    struct RouteEntry  { char id[32] = "route";
                         std::vector<std::array<float,3>> points; };

    // Spawn veicoli (19_Vehicles Fase B: authoring in editor)
    struct VehicleSpawnEntry { std::string vehicleId; float x=0, z=0, ry=0; };

    // Bersaglio strategico distruttibile (doc 25, DestroyTarget)
    struct TargetEntry { char label[64] = "Bersaglio"; float x=0, z=0; float hp=300.0f;
                         float y=0.0f;                                   // altezza sopra il suolo (0 = a terra)
                         float ry=0.0f; int team=2; float scale=1.0f;    // autorabili
                         float halfX=0.0f, halfY=0.0f, halfZ=0.0f;       // 0 = dalla scala
                         // doc 34/36: 0 generico, 1 comunicazioni, 2 controllo
                         int   role=0;
                         float priority=0.5f;      // doc 35: quanto vale distruggerla
                         float engageRadius=0.0f; };  // 0 = mai ingaggiata di iniziativa

    std::vector<BoxEntry>     m_boxes;
    std::vector<PostEntry>    m_posts;
    std::vector<PositionEntry> m_positions;   // ADR-030 (ex m_covers + m_tacticals)
    std::vector<float> m_exposure;            // ADR-033: derivata, parallela a m_positions
    // Settori / Combat Areas (ADR-034): autorati, pochi, scelte di design.
    struct SectorEntry { std::string label="Settore"; float x=0, z=0;
                         float radius=12.0f; float importance=0.5f; };
    std::vector<SectorEntry> m_sectors;
    std::vector<DangerEntry>  m_dangers;
    std::vector<RouteEntry>   m_routes;
    std::vector<VehicleSpawnEntry> m_vehSpawns;
    std::vector<TargetEntry>  m_targets;
    std::vector<std::string>       m_vehicleIds;   // dal registry (combo)
    std::vector<std::string>       m_commanderIds; // CommanderDef per il combo comandante (ADR-044)

    // Comandante strategico (ADR-024/041): uno per mappa. NON è nel roster —
    // vive nel campo `commander` del MapDef. `leashRadius` 0 = fermo sul posto.
    struct CommanderEntry { bool exists=false; std::string unit;
                            float x=0, z=0; float leashRadius=0.0f; };
    CommanderEntry m_commander;
    int                       m_selRoutePt = 0;   // punto attivo della route sel.
    std::array<float,3>       m_spawnTeam1 = {0.f, 0.86f,  8.f};
    std::array<float,3>       m_spawnTeam2 = {0.f, 0.86f, -8.f};

    // Selezione: >=0 box; -2/-3 spawn T1/T2; -10..-99 command post (-10-i);
    // -100..-199 cover point (-100-i); -200..-299 danger zone (-200-i);
    // -300..-399 patrol route (-300-i, il gizmo muove il punto m_selRoutePt);
    // -400..-499 vehicle spawn (-400-i); -500..-599 bersaglio strategico (-500-i);
    // -1000..-2000 posizione tattica; ≤-2000 settore; -5 comandante (ADR-041)
    static constexpr int kSelCommander = -5;
    int   m_selBox       = -1;   // box selezionato
    float m_gridSnap     = 0.5f; // snap griglia
    bool  m_showNavmesh  = false; // evidenzia floor

    // ── Viewport 3D ──────────────────────────────────────────────────────
    FreeCameraViewport m_viewport;

    // ── Operazioni ───────────────────────────────────────────────────────
    void loadMaps();                          // scansiona data/maps/
    void loadMap(const std::string& id);      // carica dal JSON
    bool saveMap();                           // salva sul JSON

    void addBox();
    void duplicateBox(int idx);
    void duplicateSelected();   // duplica QUALSIASI elemento selezionato (F4, doc 39)
    void deleteBox(int idx);

    void updateViewport();                    // rigenera geometria viewport
    void recomputeExposure();                 // ADR-033: esposizione per posizione
    float snap(float v) const;               // applica grid snap

    // ── Lista mappe disponibili ───────────────────────────────────────────
    struct MapEntry { std::string id; std::string path; };
    std::vector<MapEntry> m_mapList;

    // ── UI panel draw ────────────────────────────────────────────────────
    void drawToolbar();
    void drawBoxList(float panelW, float panelH);
    void drawProperties(float panelW, float panelH);
    void drawViewport(float vpW, float vpH);

    bool m_dirty = false;  // modifiche non salvate
};

} // namespace editor

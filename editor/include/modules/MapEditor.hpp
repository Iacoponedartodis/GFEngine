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

    std::string m_mapId;        // id mappa corrente
    std::string m_mapJsonPath;  // percorso file JSON

    // Command post (ADR-009): autorati qui, letti dal runtime via MapDef.
    struct PostEntry {
        char  label[64] = "Post";
        float x=0, y=0, z=0;
        float radius = 4.0f;
        int   team   = 0;       // 0 neutrale, 1 alleati, 2 nemici
        float captureTime = 8.0f;
    };

    std::vector<BoxEntry>     m_boxes;
    std::vector<PostEntry>    m_posts;
    std::array<float,3>       m_spawnTeam1 = {0.f, 0.86f,  8.f};
    std::array<float,3>       m_spawnTeam2 = {0.f, 0.86f, -8.f};

    // Selezione: >=0 box; -2/-3 spawn T1/T2; <=-10 command post (indice -10-i)
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
    void deleteBox(int idx);

    void updateViewport();                    // rigenera geometria viewport
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

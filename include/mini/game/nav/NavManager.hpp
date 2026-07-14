#pragma once
// NavManager — navigazione basata su Recast/Detour (ADR-017).
//
// Phase A (attuale): costruisce un dtNavMesh dai box di MapDef.geometry al
// caricamento mappa e offre query di path. NON muove ancora l'AI (il movimento
// resta su aiMove) — questa fase de-riska conversione geometria + parametri.
// Phase B: dtCrowd per steering/avoidance, con write-back sui transform.
//
// Header volutamente leggero: i tipi Recast/Detour sono forward-declarati
// (solo puntatori nei membri); gli include pesanti stanno nel .cpp. Solo
// GFEngine linka Recast (ADR-002: l'editor non dipende dal runtime).

#include <glm/glm.hpp>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;
class dtCrowd;

namespace mini
{
struct MapDef;

// Esito della costruzione del navmesh, per la validazione/telemetria.
struct NavBuildStats
{
    bool      ok         = false;
    int       inputTris  = 0;   // triangoli in ingresso (box*12)
    int       polyCount  = 0;   // poligoni del navmesh generato
    int       vertCount  = 0;   // vertici del navmesh
    int       dangerPolys = 0;  // poligoni taggati DANGER (Phase C)
    int       coverPolys  = 0;  // poligoni taggati COVER  (Phase C)
    glm::vec3 bmin{0.0f};
    glm::vec3 bmax{0.0f};
};

class NavManager
{
public:
    NavManager() = default;
    ~NavManager();
    NavManager(const NavManager&)            = delete;
    NavManager& operator=(const NavManager&) = delete;

    // Costruisce il navmesh dai box collider di map.geometry (con rotazione ry).
    // I parametri walkableClimb/Height sono tarati per far scavalcare gli scalini
    // bassi e AGGIRARE muri/coperture alti (ex "AI stuck", 06_Todo). Ricostruisce
    // da zero: chiamabile a ogni caricamento/restart mappa.
    NavBuildStats build(const MapDef& map);
    void          clear();

    [[nodiscard]] bool ready() const { return m_navMesh != nullptr; }

    // Query di path tra due punti mondo. Riempie 'out' con i waypoint del
    // percorso semplificato (straight path). Ritorna false se non c'è path.
    bool findPath(const glm::vec3& start, const glm::vec3& end,
                  std::vector<glm::vec3>& out) const;

    // ── Crowd (Phase B): steering + avoidance + pathfinding per agente ────
    [[nodiscard]] bool crowdReady() const { return m_crowd != nullptr; }
    // Incrementato a ogni build(): il CrowdSystem lo usa per resettare la sua
    // mappa idx→entità quando il crowd viene ricostruito (restart/cambio mappa).
    [[nodiscard]] unsigned generation() const { return m_generation; }

    // Registra un agente alla posizione data. Ritorna l'indice (>=0) o -1.
    int  addAgent(const glm::vec3& pos, float radius, float height, float maxSpeed);
    void removeAgent(int idx);

    // Traversata: path verso 'target' (Detour ci va INTORNO agli ostacoli).
    void requestMoveTarget(int idx, const glm::vec3& target);
    // Combattimento: velocità desiderata diretta (con avoidance del crowd).
    void requestMoveVelocity(int idx, const glm::vec3& vel);

    void updateCrowd(float dt);
    // Posizione corrente dell'agente (dal crowd). false se idx non valido.
    bool agentPos(int idx, glm::vec3& out) const;

private:
    dtNavMesh*      m_navMesh   = nullptr;
    dtNavMeshQuery* m_query     = nullptr;
    dtCrowd*        m_crowd     = nullptr;
    unsigned        m_generation = 0;
};

} // namespace mini

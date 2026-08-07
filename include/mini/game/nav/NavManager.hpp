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

    // `end` è DAVVERO raggiungibile da `start`? (path Detour non-parziale che tocca
    // il poligono destinazione). L'AI la usa per non mandare unità su isole/passaggi
    // erosi dove finirebbero in trappola. Chiamata a bassa frequenza (al commit).
    [[nodiscard]] bool isReachable(const glm::vec3& start, const glm::vec3& end) const;

    // ── Ispezione del navmesh COSTRUITO (doc 47, validazione nell'editor) ──
    // Il navmesh non è una funzione della geometria dichiarata: fra i box e le
    // superfici percorribili ci sono erosione, sfoltimento dei cigli, altezza
    // libera e area minima di regione. Un ripiano perfetto nei dati può non
    // esistere per l'AI — ed è successo davvero (KI #97). Questi due metodi
    // servono a **guardarlo**, che è l'unico modo di accorgersene prima di giocare.
    struct DebugTri
    {
        glm::vec3 a, b, c;
        int component = 0;   // componente connessa: superfici non collegate = isole
    };
    // Triangolazione a ventaglio di tutti i poligoni, con la componente connessa
    // di ciascuno. `outComponentCount` = quante isole separate esistono.
    void debugTriangles(std::vector<DebugTri>& out, int* outComponentCount = nullptr) const;
    // Componente connessa che contiene il punto (o -1 se il punto non è sul navmesh).
    // Con questa, "è raggiungibile da qui?" diventa un confronto fra due interi.
    [[nodiscard]] int componentAt(const glm::vec3& p) const;

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

    // Osservazione (ADR-050): l'agente ha una meta valida? a che velocità va?
    // Servono a distinguere le tre cause di "un'unità è ferma": nessuno le ha
    // chiesto di muoversi, glielo si è chiesto ma non c'è percorso, oppure si
    // sta muovendo e il problema è altrove.
    bool  agentHasTarget(int idx) const;
    float agentSpeed(int idx) const;

    // ── Funnel della NAVIGAZIONE (ADR-050, doc 42 buco O1) ───────────────
    // Prima qui c'erano ZERO eventi: non sapevo quante richieste di movimento
    // fallissero, né perché. Ed è il punto in cui muoiono gli ordini che l'AI
    // crede di aver dato — `requestMoveTarget` può scartare in SILENZIO.
    //
    // `snapTier` è il dato più informativo: a quale tolleranza il bersaglio si è
    // agganciato al navmesh. Livello 0 = era già camminabile. Livello 2 (14 m!)
    // significa che l'AI ha chiesto di andare in un punto lontanissimo da
    // qualunque superficie percorribile — cioè una decisione presa a monte è
    // sbagliata, non è la navigazione a essere lenta. Distinzione che senza
    // questo contatore è indistinguibile da "l'unità è incastrata".
    struct Stats
    {
        int pathQueries = 0;     // findPath() chiamate
        int pathNoPoly  = 0;     // ...fallite: start o fine fuori dal navmesh
        int pathFailed  = 0;     // ...fallite: nessun corridoio trovato
        int moveRequests = 0;    // requestMoveTarget() chiamate
        int moveSnap[3] = {0,0,0};  // agganciate a tolleranza 2 m / 6 m / 14 m
        int moveOffMesh = 0;     // scartate: nulla di camminabile nemmeno a 14 m
        int moveSameTarget = 0;  // ignorate: stesso bersaglio (niente ripianificazione)
    };
    const Stats& stats() const { return m_stats; }
    void resetStats() { m_stats = Stats{}; }

private:
    mutable Stats   m_stats;     // `mutable`: findPath è const ma va contata
    // Ultima destinazione GREZZA chiesta per agente: evita di rifare la ricerca
    // spaziale quando la richiesta è identica (68% dei casi, misurato).
    std::vector<glm::vec3>   m_lastRawTarget;
    std::vector<unsigned char> m_lastRawValid;
    dtNavMesh*      m_navMesh   = nullptr;
    dtNavMeshQuery* m_query     = nullptr;
    dtCrowd*        m_crowd     = nullptr;
    unsigned        m_generation = 0;
};

} // namespace mini

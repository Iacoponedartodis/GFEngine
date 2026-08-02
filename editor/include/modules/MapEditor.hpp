#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"   // prefab per il piazzamento (ADR-048)
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
    // VISUALE VERTICALE (KI #83): quante posizioni a QUOTA DIVERSA vede ciascuna
    // posizione. Derivata come m_exposure (mai salvata, ricalcolata a ogni modifica) e
    // calcolata con la STESSA `hasLineOfFire` del runtime → nessuna doppia verità.
    // Serve perché il combattimento cross-quota è limitato dalla GEOMETRIA, non dall'AI:
    // una posizione elevata "cieca" verso il basso non fa sparare nessuno, e senza
    // questo numero lo si scopriva solo a tentativi in partita.
    std::vector<int> m_vertSight;
    // Denominatore: quante posizioni a quota diversa ESISTONO per quella posizione.
    // "vede 0 su 0" è irrilevante (nessuna quota diversa in giro); "vede 0 su 24" è il
    // difetto da correggere — senza il denominatore i due casi si confondono.
    std::vector<int> m_vertPairs;
    // Copertura dall'alto (doc 41 B3): derivata, parallela a m_positions. È "coperto
    // dall'alto", NON "interno" (un sottopasso conta quanto un bunker): l'interno vero
    // richiede il rilevamento della chiusura, che è un'altra analisi.
    std::vector<int> m_overhead;

    // ── SALUTE TATTICA della mappa (doc 41 B4) ───────────────────────────
    // I controlli tattici esistevano già ma erano SPARSI: bisognava selezionare una
    // posizione per volta per scoprire che era cieca o esposta. Con mappe da centinaia
    // di elementi è impraticabile. Qui vengono aggregati in un elenco unico di DIFETTI,
    // ognuno **cliccabile** (seleziona l'elemento colpevole) → si passa da "ispezionare"
    // a "farsi dire cosa non va". Derivati come tutto il resto: mai salvati.
    struct TacticalIssue {
        int         sel;    // codice di selezione dell'elemento (m_selBox)
        int         sev;    // 0 = avviso, 1 = problema
        int         kind;   // categoria (TacticalDefect::Kind): raggruppa l'elenco
        std::string text;
    };
    std::vector<TacticalIssue> m_issues;
    // Settori / Combat Areas (ADR-034): autorati, pochi, scelte di design.
    struct SectorEntry { std::string label="Settore"; float x=0, z=0;
                         float radius=12.0f; float importance=0.5f; };
    std::vector<SectorEntry> m_sectors;
    std::vector<DangerEntry>  m_dangers;
    std::vector<RouteEntry>   m_routes;
    std::vector<VehicleSpawnEntry> m_vehSpawns;
    std::vector<TargetEntry>  m_targets;
    // ── PREFAB (ADR-048) ─────────────────────────────────────────────────
    // La mappa REFERENZIA i prefab (id + trasformazione); l'espansione in box e
    // posizioni tattiche la fa il motore al load. Qui si piazzano e si spostano; il
    // viewport ne disegna l'anteprima leggendo il prefab dal registry — nessun
    // secondo parser. Selezione: -4000-i.
    struct PrefabInstEntry { std::string id; float x=0, y=0, z=0, ry=0; };
    std::vector<PrefabInstEntry> m_prefabInsts;
    std::vector<std::string>     m_prefabIds;     // per il combo di piazzamento
    mini::DefinitionRegistry     m_prefabReg;     // solo i prefab (loadPrefabs)
    int m_prefabPick = 0;                         // scelta corrente nel combo
    // Creazione prefab DA ZONA: si costruisce il pezzo nella mappa (box + posizioni) e
    // lo si "promuove" ad asset — il flusso standard (in Unity/Unreal: crea in scena →
    // salva come prefab). Alternativa scartata: un modulo-editor separato per i prefab,
    // che avrebbe duplicato mezzo Map Editor per costruire le stesse cose.
    char  m_newPrefabId[64] = "";
    float m_newPrefabRadius = 6.0f;
    bool  m_newPrefabConsume = true;   // togliere dalla mappa gli elementi assorbiti
    // Modalità SELEZIONE per la creazione: mentre è attiva il viewport mostra il raggio
    // e evidenzia ciò che verrà preso, e Shift+click aggiunge/toglie singoli elementi.
    // Un raggio invisibile costringeva a indovinare cosa si stesse selezionando.
    bool  m_prefabZoneMode = false;
    float m_prefabZoneX = 0.0f, m_prefabZoneZ = 0.0f;   // centro congelato all'apertura
    std::vector<int> m_prefabPickBoxes;      // indici in m_boxes esclusi/inclusi a mano
    std::vector<int> m_prefabPickPositions;  // idem su m_positions
    bool  m_prefabPickManual = false;        // true = comanda la selezione manuale
    // Elementi che finirebbero nel prefab ORA (raggio + ritocchi manuali).
    void prefabZoneCollect(std::vector<int>& boxes, std::vector<int>& positions) const;
    bool savePrefabFromZone(std::string& err);   // true = creato

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
    // Punti di spawn AGGIUNTIVI per fazione (multi-spawn): le AI si distribuiscono
    // su questi + lo spawn principale. Selezione: team1 = -3000-i, team2 = -3100-i.
    std::vector<std::array<float,3>> m_spawnPoints1, m_spawnPoints2;

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

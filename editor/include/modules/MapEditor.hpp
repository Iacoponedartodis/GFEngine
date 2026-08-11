#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"   // prefab per il piazzamento (ADR-048)
#include "mini/game/MapStructures.hpp"             // primitive parametriche (ADR-053)
#include "mini/game/nav/NavManager.hpp"            // validazione navmesh (doc 47)
#include "mini/game/nav/NavCheck.hpp"              // classificazione isole, condivisa con --navcheck
#include "framework/UndoStack.hpp"                 // annullamento condiviso (doc 52 F2)
#include "framework/ViewportEditing.hpp"           // selezione + gizmo condivisi (doc 52 F1)
#include "framework/Toolbar.hpp"                   // barra che non puo. tagliare (doc 53)
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
    // ENTRAMBE le viewport: quella del tab struttura può avere il mouse catturato,
    // e un mouse catturato che nessuno rilascia è un editor da cui non si esce più.
    void releaseMouseCapture()
    { m_viewport.releaseMouseCapture(); m_structVp.releaseMouseCapture(); }

    // Collaudo headless delle OPERAZIONI (`--editor-selftest`). Non tocca la UI:
    // esercita duplica/serie/annulla su uno stato sintetico e verifica gli
    // invarianti. Esiste perché le regressioni di queste operazioni sono finite
    // in mano all'utente due volte — e un'operazione che nessuno può eseguire
    // senza mouse è un'operazione che nessuno verifica. Ritorna il numero di
    // controlli FALLITI (0 = tutto bene).
    int selfTest();

    // ── PROTEZIONE DEL LAVORO (doc 51) ───────────────────────────────────
    // Il Map Editor non aveva né salvataggio automatico né un avviso all'uscita:
    // chiudere la finestra buttava via ore di lavoro in silenzio, e con KI #98
    // ancora aperto un crash faceva lo stesso. Su una mappa 300 × 200 costruita in
    // più giorni è il rischio più grosso che ci sia — più di qualunque lentezza.
    // TUTTO il modulo, non solo la mappa: anche i tab delle strutture. Prima
    // guardava solo `m_dirty` della mappa, quindi chiudendo con un tipo modificato
    // non chiedeva nulla e il lavoro spariva — un avviso che vale in un posto solo
    // è peggio di nessun avviso, perché insegna a fidarsi.
    [[nodiscard]] bool hasUnsavedChanges() const
    {
        if (m_dirty) return true;
        for (const auto& t : m_structTabs) if (t.dirty) return true;
        return false;
    }
    // Cosa c'è di non salvato, in chiaro: "la mappa", "2 tipi di struttura"...
    [[nodiscard]] std::string unsavedSummary() const;
    [[nodiscard]] const std::string& currentMapId() const { return m_mapId; }

    // ── PROVARE CAMMINANDO (doc 53 L4) ───────────────────────────────────
    // Il modulo non lancia processi: dichiara l'intenzione, e chi possiede
    // l'applicazione la esegue. Ritorna gli argomenti per GFEngine, vuoto se
    // nessuno l'ha chiesto. Si svuota leggendo: una richiesta che resta appesa
    // rilancerebbe il gioco a ogni frame.
    // Finestre a sé del modulo (Problemi). Le chiama EditorApp DOPO aver chiuso la
    // finestra del modulo: una finestra top-level creata dentro un altro Begin è
    // lecita ma fragile, e a schermo intero le due si contendono il layout.
    void drawFloatingWindows();
    [[nodiscard]] std::string takePlaytestRequest()
    { std::string a; a.swap(m_playtestArgs); return a; }
    // Copia di recupero periodica, FUORI da `data/maps/` (altrimenti diventerebbe
    // una mappa fantasma nell'elenco). Non tocca il file dell'utente.
    void tickAutosave(float dt);
    [[nodiscard]] static std::string autosaveDir();
    [[nodiscard]] const std::string& lastAutosave() const { return m_lastAutosave; }
    // Salvataggio richiesto dall'esterno (avviso di uscita di EditorApp).
    bool saveMapPublic() { return saveMap(); }
    // Salva la mappa E i tipi di struttura modificati. Un "salva ed esci" che ne
    // salva solo una parte è peggio di non offrirlo.
    void saveAllPending()
    {
        // PRIMA le modifiche d'istanza: finiscono dentro la mappa, quindi vanno
        // applicate prima di scriverla. Applicarle dopo le avrebbe perse in silenzio
        // — un "salva ed esci" che ne salva una parte è peggio di non offrirlo.
        for (auto& t : m_structTabs)
            if (t.dirty && t.target == StructTab::Target::Instance) applyInstanceTab(t);
        if (m_dirty) saveMap();
        for (auto& t : m_structTabs)
            if (t.dirty && t.target == StructTab::Target::Library && !t.id.empty())
                saveStructType(t);
    }

    // ── TETTI DEI CODICI DI SELEZIONE (doc 49 R1, KI #100) ───────────────
    // Il codice di selezione è l'INDICE nell'array dentro un intervallo riservato
    // per tipo: oltre il tetto il codice cambia significato e si muove un elemento
    // di un altro tipo, **senza alcun errore**. Un controllo solo, eseguito a ogni
    // frame, invece di una guardia in 26 punti di creazione: una guardia
    // dimenticata in un punto è il difetto che si voleva evitare, e questo copre
    // anche il caricamento da disco e la duplicazione.
    struct CapacityInfo { const char* name; int used; int limit; };
    [[nodiscard]] std::vector<CapacityInfo> capacityReport() const;

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
    // ── IL GATE DEI DATI, VISTO DA QUI (doc 53 L5) ───────────────────────
    // Lo STESSO `validateContent` di `--validate` e del pannello Validazione: non
    // una seconda analisi. Le voci qui sono filtrate su QUESTA mappa, perché
    // costruendo interessa sapere se ho appena rotto questa — non l'intero
    // contenuto. Su richiesta e non a ogni frame: richiede di ricaricare l'intero
    // registry, che non è un costo da pagare mentre si trascina un box.
    std::vector<std::pair<int, std::string>> m_gateLines;   // <gravità, testo>
    bool m_gateRun = false;
    void runDataGate();
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
    // ── PRIMITIVE PARAMETRICHE (ADR-053) ─────────────────────────────────
    // Si autora la RICETTA, non i box: l'espansione la fa `mapstructures::expand`,
    // la stessa funzione che usa il motore al load — così l'anteprima nel viewport
    // e il gioco non possono divergere. I box espansi NON entrano in `m_boxes` e
    // NON si salvano mai (ADR-033): vivono solo in `m_structPreview`, rigenerata a
    // ogni modifica.
    // Rilegge data/prefabs/ e rifà la lista del menu. Un solo punto, così ogni
    // operazione che tocca gli asset (creazione, eliminazione) resta allineata da
    // sé invece di richiedere un pulsante "ricarica" da premere a mano.
    void reloadPrefabAssets();

    std::vector<mini::StructureDef> m_structures;
    std::vector<mini::MapGeometryBox> m_structPreview;   // derivata, mai salvata
    int  m_selStruct = -1;                               // indice in m_structures
    void rebuildStructurePreview();
    // L'UNICO punto in cui l'editor espande una struttura della mappa risolvendone
    // il tipo (ADR-056). Vedi la nota sull'implementazione: tre chiamate sparse su
    // quattro sbagliavano.
    void expandStructureAt(int idx, std::vector<mini::MapGeometryBox>& out) const;
    // Da oggetto unico a insieme di parti, e ritorno. Vedi la nota estesa nel .cpp.
    void explodeStructure(int idx);
    void groupSelectionIntoType(const std::string& label);
    bool m_groupOpen = false;          // finestra "Raggruppa in composita"
    char m_groupName[64] = "Composita";
    std::string m_groupError;
    void addStructure(mini::StructureKind kind);

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

    // ── SELEZIONE MULTIPLA (doc 47 E2 / G3) ──────────────────────────────
    // Insieme di CODICI di selezione (stessa codifica di `m_selBox`, più
    // -6000-i per le strutture). `m_selBox`/`m_selStruct` restano il PRIMARIO,
    // cioè l'ultimo cliccato: è quello che il pannello proprietà mostra e su cui
    // agiscono rotazione e scala. Con un solo elemento il comportamento è
    // identico a prima — la selezione multipla è additiva, non sostitutiva.
    // Motivo per cui serve: con 1.520 box su una mappa grande, senza questo un
    // edificio non si sposta, si ricostruisce (doc 47 §1.2).
    std::vector<int> m_multiSel;
    std::vector<int> selectionCodes() const;      // insieme effettivo
    int  primaryCode() const;                     // ultimo cliccato
    void setSelection(int code, bool additive);   // click normale / Ctrl+click
    bool codePosition(int code, glm::vec3& out) const;   // posizione, se ne ha una
    float* codeYaw(int code);                     // orientamento, se ne ha uno
    void applyMove(int code, const glm::vec3& delta);
    void applyGizmoRotateScale();
    // ── COSTRUZIONE (doc 53 L1) ──────────────────────────────────────────
    // Tirare una faccia dell'ingombro della selezione: la faccia opposta resta
    // ferma. Il gesto primario di costruzione secondo tutti i riferimenti.
    void applyFaceDrag();
    void applyFaceDelta(int face, float delta);   // 0=-X 1=+X 2=-Y 3=+Y 4=-Z 5=+Z
    // Ingombro della selezione, comunicato al viewport per posare le sei maniglie.
    [[nodiscard]] bool selectionBounds(glm::vec3& mn, glm::vec3& mx) const;
    // Il box appena disegnato col trascinamento sul piano di lavoro.
    void createDrawnBox(const glm::vec3& mn, const glm::vec3& mx);
    bool  m_wasDragging   = false;   // per ricalcolare i derivati UNA volta, al rilascio
    float m_drawHeight    = 3.0f;    // altezza dei box disegnati (muro standard)
    float m_drawPlaneY    = 0.0f;    // quota del piano di lavoro
    // ── PRECISIONE (doc 53 L2) ───────────────────────────────────────────
    void  moveSelectionBy(const glm::vec3& d);
    void  alignSelection(int axis, int mode);       // mode 0=min 1=centro 2=max
    void  distributeSelection(int axis);
    bool  m_offsetOpen = false;
    float m_offsetVal[3] = {0.0f, 0.0f, 0.0f};
    std::string m_playtestArgs;      // doc 53 L4: richiesta di "Prova da qui"
    std::string m_playtestNote;      // esito dell'ultima richiesta, detto all'utente
    void  requestPlaytest();
    // ── BARRA CHE NON PUÒ TAGLIARE (doc 53 L0) ───────────────────────────
    // Popup da aprire DOPO il disegno della barra: una voce scelta dentro il menu
    // «...» chiamerebbe OpenPopup da un livello di ID diverso, e il popup resterebbe
    // aperto senza essere disegnato (il modale invisibile del changelog 164).
    std::string m_pendingPopup;
    float m_savedFlash = 0.0f;       // "Salvato!" per qualche istante dopo Ctrl+S
    editor::toolbar::Report m_tbReport;   // quante voci sono rientrate: misura senza occhi
    void  drawSnapCombo();
    void  drawNavmeshWidget();
    [[nodiscard]] float navmeshWidgetWidth() const;
    // Appoggia / accosta: sposta la selezione finché tocca la geometria vicina.
    // `axis` 0=X 1=Y 2=Z, `dir` -1 o +1. Ritorna quanti elementi si sono mossi.
    int  snapSelectionToSurface(int axis, int dir);
    void deleteSelection();                       // elimina TUTTI i selezionati
    // Restituisce il CODICE della copia (-1 se non duplicabile). Non è un extra:
    // le box si inseriscono accanto all'originale, quindi il codice della copia non
    // si può dedurre dalla dimensione del vettore — dedurlo spostava un altro elemento.
    int  duplicateOne(int code);                  // una copia, per un solo codice

    // ── ARRAY: N copie con offset progressivo (doc 47 E4) ────────────────
    // Una fila di 12 colonne è UN comando, non 12 operazioni. Agisce sulla
    // selezione corrente, quindi funziona anche su un gruppo intero.
    int   m_arrayCount = 4;
    float m_arrayOff[3] = { 4.0f, 0.0f, 0.0f };
    float m_arrayYawStep = 0.0f;
    void  makeArray();

    // ── FILTRI DI VISIBILITÀ (doc 47 E5) ─────────────────────────────────
    // Su 1.520 box il problema diventa VEDERE. Nascondere per tipo e per quota è
    // ciò che rende lavorabile un interno senza il tetto davanti agli occhi.
    // Solo visivo: non tocca i dati e non si salva.
    bool  m_showType[5] = { true, true, true, true, true };   // floor/wall/platform/cover/decoration
    float m_hideAboveY  = 1000.0f;   // nasconde ciò che sta più in alto
    bool  m_showStructures = true;
    // I MARCATORI, separati dalla geometria. Nascono da una richiesta precisa:
    // guardare il navmesh con 169 posizioni tattiche, 5 post, settori e percorsi
    // colorati davanti è illeggibile. La verifica navmesh serve a vedere le
    // superfici, e i metadati le coprono.
    bool  m_showPositions  = true;   // posizioni tattiche (il gruppo più fitto)
    bool  m_showAreas      = true;   // settori e zone di pericolo (dischi ampi)
    bool  m_showRoutes     = true;   // percorsi di pattuglia
    bool  m_showGamePoints = true;   // spawn, post, comandante, bersagli, veicoli
    bool  filtersActive() const;

    // ── FIGURA DI SCALA (doc 47 E6) ──────────────────────────────────────
    // Il rimedio raccomandato all'errore più comune del blockout: gli sbagli di
    // scala. Si piazza dove stai guardando, così il confronto è immediato.
    // Si ANCORA dove la piazzi, non segue la telecamera: `updateViewport` ricalcola
    // anche l'esposizione (O(n²) sulle posizioni), quindi rinfrescarla ogni frame
    // costerebbe caro — e una sagoma che insegue lo sguardo distrae invece di aiutare.
    bool  m_showScaleFigure = false;
    float m_scaleFigX = 0.0f, m_scaleFigY = 0.0f, m_scaleFigZ = 0.0f;

    // ── VALIDAZIONE DAL VIVO (doc 47 G7) ─────────────────────────────────
    // I difetti si vedono nel viewport MENTRE costruisci, non solo in un elenco a
    // parte o nel gate a mappa finita. Sorgente: `m_issues`, cioè la STESSA
    // `analyzeTacticalHealth` che usa `--validate` — nessuna seconda analisi che
    // possa dare un verdetto diverso da quello del gioco.
    // Acceso di default: un controllo che va ricordato di accendere è un controllo
    // che non si usa.
    bool  m_showDefects = true;

    // ── VALIDAZIONE NAVMESH (doc 47) ─────────────────────────────────────
    // Costruisce il navmesh VERO con lo stesso `NavManager` del motore. È l'unico
    // controllo che non può mentire: fra i box e le superfici percorribili ci sono
    // erosione, sfoltimento dei cigli, altezza libera e area minima di regione, e
    // il gate sui dati non li vede — su Training Ground dice 0 problemi mentre un
    // intero recinto è irraggiungibile (KI #97).
    // Su richiesta, non a ogni frame: la costruzione costa ~0,11 s su Training
    // Ground e ~1,4 s su una mappa 300 × 200.
    mini::NavManager m_nav;
    std::vector<FreeCameraViewport::NavTriDraw> m_navTris;
    bool m_showNav  = false;   // overlay acceso
    bool m_navBuilt = false;   // esiste un risultato da mostrare
    bool m_navStale = true;    // la mappa è cambiata dall'ultima verifica
    struct NavReport {
        int polys = 0, components = 0, mainComponent = -1;
        int islandTris = 0;                 // triangoli fuori dalla componente dello spawn
        std::vector<int> badPositions;      // indici in m_positions
        std::vector<int> badPosts;          // indici in m_posts
        float buildSeconds = 0.0f;
        // ── QUANTO SONO GRANDI, E DOVE ───────────────────────────────────
        // "10 isole" senza la dimensione è un numero che non si può usare: dieci
        // schegge da tre triangoli e dieci stanze scollegate sono due mappe
        // diversissime, e l'indicatore le diceva allo stesso modo. Segnalato
        // dall'utente su Warfare Ground — *"tutto verde, però mi segna 10 isole"*:
        // i triangoli rossi c'erano, erano solo troppo piccoli per notarli.
        // L'AREA, non il numero di triangoli: `minRegionArea` di Recast scarta le
        // regioni sotto 2,56 m², quindi un'isola che è sopravvissuta è grande almeno
        // così — e "2,6 m²" dice quanto pavimento è tagliato fuori, "3 triangoli" no
        // (un triangolo può essere grande quanto una stanza).
        struct Island { int tris = 0; float area = 0.0f; glm::vec3 center{0.0f};
                        float covered = 0.0f; };   // frazione sotto un ostacolo
        std::vector<Island> islands;        // ordinate per area, la più grande prima
    };

    // ── UN SOLO ELENCO DI PROBLEMI (doc 53 L5, richiesta utente 2026-08-11) ──
    // Prima erano TRE posti diversi con tre presentazioni diverse: la salute tattica
    // nel pannello di sinistra, l'esito del navmesh scritto in barra, il gate dei
    // dati sotto l'elenco. Tre modi di dire "qui c'è un problema" sono tre cose da
    // imparare, e quella in barra restava rossa in permanenza per delle schegge.
    // Una sola finestra, raggruppata per tipo, ogni voce ti porta sul posto.
    struct Problem {
        int         sev = 0;          // 0 avviso, 1 problema
        std::string group;            // titolo del gruppo (l'elenco si raggruppa qui)
        std::string text;
        int         sel = -1;         // codice di selezione, -1 = nessun elemento
        bool        hasPos = false;
        glm::vec3   pos{0.0f};
        float       radius = 6.0f;    // quanto inquadrare: un'isola da 40 m² non è un box
        // Un'isola non è un elemento selezionabile: arrivando sul posto non ci
        // sarebbe niente di evidenziato. Serve accendere l'overlay del navmesh —
        // il rosso È il difetto.
        bool        needsNav = false;
    };
    [[nodiscard]] std::vector<Problem> collectProblems() const;
    void drawProblemsWindow();   // chiamata da drawFloatingWindows()
    bool m_showProblems = false;
    NavReport m_navReport;
    void validateNavmesh();
    // Impronta della geometria: se cambia, il risultato della verifica è vecchio.
    // Calcolata invece di essere "marcata a mano" nei venti punti che modificano
    // la mappa — un flag da ricordarsi di alzare è un flag che prima o poi resta
    // basso, e mostrare un navmesh stantio come se fosse buono è peggio che non
    // mostrarlo affatto.
    [[nodiscard]] std::size_t geometryFingerprint() const;
    std::size_t m_navFingerprint = 0;
    float m_gridSnap     = 0.5f; // snap griglia
    bool  m_showNavmesh  = false; // evidenzia floor

    // ── UNDO / REDO (doc 47 E1) ──────────────────────────────────────────
    // A SNAPSHOT del documento, non a comandi. Motivo: l'editor muta lo stato in
    // decine di punti sparsi (ogni DragFloat scrive direttamente nel vettore), e un
    // command pattern richiederebbe di riscriverli tutti. Una mappa in memoria sono
    // poche decine di KB: copiarla per intero a ogni operazione costa nulla ed è
    // impossibile da sbagliare.
    // È il #1 dichiarato dagli editor a brush, e finora non esisteva affatto: senza,
    // su una mappa grande ogni esperimento è irreversibile, ed è la paura di
    // sbagliare che rende lenti.
    struct Snapshot
    {
        std::vector<BoxEntry>          boxes;
        std::vector<PostEntry>         posts;
        std::vector<PositionEntry>     positions;
        std::vector<SectorEntry>       sectors;
        std::vector<DangerEntry>       dangers;
        std::vector<RouteEntry>        routes;
        std::vector<VehicleSpawnEntry> vehSpawns;
        std::vector<TargetEntry>       targets;
        std::vector<PrefabInstEntry>   prefabInsts;
        std::vector<mini::StructureDef> structures;
        CommanderEntry                 commander;
        std::array<float,3>            spawnTeam1, spawnTeam2;
        std::vector<std::array<float,3>> spawnPoints1, spawnPoints2;
    };
    // La pila CONDIVISA (doc 52 F2). Prima era scritta qui — ed era l'unica dei
    // sette moduli ad averla. Il componente è stato estratto da questa, quindi la
    // migrazione non cambia comportamento: cambia solo chi lo implementa.
    static constexpr size_t kUndoDepth = 64;
    UndoStack<Snapshot> m_hist{ kUndoDepth };
    // Ogni due minuti: abbastanza spesso da non perdere lavoro vero, abbastanza raro
    // da non intralciare (il salvataggio di una mappa grande non è gratis).
    static constexpr float  kAutosaveSeconds = 120.0f;
    float       m_autosaveTimer = 0.0f;
    std::string m_lastAutosave;
    // Coalescenza: trascinare un gizmo produce uno stato nuovo a ogni frame, e senza
    // raggruppamento un solo trascinamento riempirebbe tutta la pila. `pushUndo`
    // ignora le chiamate ravvicinate con la stessa etichetta.
    // (l'etichetta e il tempo dell'ultimo gesto vivono ora dentro `UndoStack`)
    float       m_editorClock  = 0.0f;

    Snapshot captureState() const;
    void     applyState(const Snapshot& s);
    void     pushUndo(const char* tag);   // da chiamare PRIMA di modificare
    void     doUndo();
    void     doRedo();

    // ── Viewport 3D ──────────────────────────────────────────────────────
    FreeCameraViewport m_viewport;

    // ── Operazioni ───────────────────────────────────────────────────────
    void loadMaps();                          // scansiona data/maps/
    void loadMap(const std::string& id);      // carica dal JSON
    // Salva sul JSON. `overridePath` non vuoto = scrive ALTROVE con la stessa
    // serializzazione (lo usa il salvataggio automatico).
    bool saveMap(const std::string& overridePath = "");

    void addBox();
    int  duplicateBox(int idx);   // -> indice della copia (inserita ACCANTO, non in coda)
    void duplicateSelected();   // duplica QUALSIASI elemento selezionato (F4, doc 39)
    void deleteBox(int idx);

    // Rigenera la geometria del viewport. `recomputeDerived = false` salta il
    // ricalcolo dei dati derivati (esposizione, visuale verticale): serve ai cambi
    // di sola VISTA — filtri, taglio in quota — che vanno aggiornati a ogni frame
    // mentre si trascina uno slider. `recomputeExposure` è O(n²) sulle posizioni:
    // rifarlo per una spunta di visibilità sarebbe pagare un'analisi tattica per
    // nascondere un tetto.
    void updateViewport(bool recomputeDerived = true);
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

    // ── TAB (doc 48 S1) ──────────────────────────────────────────────────
    // Un tab, non un modulo: vincolo esplicito dell'utente — *"così non apre un
    // altro modulo, ma posso rimanere in map editor avendo una viewport separata"*.
    // La forma è quella di Unity Prefab Mode: si entra nella definizione **da dove
    // la si usa** (in fondo al menu `+ Struttura`), si edita in ISOLAMENTO, e
    // all'uscita c'è un contratto esplicito su cosa è stato salvato.
    struct StructTab
    {
        std::string id;                     // filename stem; vuoto = mai salvato
        mini::StructureTypeDef def;
        bool dirty = false;

        // ── CHE COSA si sta modificando ──────────────────────────────────
        // `Library`  — il TIPO in `data/structures/`: cambia ogni sua copia, ovunque.
        // `Instance` — UNA sola struttura in mappa (richiesta utente 2026-08-10:
        //   *"ne piazzo 4 diverse, ma su una devo fare una modifica specifica"*).
        //   Il risultato finisce nelle PARTI LOCALI dell'istanza, cioè nel file
        //   della mappa; il tipo resta intatto e le altre tre copie non si accorgono
        //   di niente. È la distinzione fra Prefab Mode e override d'istanza in
        //   Unity, e senza di essa l'unica strada era duplicare il tipo.
        enum class Target { Library, Instance };
        Target      target  = Target::Library;
        int         instIdx = -1;          // indice in m_structures (solo Instance)
        std::string originType;            // tipo da cui deriva (etichetta e ritorno)

        // ── ISOLAMENTO ───────────────────────────────────────────────────
        // -1 = si lavora sulla struttura intera. Altrimenti è l'indice della
        // parte-RIFERIMENTO che si sta modificando "da dentro": l'elenco, il gizmo e
        // l'anteprima mostrano solo le sue parti, e alla fine tutto si richiude in un
        // oggetto solo. È la vista locale di Blender, applicata a un assemblaggio.
        int isolated = -1;

        // Le parti su cui agiscono TUTTI i comandi. Un accessore solo, così un
        // comando nuovo non può dimenticarsi dell'isolamento — che è esattamente il
        // modo in cui una modalità si rompe.
        [[nodiscard]] std::vector<mini::StructurePart>& parts()
        {
            if (isolated >= 0 && isolated < (int)def.parts.size())
                return def.parts[isolated].localParts;
            return def.parts;
        }
        [[nodiscard]] const std::vector<mini::StructurePart>& parts() const
        {
            if (isolated >= 0 && isolated < (int)def.parts.size())
                return def.parts[isolated].localParts;
            return def.parts;
        }
        // La struttura EFFETTIVAMENTE in lavorazione: in isolamento è sintetica,
        // fatta delle sole parti locali del riferimento. Serve a espansione,
        // anteprima e verifica, che ragionano su un tipo intero.
        [[nodiscard]] mini::StructureTypeDef activeDef() const
        {
            if (isolated < 0) return def;
            mini::StructureTypeDef d;
            d.kind  = def.kind;
            d.parts = parts();
            return d;
        }
        // Esito della verifica sulla struttura ISOLATA (doc 48 §Osservabilità).
        struct Check {
            bool  run = false;
            bool  stale = true;
            int   boxes = 0;                // box espansi dalla ricetta
            int   tris = 0;                 // triangoli di navmesh prodotti
            int   components = 0;           // >1 = la struttura si spezza
            float declaredArea = 0.0f;      // m² calpestabili dichiarati
            float navArea = 0.0f;           // m² effettivamente percorribili
            bool  hasWalkable = false;      // falso = ostacolo puro (muro, barricata)
            std::vector<std::string> mute;  // box che non producono superficie
            float seconds = 0.0f;
        } check;
        std::size_t fingerprint = 0;
        std::string saveError;   // non vuoto = l'ultimo salvataggio è fallito
        // I triangoli del navmesh restano nel TAB, non nel viewport: il viewport è
        // uno solo e condiviso, quindi tenerli lì li faceva sopravvivere al cambio
        // di tab (si vedeva il navmesh della struttura precedente).
        std::vector<FreeCameraViewport::NavTriDraw> navTris;
        // Annullamento PROPRIO del tab (doc 52 F2): la pila condivisa, non una
        // seconda implementazione. Lo stato fotografato è la definizione del tipo
        // più la parte selezionata — annullare e ritrovarsi un'altra parte scelta
        // sarebbe disorientante quanto non annullare.
        struct UndoState { mini::StructureTypeDef def; int selPart = -1; };
        UndoStack<UndoState> undo;
        // Un solo punto in cui si fotografa e si ripristina il tab.
        [[nodiscard]] UndoState snapshot(int selPart) const { return { def, selPart }; }
    };
    std::vector<StructTab> m_structTabs;
    int  m_activeTab = 0;          // 0 = Mappa; 1+ = m_structTabs[idx-1]
    bool m_focusLastTab = false;   // un tab appena aperto va messo in primo piano
    // Tab di cui si è chiesta la chiusura con modifiche non salvate. Serve un indice
    // ESPLICITO: il tab che si chiude non è per forza quello attivo — la ✕ si può
    // premere su un tab in secondo piano, e agire su `m_activeTab` scarterebbe il
    // lavoro sbagliato.
    int  m_pendingCloseTab = -1;
    // Tab da aprire DOPO il ciclo di disegno: aprirlo durante invaliderebbe il
    // riferimento all'elemento corrente di `m_structTabs`.
    std::string m_pendingOpenType;
    int         m_pendingEditInstance = -1;   // idem, per "Modifica solo QUESTA"
    // Apre un tab legato a UNA struttura in mappa (parti locali), non al tipo.
    void openInstanceTab(int idx);
    // Riporta le parti del tab nell'istanza. Nessun file toccato: è roba di mappa.
    void applyInstanceTab(StructTab& t);
    // Filtro per nome delle liste. Solo visivo, non si salva.
    char m_listFilter[64] = "";
    // Viewport SEPARATA, una sola riusata dal tab attivo: mostra la sola struttura.
    FreeCameraViewport m_structVp;
    ViewportEditing    m_structEdit;   // selezione + gizmo, dal componente condiviso
    bool m_structShowNav = false;      // overlay navmesh acceso nel tab struttura
    void applyStructNavOverlay(const StructTab& t);
    mini::NavManager   m_structNav;
    std::vector<std::string> m_structTypeIds;   // libreria su disco, per i menu

    void drawMapTab(float totalW, float totalH);
    void drawStructTab(StructTab& t, float totalW, float totalH);
    void openStructTab(const std::string& id);         // vuoto = nuovo tipo
    void saveStructType(StructTab& t);
    // "Salva come copia": crea una VARIANTE lasciando intatto l'originale, e il tab
    // passa a lavorare sulla copia (semantica di un Salva-con-nome fatto bene).
    void drawSaveAsCopyPopup(StructTab& t);
    [[nodiscard]] static std::string idFromLabel(const std::string& label);
    bool m_copyOpen = false;
    char m_copyName[96] = "";
    std::string m_copyError;
    void checkStructType(StructTab& t);
    void refreshStructTypeIds();
    void rebuildStructTabPreview(StructTab& t);
    // Non statica: deve poter risolvere i riferimenti ad altri tipi (ADR-056
    // rivisto) attraverso il registry dei prefab.
    void expandTypeForEdit(const mini::StructureTypeDef& def,
                           std::vector<mini::MapGeometryBox>& out) const;
    // Come si risolve un id di tipo: attraverso il registry dei prefab. Costruito
    // una volta nel costruttore, così ogni espansione usa la stessa strada.
    mini::mapstructures::TypeResolver m_typeResolver;
    int  m_selPart = -1;   // parte selezionata nel tab assemblaggio
    void placePartClear(const StructTab& t, mini::StructurePart& p);
    // L'origine di un assemblaggio è il perno di rotazione e il punto del gizmo in
    // mappa: averla fuori dalla struttura significa ruotare attorno al vuoto.
    void  centerAssemblyOrigin(StructTab& t);
    [[nodiscard]] float assemblyOriginOffset(const StructTab& t) const;
    static void scalePrimitivePart(mini::StructureDef& s, const glm::vec3& d);
    [[nodiscard]] static std::size_t structFingerprint(const mini::StructureTypeDef& d);

    bool m_dirty = false;  // modifiche non salvate
};

} // namespace editor

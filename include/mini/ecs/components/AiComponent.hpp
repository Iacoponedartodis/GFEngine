#pragma once

#include "mini/ecs/Entity.hpp"   // EntityId (target cachato per il time-slicing)

namespace mini { class Mesh; class Texture; }

namespace mini
{

enum class AiState : unsigned char {
    Patrol,   // nessun contatto — cammina tra waypoint
    Alert,    // LOS attivo — spara e strafea
    Hunt,     // no LOS ma conosce lastKnown — va verso il bersaglio
    Search    // raggiunto lastKnown ma nessuno — cerca in punti random
};

struct AiComponent
{
    float    shootCooldown  = 0.0f;
    float    shootInterval  = 2.5f;
    float    aggroRange     = 16.0f;
    // ── Percezione (doc 40 Fase 1) ────────────────────────────────────────
    // `fov_deg` e `hearing_range` erano AUTORATI nei profili ma non arrivavano mai
    // qui: le AI vedevano a 360° ed erano sorde (authoring inerte, trovato 2026-07-27).
    // Un soldato credibile non vede alle spalle e ACCORRE al rumore degli spari.
    float    fovDeg         = 110.0f;   // ampiezza TOTALE del cono visivo
    float    hearingRange   = 12.0f;    // raggio entro cui percepisce un evento sonoro
    // SOPPRESSIONE (doc 40 A3), 0..1: quanto l'unità è "sotto tiro" ORA. Cresce coi
    // colpi che passano vicino — anche quando NON colpiscono — e decade da sé.
    // Peggiora la mira e spinge a stare coperti: è ciò che rende una raffica utile
    // anche se non uccide, cioè la differenza fra una sparatoria militare e un duello.
    float    suppression    = 0.0f;
    // Serve a rilevare la TRANSIZIONE verso/da "inchiodato" e annunciarla una sola
    // volta: un sistema che non si vede, per il giocatore non esiste — e senza vederlo
    // non lo si può nemmeno tarare.
    bool     wasPinned      = false;
    // RUOLO DI COMBATTIMENTO (doc 40 A4), assegnato all'ingaggio e per SATURAZIONE:
    // 0 nessuno · 1 SOPPRIME (fissa il nemico, non si espone) · 2 AGGIRA (cerca il
    // fianco) · 3 AVANZA (preme di fronte). Prima ogni soldato tirava i dadi per conto
    // suo (`flankChance`), quindi la squadra non si divideva i compiti: potevano
    // aggirare tutti o nessuno. Il ruolo rende esplicito il "tu fissi, io aggiro" —
    // che ha senso solo perché la soppressione (A3) morde davvero.
    int      combatRole     = 0;
    Mesh*    bulletMesh     = nullptr;
    Texture* bulletTexture  = nullptr;
    float    bulletR = 1.0f, bulletG = 0.25f, bulletB = 0.1f;

    // Statistiche proiettile — popolate da WeaponDef al momento dello spawn
    float    bulletSpeed    = 8.0f;
    float    bulletDamage   = 20.0f;
    float    bulletLifetime = 5.0f;

    // ── Modello arma (da WeaponDef): cadenza reale + surriscaldamento ──
    // fireInterval <= 0 → usa il legacy shootInterval del profilo AI.
    float    fireInterval    = 0.0f;   // 1 / fire_rate dell'arma
    float    heat            = 0.0f;   // 0..1
    float    heatPerShot     = 0.0f;   // 0 = arma senza surriscaldamento
    float    cooldownRate    = 0.30f;  // raffreddamento al secondo
    float    overheatPenalty = 2.0f;   // pausa forzata a overheat (s)
    bool     overheated      = false;

    float patrolAx = 0, patrolAz = 0;
    float patrolBx = 0, patrolBz = 0;
    float patrolSpeed = 2.0f;

    // Route autorata seguita da questa unità (ADR-028, ampliato ADR-045). Indice
    // in MapDef.patrolRoutes; `patrolSeg` è ora l'indice del PUNTO-obiettivo
    // corrente (0..P-1), non del segmento. -1 = nessuna route → A/B legacy.
    // Le route sono percorse BIDIREZIONALMENTE (si inverte agli estremi, niente
    // salto-wrap) e si possono raccogliere dal punto più vicino / cambiare.
    int  patrolRoute   = -1;
    int  patrolSeg     = 0;
    bool patrolReverse = false;   // ADR-045: verso di percorrenza della route

    // Sosta ai waypoint di pattuglia (secondi). > del capture_time dei
    // command post → l'AI resta nell'area abbastanza da catturarli.
    // 0 = comportamento legacy (inversione immediata).
    float patrolDwell = 0.0f;
    float waitTimer   = 0.0f;

    // ── Dal profilo AI (AiProfileDef, risolto allo spawn) ─────────────
    bool  jumpEnabled  = true;   // salto anti-ostacolo quando bloccata
    float accuracy     = 0.55f;  // 1 = colpo perfetto; <1 = dispersione
    float reactionTime = 0.4f;   // ritardo primo colpo all'acquisizione

    // ── Comportamento tattico dal profilo (16_AiBehavior) ─────────────
    float aggression      = 0.65f; // 1 = chiude la distanza, 0 = tiene il raggio
    float retreatHpThresh = 0.0f;  // frazione HP sotto cui si disimpegna (0 = mai)
    float coverPreference = 0.0f;  // probabilità di fase evasiva a fine peek
    float peekMin = 0.6f, peekMax = 1.1f; // durata finestra di fuoco (s)
    float hideMin = 0.8f, hideMax = 1.8f; // durata finestra evasiva (s)
    float flankChance     = 0.0f;  // probabilità di approccio laterale in Hunt

    // ── Personalità individuale (ADR-029) ─────────────────────────────
    // Valore [0,1) assegnato allo spawn (hash dell'entity id): deterministico ma
    // DIVERSO per ogni unità. Rompe le parità, sfasa i tempi, sceglie lato e
    // distanza degli aggiramenti e la posizione in formazione. Senza di esso
    // unità con lo stesso profilo prendono SEMPRE la stessa decisione e la
    // squadra si muove come un corpo solo (feedback utente 2026-07-20).
    float bias = 0.5f;

    // ── Manovra in combattimento (ADR-035) ────────────────────────────
    // Prima, entrati in Alert, l'AI restava dov'era: tutti i metadata tattici
    // erano usati SOLO prima del contatto e lo scontro diventava statico. Ora
    // valuta periodicamente se spostarsi su una posizione migliore (aggiramento
    // o posizione di tiro) CONTINUANDO a sparare. `repositionTimer` sfasa le
    // valutazioni e fa da cooldown; `repositionActive` dice che è in movimento.
    float repositionTimer  = 0.0f;
    bool  repositionActive = false;
    float repositionX = 0.0f, repositionZ = 0.0f;
    // Appena ENTRATO in Alert (nuovo ingaggio): fa scattare UNA valutazione
    // proattiva della copertura (cerca subito una posizione di tiro coperta invece
    // di sparare allo scoperto e aspettare la rivalutazione a timer). Poi si azzera.
    bool  justEngaged = false;

    // Roll attivo (abilità "roll", 16 est.): scatto in corso
    float rollTimer = 0.0f;
    float rollVX = 0.0f, rollVZ = 0.0f;

    // Stato runtime peek/hide + flank
    // Cover point scelto per la fase evasiva (18_AiMapConsumption)
    bool  hasCover = false;
    float coverX = 0.0f, coverZ = 0.0f;
    float searchTimer = 0.0f;   // tempo in Search: oltre il limite → Patrol
    float huntTimer   = 0.0f;   // tempo in Hunt: oltre il limite → Search (KI #68)
    float huntPatience = 20.0f; // soglia di huntTimer, dal profilo AI (doc 16)
    float exposeTimer = 0.0f;   // countdown della fase corrente
    bool  evading     = false;  // true = fase evasiva (non spara)
    // Da quanto dura la fase evasiva CORRENTE. Non è decorazione: `evading` chiude il
    // gate di fuoco, quindi una fase che non finisce è un'AI che non spara più — il
    // sintomo di KI #86. Con hide_duration_max = 1.8 s, qualunque valore molto oltre
    // è un CONGELAMENTO per definizione, e questo lo rende misurabile invece che
    // giudicabile a occhio (gli eventi di combattimento aggregati non lo vedono).
    float evadeElapsed = 0.0f;

    // ── Scatola nera (AiTrace, KI #86) ──────────────────────────────────────
    // Stato dell'OSSERVAZIONE, non del comportamento: nessuna decisione lo legge.
    // Vive qui e non in un sistema di proposito — la memoria di progetto
    // [[systems-survive-world-initialize]] ricorda che lo stato dentro un sistema
    // sopravvive a `initialize()` e va azzerato a mano; dentro il componente muore
    // con l'entità, quindi una partita nuova non può ereditare stalli della vecchia.
    float obsX = 0.0f, obsZ = 0.0f;  // posizione a inizio finestra d'osservazione
    float obsTimer  = 0.0f;          // durata della finestra corrente
    float stallSec  = 0.0f;          // da quanti secondi è ferma senza combattere
    bool  obsFired  = false;         // ha sparato in questa finestra?
    bool  stallSeen = false;         // stallo già segnalato (un evento per episodio)
    bool  flankActive = false;  // sta raggiungendo il punto di fiancheggiamento
    float flankX = 0.0f, flankZ = 0.0f;

    float seekSpeed      = 3.5f;
    float lastKnownX     = 0.0f;
    float lastKnownZ     = 0.0f;
    bool  hasLastKnown   = false;

    float strafeTimer = 1.4f;
    float strafeSign  = 1.0f;
    float velY        = 0.0f;
    bool  stationary  = false;
    // Leash del comandante (ADR-041): area circolare da cui non esce. `leashRadius`
    // 0 = nessun leash (le altre unità). Dentro il raggio si muove per difendersi;
    // oltre, viene ricondotto verso `leashX/Z`. Non insegue e non cerca obiettivi.
    float leashX = 0.0f, leashZ = 0.0f, leashRadius = 0.0f;
    // Àncora di PRESIDIO (ADR-046): quando il comando ordina TIENI, il droide si
    // ancora a una posizione difensiva/chokepoint e ci combatte DA lì senza
    // inseguire (stesso clamp del leash, centro diverso). Dinamica: impostata/
    // azzerata ogni volta che rivaluta la direttiva. 0 = non sta presidiando.
    float holdX = 0.0f, holdZ = 0.0f, holdRadius = 0.0f;
    // Segnale della torre di controllo IMPEGNATO (doc 36): il clone COMMITTA la
    // scelta finché non la raggiunge o il segnale sparisce, invece di ri-sceglierla
    // ogni tick (il filtro-saturazione volatile faceva oscillare i cloni avanti e
    // indietro — segnalato dall'utente; i droidi non lo fanno perché le loro
    // direttive sono stabili). [[control-tower-informs-not-orders]]
    float allySigX = 0.0f, allySigZ = 0.0f;   // WAYPOINT impegnato (già raggiungibile)
    bool  allySigValid = false;
    float allySigTimer = 0.0f;                 // re-eval periodico: bounda il findPath
    // Indice della POSIZIONE tattica rivendicata (ordini del player, occupancy): il
    // membro tiene "prenotata" quella posizione finché ci è impegnato, così i compagni
    // ne scelgono un'altra (niente ammasso). -1 = nessuna (es. Regroup/segnale torre).
    int   allySigIdx = -1;

    float stuckTimer  = 0.0f;
    bool  stuckReported = false;   // telemetria: una WARN per episodio (ADR-016)
    float prevX       = 0.0f;
    float prevZ       = 0.0f;

    // Search: punto random sulla mappa dove l'AI sta cercando
    float searchX     = 0.0f;
    float searchZ     = 0.0f;

    // Alert→Hunt timer: quanto tempo resta in Alert senza LOS prima di passare a Hunt
    float alertTimer  = 0.0f;

    AiState state     = AiState::Patrol;
    bool    goingToB  = true;

    // ── Time-slicing sensing (Fase 4) ─────────────────────────────────
    // La ricerca target O(N²) + LOS gira solo 1 tick su AI_SENSE_INTERVAL
    // (scaglionata per entità); fra un sensing e l'altro l'AI riusa questo
    // bersaglio cachato per mirare/muoversi. 0 = nessun bersaglio.
    EntityId targetEntity = 0;

    // ── Navigazione crowd (ADR-017 Phase B) ───────────────────────────
    // Indice dell'agente dtCrowd; -1 = non registrato (o navmesh assente →
    // fallback su aiMove). Registrato/reaped dal CrowdSystem.
    int crowdAgentIdx = -1;
};

} // namespace mini
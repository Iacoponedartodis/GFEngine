#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/Definitions.hpp"   // MapDef (18_AiMapConsumption)
#include "mini/game/ai/WorldIntel.hpp"       // World Intelligence query layer (ADR-025)
#include "mini/game/MapQuery.hpp"            // groundHeightAt (quota bersaglio per la LOS verticale)
#include "mini/game/nav/NavManager.hpp"      // crowd/pathfinding (ADR-017 Phase B)
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <tracy/Tracy.hpp>   // ADR-015: no-op se USE_TRACY_PROFILER=OFF
#include <nlohmann/json.hpp>   // event() data (ADR-016)
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <vector>

// Livello di COMANDO (settori, torre di controllo, quadro tattico, direttive del
// Droide Tattico, scelta posizione per gli ordini): vive in AiCommandLayer.cpp
// dopo lo split del monolite (audit #7). Qui resta il COME la singola unità esegue.
#include "AiInternal.hpp"

namespace mini
{

// I nomi del command layer si usano non qualificati: erano `static` in questo file
// e i chiamanti restano identici — lo split non tocca il comportamento.
using namespace aicmd;

static constexpr float PI = 3.14159265f;

// Offset dell'origine LOS sopra il TRANSFORM (centro corpo, a ground+AI_HALF_Y):
// porta la linea di vista/tiro a COMBAT_EYE_HEIGHT dal suolo (peek-over cover),
// non a 0.5 m dove qualsiasi copertura la bloccava → KI #79 (non sparavano da
// cover/rialzato). Vale come origine E come mira sui bersagli-unità (eye-to-eye).
static constexpr float kEyeUp = config::COMBAT_EYE_HEIGHT - config::AI_HALF_Y;

// ── Osservabilità delle DECISIONI tattiche ───────────────────────────────
// Finora si misuravano solo crash e stuck: nulla diceva se le AI stessero
// davvero *usando* i metadata o se le query tornassero sempre a vuoto — e senza
// quel dato né l'utente né io possiamo capire perché il comportamento non cambia.
// Contatori azzerati a ogni report periodico (vedi heartbeat più sotto).
struct TacticalStats
{
    int approachDirect = 0, approachFlank = 0, approachFiring = 0, approachVantage = 0;
    int repoEval = 0;          // quante volte un'AI ingaggiata ha valutato se spostarsi
    int repoBlockedBusy = 0;   // bloccata dal cap di concorrenza (troppi già in manovra)
    int repoFlankHit = 0, repoFlankMiss = 0;    // la mappa offriva un aggiramento?
    int repoFiringHit = 0, repoFiringMiss = 0;  // ...una posizione di tiro?
    int repoStarted = 0, repoTooClose = 0;      // manovre avviate / scartate perché vicine
    int structureEngage = 0;   // strutture ingaggiate di iniziativa (doc 35)
    int allySignalFollow = 0;  // cloni che seguono un segnale della torre (doc 36)
    int overwatchStarted = 0;  // manovre di copertura esplicite via grafo (ADR-032)
    int holdPosOccupied = 0;   // Hold su posizione difensiva/chokepoint (ADR-046)
    int obsSightBoost   = 0;   // vista estesa da punto d'osservazione (ADR-046)
    // ── FUNNEL dell'ingaggio CROSS-QUOTA (ponti/rialzati) ────────────────
    // Domanda dell'utente: "i cloni sul ponte sparano davvero? i droidi sotto li
    // prendono di mira?". A occhio è impossibile dirlo (proiettili lenti = schermo
    // pieno di tracce non attribuibili). Questi contatori misurano il funnel nei suoi
    // TRE punti di morte, separando cross-quota (|Δy| > VERTICAL_ENGAGE_DY) da piano:
    //   candidati in gittata → bersaglio ACQUISITO → colpo SPARATO.
    // Se vertCand è alto ma vertAcq ~0 → muore in acquisizione (LOS/geometria o i K
    // candidati più vicini sono tutti bloccati). Se vertAcq è alto ma vertShot ~0 →
    // muore al gate di fuoco. `flatShot` è il CONTROLLO: se è alto e vertShot ~0, il
    // combattimento verticale è rotto e non è un problema generale di tiro.
    int vertCand = 0;        // candidati a quota diversa entro l'aggro
    // (Indagine 2026-07-27: qui erano anche `vertCandLos` e i bucket per distanza
    // orizzontale, che costavano una LOS EXTRA per candidato cross-quota. Servivano a
    // stabilire la causa — fatto, vedi 08_KnownIssues #83 — e sono stati tolti: il
    // funnel qui sotto basta come guardia permanente e costa solo confronti.)
    int candTotal = 0;       // TUTTI i candidati (denominatore di vertCand)
    int vertAcq  = 0;        // bersagli acquisiti a quota diversa
    int acqTotal = 0;        // TUTTE le acquisizioni (denominatore di vertAcq)
    int vertShot = 0;        // colpi effettivamente sparati cross-quota
    int vertFireBlocked = 0; // acquisito cross-quota ma LOS di tiro fallita
    int flatShot = 0;        // colpi sparati sullo stesso piano (controllo)
    int acqAllBlocked = 0;   // c'erano candidati ma NESSUNO con LOS (saturazione dei K)
    // ── PERCEZIONE (doc 40 Fase 1): l'effetto va MISURATO, non supposto ──
    int fovRejected = 0;     // bersagli scartati perché FUORI dal campo visivo
    int heardShots  = 0;     // contatti nati dall'UDITO (spari sentiti)
    // Confidenza (A2): quante volte l'informazione era buona abbastanza da INGAGGIARE
    // e quante invece solo da andare a GUARDARE. Il rapporto dice se il modello vive.
    int highConfContact = 0;
    int lowConfInvestigate = 0;
    // Soppressione (A3): tick sotto tiro e tick INCHIODATI. Se "inchiodati" fosse
    // vicino a "soppressi", la taratura starebbe paralizzando le AI invece di premerle.
    int suppressedTicks = 0;
    int pinnedTicks = 0;
    // Quante volte si e ANNUNCIATO un inchiodamento: serve a sapere se il feed
    // informa o SPAMMA — un canale che spamma smette di essere letto.
    int pinnedAnnounced = 0;
    // Ruoli assegnati (A4), indicizzati 1=sopprime 2=aggira 3=avanza. Se uno restasse
    // a zero, la composizione o le affinita sarebbero sbagliate.
    int roleAssigned[4] = {0,0,0,0};
    // ── FUNNEL D'INGAGGIO (KI #86) ──────────────────────────────────────────
    // Sintomo riportato: "è estremamente comune vedere AI che stanno davanti a dei
    // nemici senza sparare". A occhio non si distingue **non vede** da **vede e non
    // spara**, e sono due bug diversi in due file diversi. Il funnel separa i due
    // tronconi e, dentro il secondo, QUALE gate uccide il colpo.
    //
    // PERCEZIONE (contati solo nel tick di sensing, così i tre numeri hanno lo
    // stesso denominatore e il rapporto è leggibile):
    int occInRange = 0;      // AI-tick con almeno un nemico ENTRO L'AGGRO (ignorando il cono)
    int occInCone  = 0;      // ...di cui almeno uno passa il CAMPO VISIVO
    int occAcquired = 0;     // ...di cui almeno uno con LOS → bersaglio acquisito
    // GATE DI FUOCO (contati ogni tick, ma SOLO per chi HA un bersaglio: è
    // esattamente la popolazione del sintomo). `gateCooldown` che domina è NORMALE
    // — la cadenza di tiro è ~1 colpo/s su 60 tick: il segnale sta negli ALTRI.
    int gateState = 0;       // ha un bersaglio ma NON è in Alert (anomalia vera)
    int gateEvading = 0;     // in fase "hide" del peek/hide
    // Decomposizione di `gateEvading` (indagine KI #86): `exposeTimer` scorre SOLO
    // nel ramo "Alert + bersaglio + non in manovra + non stationary". Negli altri
    // rami la fase di hide è CONGELATA — e il gate di fuoco continua a bloccare.
    // Questi due contatori dicono quanta parte del "non spara" è ciclo legittimo e
    // quanta è timer fermo.
    int evadingInRepo = 0;   // evasivo MENTRE manovra (timer congelato)
    int evadingFrozen = 0;   // evasivo e stationary (timer mai decrementato)
    // CONGELAMENTO della fase evasiva: la misura DIRETTA del sintomo di KI #86.
    // Gli eventi di combattimento aggregati non servono a deciderlo — fra due run la
    // simulazione diverge e la differenza non è attribuibile. Questi due sì: con
    // hide_duration_max = 1.8 s, una fase oltre 3 s è un bug per definizione.
    int evadeStuckTicks = 0;   // tick-AI con fase evasiva oltre 3 s
    float evadeMaxSec = 0.0f;  // la fase evasiva più lunga osservata nella finestra
    // La RICERCA usa il mondo o tira a caso? (KI #86 causa 3) Se il fallback dominasse,
    // la mappa non offre posizioni che vedano le zone dove si perde il contatto — è un
    // difetto di autoring, non di AI, e va letto lì invece che dedotto dal comportamento.
    int searchTactical = 0, searchRandom = 0;
    // ABILITY (O5): quante volte si attiva davvero l'unica ability con un trigger
    // a runtime. Se resta 0 con entità che la portano, è peso morto misurato.
    int rollUsed = 0;
    int gateCooldown = 0;    // cadenza di tiro (atteso alto)
    int gateOverheat = 0;    // arma surriscaldata
    int gateLos = 0;         // LOS ri-verificata al tiro: fallita (bersaglio coperto)
    int gateFired = 0;       // colpo partito
    void reset() { *this = TacticalStats{}; }
};
static TacticalStats g_tac;

// Pseudo-random leggero e deterministico per la dispersione dei colpi
// (niente <random>: qualità sufficiente e zero stato globale condiviso).
static float aiRand01()
{
    static unsigned s = 0x9E3779B9u;
    s = s * 1664525u + 1013904223u;
    return (float)(s >> 8) / 16777216.0f;
}

static void aiMove(TransformComponent& et, float nx, float nz,
                   AiComponent& ai, float dt, World& world)
{
    const glm::vec3 H = config::aiHalf();
    const glm::vec3 prev = {et.x, et.y, et.z};
    const glm::vec3 next = {nx, et.y, nz};
    const glm::vec3 r = physics::slideMoveWithStepUp(prev, next, H, world, config::STEP_HEIGHT);
    et.x = r.x; et.y = r.y; et.z = r.z;

    ai.velY += config::AI_GRAVITY * dt;
    const float ny = et.y + ai.velY * dt;
    if (!physics::hasCollision({et.x, ny, et.z}, H, world))
        et.y = ny;
    else if (ai.velY < 0.0f)
        ai.velY = 0.0f;
}

static float norm2D(float& dx, float& dz)
{
    float len = std::sqrt(dx*dx + dz*dz);
    if (len > 0.001f) { dx /= len; dz /= len; }
    return len;
}

static float aiRandRange(float lo, float hi)
{
    return lo + (hi - lo) * aiRand01();
}

// Nome leggibile dello stato AI per la telemetria (ADR-016, 06_Todo #1).
// Non più `static`: lo usa anche AiTrace.cpp (dichiarato in AiInternal.hpp) — due
// tabelle di nomi divergerebbero al primo stato nuovo.
const char* aiStateName(AiState s)
{
    switch (s) {
        case AiState::Patrol: return "Patrol";
        case AiState::Alert:  return "Alert";
        case AiState::Hunt:   return "Hunt";
        case AiState::Search: return "Search";
    }
    return "?";
}

// Ingresso in Hunt — SCELTA DELL'APPROCCIO (ADR-029).
// Il mondo offre le opzioni (coperture con protezione, punti dominanti, fianchi),
// il PROFILO le pesa (aggression / flank_chance / cover_preference) e il `bias`
// individuale decorrela le unità. Prima si andava quasi sempre addosso in linea
// retta: da qui nascono gli attacchi da più punti. Il punto scelto diventa un
// waypoint di approccio (`flankActive`), poi si prosegue sulla posizione nota →
// nessuno stato nuovo nella macchina AI.
static void enterHunt(AiComponent& ai, const TransformComponent& et, const World& world)
{
    ai.state = AiState::Hunt;
    ai.repositionActive = false;   // la manovra di combattimento finisce col contatto (ADR-035)
    // Il RUOLO (A4) vale per QUESTO ingaggio: perso il contatto si rilascia, così al
    // prossimo scontro la squadra si ridistribuisce sulla situazione nuova invece di
    // trascinarsi compiti decisi per una battaglia che non esiste più.
    ai.combatRole = 0;
    if (!ai.hasLastKnown || ai.flankActive) return;

    const float tx = ai.lastKnownX, tz = ai.lastKnownZ;
    float dx = tx - et.x, dz = tz - et.z;
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len < 2.0f) return;          // già addosso: nessuna manovra sensata
    dx /= len; dz /= len;

    struct Opt { float w, x, z; int kind; };   // kind: 0 diretto,1 fianco,2 tiro,3 dominante
    Opt opts[4];
    int n = 0;
    // 1) Assalto diretto sulla posizione nota.
    opts[n++] = {0.2f + ai.aggression, tx, tz, 0};
    // 2) Aggiramento: lato e ampiezza dal bias → unità diverse aggirano da lati
    //    diversi invece di infilarsi tutte nello stesso corridoio.
    if (ai.flankChance > 0.001f)
    {
        const float side = (ai.bias < 0.5f) ? 1.0f : -1.0f;
        const float off  = 5.0f + ai.bias * 4.0f;
        opts[n++] = {ai.flankChance * 1.5f,
                     tx + (-dz * side) * off, tz + (dx * side) * off, 1};
    }
    // 3) POSIZIONE DI TIRO (ADR-031): non una copertura qualsiasi rivolta verso il
    //    bersaglio, ma una da cui si può davvero fare fuoco su di esso (settore +
    //    gittata). È questo che trasforma la copertura da nascondiglio in posizione
    //    d'attacco: l'AI si sposta lì PER COLPIRE, restando riparata.
    if (world.activeMap && ai.coverPreference > 0.001f)
    {
        // Quota del bersaglio (ultimo contatto, a terra): suolo alla sua XZ + corpo.
        const float ty = mapquery::groundHeightAt(world.activeMap, tx, tz) + 0.5f;
        if (const TacticalPositionDef* c = worldintel::bestFiringPosition(
                *world.activeMap, et.x, et.z, tx, ty, tz, 18.0f))
            opts[n++] = {ai.coverPreference * (0.5f + c->protection), c->x, c->z, 2};
    }
    // 4) Punto dominante autorato vicino al bersaglio (Tactical Points, ADR-027).
    if (world.activeMap)
        if (const TacticalPositionDef* t = worldintel::nearestPositionByRole(
                *world.activeMap, tx, tz, "vantage", 20.0f))
            opts[n++] = {0.4f * (ai.flankChance + ai.coverPreference)
                         + t->importance * 0.5f, t->x, t->z, 3};

    float total = 0.0f;
    for (int i = 0; i < n; ++i) total += opts[i].w;
    if (total <= 0.0f) return;

    float r = aiRand01() * total;
    int chosen = 0;
    for (int i = 0; i < n; ++i) { r -= opts[i].w; if (r <= 0.0f) { chosen = i; break; } }
    // Osservabilità: quale approccio ha vinto (l'ordine di inserimento è noto).
    if      (chosen == 0)              ++g_tac.approachDirect;
    else if (opts[chosen].kind == 1)   ++g_tac.approachFlank;
    else if (opts[chosen].kind == 2)   ++g_tac.approachFiring;
    else                               ++g_tac.approachVantage;
    if (chosen > 0)   // 0 = diretto: nessun waypoint di approccio
    {
        ai.flankX = opts[chosen].x;
        ai.flankZ = opts[chosen].z;
        ai.flankActive = true;
    }
}

// ── Consumo Map Metadata (18_AiMapConsumption) ───────────────────────────

// La scelta della copertura vive nel World Intelligence Layer
// (`worldintel::bestCoverToward`, ADR-025/026) — seam unico, scelta per protezione.

// Repulsione dalle danger zone: piega il vettore di movimento lontano dal
// centro delle aree pericolose (pesata su dangerLevel e vicinanza). Solo
// fuori dall'ingaggio: sotto fuoco si combatte, non si scappa dagli hint.
static void applyDangerRepulsion(const MapDef* map, float x, float z,
                                 float& moveDX, float& moveDZ)
{
    if (!map || map->dangerZones.empty()) return;
    for (const auto& d : map->dangerZones)
    {
        const float dx = x - d.x, dz = z - d.z;
        const float dist = std::sqrt(dx*dx + dz*dz);
        if (dist >= d.radius || dist < 0.01f) continue;
        // Peso 0 al bordo, massimo al centro; scala con dangerLevel
        const float w = d.dangerLevel * (1.0f - dist / d.radius) * 1.5f;
        moveDX += (dx / dist) * w;
        moveDZ += (dz / dist) * w;
    }
    norm2D(moveDX, moveDZ);
}

// Avanzamento della pattuglia (ADR-028). Con una route autorata l'unità passa al
// segmento SUCCESSIVO (wrap a fine percorso) e ricalcola A/B dai punti autorati:
// la pattuglia percorre il tracciato invece di fare avanti-indietro su un tratto.
// Senza route (patrolRoute < 0) resta l'inversione A↔B storica.
static void advancePatrol(AiComponent& ai, const MapDef* map)
{
    if (map && ai.patrolRoute >= 0 && ai.patrolRoute < (int)map->patrolRoutes.size())
    {
        const auto& pts = map->patrolRoutes[ai.patrolRoute].points;
        const int P = (int)pts.size();
        if (P >= 2)
        {
            // `patrolSeg` = indice del PUNTO-obiettivo. Avanza nel verso corrente e
            // INVERTE agli estremi (ADR-045): niente più salto-teletrasporto dal
            // fondo all'inizio; la pattuglia percorre il tracciato avanti e indietro.
            int step = ai.patrolReverse ? -1 : 1;
            int next = ai.patrolSeg + step;
            if (next >= P)       { ai.patrolReverse = true;  next = P - 2; }
            else if (next < 0)   { ai.patrolReverse = false; next = 1; }
            if (next < 0) next = 0;   // route a 1 solo punto (degenerata)
            ai.patrolSeg = next;
            ai.patrolBx = pts[next][0]; ai.patrolBz = pts[next][2];
            ai.goingToB = true;   // per le route il bersaglio è sempre B = pts[seg]
            return;
        }
    }
    ai.goingToB = !ai.goingToB;   // legacy: avanti-indietro sul singolo segmento
}

// Raccoglie la route più vicina dal PUNTO più vicino (ADR-045): l'unità non è
// più incatenata a una route/segmento pre-assegnati — quando (ri)entra in
// pattuglia prende il tracciato più comodo e vi si aggancia dove capita, non
// solo dagli estremi. Rende le route una RETE condivisa e fluida.
static void joinNearestRoute(AiComponent& ai, const MapDef* map, float x, float z)
{
    if (!map || map->patrolRoutes.empty()) return;
    int   bestRoute = -1, bestPt = 0;
    float bestD2 = 1e18f;
    for (size_t r = 0; r < map->patrolRoutes.size(); ++r)
    {
        const auto& pts = map->patrolRoutes[r].points;
        for (size_t p = 0; p < pts.size(); ++p)
        {
            const float dx = pts[p][0] - x, dz = pts[p][2] - z;
            const float d2 = dx * dx + dz * dz;
            if (d2 < bestD2) { bestD2 = d2; bestRoute = (int)r; bestPt = (int)p; }
        }
    }
    if (bestRoute < 0) return;
    ai.patrolRoute = bestRoute;
    ai.patrolSeg   = bestPt;   // parte dal punto più vicino
    const auto& pts = map->patrolRoutes[bestRoute].points;
    ai.patrolBx = pts[bestPt][0]; ai.patrolBz = pts[bestPt][2];
    ai.goingToB = true;
    // Verso: se sono nella seconda metà del tracciato, conviene tornare indietro
    // (così non si esce subito dal bordo). Sceglie il verso con più strada davanti.
    ai.patrolReverse = (bestPt > (int)pts.size() / 2);
}


// Genera un punto di ricerca attorno all'ultima posizione nota (raggio
// ~12m) — mappa-agnostico. Le vecchie coordinate globali -8..+8 erano
// l'arena hardcoded pre-firebase: su una mappa 50x40 ammassavano tutte
// le AI al centro, uccidendo la battaglia.
// `map` serve a NON scegliere punti fuori dai confini: un punto oltre un muro
// perimetrale si aggancia al navmesh proprio dove l'unità già si trova → resta
// ferma finché l'anti-stuck non la sblocca (osservato in telemetria: unità in
// Search premute contro il muro sud). Clampando nei confini il punto è sempre
// raggiungibile e la ricerca resta utile.
static void pickSearchPoint(AiComponent& ai, float x, float z, const MapDef* map)
{
    const float cx = ai.hasLastKnown ? ai.lastKnownX : x;
    const float cz = ai.hasLastKnown ? ai.lastKnownZ : z;

    // ── PRIMA CHIEDI AL MONDO (KI #86 causa 3) ────────────────────────────────
    // Questa era l'unica decisione dell'AI che ignorava del tutto il mondo tattico:
    // un punto UNIFORMEMENTE CASUALE in 24×24 m, su una mappa con 169 posizioni
    // autorate e il grafo delle coperture. Un soldato che ha perso il contatto non
    // vaga a caso: si porta in un punto **da cui quella zona si VEDE**, e guarda.
    // Ed è la risposta giusta al collo di bottiglia misurato — la LOS, non il FOV:
    // cercare da un punto senza linea di vista è cercare senza poter trovare.
    //
    // `bestFiringPosition` verifica davvero la linea (oltre a settore e gittata), che
    // è il motivo per cui si usa questa e non una copertura qualsiasi. Il centro è
    // JITTERATO: senza, tutte le unità e tutti i tentativi successivi ricadrebbero
    // sulla stessa "migliore" posizione — la ricerca smetterebbe di essere una ricerca.
    if (map)
    {
        const float jx = cx + (aiRand01() - 0.5f) * 16.0f;
        const float jz = cz + (aiRand01() - 0.5f) * 16.0f;
        const float jy = mapquery::groundHeightAt(map, jx, jz) + 0.5f;
        if (const TacticalPositionDef* p =
                worldintel::bestFiringPosition(*map, x, z, jx, jy, jz, 22.0f))
        {
            ai.searchX = p->x; ai.searchZ = p->z;
            ++g_tac.searchTactical;
            return;
        }
    }
    // FALLBACK documentato: il mondo non offre nulla che veda quella zona (mappa
    // povera di posizioni, o zona non coperta da nessuna). Meglio guardarsi intorno
    // a caso che restare fermi — è il comportamento storico, conservato apposta.
    ++g_tac.searchRandom;
    float sx = cx + (aiRand01() - 0.5f) * 24.0f;
    float sz = cz + (aiRand01() - 0.5f) * 24.0f;

    if (map && !map->geometry.empty())
    {
        float minX = 1e18f, maxX = -1e18f, minZ = 1e18f, maxZ = -1e18f;
        for (const auto& b : map->geometry)
        {
            const float hx = b.sx * 0.5f, hz = b.sz * 0.5f;
            if (b.x - hx < minX) minX = b.x - hx;
            if (b.x + hx > maxX) maxX = b.x + hx;
            if (b.z - hz < minZ) minZ = b.z - hz;
            if (b.z + hz > maxZ) maxZ = b.z + hz;
        }
        const float pad = 2.0f;   // resta dentro, lontano dai muri perimetrali
        minX += pad; maxX -= pad; minZ += pad; maxZ -= pad;
        if (minX < maxX) { if (sx < minX) sx = minX; if (sx > maxX) sx = maxX; }
        if (minZ < maxZ) { if (sz < minZ) sz = minZ; if (sz > maxZ) sz = maxZ; }
    }
    ai.searchX = sx;
    ai.searchZ = sz;
}

void AiSystem::update(World& world, float dt)
{
    ZoneScoped;   // ADR-015: AI update loop
    // `snap` = solo le entità con un Team (unità, strutture, player) — NON i box di
    // geometria. Tutti i passaggi AI qui sotto filtrano già a team/AI (saltano ciò
    // che non ha team), quindi escludere la geometria a monte è a comportamento
    // INVARIATO, ma toglie ~175 iterazioni × ~10 passaggi/frame su Training Ground:
    // era il costo che scala con la DIMENSIONE della mappa, non col numero di AI
    // (misurato: 12→50 AI = +10%; il grosso è la geometria). Perf, ADR-015.
    std::vector<EntityId> snap;
    {
        const auto& all = world.getEntities();
        snap.reserve(all.size());
        for (EntityId e : all) if (world.getTeam(e)) snap.push_back(e);
    }
    const std::uint64_t tick = world.getTickCount();   // time-slicing (Fase 4)
    m_time += dt;   // cadenza delle decisioni di comando ed età dei contatti (doc 34)
    // Occupancy CENTRALE delle posizioni (torre-hub): azzerata ogni tick e ricostruita
    // man mano che i cloni rivendicano; dimensionata alle posizioni della mappa.
    {
        const size_t np = world.activeMap ? world.activeMap->tacticalPositions.size() : 0;
        world.allyTac.claimed.assign(np, 0);
    }

    // I SISTEMI sopravvivono a `World::initialize()`, lo stato del World no: a
    // inizio partita i contatti della partita PRECEDENTE erano ancora qui e le
    // unità nascevano già "informate" di nemici che non esistono più.
    if (tick == 0 && !m_contacts.empty()) m_contacts.clear();

    // Livello di COMANDO (settori, torre di controllo, quadro tattico, direttive del
    // Droide Tattico): vive in AiCommandLayer.cpp dopo lo split del monolite (audit #7).
    const float cmdDrift = updateEnemyCommand(world, snap, dt);

    // ── Bounding overwatch EMERGENTE (ADR-035) ───────────────────────────
    // Quanti di ogni squadra si stanno già riposizionando: sotto si consente a
    // una nuova unità di muoversi solo se non si stanno muovendo già in troppi.
    // Alcune avanzano, le altre restano a fare fuoco — l'effetto "ci copriamo a
    // vicenda" senza alcun coordinamento esplicito.
    int repositioning[3] = {0, 0, 0};
    int teamAlive[3]     = {0, 0, 0};
    // Saturazione dei RUOLI di combattimento (A4), per team: quanti stanno già
    // sopprimendo / aggirando / avanzando. È il denominatore che impedisce a tutti di
    // scegliere lo stesso compito — lo stesso principio dell'occupancy delle posizioni.
    int roleCount[3][4]  = {};
    int engagedCount[3]  = {0, 0, 0};
    for (EntityId e : snap)
    {
        const auto* a  = world.getAi(e);
        const auto* tm = world.getTeam(e);
        const auto* h  = world.getHealth(e);
        if (!a || !tm || !h || h->current <= 0.0f) continue;
        const int t = (tm->teamId == 1 || tm->teamId == 2) ? tm->teamId : 0;
        ++teamAlive[t];
        if (a->repositionActive) ++repositioning[t];
        if (a->combatRole > 0 && a->combatRole < 4)
        { ++roleCount[t][a->combatRole]; ++engagedCount[t]; }
    }

    // Heartbeat diagnostico (ogni ~10s a 60Hz): quante AI e in che stato.
    // Rende osservabile da telemetria il sintomo "AI ferme".
    if (world.getTickCount() % 600 == 1)
    {
        int nAi = 0, patrol = 0, alert = 0, hunt = 0, search = 0, stat = 0, onRoute = 0;
        for (EntityId e : snap)
            if (const auto* a = world.getAi(e))
            {
                ++nAi;
                if (a->stationary) ++stat;
                if (a->patrolRoute >= 0) ++onRoute;   // ADR-045: route fluide
                switch (a->state)
                {
                case AiState::Patrol: ++patrol; break;
                case AiState::Alert:  ++alert;  break;
                case AiState::Hunt:   ++hunt;   break;
                case AiState::Search: ++search; break;
                }
            }
        telemetry::logTrace("ai: " + std::to_string(nAi)
            + " (patrol " + std::to_string(patrol)
            + ", alert " + std::to_string(alert)
            + ", hunt " + std::to_string(hunt)
            + ", search " + std::to_string(search)
            + ", fermi " + std::to_string(stat) + ")");

        // ── Le AI stanno USANDO i metadata? ──────────────────────────
        // Report esplicito delle DECISIONI: quali approcci scelgono e, soprattutto,
        // quante volte il mondo NON offre nulla (miss). Se i "miss" dominano, il
        // problema è nei dati/nelle query, non nel comportamento — distinzione che
        // senza questi numeri non si può fare.
        int inRepo = 0;
        for (EntityId e : snap)
            if (const auto* a = world.getAi(e)) if (a->repositionActive) ++inRepo;
        // Ordini di squadra (team 1): quanti sono agganciati al Follow "fisso" e
        // quanti hanno un ordine vero. Serve a capire quanto i cloni siano di fatto
        // scortatori invece che truppe che operano.
        int ordFollow = 0, ordAltri = 0, ordNessuno = 0;
        for (EntityId e : snap)
        {
            const auto* s = world.getSquad(e);
            const auto* h = world.getHealth(e);
            if (!s || !h || h->current <= 0.0f || s->downed) continue;
            if (!s->hasActiveOrder())            ++ordNessuno;
            else if (s->order == OrderType::Follow) ++ordFollow;
            else                                  ++ordAltri;
        }
        // Riepilogo delle direttive (doc 32 v2): quanti fronti, e quanti in
        // AVANZATA vs TIENI vs RIPIEGA. Sostituisce la singola stance globale —
        // ora si vede che il comandante gestisce più fronti con posture diverse.
        int dirAdv = 0, dirHold = 0, dirRet = 0;
        for (const auto& d : world.enemyCommand.directives)
        {
            if (d.stance == World::EnemyCommand::Advance)      ++dirAdv;
            else if (d.stance == World::EnemyCommand::Retreat) ++dirRet;
            else                                               ++dirHold;
        }
        const std::string cmdTop =
            !world.enemyCommand.commanderAlive ? "nessun-comandante"
            : world.enemyCommand.directives.empty() ? "nessun-fronte"
            : world.enemyCommand.directives.front().label;
        telemetry::event(telemetry::Level::Info, "AI", "tactical decisions",
            {{"in_patrol", patrol}, {"in_alert", alert},
             {"in_hunt", hunt},     {"in_search", search},
             {"fermi", stat},       {"in_manovra", inRepo},
             {"sq_follow", ordFollow}, {"sq_altri_ordini", ordAltri},
             {"sq_senza_ordine", ordNessuno},
             {"su_route", onRoute},   // ADR-045: unità agganciate a una route
             {"cmd_fronti", (int)world.enemyCommand.directives.size()},
             {"cmd_avanzata", dirAdv}, {"cmd_tieni", dirHold}, {"cmd_ripiega", dirRet},
             {"cmd_obiettivo", cmdTop},
             {"cmd_deriva_m", cmdDrift},   // leash (ADR-041): ≤ raggio autorato
             // Rete di comunicazione (doc 34): senza questi non si distingue
             // "l'AI non reagisce" da "l'informazione non le è ancora arrivata".
             {"comms_cloni",  world.comms[1].degraded() ? "degradate" : "ok"},
             {"comms_droidi", world.comms[2].degraded() ? "degradate" : "ok"},
             {"contatti_vivi", (int)m_contacts.size()},
             {"strutture_ingaggiate", g_tac.structureEngage},   // doc 35
             {"strutture_note", (int)world.strategicTargets.size()},
             // Torre di controllo (doc 36): quanti SEGNALI esistono e quante
             // volte un clone ne ha scelto uno. Se i segnali ci sono ma nessuno
             // li segue, il problema è nel ramo di pattuglia, non nella torre.
             {"torre_controllo", world.allyIntel.active ? "attiva" : "assente"},
             {"segnali_cloni", (int)world.allyIntel.signals.size()},
             {"segnali_seguiti", g_tac.allySignalFollow},
             {"segnale_affollamento_max", [&]{ int m = 0;
                 for (const auto& s : world.allyIntel.signals) if (s.crowd > m) m = s.crowd;
                 return m; }()},
             {"strutture_ingaggiabili", [&]{ int n = 0;
                 for (const auto& s : world.strategicTargets)
                     if (s.entity != 0 && s.engageRadius > 0.0f) ++n;
                 return n; }()},
             {"approccio_diretto",   g_tac.approachDirect},
             {"approccio_fianco",    g_tac.approachFlank},
             {"approccio_tiro",      g_tac.approachFiring},
             {"approccio_dominante", g_tac.approachVantage},
             {"manovra_valutata",    g_tac.repoEval},
             {"manovra_bloccata_da_altri", g_tac.repoBlockedBusy},
             {"overwatch_avviati", g_tac.overwatchStarted},
             {"hold_su_posizione", g_tac.holdPosOccupied},   // ADR-046
             {"obs_vista_estesa",  g_tac.obsSightBoost},      // ADR-046
             // Funnel VERTICALITÀ (ponti/rialzati): candidati → acquisiti → sparati.
             // Guardia permanente: rende verificabile in ogni momento se il
             // combattimento cross-quota funziona, senza doverlo giudicare a occhio.
             {"vert_candidati",     g_tac.vertCand},
             {"tot_candidati",      g_tac.candTotal},   // denominatore di vert_candidati
             {"vert_acquisiti",     g_tac.vertAcq},
             {"tot_acquisizioni",   g_tac.acqTotal},    // denominatore di vert_acquisiti
             {"vert_colpi",         g_tac.vertShot},
             {"vert_tiro_bloccato", g_tac.vertFireBlocked},
             {"piano_colpi",        g_tac.flatShot},
             {"acq_tutti_bloccati", g_tac.acqAllBlocked},
             // Percezione (doc 40): quanto pesa il campo visivo e quanto l'udito.
             {"fuori_campo_visivo", g_tac.fovRejected},
             {"spari_uditi",        g_tac.heardShots},
             {"contatti_certi",     g_tac.highConfContact},      // A2: si ingaggia
             {"contatti_da_indagare", g_tac.lowConfInvestigate},  // A2: si va a guardare
             {"tick_soppressi",     g_tac.suppressedTicks},        // A3
             {"tick_inchiodati",    g_tac.pinnedTicks},            // A3: soglia PINNED
             {"annunci_inchiodato", g_tac.pinnedAnnounced},        // A3: informa o spamma?
             // Funnel d'INGAGGIO (KI #86): il percorso completo da "c'è un nemico"
             // a "parte un colpo", con il nome del gate che lo ferma. Guardia
             // permanente: "AI davanti a un nemico che non spara" torna misurabile.
             {"occ_in_raggio",  g_tac.occInRange},    // sensing tick: nemico entro l'aggro
             {"occ_nel_cono",   g_tac.occInCone},     // ...visibile nel campo visivo
             {"occ_acquisito",  g_tac.occAcquired},   // ...con LOS → bersaglio
             {"gate_stato",     g_tac.gateState},     // ha bersaglio ma non è in Alert
             {"gate_evasivo",   g_tac.gateEvading},
             {"evasivo_in_manovra", g_tac.evadingInRepo},   // timer congelato
             {"evasivo_fermo",      g_tac.evadingFrozen},
             {"evasivo_congelato_tick", g_tac.evadeStuckTicks},   // fase hide oltre 3 s
             {"evasivo_durata_max_s",   g_tac.evadeMaxSec},
             {"ricerca_tattica",  g_tac.searchTactical},   // ha chiesto al mondo
             {"ricerca_a_caso",   g_tac.searchRandom},     // fallback: il mondo non offriva nulla
             {"roll_attivati",    g_tac.rollUsed},        // O5: ability effettivamente usate
             {"gate_cooldown",  g_tac.gateCooldown},  // atteso alto (cadenza di tiro)
             {"gate_surriscaldato", g_tac.gateOverheat},
             {"gate_los_tiro",  g_tac.gateLos},       // coperto al momento del tiro
             {"gate_sparato",   g_tac.gateFired},
             {"ruolo_sopprime",     g_tac.roleAssigned[1]},        // A4
             {"ruolo_aggira",       g_tac.roleAssigned[2]},
             {"ruolo_avanza",       g_tac.roleAssigned[3]},
             {"fianco_trovato",      g_tac.repoFlankHit},
             {"fianco_assente",      g_tac.repoFlankMiss},
             {"tiro_trovato",        g_tac.repoFiringHit},
             {"tiro_assente",        g_tac.repoFiringMiss},
             {"manovre_avviate",     g_tac.repoStarted},
             {"scartate_troppo_vicine", g_tac.repoTooClose}});

        // ── OSSERVABILITÀ DISTRIBUZIONE (ricerca #1, 2026-07-24) ──────────
        // Occupazione per-settore nel tempo: rende MISURABILE se gli alleati/nemici
        // si spalmano sui settori (contesi/importanti) o si ammassano. È il dato che
        // mancava per giudicare se la "base" distribuisce davvero. Permanente: rete
        // anti-regressione contro i bug silenziosi di distribuzione (metodo: rendere
        // visibili le decisioni, misurare lo scarto dall'atteso).
        if (const MapDef* dm = world.activeMap)
            if (!dm->sectors.empty())
            {
                nlohmann::json secArr = nlohmann::json::array();
                for (std::size_t s = 0; s < dm->sectors.size(); ++s)
                {
                    const auto& sec = dm->sectors[s];
                    const World::SectorState st = (s < world.sectorStates.size())
                        ? world.sectorStates[s] : World::SectorState{};
                    secArr.push_back({{"s", sec.label}, {"imp", sec.importance},
                                      {"all", st.allies}, {"nem", st.enemies},
                                      {"press", st.pressure}});
                }
                telemetry::event(telemetry::Level::Info, "AI", "sector distribution",
                                 {{"n", (int)dm->sectors.size()}, {"settori", secArr}});
            }

        aitrace::flush();   // stalli per causa, stesso battito
        g_tac.reset();
    }

    // ── Raccogli bersagli per team (SoA flat, Fase 3) ────────────────
    // id + posizione catturati UNA volta in array contigui paralleli. I loop
    // di ricerca del nearest (sotto, O(AI x bersagli)) leggono team*Pos[i]
    // contiguo invece di fare un getTransform(tgt) — un lookup hash +
    // pointer-chase su heap sparso — per ogni coppia. Il componente pesante
    // viene recuperato SOLO per il bersaglio selezionato (getTransform(nearest)).
    std::vector<EntityId>  team1Tgts, team2Tgts;
    std::vector<glm::vec3> team1Pos,  team2Pos;
    // Le STRUTTURE non sono bersagli-unità (doc 35). Vanno escluse da qui,
    // altrimenti — ora che il LOS non le occlude più da sé — verrebbero
    // ingaggiate per semplice vicinanza come un soldato qualsiasi, scavalcando
    // il raggio AUTORATO e facendo sparare i droidi a un edificio invece che a
    // chi gli spara addosso. Rientrano solo dal percorso opportunistico, sotto.
    auto isStructure = [&world](EntityId id)
    {
        for (const auto& s : world.strategicTargets)
            if (s.entity == id) return true;
        return false;
    };
    for (EntityId e : snap)
    {
        const auto* tm = world.getTeam(e);
        if (!tm || world.getBullet(e)) continue;
        if (isStructure(e)) continue;
        const auto* et = world.getTransform(e);
        if (!et) continue;   // senza transform non può essere un bersaglio valido
        // UN CADUTO NON È UNA MINACCIA (2026-08-04). Questa lista non filtrava gli
        // HP, quindi un alleato a terra (hp 0) restava un bersaglio a pieno titolo:
        // i droidi lo acquisivano e gli sparavano addosso — mentre `CombatSystem`
        // scartava i proiettili su chi ha `current <= 0`, cioè non potevano
        // colpirlo. Risultato osservato dall'utente: nemici che girano attorno a un
        // caduto invulnerabile sparandogli all'infinito, ignorando la battaglia.
        // Un bersaglio inerme che non si può nemmeno colpire è la peggiore
        // calamita d'attenzione possibile.
        // Il fuoco INCROCIATO può ancora finirlo (fix gemello in CombatSystem): a
        // cambiare è solo che nessuno lo SCEGLIE. È la lettura minima di "priorità
        // molto minore rispetto a chi è in piedi" (doc 26 Phase D); una priorità
        // graduata vera arriverà con quel lavoro.
        if (const auto* sqd = world.getSquad(e); sqd && sqd->downed) continue;
        const glm::vec3 p = {et->x, et->y, et->z};
        if (tm->teamId == 1)      { team1Tgts.push_back(e); team1Pos.push_back(p); }
        else if (tm->teamId == 2) { team2Tgts.push_back(e); team2Pos.push_back(p); }
    }

    // ── CONTATTI CONDIVISI (LOCALMENTE) ──────────────────────────────────
    // Prima esisteva UNA sola lastKnown per team, propagata a TUTTE le unità:
    // bastava che un droide vedesse un clone perché l'intero esercito
    // convergesse su quel punto → due blocchi che si scontrano sempre sullo
    // stesso fronte, partite identiche (feedback utente 2026-07-20).
    // Ora ogni avvistamento è un CONTATTO con la sua posizione; ogni unità
    // adotta solo il contatto più vicino entro AI_CONTACT_SHARE_RADIUS. Chi è
    // lontano non ne sa nulla e continua il suo compito → fronti indipendenti.
    // I contatti PERSISTONO fra i tick con la loro età (doc 34): è ciò che
    // permette alla rete di comunicazione di ritardarli. Prima si invecchiano i
    // vecchi e si scartano gli scaduti, poi si registrano quelli di questo tick.
    // Invecchiamento + DECADIMENTO della confidenza (doc 40 A2): c(t) = c0·e^(−t/τ).
    // L'età sola diceva "quando", non "quanto ci credo".
    const float confDecay = std::exp(-dt / config::CONTACT_CONFIDENCE_TAU);
    for (auto& c : m_contacts) { c.age += dt; c.confidence *= confDecay; }
    m_contacts.erase(std::remove_if(m_contacts.begin(), m_contacts.end(),
                                    [](const SharedContact& c)
                                    { return c.age > config::COMMS_CONTACT_TTL; }),
                     m_contacts.end());

    for (EntityId e : snap)
    {
        auto* ai   = world.getAi(e);   if (!ai) continue;
        // Time-slicing (Fase 4): solo gli AI schedulati questo tick fanno la
        // scansione LOS O(N²). Scaglionati per entità → ~1/AI_SENSE_INTERVAL
        // contribuiscono per tick, ma su INTERVAL tick contribuiscono tutti.
        if ((tick + e) % config::AI_SENSE_INTERVAL != 0) continue;
        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

        const int myTeam = team->teamId;
        const auto& targets = (myTeam == 1) ? team2Tgts : team1Tgts;
        const auto& tgtPos  = (myTeam == 1) ? team2Pos  : team1Pos;
        const glm::vec3 ePos = {et->x, et->y, et->z};
        const glm::vec3 eyePos = {et->x, et->y + kEyeUp, et->z};   // LOS ad altezza peek (KI #79)

        int losChecks = 0;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            if (targets[i] == e) continue;
            const glm::vec3& tp = tgtPos[i];   // contiguo, niente hash lookup
            float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
            if (d2 >= ai->aggroRange * ai->aggroRange) continue;
            if (++losChecks > config::AI_MAX_LOS_CHECKS) break;   // Fase 4b: bounda la LOS
            // Origine ad altezza occhi (scavalca la PROPRIA cover); bersaglio al CORPO
            // (`tp`, il transform ~0.5 m): un nemico in cover resta protetto, e il
            // colpo non passa sopra la sua testa (KI #79).
            if (!physics::hasLineOfSight(eyePos, tp, world)) continue;

            // Contatto: chi l'ha visto lo segnala coi propri compagni VICINI.
            // Età 0 = appena avvistato; quando diventa utilizzabile lo decide la
            // rete di comunicazione della SUA fazione (doc 34).
            // Deduplica: se in quell'area c'è già un campione recente della
            // stessa fazione, questo avvistamento non aggiunge informazione.
            bool dup = false;
            for (const auto& c : m_contacts)
            {
                if (c.team != myTeam || c.age > config::COMMS_CONTACT_MERGE_AGE) continue;
                const float ddx = c.x - tp.x, ddz = c.z - tp.z;
                if (ddx * ddx + ddz * ddz
                    < config::COMMS_CONTACT_MERGE_DIST * config::COMMS_CONTACT_MERGE_DIST)
                { dup = true; break; }
            }
            if (!dup) m_contacts.push_back({tp.x, tp.z, myTeam, 0.0f,
                                            config::CONTACT_CONFIDENCE_SIGHT});   // VISTA: preciso
            break;
        }
    }

    // ── UDITO (doc 40, Fase 1) ───────────────────────────────────────────────
    // Uno sparo genera un CONTATTO per chi lo sente: impreciso (posizione disturbata:
    // si sa da DOVE, non CHI) e solo per la fazione avversaria. Prima le AI erano
    // sorde — `hearing_range` era autorato ma mai letto — e una sparatoria non si
    // propagava: chi era girato dall'altra parte restava a pattugliare.
    // Costo: nullo quando nessuno spara (mailbox vuota); qui è O(unità × suoni-tick).
    if (!world.sounds.empty())
    {
        for (EntityId e : snap)
        {
            const auto* ai = world.getAi(e);   if (!ai) continue;
            const auto* et = world.getTransform(e);
            const auto* tm = world.getTeam(e);
            if (!et || !tm) continue;
            const auto* h = world.getHealth(e);
            if (h && h->current <= 0.0f) continue;
            const auto* sq = world.getSquad(e);
            if (sq && sq->downed) continue;             // un caduto non sente
            for (const auto& s : world.sounds)
            {
                if (s.team == tm->teamId) continue;      // non ci si allerta da soli
                const float dx = s.x - et->x, dz = s.z - et->z;
                const float d2 = dx * dx + dz * dz;
                // Raggio udibile = quanto è FORTE l'evento × quanto BENE sente l'unità.
                // (Non `min(orecchio, evento)`: con hearing_range 12 e sparo 45 m si
                // sentiva a 12 m ciò che si vede già a 50 → udito inerte, misurato.)
                const float r = s.radius * (ai->hearingRange / config::HEARING_REFERENCE);
                if (d2 >= r * r) continue;
                // Direzione sì, precisione no: il punto è disturbato in modo stabile
                // (dipende da chi sente e da dove) → si accorre VERSO il rumore.
                const float jx = (aiRand01() - 0.5f) * 2.0f * config::SOUND_CONTACT_SCATTER;
                const float jz = (aiRand01() - 0.5f) * 2.0f * config::SOUND_CONTACT_SCATTER;
                const float cx = s.x + jx, cz = s.z + jz;
                bool dup = false;
                for (const auto& c : m_contacts)
                {
                    if (c.team != tm->teamId || c.age > config::COMMS_CONTACT_MERGE_AGE) continue;
                    const float ddx = c.x - cx, ddz = c.z - cz;
                    if (ddx * ddx + ddz * ddz
                        < config::COMMS_CONTACT_MERGE_DIST * config::COMMS_CONTACT_MERGE_DIST)
                    { dup = true; break; }
                }
                if (!dup) { m_contacts.push_back({cx, cz, tm->teamId, 0.0f,
                                                  config::CONTACT_CONFIDENCE_SOUND});   // UDITO: vago
                            ++g_tac.heardShots; }
                break;   // un rumore per tick basta ad allertare
            }
        }
    }
    world.sounds.clear();   // mailbox del tick: consumata qui

    // Overwatch esplicito (ADR-032): le avanzate si leggono dal pool `m_advancesPrev`,
    // dove PERSISTONO per il loro TTL (~durata manovra) invece di sparire dopo un
    // tick. Le NUOVE (di questo tick, in m_advances) entrano DOPO il decadimento →
    // visibili dal prossimo tick: la scelta di chi copre resta indipendente
    // dall'ordine di iterazione, ma il segnale dura abbastanza da essere colto.
    for (auto& a : m_advancesPrev) a.ttl -= dt;
    m_advancesPrev.erase(
        std::remove_if(m_advancesPrev.begin(), m_advancesPrev.end(),
                       [](const Advance& a) { return a.ttl <= 0.0f; }),
        m_advancesPrev.end());
    for (const auto& a : m_advances) m_advancesPrev.push_back(a);
    m_advances.clear();

    // ── Seconda passata: aggiorna ogni AI ────────────────────────────
    for (EntityId e : snap)
    {
        auto* ai   = world.getAi(e);   if (!ai) continue;
        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

        // SCATOLA NERA (AiTrace, KI #86): osserva l'esito del tick PRECEDENTE. Sta in
        // testa al ciclo di proposito — il corpo sotto è pieno di `continue` (gate di
        // fuoco, stati, guardie) e in coda verrebbe saltata proprio per le unità che
        // interessa osservare: quelle che non arrivano a sparare.
        aitrace::observe(world, e, dt, tick);

        // SOPPRESSIONE (A3): decade da sé — l'unità "riprende fiato" quando smette di
        // arrivarle addosso. Senza decadimento resterebbe inchiodata per sempre.
        if (ai->suppression > 0.0f)
        {
            ai->suppression *= std::exp(-dt / config::SUPPRESSION_TAU);
            if (ai->suppression < 0.01f) ai->suppression = 0.0f;
            else
            {
                ++g_tac.suppressedTicks;
                if (ai->suppression >= config::SUPPRESSION_PINNED) ++g_tac.pinnedTicks;
            }
        }
        // LEGGIBILITÀ della soppressione: si annuncia la TRANSIZIONE, non lo stato (che
        // spammerebbe ogni tick). Senza questo il sistema resta invisibile: il giocatore
        // non capisce perché un nemico ha smesso di avanzare, né che il suo fuoco di
        // soppressione sta funzionando — e nemmeno noi possiamo tararlo a occhio.
        {
            const bool pinnedNow = ai->suppression >= config::SUPPRESSION_PINNED;
            if (pinnedNow != ai->wasPinned)
            {
                ai->wasPinned = pinnedNow;
                if (pinnedNow)
                {
                    world.pushEvent(team->teamId == 1
                        ? "Compagno INCHIODATO dal fuoco nemico"
                        : "Nemico INCHIODATO: non avanza finche' lo tieni sotto tiro");
                    ++g_tac.pinnedAnnounced;
                }
            }
        }

        const AiState oldState = ai->state;   // telemetria: log solo sul CAMBIO
        auto* sq = world.getSquad(e);         // ordine di squadra (ADR-020), può mancare
        // A TERRA (Phase C): inerme. Non sente, non decide, non spara. Va anche
        // FERMATO ATTIVAMENTE: l'agente crowd conserva l'ultimo target e il
        // CrowdSystem lo muoverebbe comunque (bug playtest: i caduti seguivano un
        // ordine di movimento). Azzerare la velocità desiderata ogni tick lo
        // inchioda dov'è caduto finché non è rianimato o distrutto.
        if (sq && sq->downed)
        {
            if (world.nav && world.nav->crowdReady() && ai->crowdAgentIdx >= 0)
                world.nav->requestMoveVelocity(ai->crowdAgentIdx, {0.0f, 0.0f, 0.0f});
            continue;
        }

        // FUOCO DI COPERTURA (doc 26): un membro in CoveringFire SOPPRIME — sta
        // fermo (leash stretto) e NON entra in fase evasiva (niente peek/hide):
        // "stand and deliver". È ciò che lo distingue da un semplice HoldPosition.
        const bool covering = sq && sq->hasActiveOrder()
                            && sq->order == OrderType::CoveringFire;
        const int myTeam = team->teamId;
        const auto& targets = (myTeam == 1) ? team2Tgts : team1Tgts;
        const auto& tgtPos  = (myTeam == 1) ? team2Pos  : team1Pos;
        const glm::vec3 ePos = {et->x, et->y, et->z};
        const glm::vec3 eyePos = {et->x, et->y + kEyeUp, et->z};   // LOS ad altezza peek (KI #79)

        // ── Presidio (ADR-046, opzione A) — valutato PRIMA dei rami di combattimento
        // Se il comando ordina TIENI su un fronte a cui questo droide è assegnato,
        // lo si àncora alla miglior posizione difensiva/chokepoint dell'area: ci
        // COMBATTE da lì senza inseguire (clamp più sotto). Va fatto qui, non nel
        // ramo waypoint (raggiunto solo fuori dal combattimento): il TIENI scatta
        // proprio DURANTE la minaccia, quindi prima `hold_su_posizione` restava 0.
        // Escluso il comandante (leash proprio) e i fermi. Rivalutato ogni tick →
        // Advance/Retreat rilasciano l'àncora.
        ai->holdRadius = 0.0f;
        if (myTeam == 2 && !ai->stationary && ai->leashRadius <= 0.0f
            && world.enemyCommand.active && world.activeMap)
        {
            const auto* hd = pickEnemyDirective(world, ai->bias, et->x, et->z);
            if (hd && hd->stance == World::EnemyCommand::Hold)
                if (const TacticalPositionDef* h = worldintel::bestHoldPosition(
                        *world.activeMap, et->x, et->z, hd->x, hd->z, hd->radius))
                {
                    ai->holdX = h->x; ai->holdZ = h->z; ai->holdRadius = 8.0f;
                    ++g_tac.holdPosOccupied;
                }
        }
        // ── ORDINI di POSTURA del PLAYER (ADR-020 / [[orders-design-vision]]) ──
        // Motore unico (tower-hub): il membro salta di posizione LIBERA in posizione
        // libera (occupancy CENTRALE world.allyTac.claimed, dati di fuoco pre-calcolati
        // dalla torre) scegliendo la priorità più alta fra i ruoli utili, nella DIREZIONE
        // della postura (Hold/Advance/Retreat/Follow/Regroup).
        // Imposta ai->allySig* (waypoint, letto nel ramo waypoint) e, per Hold,
        // ai->holdX/Z/holdRadius (clamp: difende la posizione anche in combattimento).
        // L'ordine dà l'intento; l'AI sceglie posto e combattimento (bias, non override).
        else if (myTeam == 1 && sq && sq->hasActiveOrder() && !ai->stationary
                 && world.activeMap
                 && (sq->order == OrderType::HoldPosition || sq->order == OrderType::Advance
                     || sq->order == OrderType::Retreat   || sq->order == OrderType::Follow
                     || sq->order == OrderType::Regroup))
        {
            selectOrderWaypoint(world, *ai, dt, *sq, et->x, et->y, et->z);
        }

        // Contatto condiviso LOCALMENTE: adotta solo il più vicino entro il
        // raggio. Chi combatte a ovest non trascina chi presidia a est.
        // La rete di comunicazione (doc 34) scala il raggio e ritarda l'arrivo:
        // senza torre si sente meno lontano e si sa più tardi — quindi si accorre
        // dove il nemico ERA, non dov'è. Con torre viva: raggio pieno, ritardo 0
        // → comportamento identico a prima.
        {
            const auto& cs = world.comms[myTeam];
            const float shareR = config::AI_CONTACT_SHARE_RADIUS * cs.shareRangeMult;
            float bestD2 = shareR * shareR;
            bool  got = false; float cx = 0.0f, cz = 0.0f, cconf = 0.0f;
            for (const auto& c : m_contacts)
            {
                if (c.team != myTeam) continue;
                if (c.age < cs.shareDelay) continue;   // ancora "in transito"
                if (c.age > cs.shareDelay + config::COMMS_CONTACT_FRESH) continue;  // già vecchio
                const float dx = c.x - et->x, dz = c.z - et->z;
                const float d2 = dx * dx + dz * dz;
                if (d2 < bestD2) { bestD2 = d2; cx = c.x; cz = c.z; cconf = c.confidence; got = true; }
            }
            if (got)
            {
                ai->lastKnownX = cx;
                ai->lastKnownZ = cz;
                ai->hasLastKnown = true;
                // CONFIDENZA (doc 40 A2): alta = "so dov'è" → ci si va per ingaggiare
                // (comportamento storico). Bassa = "lì è successo qualcosa" → diventa un
                // punto da PERLUSTRARE, non un nemico da inseguire. È la differenza fra
                // un soldato che indaga e uno che rincorre un fantasma con certezza.
                if (cconf < config::CONTACT_CONFIDENCE_ENGAGE)
                {
                    ai->searchX = cx; ai->searchZ = cz;
                    ++g_tac.lowConfInvestigate;
                }
                else ++g_tac.highConfContact;
            }
        }

        // ── Bersaglio con LOS (questo specifico AI) ──────────────────
        // Time-slicing (Fase 4): la ricerca O(bersagli)+LOS gira solo nel tick
        // schedulato per questa entità; negli altri tick si riusa il bersaglio
        // cachato. La morte del target è comunque rilevata ogni frame (sotto,
        // getTransform); il LOS è ri-verificato al momento dello sparo.
        EntityId nearest;
        if ((tick + e) % config::AI_SENSE_INTERVAL == 0)
        {
            // Fase 4b: raccogli i K bersagli PIÙ VICINI in range (solo distanze,
            // niente LOS — inserimento in array ordinato, flop economici), poi
            // verifica il LOS solo su questi K dal più vicino: il primo visibile
            // è il nearest visibile. Bounda la LOS costosa a K per AI → la
            // sensing passa da O(N²) a O(N·K) senza griglia spaziale (inutile
            // qui: aggro ~ dimensione mappa).
            constexpr int K = config::AI_MAX_LOS_CHECKS;
            int   kIdx[K]; float kD2[K]; int kn = 0;
            // Punto d'OSSERVAZIONE (ADR-046): un'unità nella ZONA di un punto
            // d'osservazione autorato avvista da PIÙ LONTANO — è la funzione del
            // ruolo (early-warning), prima decorativo. +50% di aggro entro 10 m
            // (la sua area d'influenza, non il singolo punto).
            float aggro = ai->aggroRange;
            if (world.activeMap
                && worldintel::nearestPositionByRole(*world.activeMap,
                                                     et->x, et->z, "observation", 10.0f))
                { aggro *= 1.5f; ++g_tac.obsSightBoost; }
            const float aggro2 = aggro * aggro;
            // Direzione dello sguardo, per il campo visivo (doc 40).
            const float faceR = et->ry * (PI / 180.0f);
            const float fwdX = std::sin(faceR), fwdZ = std::cos(faceR);
            const float cosHalfFov = std::cos(ai->fovDeg * 0.5f * (PI / 180.0f));
            bool anyInRange = false;   // funnel KI #86: il mondo offriva qualcosa?
            for (size_t i = 0; i < targets.size(); ++i)
            {
                if (targets[i] == e) continue;
                const glm::vec3& tp = tgtPos[i];   // contiguo, niente hash lookup
                const float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
                if (d2 >= aggro2) continue;
                anyInRange = true;
                // ── CAMPO VISIVO (doc 40) ────────────────────────────────
                // Prima le AI vedevano a 360°: arrivare alle spalle non valeva nulla.
                // NON è un taglio netto, che darebbe il difetto "mi sta di fianco e non
                // mi vede": restano percepibili chi è RAVVICINATO (visione periferica,
                // rumore, movimento) e chi STA SPARANDO (lampo e fragore lo rivelano).
                {
                    const float hx = tp.x - ePos.x, hz = tp.z - ePos.z;
                    const float hl = std::sqrt(hx * hx + hz * hz);
                    const bool inCone = (hl < 0.001f) ||
                        ((hx * fwdX + hz * fwdZ) / hl >= cosHalfFov);
                    if (!inCone)
                    {
                        const bool close = d2 < config::PERCEPTION_PERIPHERAL_RADIUS
                                              * config::PERCEPTION_PERIPHERAL_RADIUS;
                        bool firing = false;   // ha sparato in questo tick? → si rivela
                        if (!close && d2 < config::PERCEPTION_MUZZLE_REVEAL
                                         * config::PERCEPTION_MUZZLE_REVEAL)
                            for (const auto& s : world.sounds)
                                if (s.source == targets[i]) { firing = true; break; }
                        if (!close && !firing) { ++g_tac.fovRejected; continue; }
                    }
                }
                if (kn == K && d2 >= kD2[K-1]) continue;   // più lontano dei K correnti
                int pos = (kn < K) ? kn : K - 1;           // inserimento ordinato
                while (pos > 0 && kD2[pos-1] > d2)
                { kD2[pos] = kD2[pos-1]; kIdx[pos] = kIdx[pos-1]; --pos; }
                kD2[pos] = d2; kIdx[pos] = (int)i;
                if (kn < K) ++kn;
            }
            nearest = 0;
            for (int j = 0; j < kn; ++j)
            {
                // Origine occhi (KI #79), bersaglio al BUSTO ALTO — lo STESSO punto
                // che userà il gate di fuoco (`tt->y + AI_HALF_Y*0.7`). Prima qui si
                // mirava al transform nudo (~0.35 m più in basso): il raggio cadeva e
                // un muretto vicino al bersaglio lo tagliava, quindi **l'AI non
                // riusciva a VEDERE bersagli che sarebbe riuscita a COLPIRE** — il
                // fix interim di KI #82 era stato applicato al tiro e mai qui.
                // Misurato (KI #86): acquisizione 39% → 61% dei nemici nel cono.
                const glm::vec3& raw = tgtPos[kIdx[j]];
                const glm::vec3 aimPt{ raw.x, raw.y + config::AI_HALF_Y * 0.7f, raw.z };
                if (physics::hasLineOfSight(eyePos, aimPt, world, targets[kIdx[j]]))
                { nearest = targets[kIdx[j]]; break; }
            }
            // Funnel verticalità (1/3): quanti candidati erano a quota diversa, e il
            // bersaglio scelto lo è? `acqAllBlocked` isola il caso "c'erano candidati
            // ma nessuno visibile" — il sintomo della saturazione dei K più vicini
            // (es. sul ponte i più vicini in 3D sono quelli SOTTO, coperti dal ponte).
            // I totali sono i DENOMINATORI: senza, "1291 candidati cross vs 28
            // acquisizioni" è un confronto falso (un'unità produce fino a K candidati
            // ma UNA sola acquisizione per tick). Il dato che conta è il rapporto fra
            // la quota di OPPORTUNITÀ cross-quota e la quota di ACQUISIZIONI cross-quota:
            // se le opportunità sono il 30% e le acquisizioni il 2%, c'è un bias reale.
            // Funnel d'ingaggio, troncone PERCEZIONE (KI #86): raggio → cono → LOS.
            // Se il calo grosso è raggio→cono il problema è il FOV senza scansione
            // (l'AI guarda dove cammina); se è cono→LOS è geometria/saturazione dei K.
            if (anyInRange) ++g_tac.occInRange;
            if (kn > 0)     ++g_tac.occInCone;
            if (nearest != 0) ++g_tac.occAcquired;
            g_tac.candTotal += kn;
            for (int j = 0; j < kn; ++j)
                if (std::fabs(tgtPos[kIdx[j]].y - ePos.y) > config::VERTICAL_ENGAGE_DY)
                    ++g_tac.vertCand;
            // Indagine 2026-08-02 (KI #86 causa 3): qui c'era la classificazione del
            // BLOCCANTE (una LOS extra + una mappa per-entità su ogni acquisizione
            // fallita). Ha dato la sua risposta ed è stata tolta. Ha anche mostrato il
            // LIMITE della misura a runtime: la classificazione "autorato vs muto" usava
            // un raggio fisso dal centro del bloccante, senza senso per un impalcato
            // largo 31 m → la stessa domanda si risponde meglio a costo ZERO nell'editor
            // (`UnmarkedCover`), dove la geometria si conosce per intero.
            // Resta l'out-param `outBlocker` in `hasLineOfSight`: gratis se inutilizzato.
            if (kn > 0 && nearest == 0) ++g_tac.acqAllBlocked;
            if (nearest != 0)
            {
                ++g_tac.acqTotal;
                if (const auto* ntr = world.getTransform(nearest))
                    if (std::fabs(ntr->y - ePos.y) > config::VERTICAL_ENGAGE_DY) ++g_tac.vertAcq;
            }

            // ── Ingaggio delle STRUTTURE (doc 35) ────────────────────────
            // Una struttura è un bersaglio a PRIORITÀ PIÙ BASSA delle unità: si
            // considera SOLO se non c'è un bersaglio-unità (`nearest == 0`) — una
            // struttura non spara, preferirla a chi ti spara addosso sarebbe
            // stupido. Due modi di diventare eleggibile:
            //  · entro il raggio AUTORATO `engage_radius` (peel-off proattivo);
            //  · quando NON restano unità nemiche (`targets.empty()`): la
            //    struttura è l'ULTIMO bersaglio, la si ingaggia entro l'aggro.
            // Con nemici in campo e `engage_radius = 0` resta ignorata: default
            // a bassa priorità, come richiesto dall'utente (2026-07-21).
            const bool noEnemyUnits = targets.empty();
            if (nearest == 0)
            {
                float bestD2 = 1e18f;
                for (const auto& stx : world.strategicTargets)
                {
                    if (stx.entity == 0 || stx.team == myTeam) continue;
                    if (!world.isValidEntity(stx.entity)) continue;
                    // Portata utile: il raggio autorato, oppure — se è l'ultimo
                    // bersaglio rimasto — almeno l'aggro range.
                    const float reach = noEnemyUnits
                        ? std::max(stx.engageRadius, ai->aggroRange)
                        : stx.engageRadius;
                    if (reach <= 0.0f) continue;   // non ingaggiabile di iniziativa
                    const auto* stt = world.getTransform(stx.entity);
                    if (!stt) continue;
                    const float dx = stt->x - ePos.x, dz = stt->z - ePos.z;
                    const float d2 = dx * dx + dz * dz;
                    if (d2 >= reach * reach || d2 >= bestD2) continue;
                    // Si mira al CORPO, non all'origine: il transform di una
                    // struttura sta a terra, quindi un segmento verso di esso
                    // raschia il collider del pavimento e il LOS falliva sempre.
                    const auto* scol = world.getCollider(stx.entity);
                    const float aimY = stt->y + (scol ? scol->hy * 0.5f : 1.0f);
                    // `stx.entity` da ignorare: è il bersaglio, non un ostacolo.
                    // Origine ad altezza occhi (KI #79); la struttura si mira al corpo.
                    if (!physics::hasLineOfSight(eyePos, {stt->x, aimY, stt->z},
                                                 world, stx.entity)) continue;
                    bestD2 = d2; nearest = stx.entity;
                }
                if (nearest != 0) ++g_tac.structureEngage;
            }

            // ── Vincolo FocusFire (ADR-020 Phase B) ──────────────────────
            // L'ordine cambia la SCELTA del bersaglio, non la mira: se il
            // designato è vivo e visibile l'AI lo preferisce al più vicino.
            // Se NON lo vede resta autonoma — un ordine non deve farle sparare
            // a un muro. Sta qui dentro (ramo di sensing) per non violare il
            // time-slicing: il vincolo viaggia nella cache come ogni bersaglio.
            if (const auto* sqf = world.getSquad(e))
                if (sqf->hasActiveOrder() && sqf->order == OrderType::FocusFire
                    && sqf->targetEntity != 0)
                {
                    const auto* ft = world.getTransform(sqf->targetEntity);
                    const auto* fh = world.getHealth(sqf->targetEntity);
                    // Il designato può essere una STRUTTURA (doc 35): il
                    // giocatore forza la priorità con FocusFire, e comandi più
                    // avanzati in futuro. Si mira al corpo e si ignora il
                    // bersaglio come ostacolo — senza, una struttura era
                    // designabile ma il LOS falliva sempre (transform a terra).
                    if (ft && fh && fh->current > 0.0f)
                    {
                        const auto* fcol = world.getCollider(sqf->targetEntity);
                        // Origine occhi (KI #79); mira al CORPO: struttura = collider,
                        // unità = BUSTO ALTO — lo stesso punto dell'acquisizione e del
                        // gate di fuoco (KI #86): col transform nudo un muretto vicino
                        // al designato tagliava il raggio e il FocusFire non prendeva mai.
                        const float aimY = ft->y + (fcol ? fcol->hy * 0.5f
                                                         : config::AI_HALF_Y * 0.7f);
                        if (physics::hasLineOfSight(eyePos, {ft->x, aimY, ft->z},
                                                    world, sqf->targetEntity))
                            nearest = sqf->targetEntity;
                    }
                }
            ai->targetEntity = nearest;   // cache per i tick non-sensing
        }
        else
        {
            nearest = ai->targetEntity;   // riusa il bersaglio cachato
        }

        // Durata della fase evasiva corrente + guardia sul congelamento (KI #86).
        if (ai->evading)
        {
            ai->evadeElapsed += dt;
            if (ai->evadeElapsed > 3.0f) ++g_tac.evadeStuckTicks;
            if (ai->evadeElapsed > g_tac.evadeMaxSec) g_tac.evadeMaxSec = ai->evadeElapsed;
        }
        else ai->evadeElapsed = 0.0f;

        // LA FASE DI HIDE NON SI CONGELA MAI (KI #86). `exposeTimer` scorreva solo nel
        // ramo d'ingaggio sul posto, che richiede un bersaglio — ma nascondersi dietro
        // una cover è ESATTAMENTE ciò che rompe il proprio LOS: si entrava in hide, si
        // perdeva il contatto, il timer si CONGELAVA, e al ri-contatto il gate di fuoco
        // restava chiuso per tutta la fase residua. È il sintomo riportato dall'utente
        // ("si piazzano dietro a un muro e rimangono lì").
        //
        // Qui si fa la cosa MINIMA: il timer continua a scorrere e la fase finisce da
        // sola. Non si azzera `hasCover` — un primo tentativo che lo faceva ha fatto
        // CALARE il combattimento del 17% (255→212 eventi): senza bersaglio il LOS
        // sfarfalla di continuo, e togliere la copertura sfrattava le AI dalle
        // posizioni autorate, che sono proprio quelle con le buone linee di tiro.
        if (nearest == 0 && ai->evading)
        {
            ai->exposeTimer -= dt;
            // Scaduta: si torna esposti. Niente nuovo roll di hide — non ci si
            // nasconde da un nemico che non si vede; il roll spetta all'ingaggio.
            if (ai->exposeTimer <= 0.0f)
            {
                ai->evading     = false;
                ai->exposeTimer = aiRandRange(ai->peekMin, ai->peekMax);
            }
        }

        // ── Transizioni stato ────────────────────────────────────────
        if (nearest != 0)
        {
            // LOS diretto: Alert (spara)
            const auto* tt = world.getTransform(nearest);
            if (!tt) { nearest = 0; }
            else { ai->lastKnownX = tt->x; ai->lastKnownZ = tt->z; }
            ai->hasLastKnown = true;
            // Tempo di reazione (dal profilo AI): il primo colpo dopo una
            // nuova acquisizione arriva con ritardo, non istantaneo.
            if (ai->state != AiState::Alert)
            {
                if (ai->reactionTime > 0.0f)
                    ai->shootCooldown = std::max(ai->shootCooldown, ai->reactionTime);
                // Nuova acquisizione: SEMPRE una finestra di fuoco piena —
                // senza questo il roll evasivo poteva sopprimere il primo
                // colpo e l'ingaggio moriva prima di iniziare.
                ai->evading     = false;
                ai->exposeTimer = aiRandRange(ai->peekMin, ai->peekMax) + ai->reactionTime;
                // Ingaggio APPENA iniziato: cerca SUBITO una posizione di tiro
                // coperta invece di restare allo scoperto fino alla prossima
                // valutazione a timer (3-6s). Riusa il sistema di riposizionamento
                // ADR-035 (nessun sistema nuovo): timer a 0 = valuta questo tick,
                // `justEngaged` = quella valutazione è deterministica, non a
                // probabilità. Segnalato dall'utente: "sparano allo scoperto".
                ai->repositionTimer = 0.0f;
                ai->justEngaged     = true;

                // ── RUOLO DI COMBATTIMENTO (A4) ─────────────────────────
                // Si assegna all'INGAGGIO, una volta: la squadra si divide i compiti
                // invece che ognuno tirare i dadi per sé. Il ruolo più adatto al
                // profilo, fra quelli NON ancora saturi — stessa logica dell'occupancy
                // delle posizioni: chi sceglie dopo trova occupato ciò che è già preso.
                {
                    const int t = (myTeam == 1 || myTeam == 2) ? myTeam : 0;
                    const int engaged = engagedCount[t] + 1;
                    // Composizione desiderata: ~metà sopprime, un terzo avanza, il resto
                    // aggira. Una squadra che aggira tutta insieme lascia il nemico
                    // libero di muoversi; una che sopprime e basta non conclude nulla.
                    const int wantSup   = (engaged + 1) / 2;
                    const int wantFlank = engaged / 3;
                    // Affinità dal PROFILO: chi ama la copertura sopprime, chi è
                    // aggressivo e incline al fianco aggira (i parametri esistono già).
                    const float affSup   = (1.0f - ai->flankChance) * (0.5f + ai->coverPreference);
                    const float affFlank = ai->flankChance * (0.5f + ai->aggression);
                    const bool supFree   = roleCount[t][1] < wantSup;
                    const bool flankFree = roleCount[t][2] < wantFlank;
                    int chosen = 3;                                   // default: avanza
                    if (supFree && (!flankFree || affSup >= affFlank)) chosen = 1;
                    else if (flankFree)                               chosen = 2;
                    ai->combatRole = chosen;
                    ++roleCount[t][chosen]; ++engagedCount[t];
                    ++g_tac.roleAssigned[chosen];
                    // LEGGIBILITÀ (stessa regola imposta per la soppressione: il canale
                    // si scrive INSIEME alla meccanica, non dopo). Si annuncia solo
                    // l'AGGIRAMENTO di un compagno — l'informazione che cambia le scelte
                    // del giocatore ("qualcuno sta girando, io tengo il fronte") — e solo
                    // per la sua squadra, altrimenti il feed diventa un bollettino.
                    if (myTeam == 1 && chosen == 2)
                        world.pushEvent("Compagno in AGGIRAMENTO: tieni il fronte");
                }
            }
            ai->state = AiState::Alert;
            ai->alertTimer = 3.0f;
            ai->searchTimer = 0.0f;
            ai->flankActive = false; // contatto diretto: niente più flanking
        }
        else if (ai->state == AiState::Alert)
        {
            // Aveva LOS ma l'ha perso: attendi un po' poi Hunt
            ai->alertTimer -= dt;
            if (ai->alertTimer <= 0.0f)
                enterHunt(*ai, *et, world);
        }
        else if (ai->state == AiState::Patrol && ai->hasLastKnown)
        {
            // Era in pattuglia ma il team ha condiviso una posizione: Hunt
            enterHunt(*ai, *et, world);
        }
        // ── Hunt SCADE (KI #68) ──────────────────────────────────────────
        // Prima Hunt non aveva timeout ("l'AI non dimentica MAI"), ma Search uno
        // ce l'ha (15 s → Patrol): un'unità che perdeva il contatto restava a
        // inseguire un `lastKnown` inesistente **per centinaia di secondi**,
        // visibile in sandbox come unità perennemente in `hunt`. Scaduto il
        // tempo si degrada a Search — non a Patrol: ha comunque senso guardarsi
        // intorno prima di tornare al proprio compito.
        else if (ai->state == AiState::Hunt)
        {
            ai->huntTimer += dt;
            if (ai->huntTimer > ai->huntPatience)   // soglia dal profilo (doc 16)
            {
                ai->huntTimer = 0.0f;
                ai->state     = AiState::Search;
                ai->searchTimer = 0.0f;
                pickSearchPoint(*ai, et->x, et->z, world.activeMap);
            }
        }
        if (ai->state != AiState::Hunt) ai->huntTimer = 0.0f;

        const bool patrolOk = !(ai->patrolAx==0 && ai->patrolAz==0 &&
                                 ai->patrolBx==0 && ai->patrolBz==0);

        // ── Anti-stuck ───────────────────────────────────────────────
        // Soglia PROPORZIONALE alla velocità, non fissa. Con la vecchia soglia
        // fissa 0.05 m/tick un droide in pattuglia (patrol_speed 2.5 → 0.042
        // m/tick a 60Hz) risultava "bloccato" mentre marciava normalmente: dopo
        // 1.2s scattava l'anti-stuck e saltava al segmento di route successivo
        // senza esserci mai arrivato → pattuglie a scatti, percorsi mai
        // completati (feedback utente 2026-07-20). Ora è bloccato solo chi si
        // muove a meno di 1/4 di quanto dovrebbe.
        const float movedDist = std::abs(et->x - ai->prevX) + std::abs(et->z - ai->prevZ);
        const float expectedStep = (ai->patrolSpeed > 0.1f ? ai->patrolSpeed : 2.0f) * dt;
        // In ALERT stare fermi è LEGITTIMO (in copertura, o a distanza d'ingaggio):
        // il timer va azzerato, non solo soppresso il log. Prima si accumulava in
        // mischia e veniva riportato all'uscita dallo stato — gli "stuck" da ~2.8s
        // in Hunt erano quasi tutti falsi positivi maturati in Alert, e rendevano
        // il segnale inutile per diagnosticare i veri problemi di percorso.
        if (ai->state == AiState::Alert)
            { ai->stuckTimer = 0.0f; ai->stuckReported = false; }
        else if (movedDist < expectedStep * 0.25f && !ai->stationary)
            ai->stuckTimer += dt;
        else
            { ai->stuckTimer = 0.0f; ai->stuckReported = false; }  // si è mosso: reset
        ai->prevX = et->x; ai->prevZ = et->z;
        const bool isStuck = ai->stuckTimer > config::AI_STUCK_TIME;

        // Telemetria (ADR-016 / 06_Todo #1): bot bloccato → WARN con coordinate
        // esatte (cross-ref con la geometria MapDef). UNA per episodio: il flag
        // si azzera sopra quando l'AI torna a muoversi. In Alert NON logga: uno
        // strafe sul posto in mischia non è "bloccato" (falso positivo del crowd).
        if (isStuck && !ai->stuckReported && ai->state != AiState::Alert)
        {
            ai->stuckReported = true;
            telemetry::event(telemetry::Level::Warn, "AI", "stuck",
                {{"bot_id", e}, {"state", aiStateName(ai->state)},
                 {"pos", {et->x, et->y, et->z}}, {"stuck_time", ai->stuckTimer}});
            // RECUPERO (2026-07-23): un bot bloccato quasi sempre insegue un target
            // IRRAGGIUNGIBILE (posizione su un'isola navmesh o passaggio eroso) e ci
            // resta contro un muro — prima ci si limitava a loggare. Ora ABBANDONA
            // ogni target impegnato → ri-valuta invece di restare inchiodato. Sana
            // anche l'interazione col commitment del segnale torre.
            ai->allySigValid     = false;   // molla il waypoint torre impegnato
            ai->repositionActive = false;   // molla la manovra
            ai->holdRadius       = 0.0f;    // molla l'àncora di presidio
            ai->patrolRoute      = -1;      // ricambia route (ADR-045)
        }

        // ── Movimento ────────────────────────────────────────────────
        float moveDX = 0, moveDZ = 0, moveSpeed = 0;
        // Distanza REALE alla destinazione. norm2D() normalizza moveDX/DZ in
        // place, quindi da soli non bastano a ricostruire il punto d'arrivo: senza
        // questa, requestMoveTarget riceve una "carota" a 1 m e Detour non può
        // pianificare nulla (non aggira gli ostacoli). I rami traversal la
        // impostano; default 1.0 = comportamento storico per chi non la imposta.
        float moveDist = 1.0f;
        // true quando un ordine di squadra sta facendo PERCORRERE distanza all'AI:
        // richiede pathfinding (requestMoveTarget) anche in Alert, perché la
        // modalità velocità dell'Alert non pianifica e si incastra sui muri.
        bool orderTravel = false;

        if (!ai->stationary)
        {
            // LA MANOVRA NON HA BISOGNO DI UN BERSAGLIO (KI #86, changelog 121).
            // Osservato con la scatola nera su una singola unità: ingaggia al tick
            // 2838, PERDE il contatto al 2850 — e resta immobile fino al 2988, con
            // `repositionActive` acceso, posizione e sguardo congelati. Causa: tutta
            // la manovra viveva dentro il ramo `Alert && nearest != 0`, quindi al
            // primo LOS che si rompe (e si rompe nel 61-72% dei casi) lo spostamento
            // si fermava, il suo timer smetteva di scorrere e non c'era NESSUN altro
            // ramo di movimento per "Alert senza bersaglio" → ~3 s di paralisi, fino
            // a che `alertTimer` scadeva e `enterHunt` ripuliva il flag.
            //
            // È la TERZA volta che affiora lo stesso schema (la fase di hide, il
            // fuoco durante la manovra, ora la manovra stessa): stato che avanza solo
            // dentro un ramo condizionato al bersaglio. Qui si risolve alla radice —
            // una manovra è uno spostamento verso un POSTO, e perdere di vista il
            // nemico non annulla il terreno che si voleva prendere. Si continua, si
            // arriva, e proprio lì si ha la migliore probabilità di ri-acquisire.
            if (ai->state == AiState::Alert && ai->repositionActive)
            {
                const auto* tt = (nearest != 0) ? world.getTransform(nearest) : nullptr;
                {
                    // ── MANOVRA IN CORSO (ADR-035) ───────────────────────
                    // Si percorre la strada verso la posizione scelta CONTINUANDO
                    // a mirare e sparare (il fuoco resta autonomo: si vincola solo
                    // il movimento). `orderTravel` forza il pathfinding, altrimenti
                    // in Alert si userebbe lo steering diretto e ci si incastra.
                    //
                    // MANOVRARE NON È NASCONDERSI (KI #86). `exposeTimer` scorre solo
                    // nel ramo d'ingaggio sul posto: chi entrava in manovra con
                    // `evading` attivo restava evasivo per TUTTA la manovra — timer
                    // congelato, gate di fuoco chiuso — e la promessa qui sopra ("il
                    // fuoco resta autonomo") era falsa. Misurato fino a 1492 tick-AI
                    // per finestra. Una manovra deliberata SUPERA la fase di hide.
                    ai->evading  = false;
                    ai->hasCover = false;
                    moveDX = ai->repositionX - et->x;
                    moveDZ = ai->repositionZ - et->z;
                    const float rd = norm2D(moveDX, moveDZ);
                    ai->repositionTimer -= dt;
                    if (rd < 1.2f || ai->repositionTimer <= 0.0f)
                    {
                        // Arrivato o tempo scaduto: si torna a combattere sul posto
                        // e parte il cooldown prima di rivalutare (niente oscillazioni).
                        ai->repositionActive = false;
                        ai->repositionTimer  = 2.5f + ai->bias * 2.0f;
                    }
                    else
                    {
                        moveDist    = rd;
                        moveSpeed   = ai->seekSpeed;
                        orderTravel = true;   // pathfinding, non steering
                    }
                    // Il facing resta sul bersaglio: si spara mentre ci si sposta.
                    // Senza bersaglio si guarda dove si VA — e non è un ripiego: era
                    // lo sguardo congelato a rendere l'unità cieca proprio mentre
                    // attraversava terreno nuovo (il campo visivo segue il facing).
                    if (tt)
                        et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);
                    else if (moveSpeed > 0.0f)
                        et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI);
                }
            }
            else if (ai->state == AiState::Alert && nearest != 0)
            {
                // Ingaggio diretto sul posto: strafing + avanzamento.
                const auto* tt = world.getTransform(nearest);
                if (!tt) { nearest = 0; }
                else
                {
                    float advX = tt->x - et->x, advZ = tt->z - et->z;
                    float dist = norm2D(advX, advZ);

                    ai->strafeTimer -= dt;
                    if (ai->strafeTimer <= 0.0f || isStuck)
                    { ai->strafeSign = -ai->strafeSign; ai->strafeTimer = 1.4f; ai->stuckTimer = 0; }

                    const float perpX = -advZ * ai->strafeSign;
                    const float perpZ =  advX * ai->strafeSign;

                    // ── Ciclo peek/hide (16_AiBehavior) ──────────────
                    // A fine finestra di fuoco, con probabilità
                    // coverPreference entra in fase evasiva (non spara).
                    ai->exposeTimer -= dt;
                    if (ai->exposeTimer <= 0.0f)
                    {
                        // Chi fa fuoco di copertura NON si copre: resta esposto e
                        // continua a sparare (soppressione). Gli altri peek/hide.
                        // SOPPRESSIONE (A3): sotto tiro ci si copre MOLTO più volentieri.
                        // È il primo effetto osservabile — una raffica che non colpisce
                        // fa comunque abbassare la testa.
                        const float coverWant = ai->coverPreference
                            + ai->suppression * config::SUPPRESSION_COVER_BONUS;
                        if (!covering && !ai->evading && aiRand01() < coverWant)
                        {
                            ai->evading = true;
                            ai->exposeTimer = aiRandRange(ai->hideMin, ai->hideMax);
                            // Copertura vera se la mappa la offre (doc 18):
                            // altrimenti resta lo strafe evasivo di fallback.
                            // Query al World Intelligence Layer (ADR-025).
                            const TacticalPositionDef* cov = world.activeMap
                                ? worldintel::bestCoverToward(*world.activeMap,
                                      et->x, et->z, tt->x, tt->z, 12.0f)
                                : nullptr;
                            ai->hasCover = (cov != nullptr);
                            if (cov) { ai->coverX = cov->x; ai->coverZ = cov->z; }

                            // Abilità ROLL (16 est.): entrando in evasione,
                            // se pronta, scatto laterale col cooldown del def.
                            if (auto* ab = world.getAbilities(e))
                                for (auto& s : ab->states)
                                {
                                    if (s.type != "roll" || s.cooldown > 0.0f)
                                        continue;
                                    s.cooldown    = s.cooldownMax;
                                    ai->rollTimer = (s.param2 > 0.05f)
                                                  ? s.param2 : 0.3f;
                                    const float spd = (s.param1 > 0.5f)
                                                    ? s.param1 : 9.0f;
                                    ai->rollVX = perpX * spd;
                                    ai->rollVZ = perpZ * spd;
                                    ++g_tac.rollUsed;   // O5: attivazioni reali
                                    world.pushEvent("ROLL #" + std::to_string(e));
                                    telemetry::logTrace("roll: entita' "
                                        + std::to_string(e));
                                    break;
                                }
                        }
                        else
                        {
                            ai->evading  = false;
                            ai->hasCover = false;
                            ai->exposeTimer = aiRandRange(ai->peekMin, ai->peekMax);
                        }
                    }

                    // ── Ritirata sotto soglia HP — o su ORDINE Retreat ─────────
                    // L'ordine Retreat del player attiva lo stesso disimpegno anche a
                    // piena salute: senza questo, in combattimento il membro seguiva la
                    // logica d'ingaggio (che chiude verso il nemico) e l'ordine
                    // "indietreggia" veniva ignorato → sembrava avanzare (feedback utente).
                    const auto* ehp = world.getHealth(e);
                    const float hpFrac = (ehp && ehp->max > 0.0f)
                                       ? ehp->current / ehp->max : 1.0f;
                    const bool orderRetreat = myTeam == 1 && sq && sq->hasActiveOrder()
                                           && sq->order == OrderType::Retreat;
                    const bool retreating = (ai->retreatHpThresh > 0.0f
                                          && hpFrac < ai->retreatHpThresh)
                                          || orderRetreat;

                    // Distanza d'ingaggio preferita dall'aggressione:
                    // aggression 1 → ~3m (chiude), 0 → ~12m (tiene il raggio).
                    const float prefDist = 12.0f - 9.0f * ai->aggression;

                    if (retreating)
                    { // Disimpegno: arretra dal bersaglio con fuoco di copertura
                      moveDX = -advX*0.8f + perpX*0.4f; moveDZ = -advZ*0.8f + perpZ*0.4f;
                      moveSpeed = ai->seekSpeed; }
                    else if (ai->evading && ai->hasCover)
                    { // Fase evasiva CON copertura autorata: raggiungila e restaci
                      moveDX = ai->coverX - et->x; moveDZ = ai->coverZ - et->z;
                      const float cd = norm2D(moveDX, moveDZ);
                      if (cd < 0.7f) { moveDX = 0; moveDZ = 0; moveSpeed = 0; }
                      else           moveSpeed = ai->seekSpeed; }
                    else if (ai->evading)
                    { // Fase evasiva senza cover: strafe ampio + arretramento
                      moveDX = perpX*0.9f - advX*0.3f; moveDZ = perpZ*0.9f - advZ*0.3f;
                      moveSpeed = ai->seekSpeed*0.8f; }
                    else if (dist > prefDist)
                    { moveDX = advX*0.45f + perpX*0.65f; moveDZ = advZ*0.45f + perpZ*0.65f; moveSpeed = ai->seekSpeed*0.75f; }
                    else if (dist < prefDist * 0.6f)
                    { // Troppo vicino per il suo profilo: guadagna distanza
                      moveDX = -advX*0.5f + perpX*0.7f; moveDZ = -advZ*0.5f + perpZ*0.7f;
                      moveSpeed = ai->seekSpeed*0.6f; }
                    else
                    { moveDX = perpX; moveDZ = perpZ; moveSpeed = ai->seekSpeed*0.55f; }

                    et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);

                    // ── Vale la pena SPOSTARSI? (ADR-035) ───────────────
                    // Valutazione periodica (timer sfasato dal bias → non tutti
                    // insieme). Non si manovra in ritirata, sotto CoveringFire
                    // ("stand and deliver"), né se troppi compagni si stanno già
                    // muovendo: chi resta fa fuoco di copertura — è il bounding
                    // overwatch che emerge da due regole semplici.
                    ai->repositionTimer -= dt;
                    const int myT = (myTeam == 1 || myTeam == 2) ? myTeam : 0;
                    const bool tooManyMoving =
                        repositioning[myT] * 2 >= (teamAlive[myT] > 0 ? teamAlive[myT] : 1);

                    // SOPPRESSIONE (A3): chi è INCHIODATO non attraversa lo scoperto per
                    // manovrare — resta dov'è finché la pressione non cala. È ciò che
                    // permette a una squadra di fissare il nemico mentre un'altra aggira.
                    // RUOLO SOPPRESSIONE (A4): chi fissa il nemico NON manovra dopo
                    // essersi sistemato — è il suo compito restare lì e tenerlo sotto
                    // tiro mentre gli altri aggirano. Il primo riposizionamento (quello
                    // d'ingaggio, per non restare allo scoperto) resta permesso.
                    const bool holdingFire = (ai->combatRole == 1) && !ai->justEngaged;
                    if (ai->repositionTimer <= 0.0f && !retreating && !covering
                        && !holdingFire
                        && ai->suppression < config::SUPPRESSION_PINNED
                        && world.activeMap && !ai->repositionActive)
                    {
                        ++g_tac.repoEval;
                        ai->repositionTimer = 3.0f + ai->bias * 3.0f;   // riprova più tardi
                        bool advanced = false;
                        // Valutazione proattiva d'INGAGGIO (una tantum): appena
                        // ingaggiato, cerca una posizione di tiro coperta a
                        // prescindere dalla probabilità `coverPreference`, invece di
                        // restare allo scoperto. Si consuma qui (una volta per
                        // ingaggio). Il flanking resta a probabilità (è un'altra
                        // tattica, non "mettersi al riparo").
                        const bool engageSeek = ai->justEngaged;
                        ai->justEngaged = false;

                        // ── AVANZATA ─────────────────────────────────────
                        // Solo se non si stanno già muovendo in troppi: parte del
                        // gruppo avanza, il resto copre (sotto). È il bounding
                        // overwatch, ora ESPLICITO invece che solo emergente.
                        if (!tooManyMoving)
                        {
                            const TacticalPositionDef* dest = nullptr;
                            // Aggiramento: ora lo decide il RUOLO (A4), non un tiro di
                            // dadi individuale. Prima ognuno estraeva su `flankChance`,
                            // quindi potevano aggirare tutti insieme (nessuno fissava il
                            // nemico) o nessuno. Col ruolo la squadra si divide i compiti.
                            if (ai->combatRole == 2)
                            {
                                dest = worldintel::bestFlankingPosition(
                                    *world.activeMap, et->x, et->z, tt->x, tt->y, tt->z,
                                    et->x, et->z, 20.0f);
                                if (dest) ++g_tac.repoFlankHit; else ++g_tac.repoFlankMiss;
                            }
                            // Una posizione da cui sparare restando coperto: SEMPRE
                            // all'ingaggio, altrimenti a probabilità (personalità).
                            if (!dest && (engageSeek || aiRand01() < ai->coverPreference))
                            {
                                dest = worldintel::bestFiringPosition(
                                    *world.activeMap, et->x, et->z, tt->x, tt->y, tt->z, 16.0f);
                                if (dest) ++g_tac.repoFiringHit; else ++g_tac.repoFiringMiss;
                            }
                            // Ci si sposta solo se è davvero un altro posto.
                            if (dest)
                            {
                                const float ddx = dest->x - et->x, ddz = dest->z - et->z;
                                if (ddx * ddx + ddz * ddz > 3.0f * 3.0f)
                                {
                                    ai->repositionActive = true;
                                    ai->repositionX = dest->x;
                                    ai->repositionZ = dest->z;
                                    ai->repositionTimer = 6.0f;   // durata massima manovra
                                    ++repositioning[myT];
                                    ++g_tac.repoStarted;
                                    advanced = true;
                                    // Registra l'AVANZATA per l'overwatch dei compagni
                                    // (ADR-032): l'indice della posizione tattica.
                                    const int di = (int)(dest - world.activeMap->tacticalPositions.data());
                                    if (di >= 0 && di < (int)world.activeMap->tacticalPositions.size())
                                        m_advances.push_back({di, myT, dest->x, dest->z, 5.0f});
                                }
                                else ++g_tac.repoTooClose;
                            }
                        }
                        else ++g_tac.repoBlockedBusy;

                        // ── OVERWATCH ESPLICITO (ADR-032) ────────────────
                        // Chi NON avanza in questa valutazione (cap raggiunto, o
                        // nessuna destinazione utile) non resta passivo: se un
                        // compagno sta avanzando verso una posizione tattica, si
                        // sposta su una che — per il grafo `positionCovers` — la
                        // COPRE. È il consumo esplicito del grafo.
                        if (!advanced)
                        {
                            const Advance* adv = nullptr; float advD2 = 1e18f;
                            for (const auto& a : m_advancesPrev)
                            {
                                if (a.team != myT) continue;
                                const float dx = a.x - et->x, dz = a.z - et->z;
                                const float d2 = dx * dx + dz * dz;
                                if (d2 < advD2) { advD2 = d2; adv = &a; }
                            }
                            if (adv)
                            {
                                const TacticalPositionDef* ow =
                                    worldintel::bestOverwatchForPosition(
                                        *world.activeMap, et->x, et->z,
                                        adv->coveredIdx, 18.0f, nullptr);
                                if (ow)
                                {
                                    const float ddx = ow->x - et->x, ddz = ow->z - et->z;
                                    if (ddx * ddx + ddz * ddz > 3.0f * 3.0f)
                                    {
                                        ai->repositionActive = true;
                                        ai->repositionX = ow->x;
                                        ai->repositionZ = ow->z;
                                        ai->repositionTimer = 6.0f;
                                        ++repositioning[myT];
                                        ++g_tac.overwatchStarted;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // ANCHE "Alert senza bersaglio" si muove verso l'ultima posizione nota
            // (KI #86 residuo, changelog 133). Osservato con la scatola nera su una
            // singola unità: immobile dal tick 1128 al 1206 — sguardo congelato —
            // e ripartita **nell'istante esatto** in cui è passata a Hunt. Causa:
            // perso il contatto, l'AI resta in Alert per `alertTimer` (3 s) e in
            // quello stato non esiste alcun ramo di movimento: cade fuori da tutte
            // le condizioni della catena e non fa nulla.
            //
            // In 121 avevo corretto solo il sotto-caso "stava manovrando"; questo è
            // lo stesso difetto senza manovra. La pausa prima di lanciarsi
            // all'inseguimento è voluta (`alertTimer` = esitazione), ma **esitare
            // non è pietrificarsi**: un soldato che perde di vista il nemico non
            // resta fermo tre secondi a fissare il vuoto, si muove verso l'ultimo
            // punto noto. Lo stato resta Alert, quindi allo scadere `enterHunt`
            // sceglie comunque il suo approccio: cambia solo che nel frattempo
            // l'unità è viva.
            else if ((ai->state == AiState::Hunt
                      || (ai->state == AiState::Alert && nearest == 0))
                     && ai->hasLastKnown)
            {
                // Va verso l'ultima posizione nota (o, se sta fiancheggiando,
                // prima verso il punto laterale scelto in enterHunt).
                const float destX = ai->flankActive ? ai->flankX : ai->lastKnownX;
                const float destZ = ai->flankActive ? ai->flankZ : ai->lastKnownZ;
                moveDX = destX - et->x;
                moveDZ = destZ - et->z;
                float dist = norm2D(moveDX, moveDZ);
                moveDist = dist;

                // Le TRANSIZIONI di stato restano di chi è davvero in Hunt. Un'unità
                // in Alert che arriva sull'ultimo punto noto non deve saltare a
                // Search da qui: `alertTimer` sta già per portarla in Hunt tramite
                // `enterHunt`, che è il posto in cui si SCEGLIE l'approccio (diretto,
                // aggiramento, posizione di tiro, terreno dominante). Scavalcarlo
                // butterebbe via quella decisione per guadagnare mezzo secondo.
                const bool inHunt = (ai->state == AiState::Hunt);
                if (ai->flankActive && (dist < 1.5f || isStuck))
                {
                    // Punto di fiancheggiamento raggiunto (o irraggiungibile):
                    // prosegui dritto sulla lastKnown.
                    ai->flankActive = false;
                    ai->stuckTimer = 0;
                }
                else if (dist < 1.0f && inHunt)
                {
                    // Raggiunto lastKnown: passa a Search
                    ai->state = AiState::Search;
                    pickSearchPoint(*ai, et->x, et->z, world.activeMap);
                }
                else if (isStuck && inHunt)
                {
                    // Bloccato verso lastKnown: prova Search da un altro punto
                    ai->state = AiState::Search;
                    pickSearchPoint(*ai, et->x, et->z, world.activeMap);
                    ai->stuckTimer = 0;
                }
                else if (dist >= 1.0f)
                {
                    // In Alert si avanza con più CAUTELA: il contatto è appena
                    // andato perso, non è un inseguimento deciso. Guardare dove si
                    // va conta doppio qui — il campo visivo segue il facing, e uno
                    // sguardo congelato rendeva l'unità cieca proprio mentre
                    // attraversava il terreno dove il nemico è sparito.
                    moveSpeed = inHunt ? ai->seekSpeed : ai->seekSpeed * 0.6f;
                    et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI);
                }
            }
            else if (ai->state == AiState::Search)
            {
                // Cerca attorno alla lastKnown; se la ricerca resta
                // infruttuosa troppo a lungo torna in PATTUGLIA (verso i
                // post) — prima restava in Search per sempre e la partita
                // si spegneva in un vagare senza obiettivi.
                ai->searchTimer += dt;
                if (ai->searchTimer > 15.0f)
                {
                    ai->searchTimer  = 0.0f;
                    ai->hasLastKnown = false;
                    ai->state        = AiState::Patrol;
                    // Persa la pista, l'unità è altrove: si sgancia dalla route e
                    // ne raccoglierà la più vicina rientrando in pattuglia — è così
                    // che CAMBIA route liberamente invece di tornare alla sua di
                    // partenza (ADR-045).
                    ai->patrolRoute  = -1;
                }
                moveDX = ai->searchX - et->x;
                moveDZ = ai->searchZ - et->z;
                float dist = norm2D(moveDX, moveDZ);
                moveDist = dist;

                if (dist < 1.5f || isStuck)
                {
                    // Raggiunto punto di ricerca o bloccato: scegli un nuovo punto
                    pickSearchPoint(*ai, et->x, et->z, world.activeMap);
                    ai->stuckTimer = 0;
                }
                else
                {
                    moveSpeed = ai->seekSpeed * 0.8f;
                    et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI);
                }
            }
            else if (ai->state == AiState::Patrol
                     && (patrolOk || (world.enemyCommand.active && team->teamId == 2)
                                  || (world.allyIntel.active && team->teamId == 1)
                                  || targets.empty()))   // ultimo bersaglio: doc 35
            {
                // Pattuglia (prima del contatto). Il comando dà un INTENTO, non una
                // destinazione per ognuno; ogni unità decide il COME. **Novità
                // ADR-045**: l'intento vale anche per chi è su una route.
                //  · Advance → TUTTI spingono sull'obiettivo del proprio fronte.
                //  · Retreat → TUTTI ripiegano.
                //  · Hold / nessun comando → si pattuglia, con route FLUIDE:
                //    raccolte dal punto più vicino, bidirezionali, cambiabili.
                // Ai waypoint SOSTA per patrolDwell secondi: presenza continuativa
                // che cattura i command post.
                const bool cmdActive = world.enemyCommand.active && team->teamId == 2;
                float wx = 0.0f, wz = 0.0f;
                bool  haveTarget = false;
                if (ai->leashRadius > 0.0f)
                {
                    // Comandante con leash (ADR-041): NON insegue obiettivi né
                    // segnali — tiene la sua area. La "pattuglia" è restare a casa;
                    // il clamp del leash fa il resto. Combatte solo se attaccato
                    // (rami Alert/Hunt sopra), sempre dentro il raggio.
                    wx = ai->leashX; wz = ai->leashZ; haveTarget = true;
                }
                else if (myTeam == 1 && sq && sq->hasActiveOrder() && ai->allySigValid
                         && (sq->order == OrderType::HoldPosition || sq->order == OrderType::Advance
                             || sq->order == OrderType::Retreat   || sq->order == OrderType::Follow
                             || sq->order == OrderType::Regroup))
                {
                    // ORDINE di postura del player: il waypoint è la posizione LIBERA scelta
                    // a inizio tick (selectOrderWaypoint: occupancy + direzione della postura).
                    // Per Hold il clamp di presidio (holdRadius) ce lo tiene mentre combatte;
                    // per Advance/Retreat/Follow salta di posizione in posizione. Precede la
                    // TORRE: l'ordine diretto del giocatore vince sul segnale. [[orders-design-vision]]
                    wx = ai->allySigX; wz = ai->allySigZ; haveTarget = true;
                }
                else
                {
                    // ── Intento del COMANDO: sovrascrive la pattuglia (ADR-045) ──
                    // Prima le unità su route ignoravano comandante e torre: metà
                    // forza sorda al comando, su un binario fisso. Ora Advance e
                    // Retreat valgono per TUTTI (route o no); Hold — o nessun
                    // comando — lascia pattugliare (route FLUIDA, più sotto).
                    if (cmdActive)   // droidi: direttiva del Droide Tattico (doc 32)
                    {
                        const auto* d = pickEnemyDirective(world, ai->bias, et->x, et->z);
                        if (d && d->stance == World::EnemyCommand::Retreat && world.activeMap)
                        {
                            // Ripiego GLOBALE o PER-FRONTE: cadi sulla linea tenuta più
                            // vicina (non fino allo spawn), poi riprendi da lì.
                            retreatPointForTeam(world, 2, et->x, et->z, wx, wz);
                            haveTarget = true;
                        }
                        else if (d && d->stance == World::EnemyCommand::Advance)
                        {
                            // #2b: come i cloni — commitment su un waypoint RAGGIUNGIBILE
                            // + timer, così i droidi occupano il buon terreno del fronte
                            // (vantage/cover, anche ELEVATO) senza oscillare né finire su
                            // isole irraggiungibili. (Riusa i campi allySig*: un'unità è di
                            // una sola fazione, quindi non c'è conflitto col ramo clone.)
                            ai->allySigTimer -= dt;
                            const float rdx = et->x - ai->allySigX, rdz = et->z - ai->allySigZ;
                            // "Arrivato" deve valere anche in QUOTA: senza, chi sta sotto
                            // una piattaforma a 2 m in pianta si crede arrivato e molla
                            // il waypoint senza mai salire le scale (misurato su firebase).
                            const float rdy = et->y - ai->allySigY;
                            const bool reached = ai->allySigValid
                                              && (rdx*rdx + rdz*rdz < 3.0f*3.0f)
                                              && std::fabs(rdy) < config::STEP_HEIGHT * 2.0f;
                            if (ai->allySigValid && !reached && ai->allySigTimer > 0.0f)
                            { wx = ai->allySigX; wz = ai->allySigZ; haveTarget = true; }
                            else
                            {
                                // Obiettivo del fronte: un post da catturare, o un punto
                                // sull'anello (diverso per bias → non si ammassano).
                                float tx = 0.0f, tz = 0.0f, ty = 0.0f;   // ty: quota del waypoint (arrivo 3D)
                                bool got = nearestCapturablePost(world, et->x, et->z, 2,
                                                                 tx, tz, d->x, d->z, d->radius);
                                if (!got) got = nearestCapturablePost(world, et->x, et->z, 2, tx, tz);
                                if (!got)
                                {
                                    const float ang = ai->bias * 6.2831853f;
                                    tx = d->x + std::cos(ang) * d->radius * 0.6f;
                                    tz = d->z + std::sin(ang) * d->radius * 0.6f;
                                }
                                // ENEMY-AWARE (KI #82): posizione di TIRO che batte un nemico
                                // noto del fronte (LOS verificata), anche elevata, invece del
                                // solo terreno importante (che può essere cieco). Senza nemico
                                // noto → miglior terreno (proattivo, superiorità di fuoco).
                                const TacticalPositionDef* adv = nullptr;
                                float ex, ey, ez;
                                if (world.activeMap
                                    && nearestEnemyNear(world, 2, d->x, d->z, d->radius + 15.0f, ex, ey, ez))
                                    adv = worldintel::bestFiringPosition(*world.activeMap,
                                                                         d->x, d->z, ex, ey, ez, d->radius + 4.0f);
                                if (!adv && world.activeMap)
                                    adv = worldintel::bestAdvantageInArea(*world.activeMap,
                                                                          et->x, et->z, d->x, d->z, d->radius);
                                if (adv && (!world.nav || !world.nav->crowdReady()
                                            || world.nav->isReachable({et->x, et->y, et->z},
                                                                      {adv->x, adv->y, adv->z})))
                                { tx = adv->x; tz = adv->z; ty = adv->y; }
                                ai->allySigX = tx; ai->allySigZ = tz; ai->allySigY = ty;
                                ai->allySigValid = true; ai->allySigTimer = config::ALLYSIG_COMMIT_TIME;
                                wx = tx; wz = tz; haveTarget = true;
                            }
                        }
                        else if (d && d->stance == World::EnemyCommand::Hold
                                 && ai->holdRadius > 0.0f)
                        {
                            // Àncora di presidio già scelta a inizio tick (ADR-046,
                            // opzione A): il waypoint È la posizione difensiva. Il
                            // clamp ci tiene il droide, che combatte DA lì senza
                            // inseguire. Àncora 0 (nessuna posizione) → pattuglia.
                            wx = ai->holdX; wz = ai->holdZ; haveTarget = true;
                        }
                        // Hold senza posizione / nessuna direttiva → pattuglia.
                    }
                    // Cloni: torre di controllo (doc 36) **oppure** obiettivo di
                    // missione (A6). La condizione era il solo `allyIntel.active`, e
                    // senza torre l'intero ramo era irraggiungibile: su firebase i
                    // cloni restavano in pattuglia e il post richiesto dalla
                    // missione non veniva preso MAI (KI #90). La torre degrada
                    // l'INTEL tattica; l'obiettivo di missione è un'altra cosa — è
                    // ciò che alla squadra è stato detto prima di schierarsi, e non
                    // dipende da un'antenna.
                    else if (team->teamId == 1
                             && (world.allyIntel.active || [&] {
                                    for (const auto& ao : world.activeObjectives)
                                        if (ao.actorTeam == 1) return true;
                                    return false; }()))
                    {
                        // La torre SEGNALA (non ordina): il clone sceglie un segnale
                        // decorrelato dal bias e vi si muove. Ora vale anche per i
                        // cloni su route (prima li ignoravano). Se tutti i segnali
                        // sono saturi, `pickAllySignal` fallisce → pattuglia.
                        // COMMITMENT su un WAYPOINT già scelto e verificato
                        // raggiungibile: ci si va senza ricalcolare, finché non lo si
                        // raggiunge o scade il timer di ri-valutazione. Toglie
                        // l'oscillazione avanti-indietro E bounda il findPath di
                        // raggiungibilità (~1 ogni 4 s per clone, non a ogni tick).
                        ai->allySigTimer -= dt;
                        const float rdx = et->x - ai->allySigX, rdz = et->z - ai->allySigZ;
                        // Stesso controllo in quota del ramo sopra: un waypoint su
                        // una piattaforma non è raggiunto stando a terra sotto di essa.
                        const float rdy = et->y - ai->allySigY;
                        const bool reached = ai->allySigValid
                                          && (rdx*rdx + rdz*rdz < 3.0f*3.0f)
                                          && std::fabs(rdy) < config::STEP_HEIGHT * 2.0f;
                        if (ai->allySigValid && !reached && ai->allySigTimer > 0.0f)
                        {
                            wx = ai->allySigX; wz = ai->allySigZ; haveTarget = true;
                            // Ri-rivendica la posizione impegnata (occupancy): senza, durante
                            // il commitment risultava "libera" e un compagno la prendeva → ammasso
                            // residuo dei cloni autonomi (fix audit #1).
                            if (ai->allySigIdx >= 0 && ai->allySigIdx < (int)world.allyTac.claimed.size())
                                world.allyTac.claimed[ai->allySigIdx] = 1;
                            ++g_tac.allySignalFollow;
                        }
                        else
                        {
                            float sx = 0.0f, sz = 0.0f, srad = 8.0f;
                            bool sigIsObjective = false;
                            if (pickAllySignal(world, ai->bias, et->x, et->z, sx, sz, srad,
                                               &sigIsObjective))
                            {
                                // ── OCCUPARE ≠ CATTURARE (A6) ────────────────
                                // Misurato: con il solo bias sul settore i cloni
                                // convergevano su Alpha (tutti fra 7,7 e 16,4 m) ma
                                // **nessuno entrava nei 6 m** del post — perché
                                // `bestOrderPosition` li manda su posizioni tattiche
                                // VICINE, che è la lettura giusta per un segnale
                                // della torre ("stai qui tatticamente") e sbagliata
                                // per un obiettivo di cattura ("stai SU questo").
                                //
                                // Divisione del compito col `bias`, l'idioma già in
                                // uso ovunque per decorrelare: una parte va sul
                                // punto, il resto prende posizione attorno e copre.
                                // Mandarli tutti sul punto sarebbe il "branco" che
                                // abbiamo già revertato una volta (changelog 77).
                                const bool ontoPoint = sigIsObjective && ai->bias < 0.5f;

                                // Default: centro del segnale (a terra → raggiungibile).
                                float tx = sx, tz = sz, ty = 0.0f;   // ty: quota del waypoint (arrivo 3D)
                                // Miglior terreno del fronte (vantage/cover/defensive per
                                // importanza, incluse le ELEVATE) — ma SOLO se davvero
                                // RAGGIUNGIBILE: niente isole (scale troppo ripide) o
                                // passaggi erosi dove il clone finirebbe in trappola
                                // contro un muro (screenshot utente 2026-07-23).
                                // TOWER-HUB (Fase torre-hub): la posizione la sceglie dai
                                // dati PRE-CALCOLATI dalla torre (bestOrderPosition: score con
                                // LOS sui nemici + occupancy CENTRALE), non rifacendo la LOS
                                // per-clone. Premia le posizioni che BATTONO un nemico ora →
                                // niente punti ciechi; la claimed evita l'ammasso.
                                int aidx = -1; bool claimedPos = false;
                                // `ontoPoint`: chi ha il compito di PRENDERE il punto
                                // non cerca una posizione tattica — il centro del
                                // segnale (il post) è già la sua meta, e `tx/tz` lo
                                // sono già. Gli altri coprono dai dintorni.
                                const TacticalPositionDef* adv = ontoPoint ? nullptr
                                    : bestOrderPosition(
                                        world, et->x, et->z, sx, sz, srad, 0, sx, sz, &aidx);
                                if (adv && (!world.nav || !world.nav->crowdReady()
                                            || world.nav->isReachable({et->x, et->y, et->z},
                                                                      {adv->x, adv->y, adv->z})))
                                { tx = adv->x; tz = adv->z; ty = adv->y;
                                  if (aidx >= 0 && aidx < (int)world.allyTac.claimed.size())
                                      { world.allyTac.claimed[aidx] = 1; claimedPos = true; } }
                                else if (!ontoPoint)
                                {
                                    float px, pz;   // niente posizione raggiungibile → il post del fronte
                                    if (nearestCapturablePost(world, et->x, et->z, 1, px, pz, sx, sz, srad))
                                    { tx = px; tz = pz; }
                                }
                                ai->allySigX = tx; ai->allySigZ = tz; ai->allySigY = ty;
                                // Indice rivendicato: serve a RI-RIVENDICARLO durante il
                                // commitment (sotto), così la posizione resta "occupata" per
                                // i compagni e la squadra non si ammassa (fix audit #1).
                                ai->allySigIdx = claimedPos ? aidx : -1;
                                // Ri-valuta solo DOPO essere arrivato (o dopo un timeout
                                // generoso di sicurezza): un timer corto scattava a metà
                                // di una salita e faceva tornare indietro il clone
                                // (segnalato dall'utente). Lo stuck-recovery copre il caso
                                // "non arriva mai", quindi qui il timeout può essere lungo.
                                ai->allySigValid = true; ai->allySigTimer = config::ALLYSIG_COMMIT_TIME;
                                wx = tx; wz = tz; haveTarget = true;
                                ++g_tac.allySignalFollow;
                            }
                            else ai->allySigValid = false;
                        }
                    }

                    // Ultimo bersaglio: nessuna unità nemica → finisci la struttura.
                    if (!haveTarget && targets.empty())
                        haveTarget = nearestEnemyStructure(world, team->teamId,
                                                           et->x, et->z, wx, wz);

                    // ── Pattuglia: route FLUIDA (ADR-045) ────────────────────
                    // Chi non ha una route se ne aggancia una: la PIÙ VICINA, dal
                    // PUNTO più vicino (non solo dagli estremi). Le route diventano
                    // una rete condivisa. Il bersaglio è il punto-obiettivo corrente
                    // (`patrolSeg`), percorso bidirezionalmente da advancePatrol.
                    if (!haveTarget)
                    {
                        if (ai->patrolRoute < 0)
                            joinNearestRoute(*ai, world.activeMap, et->x, et->z);
                        if (ai->patrolRoute >= 0 && world.activeMap
                            && ai->patrolRoute < (int)world.activeMap->patrolRoutes.size())
                        {
                            const auto& pts = world.activeMap->patrolRoutes[ai->patrolRoute].points;
                            int si = ai->patrolSeg;
                            if (si < 0 || si >= (int)pts.size()) si = 0;
                            wx = pts[si][0]; wz = pts[si][2];
                        }
                        else   // nessuna route autorata: A/B legacy (avanti-indietro)
                        {
                            wx = ai->goingToB ? ai->patrolBx : ai->patrolAx;
                            wz = ai->goingToB ? ai->patrolBz : ai->patrolAz;
                        }
                        haveTarget = true;
                    }
                }
                moveDX = wx - et->x; moveDZ = wz - et->z;

                if (ai->waitTimer > 0.0f)
                {
                    // In sosta: fermo, niente anti-stuck
                    ai->waitTimer -= dt;
                    ai->stuckTimer = 0.0f;
                    moveDX = 0; moveDZ = 0;
                    if (ai->waitTimer <= 0.0f)
                        advancePatrol(*ai, world.activeMap);
                }
                else if ((moveDist = norm2D(moveDX, moveDZ)) < 0.6f)
                {
                    // Sosta LUNGA solo se il waypoint è su un command post: lì la
                    // presenza continuativa serve a catturare. Su un waypoint
                    // qualunque della route si prosegue subito — prima si sostava
                    // 12s OVUNQUE e la pattuglia passava la vita ferma (feedback
                    // utente: "non usano i path in maniera fluida").
                    const bool atObjective =
                        world.activeMap
                        && worldintel::nearCommandPost(*world.activeMap, et->x, et->z, 1.0f);
                    if (ai->patrolDwell > 0.0f && atObjective)
                        ai->waitTimer = ai->patrolDwell;
                    else
                        advancePatrol(*ai, world.activeMap);
                    moveDX = 0; moveDZ = 0;
                }
                else if (isStuck)
                { advancePatrol(*ai, world.activeMap); ai->stuckTimer = 0; }
                else
                { moveSpeed = ai->patrolSpeed; et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI); }
            }
        }
        else if (nearest != 0)
        {
            const auto* tt = world.getTransform(nearest);
            if (tt)
                et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);
        }

        // ── Vincolo di squadra: modello a GUINZAGLIO (ADR-020 / doc 26) ──
        // L'ordine vincola il MOVIMENTO, mai il combattimento: mirare e sparare
        // restano autonomi (codice sotto). E vincola SOLO quando non è
        // soddisfatto: dentro il raggio l'AI è libera di strafare, coprirsi e
        // fare micro-combattimento — è il "autonoma DENTRO il vincolo" del doc.
        // Fuori dal raggio l'ordine ha precedenza su qualunque stato (anche
        // Alert: è ciò che impedisce agli alleati di inseguire i nemici lontano
        // dal leader). Non scrive mai il transform: passa per il crowd (doc 22).
        // FocusFire è escluso di proposito: vincola il BERSAGLIO (sopra), non il
        // movimento — l'AI continua a manovrare come sa mentre concentra il fuoco.
        // Il guinzaglio NON annulla una MANOVRA TATTICA in corso (ADR-035): girando
        // dopo il blocco Alert, sovrascriveva moveDX/DZ e riagganciava il membro al
        // leader appena superava il raggio → la manovra non partiva mai e il clone
        // oscillava avanti e indietro. È il motivo per cui i droidi (senza squadra,
        // quindi senza guinzaglio) sembravano più intelligenti dei cloni.
        if (sq && sq->hasActiveOrder() && !ai->stationary
            && !ai->repositionActive
            && sq->order != OrderType::FocusFire
            // Le posture a WAYPOINT/clamp NON usano il guinzaglio: Hold (clamp di presidio
            // sulla posizione scelta), Advance/Retreat (sbalzi verso/lontano dal nemico),
            // Regroup (settore). Hanno già il loro waypoint (selectOrderWaypoint).
            && sq->order != OrderType::HoldPosition && sq->order != OrderType::Advance
            && sq->order != OrderType::Retreat     && sq->order != OrderType::Regroup)
        {
            float ox = sq->targetX - et->x, oz = sq->targetZ - et->z;
            const float od = norm2D(ox, oz);   // normalizza ox/oz, ritorna la distanza
            // FOLLOW tiene il guinzaglio come SICUREZZA (non restare indietro se il leader
            // corre) MENTRE il suo waypoint è una copertura vicino al leader
            // (selectOrderWaypoint) → segue di cover in cover, non incollato. In Alert
            // si allarga per poter manovrare/chiudere la distanza.
            const float followLeash = (ai->state == AiState::Alert) ? 15.0f : 8.0f;
            const float leash = (sq->order == OrderType::Follow)       ? followLeash
                              : (sq->order == OrderType::CoveringFire) ? 2.0f
                                                                       : 1.5f;  // MoveTo/TakeCover/Revive
            if (od > leash)
            {
                moveDX = ox; moveDZ = oz;      // direzione unitaria verso il target
                moveDist = od;                 // destinazione reale per il pathfinding
                moveSpeed = ai->seekSpeed;
                orderTravel = true;
                // In Alert NON si tocca il facing: l'AI continua a mirare al
                // nemico mentre si riposiziona (il combattimento resta suo).
                if (ai->state != AiState::Alert)
                    et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI);
            }
            // dentro il guinzaglio: nessun override — decide l'AI.
        }

        // Danger zone: SOLO come fallback senza navmesh (ADR-025). Col crowd il
        // navmesh marca le danger come area a costo alto (doc 22 Phase C) e il
        // pathfinding le aggira già → la repulsione manuale sarebbe una doppia
        // verità. Fuori dall'ingaggio (in Alert si combatte, non si evita).
        const bool navActive = world.nav && world.nav->crowdReady()
                             && ai->crowdAgentIdx >= 0;
        if (!navActive && ai->state != AiState::Alert && moveSpeed > 0.0f)
            applyDangerRepulsion(world.activeMap, et->x, et->z, moveDX, moveDZ);

        // Cooldown abilità attive + scatto roll in corso (16 est.)
        if (auto* ab = world.getAbilities(e))
            for (auto& s : ab->states)
                if (s.cooldown > 0.0f) s.cooldown -= dt;

        // ── Leash del comandante (ADR-041) ───────────────────────────────
        // Clamp universale: qualunque cosa l'AI voglia fare (difendersi, coprirsi,
        // inseguire), non può portarla oltre `leashRadius` dal suo punto di
        // spawn. Se è già fuori (spinta, knockback), il movimento di questo tick
        // punta a rientrare. Il passo per-tick è piccolo (~cm), quindi lo sforo
        // oltre il bordo è trascurabile. 0 = nessun leash (tutte le altre unità).
        if (ai->leashRadius > 0.0f)
        {
            const float hx = et->x - ai->leashX, hz = et->z - ai->leashZ;
            const float d2 = hx * hx + hz * hz;
            if (d2 > ai->leashRadius * ai->leashRadius)
            {
                const float d = std::sqrt(d2);
                moveDX = -hx / d; moveDZ = -hz / d;   // torna verso casa
                moveDist  = d - ai->leashRadius;
                moveSpeed = (moveSpeed > 0.0f) ? moveSpeed : ai->seekSpeed;
            }
        }
        // ── Àncora di PRESIDIO (ADR-046, opzione A) ──────────────────────
        // Stesso clamp del leash ma con centro dinamico (la posizione difensiva):
        // il droide in TIENI combatte/si copre entro il raggio ma non insegue oltre.
        // Esclusiva col leash del comandante (holdRadius resta 0 per il comandante).
        else if (ai->holdRadius > 0.0f)
        {
            const float hx = et->x - ai->holdX, hz = et->z - ai->holdZ;
            const float d2 = hx * hx + hz * hz;
            if (d2 > ai->holdRadius * ai->holdRadius)
            {
                const float d = std::sqrt(d2);
                moveDX = -hx / d; moveDZ = -hz / d;   // rientra sulla posizione
                moveDist  = d - ai->holdRadius;
                moveSpeed = (moveSpeed > 0.0f) ? moveSpeed : ai->seekSpeed;
            }
        }

        // Esecuzione movimento (ADR-017 Phase B): via crowd Detour se attivo,
        // altrimenti fallback su aiMove (navmesh assente). Traversata (Hunt/
        // Search/Patrol) → requestMoveTarget (pathfinding: AGGIRA gli ostacoli,
        // i rami traversal impostano moveDX/DZ = destinazione − posizione).
        // Alert/roll → requestMoveVelocity (velocità tattica + avoidance del
        // crowd). Il write-back npos→transform lo fa CrowdSystem dopo il tick.
        const bool useCrowd = navActive;   // (calcolato sopra per il gating danger)
        if (useCrowd)
        {
            NavManager& nv = *world.nav;
            if (ai->rollTimer > 0.0f)
            {
                ai->rollTimer -= dt;
                nv.requestMoveVelocity(ai->crowdAgentIdx, {ai->rollVX, 0.0f, ai->rollVZ});
            }
            else if (ai->state == AiState::Alert && !orderTravel)
            {
                const float m = norm2D(moveDX, moveDZ);
                nv.requestMoveVelocity(ai->crowdAgentIdx,
                    m > 0.001f ? glm::vec3{moveDX/m*moveSpeed, 0.0f, moveDZ/m*moveSpeed}
                               : glm::vec3{0.0f, 0.0f, 0.0f});
            }
            else if (moveSpeed > 0.0f && (moveDX != 0.0f || moveDZ != 0.0f))
                // Destinazione REALE (moveDX/DZ è un versore: va riscalato per
                // moveDist). Con la destinazione vera Detour pianifica un path e
                // aggira gli ostacoli; col vecchio +moveDX puntava a 1 m e spingeva
                // dritto contro i muri.
            {
                // LA DESTINAZIONE PORTA LA SUA QUOTA (2026-08-04). Qui si passava
                // `et->y`, cioè l'altezza di CHI CAMMINA: `findNearestPoly` cercava
                // il poligono più vicino a quella quota, quindi puntando un posto
                // su una piattaforma agganciava il **pavimento sotto** invece del
                // ripiano sopra. L'unità ci andava a terra credendo di aver
                // obbedito — misurato: quota 0,60 costante per 9000 tick mentre
                // l'obiettivo stava a 1,0. È il motivo per cui un ordine MoveTo
                // funzionava (lì la quota arriva col punto) e l'iniziativa no.
                const float dstX = et->x + moveDX * moveDist;
                const float dstZ = et->z + moveDZ * moveDist;
                const float dstY = world.activeMap
                                 ? mapquery::groundHeightAt(world.activeMap, dstX, dstZ)
                                 : et->y;
                nv.requestMoveTarget(ai->crowdAgentIdx, {dstX, dstY, dstZ});
            }
            else
                nv.requestMoveVelocity(ai->crowdAgentIdx, {0.0f, 0.0f, 0.0f});
        }
        else
        {
            float nx, nz;
            if (ai->rollTimer > 0.0f)
            {
                ai->rollTimer -= dt;
                nx = et->x + ai->rollVX * dt;   // lo scatto vince sul movimento
                nz = et->z + ai->rollVZ * dt;
            }
            else
            {
                nx = et->x + moveDX * moveSpeed * dt;
                nz = et->z + moveDZ * moveSpeed * dt;
            }
            aiMove(*et, nx, nz, *ai, dt, world);
        }

        // ── Salto anti-ostacolo (jump_enabled dal profilo AI) ─────────
        // Solo nel fallback aiMove: col crowd il pathfinding aggira gli
        // ostacoli, il salto non serve (e velY non verrebbe integrato).
        if (!useCrowd && ai->jumpEnabled && !ai->stationary && moveSpeed > 0.0f
            && ai->velY == 0.0f
            && ai->stuckTimer > config::AI_STUCK_TIME * 0.5f)
        {
            ai->velY = config::AI_JUMP_IMPULSE;
        }

        // ── Raffreddamento arma (sempre, anche fuori combattimento) ───
        if (ai->heat > 0.0f)
        {
            ai->heat -= ai->cooldownRate * dt;
            if (ai->heat <= 0.0f) { ai->heat = 0.0f; ai->overheated = false; }
        }

        // Telemetria (ADR-016 / 06_Todo #1): transizione di stato AI. Solo sul
        // CAMBIO (oldState catturato a inizio iterazione) → niente flooding.
        // Piazzato PRIMA del blocco sparo, che ha molti `continue`. Lo stato è
        // già finalizzato qui (le transizioni avvengono sopra).
        if (ai->state != oldState)
        {
            nlohmann::json d;
            d["bot_id"] = e;
            d["state"]  = aiStateName(ai->state);
            d["pos"]    = { et->x, et->y, et->z };
            if (nearest != 0)
            { if (const auto* tt2 = world.getTransform(nearest))
                  d["target_pos"] = { tt2->x, tt2->y, tt2->z }; }
            else if (ai->hasLastKnown)
                d["target_pos"] = { ai->lastKnownX, 0.0f, ai->lastKnownZ };
            // DECLASSATO a Debug (2026-08-03). Misurato: era il **39% del file**
            // di telemetria (743 righe su una sim da 3000 tick) e non è mai
            // servito a nessuna diagnosi — la storia di un agente la racconta
            // meglio `--trace-ai`, e gli stati aggregati stanno in "tactical
            // decisions". Resta disponibile con `--telemetry-verbose`: il punto
            // non è togliere informazione, è non affogare quella che si legge.
            telemetry::event(telemetry::Level::Debug, "AI", "state change", d);
        }

        // ── Sparo (solo in Alert con LOS, mai in fase evasiva) ───────
        // Funnel d'ingaggio, troncone GATE (KI #86): la popolazione è "ha un
        // bersaglio", cioè esattamente il sintomo "sta davanti a un nemico". Ogni
        // ramo che NON spara viene attribuito, così il colpo mancato ha un nome.
        if (nearest == 0) continue;
        if (ai->state != AiState::Alert) { ++g_tac.gateState; continue; }
        if (ai->evading)
        {   // peek/hide: in "hide" non spara
            ++g_tac.gateEvading;
            if (ai->repositionActive) ++g_tac.evadingInRepo;
            if (ai->stationary)       ++g_tac.evadingFrozen;
            continue;
        }
        if (ai->shootCooldown > 0.0f)
        { ++g_tac.gateCooldown; ai->shootCooldown -= dt; continue; }
        if (ai->overheated) { ++g_tac.gateOverheat; continue; } // arma surriscaldata: attende il cooling

        const auto* tt = world.getTransform(nearest);
        if (!tt) continue;
        // Il bersaglio è cachato per più frame (time-slicing): ri-verifica il
        // LOS al tiro, così non spara attraverso i muri se nel frattempo si è
        // coperto. Costa un LOS solo quando il cooldown è scaduto (raro), non
        // ogni frame → economico anche a scala.
        // Punto di mira: il CORPO del bersaglio. Per un'unità è il transform,
        // ma una struttura ha il transform a terra — mirandolo si spara nel
        // pavimento e il LOS fallisce sempre. E il bersaglio non deve occludere
        // sé stesso (doc 35): senza `ignore` una struttura colpibile era
        // ingaggiabile ma mai colpibile.
        const auto* tcol = world.getCollider(nearest);
        // Mira: STRUTTURA al corpo (collider, transform a terra); UNITÀ al BUSTO ALTO
        // (~ground+0.85 m). Restare nella hitbox (~1 m) ma abbastanza in alto da far
        // SCAVALCARE i muretti bassi lungo il raggio (fix interim KI #82) — puntare al
        // centro-corpo (~0.5 m) faceva cadere il raggio e un muretto vicino al bersaglio
        // lo tagliava, anche se il nemico era colpibile in alto.
        const float aimY = isStructure(nearest)
            ? tt->y + (tcol ? tcol->hy * 0.5f : 1.0f)
            : tt->y + config::AI_HALF_Y * 0.7f;
        // Origine del tiro = OCCHI (KI #79): coerente con l'acquisizione E con lo
        // spawn/traiettoria del proiettile sotto → l'unità spara SOPRA la propria
        // cover verso il corpo del nemico, invece di piantare il colpo nel muro davanti.
        // Funnel verticalità (2/3 e 3/3): l'ingaggio è cross-quota? e il gate di tiro
        // lo lascia passare o lo uccide qui? È la distinzione che a occhio non si vede:
        // "acquisisce ma non spara" (LOS di tiro) vs "non acquisisce" (sopra).
        const bool vertEngage =
            std::fabs(tt->y - et->y) > config::VERTICAL_ENGAGE_DY;
        if (!physics::hasLineOfSight(eyePos, {tt->x, aimY, tt->z}, world, nearest))
        { ++g_tac.gateLos; if (vertEngage) ++g_tac.vertFireBlocked; continue; }
        ++g_tac.gateFired;
        ai->obsFired = true;   // scatola nera: in questa finestra ha combattuto
        // Indagine 2026-08-02 ("sparano a qualcuno ignorando i nemici vicinissimi"):
        // qui si contavano i tiri con un nemico PIÙ VICINO del bersaglio, e se quel
        // nemico fosse colpibile. Risposta netta: 15% dei tiri hanno un nemico più
        // vicino entro 10 m, ma **0%** di quei nemici era colpibile — erano tutti
        // dietro copertura. La selezione del bersaglio è corretta (prende il più
        // vicino con LOS, per costruzione); il difetto percepito è di LETTURA, non di
        // targeting. Diagnostica rimossa: costava una LOS extra per colpo.
        if (vertEngage) ++g_tac.vertShot; else ++g_tac.flatShot;
        float dx = tt->x-eyePos.x, dy = aimY-eyePos.y, dz = tt->z-eyePos.z;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 0.001f) continue;

        // MUZZLE (interim, senza pose): il proiettile parte AVANTI lungo la linea di
        // tiro GIÀ verificata (non dal petto) → sembra uscire dall'arma e supera un
        // ostacolo a ridosso del tiratore. Sicuro: il punto è sul raggio LOS-ok.
        const float kMuzzleFwd = 0.5f;
        const glm::vec3 muzzle{ eyePos.x + dx/len*kMuzzleFwd,
                                eyePos.y + dy/len*kMuzzleFwd,
                                eyePos.z + dz/len*kMuzzleFwd };

        // Dispersione dal profilo AI: accuracy 1 = perfetto, 0 = max spread.
        // Perturba la direzione normalizzata di un angolo casuale.
        // Conseguenza di un obiettivo (doc 25): un nemico "disorganizzato" (es.
        // torre delle comunicazioni distrutta) spara peggio. Vale solo per il
        // team 2 e solo se un obiettivo l'ha deciso — di default il
        // moltiplicatore è 1 e il comportamento è identico a prima.
        float effAccuracy = ai->accuracy;
        if (team->teamId == 2 && world.battleState.enemyAccuracyMult != 1.0f)
            effAccuracy = std::clamp(effAccuracy * world.battleState.enemyAccuracyMult,
                                     0.0f, 1.0f);
        // SOPPRESSIONE (A3): sotto tiro si mira peggio. È l'effetto che rende il fuoco
        // di soppressione una tattica reale invece che munizioni sprecate.
        if (ai->suppression > 0.001f)
            effAccuracy *= (1.0f - ai->suppression * config::SUPPRESSION_ACC_PENALTY);
        const float spread = (1.0f - effAccuracy) * config::AI_SPREAD_MAX;
        if (spread > 0.0f)
        {
            dx += (aiRand01() - 0.5f) * 2.0f * spread * len;
            dy += (aiRand01() - 0.5f) * 2.0f * spread * len;
            dz += (aiRand01() - 0.5f) * 2.0f * spread * len;
            len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len < 0.001f) continue;
        }

        float inv = ai->bulletSpeed / len;
        EntityId b = world.createEntity();
        // Spawn dal MUZZLE stimato (avanti sulla linea di tiro): coerente con LOS e
        // traiettoria; sembra uscire dall'arma invece che dal petto (fix interim KI #82).
        world.addTransform(b, TransformComponent{.x=muzzle.x,.y=muzzle.y,.z=muzzle.z,.sx=0.10f,.sy=0.10f,.sz=0.10f});
        world.addVelocity(b, {dx*inv, dy*inv, dz*inv});
        world.addTeam(b, {myTeam});
        world.addBullet(b, {ai->bulletDamage, ai->bulletLifetime, myTeam});
        if (myTeam >= 1 && myTeam <= 2) ++world.shotsFired[myTeam];   // denominatore del funnel (O4)
        // Lo sparo FA RUMORE (doc 40): chiunque abbia orecchie nel raggio se ne accorge.
        // È il meccanismo che propaga una battaglia invece di lasciarla locale.
        world.sounds.push_back({et->x, et->z, config::SOUND_GUNSHOT_RADIUS, myTeam, e});
        if (ai->bulletMesh)
            world.addMeshRenderer(b, {ai->bulletMesh, ai->bulletTexture, ai->bulletR, ai->bulletG, ai->bulletB});

        // Cadenza dall'arma se disponibile, altrimenti legacy dal profilo AI.
        // Fuoco di copertura: ~30% di volume in più (soppressione = cadenza).
        ai->shootCooldown = (ai->fireInterval > 0.0f) ? ai->fireInterval
                                                      : ai->shootInterval;
        if (covering) ai->shootCooldown *= 0.7f;

        // Surriscaldamento (se l'arma lo prevede)
        if (ai->heatPerShot > 0.0f)
        {
            ai->heat += ai->heatPerShot;
            if (ai->heat >= 1.0f)
            {
                ai->heat = 1.0f;
                ai->overheated = true;
                ai->shootCooldown = ai->overheatPenalty;
            }
        }
    }
}

} // namespace mini
// AiCommandLayer.cpp — livello di COMANDO dell'AI (audit #7, changelog 95).
//
// Separato da `AiSystem.cpp` (era 2578 righe: sensing, combattimento, manovra,
// comando, ordini e movimento in un solo file e in una `update()` da 1740 righe).
// Qui vive il "cosa si decide a livello di teatro":
//   · stato dei settori (ADR-034) e loro peso tattico condiviso (changelog 88);
//   · TORRE DI CONTROLLO dei cloni — segnali, non ordini (doc 36, ADR-040) e il
//     QUADRO TATTICO pre-calcolato per tutti (torre-hub, changelog 93);
//   · direttive del DROIDE TATTICO lette per-droide (doc 32 v2, changelog 86);
//   · selezione della posizione per gli ORDINI di postura del player
//     (Hold/Advance/Retreat/Follow/Regroup — [[orders-design-vision]]).
// In `AiSystem.cpp` resta il "come la singola unità esegue".
//
// Le funzioni sono state spostate VERBATIM dai loro `static` originali: nessuna
// modifica di logica, comportamento invariato per costruzione (verificato con
// `--sim-ticks`, deterministico: `--sim` NON lo era e la prima verifica dello split
// con quello non provava nulla — vedi changelog 95). Dichiarate in AiInternal.hpp.

#include "AiInternal.hpp"
#include "mini/ecs/systems/AiSystem.hpp"

#include "mini/game/ai/WorldIntel.hpp"       // query pure sulla mappa (ADR-025)
#include "mini/game/ai/AiUtility.hpp"        // pesi delle decisioni tattiche (doc 40 A5)
#include "mini/game/nav/NavManager.hpp"      // isReachable: niente posizioni-isola
#include "mini/core/GameConfig.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mini
{
namespace aicmd
{

// Command post più vicino a (x,z) NON posseduto da `team`: l'obiettivo naturale
// da prendere per QUELLA unità. Ogni droide sceglie il proprio (ADR-024 v2): il
// comandante dà l'intento "avanzate", non un unico punto per tutti — così la
// forza si distribuisce su più obiettivi invece di ammassarsi su uno solo.
// `areaRadius > 0` limita la scelta ai post dentro il settore-obiettivo
// (ADR-034): il comandante indirizza la forza su una ZONA, ma ogni droide sceglie
// da sé il punto più vicino lì dentro — la direzione resta del comandante, il COME
// resta dell'AI, e le unità non si ammassano tutte sullo stesso post.
bool nearestCapturablePost(const World& world, float x, float z, int team,
                                  float& outX, float& outZ,
                                  float areaX, float areaZ,
                                  float areaRadius)
{
    if (!world.activeMap) return false;
    float best2 = 1e18f; bool found = false;
    for (const auto& st : world.commandPostStates)
    {
        if (st.owner == team) continue;
        for (const auto& cp : world.activeMap->commandPosts)
        {
            if (cp.label != st.label) continue;
            if (areaRadius > 0.0f)
            {
                const float ax = cp.x - areaX, az = cp.z - areaZ;
                if (ax * ax + az * az > areaRadius * areaRadius) break;   // fuori settore
            }
            const float dx = cp.x - x, dz = cp.z - z;
            const float d2 = dx * dx + dz * dz;
            if (d2 < best2) { best2 = d2; outX = cp.x; outZ = cp.z; found = true; }
            break;
        }
    }
    return found;
}

// Aggiorna lo stato dei settori (ADR-034): una sola passata sulle entità vive.
// Presenze contrapposte → chi controlla e quanto la zona è contesa. È il livello
// su cui il comandante ragiona invece di guardare solo l'owner dei command post.
void updateSectorStates(World& world, const std::vector<EntityId>& snap)
{
    const MapDef* map = world.activeMap;
    if (!map || map->sectors.empty()) { world.sectorStates.clear(); return; }

    world.sectorStates.assign(map->sectors.size(), World::SectorState{});
    for (EntityId e : snap)
    {
        const auto* tm = world.getTeam(e);
        const auto* tr = world.getTransform(e);
        const auto* h  = world.getHealth(e);
        if (!tm || !tr || !h || h->current <= 0.0f) continue;
        // Solo TRUPPE: strutture, veicoli e comandante non "controllano" una zona.
        if (world.getBullet(e) || world.getVehicle(e) || world.hasCommander(e)) continue;

        for (size_t s = 0; s < map->sectors.size(); ++s)
        {
            const auto& sec = map->sectors[s];
            const float dx = tr->x - sec.x, dz = tr->z - sec.z;
            if (dx * dx + dz * dz > sec.radius * sec.radius) continue;
            if (tm->teamId == 1)      ++world.sectorStates[s].allies;
            else if (tm->teamId == 2) ++world.sectorStates[s].enemies;
        }
    }

    for (auto& st : world.sectorStates)
    {
        const int tot = st.allies + st.enemies;
        if (tot == 0) { st.controllingTeam = 0; st.pressure = 0.0f; continue; }
        st.controllingTeam = (st.allies > st.enemies) ? 1
                           : (st.enemies > st.allies) ? 2 : 0;
        // Contesa = quanto le due parti si equivalgono lì dentro.
        const int lo = (st.allies < st.enemies) ? st.allies : st.enemies;
        st.pressure = (float)(2 * lo) / (float)tot;
    }
}

// Peso tattico BASE di un settore dal punto di vista di `myTeam` — l'analisi
// CONDIVISA da torre (updateAllyIntel, cloni) e droide tattico (updateEnemyCommand,
// droidi). Un'unica formula, così le due non possono più divergere: la 85 fu proprio
// un riallineamento a mano dopo che erano derivate. La CONTESA (pressione) è il motore,
// l'importanza la base; poi minoranza (in inferiorità → rinforza), possesso nemico
// (riprendere) e opportunità (valore poco difeso → sfruttare). `allies`=team1,
// `enemies`=team2 ([[AiSystem.cpp:318]]). Il comandante ci aggiunge fuori il bonus di
// stance (hold conteso) e i casi speciali (collasso). Non tocca la stance: solo il peso.
float sectorTacticalWeight(const SectorDef& sec, const World::SectorState& st, int myTeam)
{
    const int mine = (myTeam == 1) ? st.allies : st.enemies;
    const int foe  = (myTeam == 1) ? st.enemies : st.allies;
    const int foeTeam = (myTeam == 1) ? 2 : 1;
    float w = sec.importance * aiutility::kSector.importance;  // valore intrinseco (baseline)
    w += st.pressure * aiutility::kSector.pressure;      // CONTESO: c'è battaglia → forte richiamo
    if (foe > mine)                                     // in MINORANZA → rinforza (urgente)
        w += (float)(foe - mine) * aiutility::kSector.minority;
    if (st.controllingTeam == foeTeam)                  // in mano nemica → riprenderlo
        w += aiutility::kSector.enemyHeld;
    if (sec.importance > 0.5f && foe <= 1 && st.controllingTeam != myTeam)
        w += aiutility::kSector.opportunity;            // valore poco difeso → sfruttarlo
    return w;
}

// ── Torre di controllo dei cloni (doc 36, ADR-040) ───────────────────────────
// Pubblica una LISTA di posti che contano. Non sceglie per nessuno: non esiste
// un "obiettivo dei cloni", esistono segnali che ogni clone valuta per conto suo.
// È la differenza deliberata col Droide Tattico, che invece dà un intento unico.
void updateAllyIntel(World& world)
{
    world.allyIntel.active = false;
    world.allyIntel.signals.clear();

    bool tower = false;
    for (const auto& s : world.strategicTargets)
        if (s.isControl && s.team == 1 && s.entity != 0
            && world.isValidEntity(s.entity))
        { tower = true; break; }
    if (!tower) return;   // senza torre i cloni restano truppe puramente autonome

    const MapDef* map = world.activeMap;
    if (map)
        for (size_t i = 0; i < map->sectors.size(); ++i)
        {
            const auto& sec = map->sectors[i];
            const World::SectorState st = (i < world.sectorStates.size())
                                        ? world.sectorStates[i] : World::SectorState{};
            // Saldamente nostro e tranquillo → non è un posto che "conta".
            if (st.controllingTeam == 1 && st.pressure < 0.2f) continue;
            World::AllyIntel::Signal sig;
            sig.x = sec.x; sig.z = sec.z; sig.radius = sec.radius;
            // ── ANALISI tattica del settore (info, non ordine, doc 36) ──────
            // La torre non dice "andate lì": PESA quanto un settore conta, con
            // criteri leggibili — così i cloni si distribuiscono da soli dove
            // serve. [[control-tower-informs-not-orders]] Analisi CONDIVISA col
            // droide tattico (sectorTacticalWeight) → i due lati non divergono.
            sig.weight = sectorTacticalWeight(sec, st, 1);
            sig.label = sec.label;
            world.allyIntel.signals.push_back(sig);
        }

    // Strutture nemiche: "lì c'è un obiettivo", non "andate a distruggerlo".
    for (const auto& s : world.strategicTargets)
    {
        if (s.entity == 0 || s.team == 1) continue;
        if (!world.isValidEntity(s.entity)) continue;
        World::AllyIntel::Signal sig;
        sig.x = s.x; sig.z = s.z; sig.radius = 8.0f;
        sig.weight = s.priority * (s.isComms ? 1.6f : 1.0f);
        sig.label = s.label;
        world.allyIntel.signals.push_back(sig);
    }

    world.allyIntel.active = !world.allyIntel.signals.empty();

    // Saturazione (KI #73): quante truppe team-1 sono GIÀ su ogni segnale. Un
    // segnale già presidiato smette di attirarne altri, così i cloni si
    // distribuiscono invece di ammassarsi tutti sul poco che resta. Prima la
    // dispersione era solo emergente dal numero di segnali: con pochi segnali
    // fallisce, ed è proprio ciò che l'utente ha osservato.
    if (world.allyIntel.active)
        for (EntityId e : world.getEntities())
        {
            const auto* tm = world.getTeam(e);
            if (!tm || tm->teamId != 1) continue;
            if (world.getBullet(e) || !world.getAi(e)) continue;   // solo truppe AI
            const auto* t = world.getTransform(e);
            if (!t) continue;
            for (auto& sig : world.allyIntel.signals)
            {
                const float dx = sig.x - t->x, dz = sig.z - t->z;
                if (dx * dx + dz * dz < sig.radius * sig.radius) { ++sig.crowd; break; }
            }
        }
}

// Nemico vivo più vicino a (x,z) entro maxDist, di team diverso da myTeam. Serve al
// positioning ENEMY-AWARE (KI #82): scegliere una posizione di TIRO che batte un
// nemico noto, non un punto cieco scelto per sola importanza. I contatti sono di
// fatto condivisi (una squadra sa dove sono i nemici), quindi si usa la posizione
// reale del nemico più vicino all'area — anche se QUESTA unità non lo vede ancora.
bool nearestEnemyNear(const World& world, int myTeam, float x, float z,
                             float maxDist, float& ox, float& oy, float& oz)
{
    float best2 = maxDist * maxDist; bool found = false;
    for (EntityId o : world.getEntities())
    {
        const auto* tm = world.getTeam(o);
        if (!tm || tm->teamId == 0 || tm->teamId == myTeam) continue;
        if (world.getBullet(o)) continue;
        const auto* h = world.getHealth(o);
        if (!h || h->current <= 0.0f) continue;
        const auto* tr = world.getTransform(o);
        if (!tr) continue;
        const float dx = tr->x - x, dz = tr->z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best2) { best2 = d2; ox = tr->x; oy = tr->y; oz = tr->z; found = true; }
    }
    return found;
}

// Quale segnale segue QUESTO clone. Non il più importante: la scelta è pesata ma
// decorrelata dal `bias` individuale, così due cloni con la stessa informazione
// vanno in posti diversi. Se scegliessero tutti il massimo avremmo ricostruito
// un comando unico con un altro nome — esattamente ciò che la torre NON deve fare.
bool pickAllySignal(const World& world, float bias,
                           float fromX, float fromZ,
                           float& outX, float& outZ, float& outRadius)
{
    const auto& sigs = world.allyIntel.signals;
    if (sigs.empty()) return false;

    // 1) Se sono GIÀ dentro un segnale, ci resto: stabilità contro l'oscillazione
    //    (senza, un segnale saturo verrebbe abbandonato → si svuota → tutti
    //    tornano → si riempie, un pendolo). Chi c'è, presidia.
    for (const auto& s : sigs)
    {
        const float dx = s.x - fromX, dz = s.z - fromZ;
        if (dx * dx + dz * dz < s.radius * s.radius)
        { outX = s.x; outZ = s.z; outRadius = s.radius; return true; }
    }

    // 2) Altrimenti scelgo, pesato dal bias, SOLO fra i segnali NON saturi
    //    (crowd < capacità): un posto già presidiato non ne attira altri → i
    //    cloni si distribuiscono, e quando è tutto coperto i restanti tornano
    //    alla propria pattuglia invece di ammassarsi (KI #73).
    float total = 0.0f;
    for (const auto& s : sigs)
        if (s.crowd < config::ALLY_SIGNAL_CAPACITY)
            total += (s.weight > 0.01f ? s.weight : 0.01f);
    if (total <= 0.0f) return false;   // tutto già coperto → pattuglia normale

    if (total > 0.0f)
    {
        float roll = bias * total;
        for (const auto& s : sigs)
        {
            if (s.crowd >= config::ALLY_SIGNAL_CAPACITY) continue;
            roll -= (s.weight > 0.01f ? s.weight : 0.01f);
            if (roll <= 0.0f)
            { outX = s.x; outZ = s.z; outRadius = s.radius; return true; }
        }
    }

    // 3) Tutti i segnali SATURI: invece di ripiegare sulla pattuglia locale (idle
    //    avanti-indietro vicino allo spawn mentre gli altri combattono — segnalato
    //    dall'utente), RINFORZA il fronte più vicino. Così TUTTI i cloni avanzano
    //    in modo coerente; la distribuzione fra fronti la garantisce già il passo 2,
    //    questo è la "seconda ondata" che converge sul fronte più prossimo.
    {
        float best2 = 1e18f; const World::AllyIntel::Signal* nearest = nullptr;
        for (const auto& s : sigs)
        {
            const float dx = s.x - fromX, dz = s.z - fromZ;
            const float d2 = dx * dx + dz * dz;
            if (d2 < best2) { best2 = d2; nearest = &s; }
        }
        if (nearest)
        { outX = nearest->x; outZ = nearest->z; outRadius = nearest->radius; return true; }
    }
    return false;
}

// Quale FRONTE segue QUESTO droide (doc 32 v2). Come per la torre di controllo:
// scelta pesata ma decorrelata dal `bias`, così la forza si DISTRIBUISCE sui vari
// fronti invece di convergere tutta sul più prezioso. Il comandante concentra
// (pochi fronti, i più importanti); il bias divide le truppe fra quei fronti.
// ASSEGNAZIONE SPAZIALE (2026-07-26): il peso strategico è modulato dalla
// PROSSIMITÀ del fronte al droide (x,z) → si serve il fronte rilevante per dove
// si è, e — soprattutto — una direttiva di RIPIEGO su un fronte viene raccolta
// dai droidi VICINI a quel fronte, non da droidi scelti dal solo bias (era la
// precondizione mancante del ripiego per-fronte). Prossimità gentile
// (COMMAND_PROXIMITY_HALFDIST): un fronte molto più caldo lontano vince ancora.
const World::EnemyCommand::Directive*
pickEnemyDirective(const World& world, float bias, float x, float z)
{
    const auto& dirs = world.enemyCommand.directives;
    if (dirs.empty()) return nullptr;
    float total = 0.0f;
    for (const auto& d : dirs)
    {
        const float dx = d.x - x, dz = d.z - z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        const float prox = 1.0f / (1.0f + dist / config::COMMAND_PROXIMITY_HALFDIST);
        total += (d.weight > 0.01f ? d.weight : 0.01f) * prox;
    }
    float roll = bias * total;
    for (const auto& d : dirs)
    {
        const float dx = d.x - x, dz = d.z - z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        const float prox = 1.0f / (1.0f + dist / config::COMMAND_PROXIMITY_HALFDIST);
        roll -= (d.weight > 0.01f ? d.weight : 0.01f) * prox;
        if (roll <= 0.0f) return &d;
    }
    return &dirs.back();
}

// Punto di RIPIEGO per un droide (ripiego per-fronte, doc 32): il settore che i
// droidi CONTROLLANO più vicino — la linea su cui cadere e riorganizzarsi —
// altrimenti lo spawn separatista (retro). Così un fronte in avanti che collassa
// non manda i droidi fino a casa: li fa arretrare sulla linea tenuta più vicina.
void retreatPointForTeam(const World& world, int team, float x, float z,
                                float& outX, float& outZ)
{
    float best2 = 1e18f; bool got = false;
    const MapDef* map = world.activeMap;
    if (map)
        for (size_t i = 0; i < map->sectors.size(); ++i)
        {
            const World::SectorState st = (i < world.sectorStates.size())
                                        ? world.sectorStates[i] : World::SectorState{};
            if (st.controllingTeam != team) continue;   // solo terreno NOSTRO
            const float dx = map->sectors[i].x - x, dz = map->sectors[i].z - z;
            const float d2 = dx * dx + dz * dz;
            if (d2 < best2) { best2 = d2; outX = map->sectors[i].x; outZ = map->sectors[i].z; got = true; }
        }
    if (got) return;
    if (map)   // niente linea → retro (spawn della propria fazione)
    { const auto& sp = (team == 1) ? map->spawnTeam1 : map->spawnTeam2;
      outX = sp[0]; outZ = sp[2]; }
    else { outX = x; outZ = z; }
}

// ── QUADRO TATTICO della torre (Fase torre-hub) ──────────────────────────────
// La TORRE fa il lavoro pesante UNA volta per tutti: per ogni posizione tattica utile,
// se BATTE un nemico ORA (LOS verificata) e con che valore. I cloni (ordini + autonomi)
// poi LEGGONO questi dati (world.allyTac) invece di rifare la LOS ciascuno → meno calcolo
// per-clone, scelte coerenti. Attivo solo con una torre di controllo team-1 VIVA; altrimenti
// `active=false` e i cloni ricadono sul punteggio locale ([[structures-degrade-not-block]]).
void updateAllyTactical(World& world)
{
    auto& at = world.allyTac;
    const MapDef* map = world.activeMap;
    const size_t n = map ? map->tacticalPositions.size() : 0;
    at.score.assign(n, 0.0f);
    at.canFire.assign(n, 0);
    at.active = false;
    for (const auto& s : world.strategicTargets)
        if (s.isControl && s.team == 1 && s.entity != 0 && world.isValidEntity(s.entity))
        { at.active = true; break; }
    if (!map || n == 0) return;

    // Nemici (team 2) vivi, una volta sola.
    std::vector<glm::vec3> foes;
    for (EntityId e : world.getEntities())
    {
        const auto* tm = world.getTeam(e);
        if (!tm || tm->teamId != 2 || world.getBullet(e)) continue;
        const auto* h = world.getHealth(e); if (!h || h->current <= 0.0f) continue;
        const auto* tr = world.getTransform(e); if (!tr) continue;
        foes.push_back({tr->x, tr->y, tr->z});
    }
    for (size_t i = 0; i < n; ++i)
    {
        const auto& p = map->tacticalPositions[i];
        if (!worldintel::isTacticalHoldRole(p.role)) continue;
        const float fr = (p.fireRange > 1.0f) ? p.fireRange : config::POSITION_DEFAULT_FIRE_RANGE;
        bool fires = false;
        for (const auto& f : foes)   // batte un nemico entro gittata con LOS pulita?
        {
            const float dx = f.x - p.x, dz = f.z - p.z;
            if (dx * dx + dz * dz > fr * fr) continue;
            if (worldintel::hasLineOfFire(*map, p.x, p.y + config::COMBAT_EYE_HEIGHT, p.z,
                                          f.x, f.y + 1.0f, f.z))
            { fires = true; break; }
        }
        at.canFire[i] = fires ? 1 : 0;
        // PRIORITÀ: importanza autorata + protezione − pericolo, + forte bonus se BATTE
        // un nemico ora → i cloni scelgono posizioni da cui SPARANO davvero (fix "sparano
        // meno": prima Hold sceglieva per sola importanza, anche posizioni cieche).
        at.score[i] = p.importance * aiutility::kPicture.importance
                    + p.protection * aiutility::kPicture.protection
                    - aiutility::kPicture.danger * worldintel::dangerAt(*map, p.x, p.z)
                    + (fires ? config::TAC_FIRE_BONUS : 0.0f);
    }
}

// Miglior posizione LIBERA per un ordine, TOWER-AWARE: usa i punteggi pre-calcolati
// dalla torre (world.allyTac.score, che premia chi BATTE un nemico) se disponibili,
// altrimenti un punteggio locale; salta le posizioni già rivendicate (occupancy CENTRALE
// world.allyTac.claimed) e rispetta la direzione (dirToThreat +1 avanti / -1 indietro /
// 0 qualunque, rispetto a threat). La reachability la verifica il chiamante. outIdx=indice.
const TacticalPositionDef* bestOrderPosition(
    World& world, float px, float pz,
    float areaX, float areaZ, float areaRadius,
    int dirToThreat, float threatX, float threatZ, int* outIdx)
{
    const MapDef* map = world.activeMap;
    if (outIdx) *outIdx = -1;
    if (!map) return nullptr;
    const auto& at = world.allyTac;
    const bool haveScores = at.active && at.score.size() == map->tacticalPositions.size();
    const float ar2 = areaRadius * areaRadius;
    const float fromToThreat = (dirToThreat != 0)
        ? std::sqrt((threatX - px) * (threatX - px) + (threatZ - pz) * (threatZ - pz)) : 0.0f;
    float bestScore = -1e30f; const TacticalPositionDef* best = nullptr; int bestIdx = -1;
    for (int i = 0; i < (int)map->tacticalPositions.size(); ++i)
    {
        const auto& p = map->tacticalPositions[i];
        if (!worldintel::isTacticalHoldRole(p.role)) continue;
        if (i < (int)at.claimed.size() && at.claimed[i]) continue;   // occupata (occupancy centrale)
        const float ax = p.x - areaX, az = p.z - areaZ;
        if (areaRadius > 0.0f && ax * ax + az * az > ar2) continue;
        if (dirToThreat != 0)
        {
            const float pTt = std::sqrt((threatX - p.x) * (threatX - p.x)
                                      + (threatZ - p.z) * (threatZ - p.z));
            if (dirToThreat > 0 && pTt > fromToThreat - 1.0f) continue;   // non abbastanza avanti
            if (dirToThreat < 0 && pTt < fromToThreat + 1.0f) continue;   // non abbastanza indietro
        }
        const float dx = p.x - px, dz = p.z - pz;
        float score = haveScores ? at.score[i]
                    : (p.importance * aiutility::kPicture.importance
                     + p.protection * aiutility::kPicture.protection
                     - aiutility::kPicture.danger * worldintel::dangerAt(*map, p.x, p.z));
        score -= std::sqrt(dx * dx + dz * dz) * aiutility::kPicture.distance;  // a parità, la più vicina
        if (score > bestScore) { bestScore = score; best = &p; bestIdx = i; }
    }
    if (outIdx) *outIdx = bestIdx;
    return best;
}

// Selettore UNIFICATO del waypoint per gli ORDINI di postura del player
// ([[orders-design-vision]]): il membro salta di posizione LIBERA in posizione libera
// (occupancy: `claimed`) scegliendo la PRIORITÀ più alta fra i ruoli utili, nella
// DIREZIONE della postura. Commitment via ai.allySig* (non ri-sceglie ogni tick).
// Imposta ai.allySig* (waypoint) e, per Hold, ai.holdX/Z/holdRadius (clamp di presidio).
// NON è un override: dà solo il waypoint; mira/fuoco/reposition restano autonomi.
//   Hold    → area designata, qualunque direzione, poi DIFENDE (clamp).
//   Advance → verso il nemico (posizioni più AVANTI), a sbalzi.
//   Retreat → lontano dal nemico (posizioni più INDIETRO), fronteggiandolo.
//   Follow  → attorno al player, verso il nemico (lo copre).
//   Regroup → settore CONTESO a peso massimo (raduno; non una posizione).
void selectOrderWaypoint(World& world, AiComponent& ai, float dt,
                                const SquadComponent& sq, float px, float py, float pz)
{
    const MapDef& map = *world.activeMap;
    auto& claimed = world.allyTac.claimed;   // occupancy CENTRALE (per-tick)
    ai.holdRadius = 0.0f;

    // REGROUP: raduno sul settore CONTESO a peso massimo (dinamico), non una posizione.
    if (sq.order == OrderType::Regroup)
    {
        float bestW = -1e30f, bx = px, bz = pz; bool got = false;
        for (size_t s = 0; s < map.sectors.size(); ++s)
        {
            const World::SectorState st = (s < world.sectorStates.size())
                                        ? world.sectorStates[s] : World::SectorState{};
            if (st.allies == 0 && st.enemies == 0) continue;   // solo settori ATTIVI/contesi
            const float w = sectorTacticalWeight(map.sectors[s], st, 1);
            if (w > bestW) { bestW = w; bx = map.sectors[s].x; bz = map.sectors[s].z; got = true; }
        }
        ai.allySigX = bx; ai.allySigZ = bz; ai.allySigValid = true;
        ai.allySigTimer = config::REGROUP_COMMIT_TIME; ai.allySigIdx = -1;
        return;
    }

    // Commitment: posizione già impegnata, valida e non raggiunta → resta MIA (la
    // ri-rivendico così i compagni non la prendono) e non ri-scelgo.
    ai.allySigTimer -= dt;
    const float rdx = px - ai.allySigX, rdz = pz - ai.allySigZ;
    const bool reached = ai.allySigValid && (rdx * rdx + rdz * rdz < 3.0f * 3.0f);
    if (ai.allySigValid && ai.allySigIdx >= 0 && !reached && ai.allySigTimer > 0.0f)
    {
        if (ai.allySigIdx < (int)claimed.size()) claimed[ai.allySigIdx] = 1;   // resta mia
        if (sq.order == OrderType::HoldPosition)
        { ai.holdX = ai.allySigX; ai.holdZ = ai.allySigZ; ai.holdRadius = config::HOLD_ANCHOR_RADIUS; }
        return;
    }

    // Area + direzione (minaccia) per postura.
    float areaX = px, areaZ = pz, areaR = config::HOLD_AREA_RADIUS;
    int dir = 0; float thX = px, thZ = pz; float ex, ey, ez;
    if (sq.order == OrderType::HoldPosition)
    { areaX = sq.targetX; areaZ = sq.targetZ; areaR = config::HOLD_AREA_RADIUS; }
    else if (sq.order == OrderType::Advance)
    {
        const bool foe = nearestEnemyNear(world, 1, sq.targetX, sq.targetZ,
                                          config::ORDER_ENEMY_SCAN, ex, ey, ez);
        thX = foe ? ex : sq.targetX; thZ = foe ? ez : sq.targetZ;
        areaX = px; areaZ = pz; areaR = config::ORDER_BOUND_STEP; dir = +1;
    }
    else if (sq.order == OrderType::Retreat)
    {
        const bool foe = nearestEnemyNear(world, 1, px, pz, config::ORDER_ENEMY_SCAN, ex, ey, ez);
        thX = foe ? ex : px; thZ = foe ? ez : pz;
        areaX = px; areaZ = pz; areaR = config::ORDER_BOUND_STEP; dir = foe ? -1 : 0;
    }
    else   // Follow
    {
        const bool foe = nearestEnemyNear(world, 1, sq.targetX, sq.targetZ,
                                          config::ORDER_ENEMY_SCAN, ex, ey, ez);
        thX = foe ? ex : sq.targetX; thZ = foe ? ez : sq.targetZ;
        areaX = sq.targetX; areaZ = sq.targetZ; areaR = config::FOLLOW_COVER_RADIUS;
        dir = foe ? +1 : 0;
    }

    // Miglior posizione LIBERA (tower-aware) e RAGGIUNGIBILE nella direzione; le
    // irraggiungibili le marca "occupate" per saltarle al giro dopo (isole isolate:
    // irraggiungibili per tutti, e claimed si azzera ogni tick); se nella direzione
    // non ce n'è, rilassa la direzione.
    auto pick = [&](int d) -> const TacticalPositionDef* {
        for (int t = 0; t < 4; ++t)
        {
            int idx = -1;
            const TacticalPositionDef* p =
                bestOrderPosition(world, px, pz, areaX, areaZ, areaR, d, thX, thZ, &idx);
            if (!p) return nullptr;
            const bool reach = (!world.nav || !world.nav->crowdReady()
                                || world.nav->isReachable({px, py, pz}, {p->x, p->y, p->z}));
            if (reach) { ai.allySigIdx = idx; return p; }
            if (idx >= 0 && idx < (int)claimed.size()) claimed[idx] = 1;   // salta l'irraggiungibile
        }
        return nullptr;
    };
    const TacticalPositionDef* pos = pick(dir);
    if (!pos && dir != 0) pos = pick(0);

    if (pos)
    {
        ai.allySigX = pos->x; ai.allySigZ = pos->z;
        ai.allySigValid = true; ai.allySigTimer = config::ORDER_COMMIT_TIME;
        if (ai.allySigIdx >= 0 && ai.allySigIdx < (int)claimed.size()) claimed[ai.allySigIdx] = 1;
        if (sq.order == OrderType::HoldPosition)
        { ai.holdX = pos->x; ai.holdZ = pos->z; ai.holdRadius = config::HOLD_ANCHOR_RADIUS; }
    }
    else   // niente posizione LIBERA: destinazione sensata per postura (mai "resta fermo")
    {
        if (sq.order == OrderType::Retreat)
            retreatPointForTeam(world, 1, px, pz, ai.allySigX, ai.allySigZ);   // retro
        else if (sq.order == OrderType::Advance)
        { ai.allySigX = thX; ai.allySigZ = thZ; }                 // verso il nemico/area
        else if (sq.order == OrderType::Follow)
        { ai.allySigX = sq.targetX; ai.allySigZ = sq.targetZ; }   // verso il leader
        else { ai.allySigX = areaX; ai.allySigZ = areaZ; }        // Hold → centro area
        ai.allySigValid = true; ai.allySigTimer = config::ORDER_COMMIT_FALLBACK; ai.allySigIdx = -1;
    }
}

// La struttura nemica viva più vicina (doc 35). Destinazione di RIPIEGO quando
// non restano unità nemiche: l'ultimo bersaglio da finire prima di ripattugliare.
bool nearestEnemyStructure(const World& world, int myTeam,
                                  float x, float z, float& outX, float& outZ)
{
    float best2 = 1e18f; bool got = false;
    for (const auto& s : world.strategicTargets)
    {
        if (s.entity == 0 || s.team == myTeam) continue;
        if (!world.isValidEntity(s.entity)) continue;
        const float dx = s.x - x, dz = s.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best2) { best2 = d2; outX = s.x; outZ = s.z; got = true; }
    }
    return got;
}

} // namespace aicmd

using namespace aicmd;   // il metodo qui sotto usa i nomi del layer, non qualificati

// ── Decisione del DROIDE TATTICO — metodo di AiSystem (audit #7) ─────────────
// Spostato VERBATIM da `AiSystem::update()`, che era 1740 righe: la lettura della
// situazione (settori, torre, quadro tattico) e la ricostruzione delle DIRETTIVE
// appartengono al livello di comando, non al ciclo per-unità. Usa lo stato del
// sistema (cadenza della decisione, timer del quadro tattico) e restituisce la
// deriva del comandante dalla sua casa, che la telemetria di `update()` pubblica.
float AiSystem::updateEnemyCommand(World& world, const std::vector<EntityId>& snap, float dt)
{
    float cmdDrift = -1.0f;   // deriva del comandante dalla sua casa (leash, ADR-041)
    // ── Comando nemico (Droide Tattico, ADR-024 / doc 32) ────────────────────
    // Controparte del comando del giocatore: un comandante di team 2 VIVO fa
    // convergere i droidi sul command post non-separatista più vicino a lui.
    // Precalcolo una volta per tick. Nessun comandante vivo → direttiva spenta e
    // i droidi tornano alla pattuglia; la transizione vivo→morto emette la
    // conseguenza leggibile (feed), come la torre comunicazioni.
    {
        float cmdrX = 0.0f, cmdrZ = 0.0f;
        bool  aliveCmdr = false;
        for (EntityId e : snap)
        {
            if (!world.hasCommander(e)) continue;
            const auto* tm = world.getTeam(e);
            const auto* t  = world.getTransform(e);
            if (!tm || tm->teamId != 2 || !t) continue;
            const auto* h = world.getHealth(e);
            if (h && h->current <= 0.0f) continue;
            cmdrX = t->x; cmdrZ = t->z; aliveCmdr = true;
            if (world.activeMap)
            {
                const float dx = cmdrX - world.activeMap->commander.x;
                const float dz = cmdrZ - world.activeMap->commander.z;
                cmdDrift = std::sqrt(dx * dx + dz * dz);   // leash: deve restare ≤ raggio
            }
            break;   // v0: dirige il primo comandante vivo (uno stratega per lato)
        }

        // Stato dei settori (ADR-034): serve al comandante per LEGGERE la
        // situazione. Il MONDO si osserva sempre; è la DECISIONE ad avere una
        // cadenza (sotto) — sensori continui, ordini periodici.
        updateSectorStates(world, snap);

        // Torre di controllo dei cloni (doc 36): segnala, non comanda. Sta qui
        // perché legge gli stessi settori, ma è un canale SEPARATO da
        // `enemyCommand` — fonderli farebbe dei cloni la copia dei droidi.
        updateAllyIntel(world);

        // Quadro tattico della torre (Fase torre-hub): la torre pre-calcola LOS posizioni↔
        // nemici a intervalli (pesante, non serve fresca al frame). I cloni lo leggono in
        // `selectOrderWaypoint`/ramo torre invece di rifare la LOS ciascuno.
        m_allyTacTimer -= dt;
        if (m_allyTacTimer <= 0.0f)
        { m_allyTacTimer = config::TAC_PICTURE_PERIOD; updateAllyTactical(world); }

        // ── Cadenza della decisione (doc 34, ADR-038) ─────────────────────
        // Prima la direttiva si ricalcolava OGNI TICK: un comando istantaneo, e
        // quindi impossibile da rallentare. Ora ha un periodo, che la rete di
        // comunicazione allunga quando la torre della fazione è caduta: i droidi
        // continuano a eseguire un intento ormai vecchio. La morte del comandante
        // resta invece rilevata immediatamente — è un fatto, non un ordine.
        const float period = config::COMMAND_DECISION_PERIOD
                           * world.comms[2].orderPeriodMult;
        const bool refresh = !aliveCmdr                            // direttiva da spegnere
                          || !world.enemyCommand.commanderAlive    // comandante appena entrato
                          || (m_time - m_lastCommandDecision[2]) >= period;
        if (refresh)
        {
        if (aliveCmdr) m_lastCommandDecision[2] = m_time;
        using EC = World::EnemyCommand;

        // ── Bilancio GLOBALE (solo per il ripiegamento) ───────────────────
        // Non è più la stance (che ora è per-settore): serve solo a decidere se
        // l'intero esercito deve ritirarsi perché in netta inferiorità.
        int nDroids = 0, nFoes = 0;
        for (EntityId e : snap)
        {
            const auto* tm = world.getTeam(e);
            const auto* h  = world.getHealth(e);
            if (!tm || !h || h->current <= 0.0f) continue;
            if (world.getBullet(e) || world.getVehicle(e) || world.hasCommander(e)) continue;
            bool isStruct = false;
            for (const auto& s : world.strategicTargets)
                if (s.entity == e) { isStruct = true; break; }
            if (isStruct) continue;
            if (tm->teamId == 2) ++nDroids;
            else if (tm->teamId == 1) ++nFoes;
        }
        const bool globalRetreat = aliveCmdr && nDroids <= (int)(nFoes * 0.5f) && nFoes > 0;

        // ── Costruzione delle DIRETTIVE (fronti gestiti insieme, doc 32 v2) ─
        std::vector<EC::Directive> dirs;
        const MapDef* map = world.activeMap;
        if (aliveCmdr && globalRetreat && map)
        {
            // Ripiegamento generale: un solo ordine, verso lo spawn separatista.
            EC::Directive d;
            d.x = map->spawnTeam2[0]; d.z = map->spawnTeam2[2];
            d.radius = 8.0f; d.stance = EC::Retreat; d.weight = 1.0f;
            d.label = "Ripiegamento";
            dirs.push_back(d);
        }
        else if (aliveCmdr && map)
        {
            // Un fronte per SETTORE che conta, con la stance dal bilancio LOCALE:
            // dove i droidi dominano ma sono pressati → TIENI; altrove (conteso o
            // in mano nemica) → SPINGI. È il salto da "una stance globale" a
            // "gestione di più fronti insieme".
            for (size_t s = 0; s < map->sectors.size(); ++s)
            {
                const auto& sec = map->sectors[s];
                const World::SectorState st = (s < world.sectorStates.size())
                                            ? world.sectorStates[s] : World::SectorState{};
                // Il settore ospita un command post che i droidi POSSIEDONO? Un
                // obiettivo catturato è terreno da TENERE, non solo da attraversare.
                // È la condizione STABILE che al Hold mancava: prima chiedeva una
                // maggioranza di UNITÀ droidi nel settore (evento raro in 6v6), così
                // il presidio non scattava mai (misura F5, doc 39). Il possesso del
                // post no — dura finché non te lo riprendono. (ADR-046: attiva
                // finalmente bestHoldPosition sui ruoli defensive/chokepoint.)
                bool ownsPost = false;
                for (const auto& pstt : world.commandPostStates)
                {
                    if (pstt.owner != 2) continue;
                    for (const auto& cp : map->commandPosts)
                        if (cp.label == pstt.label)
                        {
                            const float dx = cp.x - sec.x, dz = cp.z - sec.z;
                            if (dx*dx + dz*dz <= sec.radius * sec.radius) ownsPost = true;
                            break;
                        }
                    if (ownsPost) break;
                }
                const bool weHold     = (st.controllingTeam == 2) || ownsPost; // teniamo il terreno
                const bool threatened = (st.allies > 0);                       // cloni che contendono
                // Nostro e tranquillo → nessun fronte. Altrimenti: TIENI ciò che è
                // nostro ed è minacciato; SPINGI sul resto (conteso o nemico).
                if (weHold && !threatened) continue;
                // Fronte in avanti che COLLASSA: droidi presenti ma NETTAMENTE sotto,
                // su terreno NEMICO (non nostro da tenere) → RIPIEGO per-fronte invece
                // di alimentare una posizione persa. I droidi VICINI lo raccolgono
                // (assegnazione spaziale in pickEnemyDirective) e cadono sulla linea; il
                // rinforzo va ai fronti adiacenti TENIBILI, non qui. La soglia +2 e il
                // possesso nemico distinguono "collassa" (→ ripiega) da "poco sotto ma
                // rinforzabile" (→ Advance, col termine minoranza che alza il peso).
                const bool collapsing = !weHold
                                     && st.enemies >= 1                       // droidi lì da ritirare
                                     && st.allies  >= st.enemies + 2          // nettamente in inferiorità
                                     && st.controllingTeam == 1;              // in mano nemica
                EC::Directive d;
                d.x = sec.x; d.z = sec.z; d.radius = sec.radius; d.label = sec.label;
                const bool holdIt = weHold && threatened;
                d.stance = collapsing ? EC::Retreat : (holdIt ? EC::Hold : EC::Advance);
                if (collapsing)
                {
                    // Peso MODESTO: basta a farlo raccogliere dai droidi vicini, senza
                    // rubare uno slot top-3 a un fronte produttivo né "massare sul ripiego"
                    // (i termini minoranza/opportunità qui NON valgono: si lascia, non si rinforza).
                    d.weight = sec.importance + st.pressure;
                }
                else
                {
                    // Analisi CONDIVISA con la torre (sectorTacticalWeight, dal punto di
                    // vista dei droidi) + il bonus di stance specifico del comandante:
                    // difendere un obiettivo conteso conta. Prima questi termini erano
                    // duplicati inline e derivavano dalla torre (riallineati a mano, 85).
                    d.weight = sectorTacticalWeight(sec, st, 2)
                             + (holdIt ? aiutility::kSector.holdBonus : 0.0f);
                }
                dirs.push_back(d);
            }
            // Le strutture nemiche sono fronti d'attacco (priorità autorata).
            for (const auto& stx : world.strategicTargets)
            {
                if (stx.entity == 0 || stx.team == 2) continue;
                if (!world.isValidEntity(stx.entity)) continue;
                EC::Directive d;
                d.x = stx.x; d.z = stx.z; d.radius = 8.0f; d.stance = EC::Advance;
                d.weight = stx.priority * (stx.isComms ? 1.6f : 1.0f);
                d.label = stx.label;
                dirs.push_back(d);
            }
            // Tieni i K fronti più preziosi: il comandante concentra, non disperde su
            // tutto. Ma la COPERTURA DELLE CORSIE è strutturale, non emergente: invece
            // dei soli 3 pesi più alti — che possono cadere tutti nella stessa corsia
            // lasciandone una scoperta — si prendono prima fronti in CORSIE DIVERSE
            // (distanza LATERALE ≥ COMMAND_LANE_SEP), poi si riempiono gli slot rimasti
            // coi pesi più alti. La torre segnala già tutti i settori (copertura
            // implicita lato cloni); qui il troncamento top-3 la richiede esplicita.
            std::sort(dirs.begin(), dirs.end(),
                      [](const EC::Directive& a, const EC::Directive& b)
                      { return a.weight > b.weight; });
            if (dirs.size() > 3)
            {
                std::vector<float> lat(dirs.size());
                for (size_t i = 0; i < dirs.size(); ++i)
                    lat[i] = worldintel::lateralCoord(*map, dirs[i].x, dirs[i].z);
                std::vector<size_t> chosen;
                auto newLane = [&](size_t i) {
                    for (size_t c : chosen)
                        if (std::fabs(lat[i] - lat[c]) < config::COMMAND_LANE_SEP) return false;
                    return true;
                };
                for (size_t i = 0; i < dirs.size() && chosen.size() < 3; ++i)   // 1) corsie diverse
                    if (newLane(i)) chosen.push_back(i);
                for (size_t i = 0; i < dirs.size() && chosen.size() < 3; ++i)   // 2) riempi per peso
                    if (std::find(chosen.begin(), chosen.end(), i) == chosen.end())
                        chosen.push_back(i);
                std::vector<EC::Directive> pick; pick.reserve(chosen.size());
                for (size_t i : chosen) pick.push_back(dirs[i]);
                dirs.swap(pick);
            }

            // Fallback senza settori: il post non-separatista più vicino, come v1.
            if (dirs.empty())
            {
                float best2 = 1e18f; EC::Directive d; bool got = false;
                for (const auto& stt : world.commandPostStates)
                {
                    if (stt.owner == 2) continue;
                    for (const auto& cp : map->commandPosts)
                    {
                        if (cp.label != stt.label) continue;
                        const float dx = cp.x - cmdrX, dz = cp.z - cmdrZ;
                        const float d2 = dx * dx + dz * dz;
                        if (d2 < best2)
                        { best2 = d2; d.x = cp.x; d.z = cp.z; d.radius = 4.0f;
                          d.stance = EC::Advance; d.weight = 1.0f; d.label = cp.label; got = true; }
                        break;
                    }
                }
                if (got) dirs.push_back(d);
            }
        }

        // ── Feed: annuncia i CAMBIAMENTI, non ogni rivalutazione ──────────
        auto topLabel = [](const std::vector<EC::Directive>& v)
        { return v.empty() ? std::string() : v.front().label; };
        if (world.enemyCommand.commanderAlive && !aliveCmdr)
            world.pushEvent("Comandante tattico nemico eliminato: i droidi perdono coordinamento");
        else if (aliveCmdr && !dirs.empty()
                 && (dirs.size() != world.enemyCommand.directives.size()
                     || topLabel(dirs) != topLabel(world.enemyCommand.directives)))
        {
            const int st0 = dirs.front().stance;
            const char* nm = (st0 == EC::Advance) ? "AVANZATA"
                           : (st0 == EC::Retreat) ? "RIPIEGAMENTO" : "TENERE";
            world.pushEvent("Droide Tattico: " + std::to_string((int)dirs.size())
                            + " fronti — priorità " + nm + " su " + dirs.front().label);
        }

        world.enemyCommand.commanderAlive = aliveCmdr;
        world.enemyCommand.active         = aliveCmdr && !dirs.empty();
        world.enemyCommand.directives     = std::move(dirs);
        }   // fine rivalutazione periodica delle direttive
    }
    return cmdDrift;
}
} // namespace mini

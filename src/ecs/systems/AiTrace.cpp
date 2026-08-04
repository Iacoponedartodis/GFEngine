// AiTrace.cpp — la SCATOLA NERA per-agente dell'AI (KI #86, changelog 121).
//
// PERCHÉ ESISTE. Due diagnosi consecutive su "le AI si fermano e non sparano" sono
// state fuorviate da metriche AGGREGATE: gli eventi di combattimento fra run che
// divergono, e una classificazione dei bloccanti con soglia fissa. Entrambe davano
// numeri; nessuna diceva **cosa stesse facendo quella specifica unità**. La richiesta
// dell'utente — *"sarebbe comodo se tu fossi in grado di vedere precisamente il
// comportamento delle singole AI"* — è la correzione giusta al metodo.
//
// DUE STRUMENTI, UNA REGOLA. La regola: non si aggrega prima di aver guardato.
//   1. RILEVATORE DI STALLO (sempre attivo, costo ~zero): un'AI in contesto di
//      combattimento che per STALL_SECONDS non si sposta e non spara **non è vita
//      normale** — è il sintomo riportato. Alla soglia si registra UN evento con
//      tutto il contesto decisionale e una CAUSA SOSPETTA attribuita dai flag.
//      Aggregato per causa: si legge "12 stalli, 9 dei quali manovra irraggiungibile"
//      invece di "le AI a volte si fermano".
//   2. TRACCIA PER-AGENTE (`--trace-ai <id>`, spenta di default): ogni decisione di
//      quell'unità, tick per tick. È il microscopio da puntare DOPO che il rilevatore
//      ha detto quale unità guardare.
//
// COSA NON FA: non decide nulla. Nessun ramo di comportamento legge questi dati —
// se domani lo facesse, smetterebbe di essere un osservatore e diventerebbe un
// sistema, con tutto ciò che comporta (CLAUDE.md §5).

#include "AiInternal.hpp"

#include "mini/core/Telemetry.hpp"
#include "mini/core/GameConfig.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

namespace mini
{
namespace aitrace
{
namespace
{

int g_traceEntity = 0;   // 0 = spento, -1 = tutte

// Aggregato della finestra. Le cause sono ORDINATE per specificità: la prima che
// spiega lo stallo vince, così ogni episodio finisce in un solo secchio e la somma
// resta leggibile.
enum Cause { C_Manovra, C_Copertura, C_Presidio, C_Ordine, C_Ricerca, C_Ignota, C_Count };
const char* causeName(int c)
{
    switch (c)
    {
        case C_Manovra:   return "manovra_non_raggiunta";  // ha una meta e non ci arriva
        case C_Copertura: return "in_copertura";           // hide/cover: spesso legittimo
        case C_Presidio:  return "presidio";               // àncora di Hold: legittimo
        case C_Ordine:    return "ordine_player";          // sta eseguendo un ordine
        case C_Ricerca:   return "ricerca_esaurita";       // arrivata al punto, non ne cerca un altro
        default:          return "nessuna_causa_evidente"; // <-- il secchio che conta
    }
}

int   g_stalls[C_Count] = {0};
int   g_stallTotal = 0;
float g_stallMaxSec = 0.0f;

} // namespace

void setTraceEntity(int entityId) { g_traceEntity = entityId; }

void observe(World& world, EntityId e, float dt, unsigned long long tick)
{
    auto* ai = world.getAi(e);
    const auto* tr = world.getTransform(e);
    if (!ai || !tr) return;

    // ── Finestra d'osservazione da 1 s ───────────────────────────────────────
    // Si campiona a finestre invece che a ogni tick: uno spostamento per-tick è
    // rumore (strafe, crowd, respinte del collider) e direbbe "si muove" di
    // un'unità che di fatto oscilla sul posto.
    ai->obsTimer += dt;
    if (ai->obsTimer >= 1.0f)
    {
        const float dx = tr->x - ai->obsX, dz = tr->z - ai->obsZ;
        const bool  still = (dx * dx + dz * dz) < (STALL_RADIUS * STALL_RADIUS);
        // "In combattimento" = non in pattuglia. Un'unità in Patrol ferma su un
        // waypoint sta facendo il suo lavoro; una in Alert/Hunt/Search no.
        const bool fighting = (ai->state != AiState::Patrol);
        // UN CADUTO NON È UNO STALLO. Un alleato a terra non può muoversi né
        // sparare **per definizione**: contarlo produceva un episodio lungo quanto
        // bleed-out + rianimazione. È così che è comparso un falso "33,5 s fermo"
        // — il più lungo mai registrato — su un'unità che stava semplicemente
        // morendo. Lo stato "a terra" ha già la sua telemetria (`member downed`,
        // `soccorso differito`): duplicarlo qui come anomalia è solo rumore.
        const auto* sqd = world.getSquad(e);
        const bool  downed = sqd && sqd->downed;
        if (still && !ai->obsFired && fighting && !ai->stationary && !downed)
            ai->stallSec += ai->obsTimer;
        else
        {
            // FINE EPISODIO — e qui sta la DURATA VERA. L'evento alla soglia riporta
            // sempre ~3 s (è il momento in cui scatta): senza questo secondo evento
            // "41 stalli" e "il più lungo 29 s" restano due numeri che non si possono
            // incrociare, e non si sa MAI quale unità sia rimasta ferma a lungo.
            if (ai->stallSeen)
            {
                nlohmann::json f;
                f["bot_id"]   = e;
                f["durata_s"] = ai->stallSec;
                f["stato"]    = aiStateName(ai->state);
                f["uscita"]   = ai->obsFired ? "ha sparato"
                              : !fighting    ? "tornata in pattuglia"
                                             : "si e' rimessa in moto";
                telemetry::event(telemetry::Level::Warn, "AI", "stallo finito", f);
                if (ai->stallSec > g_stallMaxSec) g_stallMaxSec = ai->stallSec;
            }
            ai->stallSec = 0.0f; ai->stallSeen = false;
        }

        ai->obsX = tr->x; ai->obsZ = tr->z;
        ai->obsTimer = 0.0f; ai->obsFired = false;

        if (ai->stallSec > g_stallMaxSec) g_stallMaxSec = ai->stallSec;

        // ── Soglia superata: UN evento per episodio, con tutto il contesto ────
        if (ai->stallSec >= STALL_SECONDS && !ai->stallSeen)
        {
            ai->stallSeen = true;
            const auto* sq = world.getSquad(e);
            const auto* tm = world.getTeam(e);

            // Distanza dalla meta che l'unità dichiara di avere: è il dato che
            // distingue "non ci arriva" da "è arrivata e non fa nulla".
            float destD = -1.0f;
            if (ai->repositionActive)
            {
                const float ddx = ai->repositionX - tr->x, ddz = ai->repositionZ - tr->z;
                destD = std::sqrt(ddx * ddx + ddz * ddz);
            }
            float searchD = -1.0f;
            if (ai->state == AiState::Search)
            {
                const float sdx = ai->searchX - tr->x, sdz = ai->searchZ - tr->z;
                searchD = std::sqrt(sdx * sdx + sdz * sdz);
            }

            int cause = C_Ignota;
            if (ai->repositionActive && destD > 1.5f)                  cause = C_Manovra;
            else if (ai->evading)                                      cause = C_Copertura;
            else if (ai->holdRadius > 0.0f)                            cause = C_Presidio;
            else if (sq && sq->hasActiveOrder())                       cause = C_Ordine;
            else if (ai->state == AiState::Search && searchD >= 0.0f
                     && searchD < 2.0f)                                cause = C_Ricerca;
            ++g_stalls[cause];
            ++g_stallTotal;

            nlohmann::json d;
            d["bot_id"]      = e;
            d["team"]        = tm ? tm->teamId : 0;
            d["causa"]       = causeName(cause);
            d["ferma_da_s"]  = ai->stallSec;
            d["stato"]       = aiStateName(ai->state);
            d["pos"]         = { tr->x, tr->y, tr->z };
            d["guarda_deg"]  = tr->ry;
            d["bersaglio"]   = ai->targetEntity;
            d["evasivo"]     = ai->evading;
            d["hide_resid_s"]= ai->exposeTimer;
            d["ha_cover"]    = ai->hasCover;
            d["manovra"]     = ai->repositionActive;
            d["manovra_dist"]= destD;
            d["ricerca_dist"]= searchD;
            d["presidio_r"]  = ai->holdRadius;
            d["segnale_torre"] = ai->allySigValid;
            d["route"]       = ai->patrolRoute;
            d["ruolo"]       = ai->combatRole;
            d["soppressione"]= ai->suppression;
            d["incastrata_s"]= ai->stuckTimer;
            d["ultimo_noto"] = ai->hasLastKnown;
            if (sq) d["ordine"] = (int)sq->order;
            telemetry::event(telemetry::Level::Warn, "AI", "stallo", d);
        }
    }

    // ── Traccia per-agente su richiesta ──────────────────────────────────────
    // Campionata all'intervallo di sensing: più fitta non aggiunge informazione
    // (fra due sensing la decisione d'ingaggio non cambia) e riempie il file.
    if (g_traceEntity == 0) return;
    if (g_traceEntity > 0 && (EntityId)g_traceEntity != e) return;
    if (tick % config::AI_SENSE_INTERVAL != 0) return;

    nlohmann::json t;
    t["tick"]     = tick;
    t["bot_id"]   = e;
    t["stato"]    = aiStateName(ai->state);
    t["pos"]      = { tr->x, tr->y, tr->z };
    t["guarda_deg"] = tr->ry;
    t["bersaglio"]= ai->targetEntity;
    t["evasivo"]  = ai->evading;
    t["manovra"]  = ai->repositionActive;
    t["ruolo"]    = ai->combatRole;
    t["sopp"]     = ai->suppression;
    t["ferma_da_s"] = ai->stallSec;
    telemetry::event(telemetry::Level::Debug, "AI", "traccia", t);
}

void flush()
{
    nlohmann::json d;
    d["stalli"]      = g_stallTotal;
    d["ferma_max_s"] = g_stallMaxSec;
    for (int c = 0; c < C_Count; ++c) d[causeName(c)] = g_stalls[c];
    telemetry::event(telemetry::Level::Info, "AI", "stalli per causa", d);

    for (int c = 0; c < C_Count; ++c) g_stalls[c] = 0;
    g_stallTotal = 0;
    g_stallMaxSec = 0.0f;
}

} // namespace aitrace
} // namespace mini

// Forwarder pubblico: `main`/`Application` non possono includere AiInternal.hpp
// (header privato di src/ecs/systems/), quindi il flag CLI passa da qui.
#include "mini/ecs/systems/AiSystem.hpp"
namespace mini { void AiSystem::setTraceEntity(int id) { aitrace::setTraceEntity(id); } }

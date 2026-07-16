// SquadSystem.cpp — squadra + ordini (26_SquadAndCommand, ADR-020) — Phase A+B.
#include "mini/ecs/systems/SquadSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>   // data degli eventi (doc 21)
#include <algorithm>
#include <cmath>

namespace mini
{
namespace
{
constexpr int   kAlliedSquadId  = 1;
constexpr float kMoveToDoneDist = 1.5f;   // MoveTo completato entro questo raggio (m)

// orderName() vive in SquadComponent.hpp: la condividono telemetria e HUD.

// Eseguiti oggi: i tre di movimento (Phase A) + TakeCover/FocusFire (Phase B).
// Revive (Phase C, serve lo stato "a terra") e Regroup (ruota di comando) sono
// dichiarati nel doc 26 ma NON eseguiti: devono fallire ESPLICITAMENTE, mai
// sparire in silenzio.
bool isImplemented(OrderType t)
{
    return t == OrderType::Follow || t == OrderType::HoldPosition
        || t == OrderType::MoveTo || t == OrderType::TakeCover
        || t == OrderType::FocusFire;
}

// Ordini impartiti dal giocatore: vanno annunciati sull'HUD e, una volta finiti,
// lasciano il posto al default (Follow). Il default NON si annuncia (sarebbe rumore).
bool isPlayerOrder(OrderType t) { return t != OrderType::Follow; }

// Eventi DISCRETI (transizioni), mai per-frame (disciplina doc 21).
void emitOrder(const char* msg, EntityId e, const SquadComponent& sq,
               telemetry::Level lvl)
{
    nlohmann::json d;
    d["bot_id"] = e;
    d["squad"]  = sq.squadId;
    d["order"]  = orderName(sq.order);
    d["target"] = { sq.targetX, sq.targetZ };
    if (sq.targetEntity)  d["target_entity"] = sq.targetEntity;
    if (sq.failureReason) d["reason"]        = sq.failureReason;
    telemetry::event(lvl, "Squad", msg, d);
}
} // namespace

void SquadSystem::update(World& world, float dt)
{
    (void)dt;
    formAlliedSquad(world);
    if (m_leader == 0) { world.squadOrder.pending = false; return; }

    const auto* leaderTr = world.getTransform(m_leader);

    // ── Consuma l'ordine del giocatore (mailbox, Phase B) ────────────────
    // L'Application ha già risolto il contesto (mirino/cover) e verificato la
    // raggiungibilità: qui si traduce l'intenzione in task per ogni membro.
    SquadOrderRequest req = world.squadOrder;
    world.squadOrder.pending = false;   // consumata: mai eseguita due volte
    if (req.pending) m_outcomeAnnounced = false;   // ordine nuovo → esito da annunciare
    int assigned = 0;

    for (EntityId e : world.getEntities())
    {
        auto* sq = world.getSquad(e);
        if (!sq || sq->squadId != kAlliedSquadId || sq->isLeader) continue;
        auto* tr = world.getTransform(e);
        if (!tr) continue;

        if (req.pending && isImplemented(req.order))
        {
            sq->order         = req.order;
            sq->targetEntity  = req.targetEntity;
            sq->targetX       = req.targetX;
            sq->targetZ       = req.targetZ;
            sq->state         = OrderState::Active;
            sq->failureReason = nullptr;
            sq->issuedTick    = world.getTickCount();
            emitOrder("order issued", e, *sq, telemetry::Level::Info);
            ++assigned;
        }

        // ── Ordine non ancora eseguibile → fallisce CON CAUSA ────────────
        if (sq->hasActiveOrder() && !isImplemented(sq->order))
        {
            sq->state         = OrderState::Failed;
            sq->failureReason = (sq->order == OrderType::Revive)
                              ? "rianimazione non ancora implementata (Phase C)"
                              : "ordine non ancora implementato";
            emitOrder("order failed", e, *sq, telemetry::Level::Warn);
            world.pushEvent(std::string("Ordine ") + orderName(sq->order)
                            + " fallito: " + sq->failureReason);
            sq->order = OrderType::None;
            continue;
        }

        // ── Default Phase A: chi non ha ordini segue il leader ───────────
        if (!sq->hasActiveOrder())
        {
            sq->order         = OrderType::Follow;
            sq->targetEntity  = m_leader;
            sq->state         = OrderState::Active;
            sq->failureReason = nullptr;
            sq->issuedTick    = world.getTickCount();
            emitOrder("order issued", e, *sq, telemetry::Level::Info);
        }

        // ── Ciclo di vita dell'ordine ────────────────────────────────────
        switch (sq->order)
        {
        case OrderType::Follow:
            if (!leaderTr)
            {
                sq->state         = OrderState::Failed;
                sq->failureReason = "leader perduto";
                emitOrder("order failed", e, *sq, telemetry::Level::Warn);
                sq->order = OrderType::None;
                break;
            }
            // Bersaglio MOBILE: il task si aggiorna, l'AI decide come raggiungerlo.
            sq->targetX = leaderTr->x;
            sq->targetZ = leaderTr->z;
            break;

        case OrderType::MoveTo:
        case OrderType::TakeCover:
        {
            // Stessa condizione di completamento: TakeCover è un MoveTo su un
            // cover point REALE del MapDef (doc 15/18), scelto dall'Application.
            const float dx = sq->targetX - tr->x, dz = sq->targetZ - tr->z;
            if (std::sqrt(dx * dx + dz * dz) < kMoveToDoneDist)
            {
                sq->state = OrderState::Done;
                emitOrder("order completed", e, *sq, telemetry::Level::Info);
                if (isPlayerOrder(sq->order) && !m_outcomeAnnounced)
                {
                    world.pushEvent(std::string("Ordine ") + orderName(sq->order)
                                    + " completato");
                    m_outcomeAnnounced = true;
                }
                sq->order = OrderType::None;   // → torna al default (Follow)
            }
            break;
        }

        case OrderType::FocusFire:
        {
            // Vincola la SCELTA del bersaglio (l'AI applica il vincolo in AiSystem);
            // qui si gestisce solo la fine dell'ordine. Completo quando il bersaglio
            // designato è morto — è esattamente ciò che il giocatore ha chiesto.
            // Il bersaglio ucciso è già stato DISTRUTTO da CombatSystem in questo
            // stesso tick (gira prima di noi): interrogarne la salute darebbe
            // "non esiste", cioè un fallimento al posto di un successo. La
            // mailbox delle eliminazioni è l'unico modo di distinguerli.
            const EntityId ft = sq->targetEntity;
            const bool killed =
                std::find_if(world.killedThisTick.begin(), world.killedThisTick.end(),
                             [ft](const World::KilledUnit& k){ return k.entity == ft; })
                != world.killedThisTick.end();
            const auto* th = world.getHealth(sq->targetEntity);
            if (killed || (th && th->current <= 0.0f))
            {
                sq->state = OrderState::Done;
                emitOrder("order completed", e, *sq, telemetry::Level::Info);
                if (!m_outcomeAnnounced)
                { world.pushEvent("Ordine FocusFire completato: bersaglio eliminato");
                  m_outcomeAnnounced = true; }
                sq->order = OrderType::None;
            }
            else if (!world.isValidEntity(sq->targetEntity) || !th)
            {
                sq->state         = OrderState::Failed;
                sq->failureReason = "bersaglio non più esistente";
                emitOrder("order failed", e, *sq, telemetry::Level::Warn);
                if (!m_outcomeAnnounced)
                { world.pushEvent("Ordine FocusFire fallito: bersaglio perduto");
                  m_outcomeAnnounced = true; }
                sq->order = OrderType::None;
            }
            break;
        }

        case OrderType::HoldPosition:
        default:
            break;   // resta Active finché non viene sostituito
        }
    }

    // Un solo messaggio per ordine, non uno per membro (sarebbe illeggibile).
    if (assigned > 0)
        world.pushEvent(std::string("Squadra (") + std::to_string(assigned)
                        + "): " + orderName(req.order));
}

void SquadSystem::formAlliedSquad(World& world)
{
    // ── 1. Leader: il giocatore se è un'entità valida di team 1 (partita vera);
    //    altrimenti la prima AI alleata (in simulazione il player è team 0 e
    //    parcheggiato fuori campo → non può guidare la squadra). ────────────
    m_leader = 0;
    const EntityId pe = world.playerEntity;
    if (pe != 0 && world.isValidEntity(pe))
    {
        const auto* tm = world.getTeam(pe);
        if (tm && tm->teamId == 1 && world.getTransform(pe)) m_leader = pe;
    }
    if (m_leader == 0)
        for (EntityId e : world.getEntities())
        {
            const auto* tm = world.getTeam(e);
            if (!tm || tm->teamId != 1) continue;
            if (!world.getAi(e) || !world.getTransform(e)) continue;
            m_leader = e;
            break;
        }
    if (m_leader == 0) return;

    // ── 2. Arruola le AI alleate (i respawn creano entità NUOVE ogni volta) e
    //    aggiorna il ruolo: il leader può cambiare fra un tick e l'altro. ────
    for (EntityId e : world.getEntities())
    {
        const auto* tm = world.getTeam(e);
        if (!tm || tm->teamId != 1) continue;
        if (!world.getAi(e) || !world.getTransform(e)) continue;   // solo unità AI

        auto* sq = world.getSquad(e);
        if (!sq)
        {
            SquadComponent nsq;
            nsq.squadId = kAlliedSquadId;
            world.addSquad(e, nsq);
            sq = world.getSquad(e);
        }
        if (sq) sq->isLeader = (e == m_leader);
    }
}

} // namespace mini

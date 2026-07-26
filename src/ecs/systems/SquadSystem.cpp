// SquadSystem.cpp — squadra + ordini (26_SquadAndCommand, ADR-020) — Phase A+B.
#include "mini/ecs/systems/SquadSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/core/GameConfig.hpp"   // Phase C: bleed-out/rianimazione
#include "mini/game/data/GameplayBalance.hpp"   // ADR-043: rianimazione data-driven

#include <nlohmann/json.hpp>   // data degli eventi (doc 21)
#include <algorithm>
#include <cmath>
#include <vector>

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
        || t == OrderType::FocusFire || t == OrderType::Revive        // Phase C
        || t == OrderType::CoveringFire
        || t == OrderType::Advance    // avanzata tattica (ruota, bias sull'AI autonoma)
        || t == OrderType::Retreat    // ripiego alla zona sicura (ruota)
        || t == OrderType::Regroup;   // raduno sul settore conteso a peso max (ruota)
}

// C'è qualcuno che sta DELIBERATAMENTE rianimando questo caduto?
// Bilanciamento 2026-07-20 (feedback utente: "non muore mai nessuno"): prima
// bastava un compagno QUALSIASI entro il raggio — ma il `Follow` tiene la squadra
// ammassata, quindi c'era sempre qualcuno vicino e la rianimazione era di fatto
// GRATIS e automatica. Ora conta solo chi si sta davvero dedicando al soccorso:
//  - il leader (il GIOCATORE): sceglie lui di fermarsi lì accanto;
//  - il compagno DISPACCIATO con l'ordine `Revive` proprio su questo caduto
//    (smette di combattere per soccorrerlo).
// Un compagno che passa di lì sparando non rianima più nessuno: soccorrere costa
// un uomo e del tempo. Se il soccorritore muore o viene distolto, il progresso
// riparte da capo (logica invariata a valle).
bool reviverNearby(World& world, EntityId downed, EntityId leader, float dx, float dz)
{
    const float rr = gameplay().squadReviveRadius;
    const float r2 = rr * rr;
    auto within = [&](EntityId o) -> bool {
        const auto* otr = world.getTransform(o);
        if (!otr) return false;
        const float ddx = otr->x - dx, ddz = otr->z - dz;
        return ddx * ddx + ddz * ddz <= r2;
    };
    // Il leader (in partita vera è il GIOCATORE) non è arruolato come membro
    // AI: contarlo qui è ciò che gli permette di rianimare stando vicino.
    if (leader != 0 && leader != downed && within(leader)) return true;
    for (EntityId o : world.getEntities())
    {
        if (o == downed) continue;
        const auto* osq = world.getSquad(o);
        if (!osq || osq->squadId != kAlliedSquadId || osq->downed) continue;
        // SOLO il soccorritore dedicato a QUESTO caduto.
        if (osq->order != OrderType::Revive || osq->targetEntity != downed) continue;
        if (within(o)) return true;
    }
    return false;
}

// Ordini impartiti dal giocatore: vanno annunciati sull'HUD e, una volta finiti,
// lasciano il posto allo stato privo di ordini (ADR-037), che non si annuncia.
bool isPlayerOrder(OrderType t) { return t != OrderType::None; }

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

    std::vector<EntityId> bledOut;   // a terra senza soccorso in tempo → morte

    for (EntityId e : world.getEntities())
    {
        auto* sq = world.getSquad(e);
        if (!sq || sq->squadId != kAlliedSquadId || sq->isLeader) continue;
        auto* tr = world.getTransform(e);
        if (!tr) continue;

        // ── A TERRA (Phase C): non esegue ordini; o viene rianimato, o muore ──
        if (sq->downed)
        {
            if (reviverNearby(world, e, m_leader, tr->x, tr->z))
            {
                sq->reviveProgress += dt;
                if (sq->reviveProgress >= gameplay().squadReviveTime)
                {
                    sq->downed = false;
                    sq->reviveProgress = 0.0f;
                    ++sq->revivesUsed;   // consuma una rianimazione: la prossima caduta sarà letale
                    if (auto* h = world.getHealth(e))
                        h->current = h->max * gameplay().squadReviveHp;
                    sq->order = OrderType::None;   // riparte senza ordini (ADR-037)
                    sq->state = OrderState::None;
                    telemetry::event(telemetry::Level::Info, "Squad", "member revived",
                                     {{"entity", (int)e},
                                      {"revives_used", sq->revivesUsed},
                                      {"revives_left",
                                       gameplay().squadMaxRevives - sq->revivesUsed}});
                    world.pushEvent("RIANIMATO #" + std::to_string(e));
                }
            }
            else
            {
                sq->reviveProgress = 0.0f;   // interrotto: riparte da capo
                sq->bleedoutRemaining -= dt;
                if (sq->bleedoutRemaining <= 0.0f)
                    bledOut.push_back(e);
            }
            continue;   // un'unità a terra non riceve/esegue ordini
        }

        // Ordine DIRETTO: se req.directedMember è impostato, solo quel membro lo
        // riceve (comandi che puntano un compagno). 0 = tutta la squadra (default).
        const bool addressed = (req.directedMember == 0 || req.directedMember == e);
        // REVOCA (ADR-037): OrderType::None come richiesta è un ordine valido —
        // "liberi", si torna truppa indipendente. Va gestito qui perché il blocco
        // di assegnazione filtra su isImplemented(), che None non soddisfa.
        if (req.pending && req.order == OrderType::None && addressed)
        {
            if (sq->hasActiveOrder())
            {
                sq->order         = OrderType::None;
                sq->state         = OrderState::None;
                sq->failureReason = nullptr;
                emitOrder("order cleared", e, *sq, telemetry::Level::Info);
            }
        }
        else if (req.pending && isImplemented(req.order) && addressed)
        {
            sq->order         = req.order;
            sq->targetEntity  = req.targetEntity;
            // HOLD (ruota comandi): sq->target è il CENTRO dell'AREA da tenere. La
            // distribuzione NON convergente la fa l'AI (bestAdvantageInArea + bias-spread,
            // AiSystem): ogni membro prende una posizione diversa nell'area. Centro = il
            // punto designato dalla ruota se fornito, altrimenti la posizione del membro
            // (tieni l'area dove sei). [[orders-design-vision]]
            if (req.order == OrderType::HoldPosition)
            {
                if (req.targetX != 0.0f || req.targetZ != 0.0f)
                { sq->targetX = req.targetX; sq->targetZ = req.targetZ; }
                else
                { sq->targetX = tr->x; sq->targetZ = tr->z; }
            }
            else
            { sq->targetX = req.targetX; sq->targetZ = req.targetZ; }
            sq->state         = OrderState::Active;
            sq->failureReason = nullptr;
            sq->issuedTick    = world.getTickCount();
            emitOrder("order issued", e, *sq, telemetry::Level::Info);
            ++assigned;
        }

        // ── Ordine non ancora eseguibile → fallisce CON CAUSA ────────────
        // Oggi l'unico non implementato è Regroup (ruota di comando livello 2 —
        // Regroup/Hold/Advance passano già come MoveTo/HoldPosition; Regroup come
        // ordine a sé non è cablato). Revive/CoveringFire ORA sono implementati.
        if (sq->hasActiveOrder() && !isImplemented(sq->order))
        {
            sq->state         = OrderState::Failed;
            sq->failureReason = "ordine non ancora implementato";
            emitOrder("order failed", e, *sq, telemetry::Level::Warn);
            world.pushEvent(std::string("Ordine ") + orderName(sq->order)
                            + " fallito: " + sq->failureReason);
            sq->order = OrderType::None;
            continue;
        }

        // ── Stato privo di ordini (ADR-037) ──────────────────────────────
        // Nessun default: chi non ha un ordine attivo resta OrderType::None e
        // si muove come truppa indipendente (AiSystem → Patrol/Alert normali).
        // Follow è un ordine come gli altri, impartito dalla ruota di comando.

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
                sq->order = OrderType::None;   // → torna senza ordini (ADR-037)
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

        case OrderType::Revive:
        {
            // Phase C: manda il membro VERSO il compagno a terra; la
            // rianimazione vera scatta per PROSSIMITÀ (gestita nel ramo `downed`
            // del bersaglio). Qui si tiene aggiornato il punto e si chiude
            // l'ordine quando il bersaglio non è più a terra (rianimato) o è perso.
            const auto* dt2 = world.getTransform(sq->targetEntity);
            const auto* dsq = world.getSquad(sq->targetEntity);
            if (!dt2 || !world.isValidEntity(sq->targetEntity))
            {
                sq->state = OrderState::Failed;
                sq->failureReason = "bersaglio della rianimazione perduto";
                emitOrder("order failed", e, *sq, telemetry::Level::Warn);
                if (!m_outcomeAnnounced)
                { world.pushEvent("Ordine Revive fallito: compagno perduto");
                  m_outcomeAnnounced = true; }
                sq->order = OrderType::None;
                break;
            }
            if (!dsq || !dsq->downed)   // rianimato (dalla prossimità) → fatto
            {
                sq->state = OrderState::Done;
                emitOrder("order completed", e, *sq, telemetry::Level::Info);
                if (!m_outcomeAnnounced)
                { world.pushEvent("Ordine Revive completato: compagno rianimato");
                  m_outcomeAnnounced = true; }
                sq->order = OrderType::None;
                break;
            }
            sq->targetX = dt2->x;   // insegue il compagno a terra
            sq->targetZ = dt2->z;
            break;
        }

        case OrderType::HoldPosition:
        case OrderType::CoveringFire:   // tieni la posizione e fai fuoco di supporto
        default:
            break;   // resta Active finché non viene sostituito
        }
    }

    // ── Auto-soccorso (Phase C): il membro libero più vicino va a rianimare ──
    // È ciò che rende la squadra una RISORSA che si autoprotegge: un compagno a
    // terra non viene abbandonato. Assegna un solo soccorritore per caduto (gli
    // altri continuano a combattere), e mai sopra un ordine del GIOCATORE — la
    // sua intenzione vince (contratto di autonomia, doc 26).
    for (EntityId d : world.getEntities())
    {
        const auto* dsq = world.getSquad(d);
        if (!dsq || dsq->squadId != kAlliedSquadId || !dsq->downed) continue;
        const auto* dtr = world.getTransform(d);
        if (!dtr) continue;

        // Già servito? (qualcuno lo sta rianimando)
        bool served = false;
        for (EntityId o : world.getEntities())
        {
            const auto* osq = world.getSquad(o);
            if (osq && osq->order == OrderType::Revive && osq->targetEntity == d)
            { served = true; break; }
        }
        if (served) continue;

        // Soccorritore = membro libero (Follow/idle, mai su ordine giocatore) più vicino.
        EntityId best = 0; float bestD2 = 1e30f;
        for (EntityId o : world.getEntities())
        {
            auto* osq = world.getSquad(o);
            if (!osq || osq->squadId != kAlliedSquadId || osq->isLeader || osq->downed) continue;
            if (osq->order != OrderType::Follow && osq->order != OrderType::None) continue;
            const auto* otr = world.getTransform(o);
            if (!otr) continue;
            const float dx = otr->x - dtr->x, dz = otr->z - dtr->z;
            const float d2 = dx * dx + dz * dz;
            if (d2 < bestD2) { bestD2 = d2; best = o; }
        }
        if (best != 0)
        {
            auto* bsq = world.getSquad(best);
            bsq->order         = OrderType::Revive;
            bsq->targetEntity  = d;
            bsq->targetX       = dtr->x;
            bsq->targetZ       = dtr->z;
            bsq->state         = OrderState::Active;
            bsq->failureReason = nullptr;
            bsq->issuedTick    = world.getTickCount();
            emitOrder("order issued", best, *bsq, telemetry::Level::Info);
        }
    }

    // ── Morte per bleed-out: nessun soccorso in tempo ────────────────────
    // SOLO ORA il membro conta come perdita (missionStats), non quando è caduto
    // a terra: una perdita evitata con la rianimazione non deve pesare sul
    // giudizio (doc 25). La distruzione avviene qui, fuori dall'iterazione.
    for (EntityId e : bledOut)
    {
        telemetry::event(telemetry::Level::Info, "Squad", "member bled out",
                         {{"entity", (int)e}});
        world.pushEvent("PERSO #" + std::to_string(e) + " — a terra senza soccorso");
        if (e != world.playerEntity) ++world.missionStats.alliesLost;
        world.killedThisTick.push_back({e, 1});   // team 1: coerente con CombatSystem
        world.destroyEntity(e);
    }

    // Un solo messaggio per ordine di SQUADRA (non uno per membro). Gli ordini
    // DIRETTI a un compagno (Revive/CoveringFire) li annuncia già l'Application
    // col suo toast: dire anche "Squadra (1)" sarebbe fuorviante e ridondante.
    if (assigned > 0 && req.directedMember == 0)
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

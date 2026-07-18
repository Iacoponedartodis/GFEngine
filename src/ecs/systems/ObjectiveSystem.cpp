// ObjectiveSystem.cpp — framework obiettivi generico (25_ObjectivesAndMissions,
// ADR-019) — Phase A.
#include "mini/ecs/systems/ObjectiveSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/Definitions.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/data/ContentValidation.hpp"   // ADR-018: regole condivise

#include <nlohmann/json.hpp>   // data degli eventi (doc 21)
#include <algorithm>
#include <cmath>

namespace mini
{
namespace
{

const char* typeName(ObjectiveType t)
{
    switch (t) {
        case ObjectiveType::ReachArea:           return "ReachArea";
        case ObjectiveType::EliminateTarget:     return "EliminateTarget";
        case ObjectiveType::HoldAreaForDuration: return "HoldAreaForDuration";
        case ObjectiveType::CaptureZone:         return "CaptureZone";
        case ObjectiveType::DefendZone:          return "DefendZone";
        case ObjectiveType::DestroyTarget:       return "DestroyTarget";
        case ObjectiveType::EscortEntity:        return "EscortEntity";
        case ObjectiveType::SurviveWave:         return "SurviveWave";
        case ObjectiveType::InteractHack:        return "InteractHack";
    }
    return "?";
}

const char* tierName(ObjectiveTier t)
{
    switch (t) {
        case ObjectiveTier::Primary:   return "primary";
        case ObjectiveTier::Strategic: return "strategic";
        case ObjectiveTier::Tactical:  return "tactical";
    }
    return "?";
}

// Phase A esegue i tipi valutabili leggendo SOLO il World. Gli altri sono
// dichiarati dal doc 25 ma non ancora eseguiti: falliscono ESPLICITAMENTE —
// stessa disciplina degli ordini di squadra (ADR-020), mai in silenzio.
bool isImplemented(ObjectiveType t)
{
    return t == ObjectiveType::ReachArea
        || t == ObjectiveType::EliminateTarget
        || t == ObjectiveType::HoldAreaForDuration
        || t == ObjectiveType::CaptureZone
        || t == ObjectiveType::DefendZone
        || t == ObjectiveType::DestroyTarget;
}

// Proprietario del post con quella label. -1 = post inesistente (dato invalido:
// il gate ADR-018 lo respinge prima, questo è il paracadute a runtime).
int postOwner(World& world, const std::string& label)
{
    for (const auto& p : world.commandPostStates)
        if (p.label == label) return p.owner;
    return -1;
}

// Un'unità viva del team `team` dentro la zona? (XZ: le zone sono cilindri)
bool teamInZone(World& world, int team, float x, float z, float radius)
{
    const float r2 = radius * radius;
    for (EntityId e : world.getEntities())
    {
        const auto* tm = world.getTeam(e);
        if (!tm || tm->teamId != team || world.getBullet(e)) continue;
        const auto* h = world.getHealth(e);
        if (!h || h->current <= 0.0f) continue;
        const auto* t = world.getTransform(e);
        if (!t) continue;
        const float dx = t->x - x, dz = t->z - z;
        if (dx*dx + dz*dz <= r2) return true;
    }
    return false;
}

const char* consequenceName(ConsequenceType t)
{
    switch (t) {
        case ConsequenceType::BlockEnemyReinforcements: return "block_enemy_reinforcements";
        case ConsequenceType::EnemyAccuracy:            return "enemy_accuracy";
        case ConsequenceType::AllyReinforcements:       return "ally_reinforcements";
        case ConsequenceType::UnlockSpawn:              return "unlock_spawn";
        default:                                        return "none";
    }
}

// Applica le conseguenze scrivendo SOLO su `World::battleState`: da lì le legge
// il sistema competente (i rinforzi nemici → ConquestMode, la precisione → AiSystem).
// Nessun `if (objectiveId == ...)`: il tipo è un dato, e aggiungerne uno nuovo
// significa un case qui + un lettore nel sistema giusto, senza toccare gli altri.
void applyConsequences(World& world, const ObjectiveDef& d,
                       const std::vector<ConsequenceDef>& list, const char* when)
{
    for (const auto& c : list)
    {
        switch (c.type)
        {
        case ConsequenceType::BlockEnemyReinforcements:
            world.battleState.enemyReinforcementsBlocked = true;
            world.pushEvent("Rinforzi nemici interrotti!");
            break;
        case ConsequenceType::EnemyAccuracy:
            // Moltiplicativo: due obiettivi che disorganizzano il nemico si
            // sommano invece di sovrascriversi a vicenda.
            world.battleState.enemyAccuracyMult *= c.value;
            world.pushEvent("Nemici disorganizzati");
            break;
        case ConsequenceType::AllyReinforcements:
            // Delta consumato dal mode (che possiede i ticket): qui non si
            // conosce il numero attuale delle riserve.
            world.battleState.pendingAllyReinforcements += (int)c.value;
            world.pushEvent("Rinforzi alleati in arrivo: +" + std::to_string((int)c.value));
            break;
        case ConsequenceType::UnlockSpawn:
            world.battleState.allySpawnPost = c.target;
            world.pushEvent("Nuovo punto di schieramento: " + c.target);
            break;
        default: continue;   // None: il gate ADR-018 l'ha già respinta
        }
        telemetry::event(telemetry::Level::Info, "Objective", "consequence applied",
                         {{"objective", d.id}, {"when", when},
                          {"consequence", consequenceName(c.type)},
                          {"value", c.value}, {"target", c.target}});
    }
}

// Eventi DISCRETI (transizioni), mai per-frame (disciplina doc 21).
void emitObjective(const char* msg, const ObjectiveDef& d, telemetry::Level lvl,
                   const char* reason = nullptr)
{
    nlohmann::json j;
    j["objective"] = d.id;
    j["type"]      = typeName(d.type);
    j["tier"]      = tierName(d.tier);
    if (reason) j["reason"] = reason;
    telemetry::event(lvl, "Objective", msg, j);
}

} // namespace

void ObjectiveSystem::update(World& world, float dt)
{
    const MissionDef* m = world.activeMission;
    if (!m || !world.objectiveDefs)
    {
        // Nessuna missione: il framework è inerte e i mode restano quelli di prima.
        m_objs.clear(); m_boundMission = nullptr; m_rejected = false;
        m_outcome = Outcome::Ongoing; m_missionTime = 0.0f;
        return;
    }
    // Rebind se la missione cambia OPPURE se il mondo è ripartito (tick azzerato
    // da World::initialize): i sistemi sopravvivono al restart, quindi senza il
    // secondo controllo una missione già completata resterebbe completata per
    // sempre e il riavvio non la ricomincerebbe.
    const std::uint64_t tick = world.getTickCount();
    if (m != m_boundMission || tick < m_lastTick) bind(world);
    m_lastTick = tick;
    if (m_rejected || m_outcome != Outcome::Ongoing) return;

    m_missionTime += dt;
    world.missionStats.missionTime = m_missionTime;   // debrief (doc 25 / GDD 9.6)
    activateReady(world);

    // ── Valutazione ──────────────────────────────────────────────────────
    for (auto& r : m_objs)
    {
        if (r.state != State::Active) continue;
        r.elapsed += dt;

        if (!isImplemented(r.def->type))
        {
            r.state = State::Failed;
            ++world.missionStats.objectivesFailed;
            r.failureReason = "tipo di obiettivo non ancora implementato";
            emitObjective("objective failed", *r.def, telemetry::Level::Warn,
                          r.failureReason);
            world.pushEvent("Obiettivo " + r.def->name + " fallito: tipo non implementato");
            continue;
        }

        if (evaluate(world, r, dt))
        {
            r.state = State::Completed;
            ++world.missionStats.objectivesDone;
            emitObjective("objective completed", *r.def, telemetry::Level::Info);
            applyConsequences(world, *r.def, r.def->onSuccess, "success");
            world.pushEvent("Obiettivo completato: " + r.def->name);
            continue;
        }
        // `evaluate` può concludere da sé con un fallimento proprio del tipo
        // (es. DefendZone: post perduto). In quel caso ha già emesso l'evento:
        // proseguire farebbe scattare anche il timeout e ne emetterebbe un secondo.
        if (r.state != State::Active) continue;

        // Fallimento per tempo — OPZIONALE (timeLimit 0 = non fallisce mai):
        // il fallimento parziale è ciò che produce decisioni tattiche.
        if (r.def->timeLimit > 0.0f && r.elapsed >= r.def->timeLimit)
        {
            r.state = State::Failed;
            ++world.missionStats.objectivesFailed;
            r.failureReason = "tempo scaduto";
            emitObjective("objective failed", *r.def, telemetry::Level::Warn,
                          r.failureReason);
            applyConsequences(world, *r.def, r.def->onFailure, "failure");
            world.pushEvent("Obiettivo fallito: " + r.def->name + " (tempo scaduto)");
        }
    }

    // ── Regole di missione: DICHIARATE nel MissionDef, mai cablate ────────
    int nPrimary = 0, nPrimaryDone = 0, nPrimaryFailed = 0;
    for (const auto& r : m_objs)
    {
        if (r.def->tier != ObjectiveTier::Primary) continue;
        ++nPrimary;
        if (r.state == State::Completed) ++nPrimaryDone;
        if (r.state == State::Failed)    ++nPrimaryFailed;
    }

    bool success = false;
    switch (m->successRule)
    {
    case MissionRule::AllPrimaryComplete:
        success = (nPrimary > 0 && nPrimaryDone == nPrimary); break;
    case MissionRule::AnyPrimaryComplete:
        success = (nPrimaryDone > 0); break;
    default: break;   // le regole di fallimento non decidono il successo
    }

    bool failure = false;
    switch (m->failureRule)
    {
    case MissionRule::AnyPrimaryFailed: failure = (nPrimaryFailed > 0); break;
    case MissionRule::TimeLimit:
        failure = (m->failureTimeLimit > 0.0f && m_missionTime >= m->failureTimeLimit);
        break;
    default: break;
    }

    // Il successo vince sul fallimento a parità di tick: un primario completato
    // nell'istante in cui scade il tempo è una vittoria, non una sconfitta.
    if (success)      { m_outcome = Outcome::Success;
                        telemetry::event(telemetry::Level::Info, "Objective",
                                         "mission success", {{"mission", m->id}});
                        world.pushEvent("MISSIONE COMPLETATA: " + m->name); }
    else if (failure) { m_outcome = Outcome::Failure;
                        telemetry::event(telemetry::Level::Warn, "Objective",
                                         "mission failed", {{"mission", m->id}});
                        world.pushEvent("MISSIONE FALLITA: " + m->name); }
}

// Risolve gli id → ObjectiveDef una volta sola per missione, e applica il GATE:
// una missione che non dichiara quando è vinta E quando è persa non parte
// (doc 25 + doc 24). Un id non risolto è un errore di dati, non un warning da
// ignorare: l'obiettivo non esiste, quindi la missione è incompleta.
void ObjectiveSystem::bind(World& world)
{
    const MissionDef* m = world.activeMission;
    const DefinitionRegistry& reg = *world.objectiveDefs;
    m_objs.clear();
    m_boundMission = m;
    m_missionTime  = 0.0f;
    m_outcome      = Outcome::Ongoing;
    m_rejected     = false;

    // Le regole della missione vivono in UN SOLO posto (ADR-018): le stesse che
    // usano l'editor e `--validate`. Duplicarle qui significherebbe che runtime ed
    // editor possono divergere — il bug che il gate condiviso esiste per togliere.
    const Diagnostics diags = validateMission(*m, reg);
    if (hasErrors(diags))
    {
        m_rejected = true;
        reportDiagnostics(diags, /*printToStdout=*/false);   // → JSONL, azionabile
        for (const auto& x : diags)
            if (x.severity == telemetry::Level::Error)
                world.pushEvent("Missione rifiutata: " + x.message);
        telemetry::event(telemetry::Level::Error, "Objective", "mission rejected",
                         {{"mission", m->id},
                          {"errors", countBy(diags, telemetry::Level::Error)}});
        return;
    }

    for (const auto& id : m->primaryObjectives)
        if (const ObjectiveDef* d = reg.getObjective(id)) { Runtime r; r.def = d; m_objs.push_back(r); }
    for (const auto& id : m->optionalObjectives)
        if (const ObjectiveDef* d = reg.getObjective(id)) { Runtime r; r.def = d; m_objs.push_back(r); }

    if (m_objs.empty())   // validateMission garantisce >=1 primario: difesa in profondità
    {
        m_rejected = true;
        telemetry::event(telemetry::Level::Error, "Objective", "mission rejected",
                         {{"mission", m->id}, {"reason", "nessun obiettivo"}});
        world.pushEvent("Missione rifiutata: nessun obiettivo");
        return;
    }
    telemetry::event(telemetry::Level::Info, "Objective", "mission started",
                     {{"mission", m->id}, {"objectives", (int)m_objs.size()}});
}

bool ObjectiveSystem::isComplete(const std::string& id) const
{
    for (const auto& r : m_objs)
        if (r.def && r.def->id == id) return r.state == State::Completed;
    return false;
}

// Attivazione dichiarativa: subito / dopo un altro obiettivo / a tempo.
// È ciò che permette le dipendenze fra obiettivi senza scripting.
void ObjectiveSystem::activateReady(World& world)
{
    for (auto& r : m_objs)
    {
        if (r.state != State::Inactive) continue;
        bool ready = false;
        switch (r.def->activation)
        {
        case ActivationType::Immediate:      ready = true; break;
        case ActivationType::AfterObjective: ready = isComplete(r.def->activationObjective); break;
        case ActivationType::AfterTime:      ready = (m_missionTime >= r.def->activationTime); break;
        }
        if (!ready) continue;
        r.state   = State::Active;
        r.elapsed = 0.0f;
        emitObjective("objective activated", *r.def, telemetry::Level::Info);
        world.pushEvent("Obiettivo attivo: " + r.def->name);
    }
}

// Valutazione per tipo. Nessun id cablato: solo i CAMPI della definizione.
bool ObjectiveSystem::evaluate(World& world, Runtime& r, float dt)
{
    const ObjectiveDef& d = *r.def;
    switch (d.type)
    {
    case ObjectiveType::ReachArea:
        return teamInZone(world, d.actorTeam, d.x, d.z, d.radius);

    case ObjectiveType::HoldAreaForDuration:
        // Presenza CONTINUATIVA: uscire dalla zona azzera il progresso — è ciò
        // che rende "tenere" diverso da "passare di lì".
        if (teamInZone(world, d.actorTeam, d.x, d.z, d.radius)) r.holdTime += dt;
        else                                                    r.holdTime  = 0.0f;
        return r.holdTime >= d.holdSeconds;

    case ObjectiveType::CaptureZone:
        // La cattura la fa `CommandPosts` (ADR-009): qui si legge solo CHI lo
        // possiede. Avvolgere, non riscrivere (doc 25).
        return postOwner(world, d.targetPost) == d.actorTeam;

    case ObjectiveType::DefendZone:
        // Difendere = tenerlo per holdSeconds. Perderlo è un FALLIMENTO
        // immediato, non un timer che si azzera: un post perso è perso.
        if (postOwner(world, d.targetPost) != d.actorTeam)
        {
            r.state = State::Failed;
            ++world.missionStats.objectivesFailed;
            r.failureReason = "post perduto";
            emitObjective("objective failed", d, telemetry::Level::Warn, r.failureReason);
            applyConsequences(world, d, d.onFailure, "failure");
            world.pushEvent("Obiettivo fallito: " + d.name + " (post perduto)");
            return false;
        }
        r.holdTime += dt;
        return r.holdTime >= d.holdSeconds;

    case ObjectiveType::EliminateTarget:
        // Conta gli EVENTI di eliminazione dalla mailbox per-tick. Contare i
        // "vivi rimasti" sarebbe sbagliato: i respawn creano entità nuove, e
        // l'entità uccisa è già distrutta da CombatSystem (10_ProjectMemory) —
        // per questo la mailbox porta con sé il team.
        for (const auto& k : world.killedThisTick)
            if (k.team == d.targetTeam) ++r.progress;
        return r.progress >= d.count;

    case ObjectiveType::DestroyTarget:
    {
        // Completo quando il bersaglio NOMINATO è stato distrutto. L'entità del
        // bersaglio muore in CombatSystem (killedThisTick); la mailbox
        // strategicTargets (popolata dal game mode) collega quella entità alla
        // sua label — così l'ObjectiveSystem resta agnostico al codice di gioco.
        for (const auto& k : world.killedThisTick)
            for (const auto& st : world.strategicTargets)
                if (st.entity == k.entity && st.label == d.targetStructure)
                    return true;
        return false;
    }

    default:
        return false;   // i tipi non implementati sono già stati intercettati
    }
}

} // namespace mini

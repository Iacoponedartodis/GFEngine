#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/Definitions.hpp"   // MapDef (18_AiMapConsumption)
#include "mini/game/ai/WorldIntel.hpp"       // World Intelligence query layer (ADR-025)
#include "mini/game/nav/NavManager.hpp"      // crowd/pathfinding (ADR-017 Phase B)
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <tracy/Tracy.hpp>   // ADR-015: no-op se USE_TRACY_PROFILER=OFF
#include <nlohmann/json.hpp>   // event() data (ADR-016)
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace mini
{

static constexpr float PI = 3.14159265f;

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
static const char* aiStateName(AiState s)
{
    switch (s) {
        case AiState::Patrol: return "Patrol";
        case AiState::Alert:  return "Alert";
        case AiState::Hunt:   return "Hunt";
        case AiState::Search: return "Search";
    }
    return "?";
}

// Ingresso in Hunt (16_AiBehavior): con probabilità flankChance l'AI
// raggiunge la lastKnown da un punto laterale (~6m perpendicolare)
// invece che in linea retta.
static void enterHunt(AiComponent& ai, const TransformComponent& et)
{
    ai.state = AiState::Hunt;
    if (ai.hasLastKnown && !ai.flankActive && aiRand01() < ai.flankChance)
    {
        float dx = ai.lastKnownX - et.x, dz = ai.lastKnownZ - et.z;
        const float len = std::sqrt(dx*dx + dz*dz);
        if (len > 1.0f)
        {
            dx /= len; dz /= len;
            const float side = (aiRand01() < 0.5f) ? 1.0f : -1.0f;
            ai.flankX = ai.lastKnownX + (-dz * side) * 6.0f;
            ai.flankZ = ai.lastKnownZ + ( dx * side) * 6.0f;
            ai.flankActive = true;
        }
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

// Genera un punto di ricerca attorno all'ultima posizione nota (raggio
// ~12m) — mappa-agnostico. Le vecchie coordinate globali -8..+8 erano
// l'arena hardcoded pre-firebase: su una mappa 50x40 ammassavano tutte
// le AI al centro, uccidendo la battaglia.
static void pickSearchPoint(AiComponent& ai, float x, float z)
{
    const float cx = ai.hasLastKnown ? ai.lastKnownX : x;
    const float cz = ai.hasLastKnown ? ai.lastKnownZ : z;
    ai.searchX = cx + (aiRand01() - 0.5f) * 24.0f;
    ai.searchZ = cz + (aiRand01() - 0.5f) * 24.0f;
}

void AiSystem::update(World& world, float dt)
{
    ZoneScoped;   // ADR-015: AI update loop
    const std::vector<EntityId> snap = world.getEntities();
    const std::uint64_t tick = world.getTickCount();   // time-slicing (Fase 4)

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
            break;   // v0: dirige il primo comandante vivo (uno stratega per lato)
        }

        // Focus = command post NON separatista (owner != 2) più vicino al comandante.
        bool haveFocus = false; float fx = 0.0f, fz = 0.0f; std::string flabel;
        if (aliveCmdr && world.activeMap)
        {
            float best2 = 1e18f;
            for (const auto& st : world.commandPostStates)
            {
                if (st.owner == 2) continue;                 // già dei droidi
                for (const auto& cp : world.activeMap->commandPosts)
                {
                    if (cp.label != st.label) continue;      // posizione autorata per label
                    const float dx = cp.x - cmdrX, dz = cp.z - cmdrZ;
                    const float d2 = dx * dx + dz * dz;
                    if (d2 < best2)
                    { best2 = d2; fx = cp.x; fz = cp.z; flabel = cp.label; haveFocus = true; }
                    break;
                }
            }
        }

        if (world.enemyCommand.commanderAlive && !aliveCmdr)   // ultimo comandante caduto
            world.pushEvent("Comandante tattico nemico eliminato: i droidi perdono coordinamento");

        world.enemyCommand.commanderAlive = aliveCmdr;
        world.enemyCommand.active         = haveFocus;
        world.enemyCommand.x = fx; world.enemyCommand.z = fz;
        world.enemyCommand.label = flabel;
    }

    // Heartbeat diagnostico (ogni ~10s a 60Hz): quante AI e in che stato.
    // Rende osservabile da telemetria il sintomo "AI ferme".
    if (world.getTickCount() % 600 == 1)
    {
        int nAi = 0, patrol = 0, alert = 0, hunt = 0, search = 0, stat = 0;
        for (EntityId e : snap)
            if (const auto* a = world.getAi(e))
            {
                ++nAi;
                if (a->stationary) ++stat;
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
    }

    // ── Raccogli bersagli per team (SoA flat, Fase 3) ────────────────
    // id + posizione catturati UNA volta in array contigui paralleli. I loop
    // di ricerca del nearest (sotto, O(AI x bersagli)) leggono team*Pos[i]
    // contiguo invece di fare un getTransform(tgt) — un lookup hash +
    // pointer-chase su heap sparso — per ogni coppia. Il componente pesante
    // viene recuperato SOLO per il bersaglio selezionato (getTransform(nearest)).
    std::vector<EntityId>  team1Tgts, team2Tgts;
    std::vector<glm::vec3> team1Pos,  team2Pos;
    for (EntityId e : snap)
    {
        const auto* tm = world.getTeam(e);
        if (!tm || world.getBullet(e)) continue;
        const auto* et = world.getTransform(e);
        if (!et) continue;   // senza transform non può essere un bersaglio valido
        const glm::vec3 p = {et->x, et->y, et->z};
        if (tm->teamId == 1)      { team1Tgts.push_back(e); team1Pos.push_back(p); }
        else if (tm->teamId == 2) { team2Tgts.push_back(e); team2Pos.push_back(p); }
    }

    // ── SHARED AWARENESS: se un qualsiasi nemico vede un bersaglio,
    //    TUTTI i nemici dello stesso team ricevono la lastKnown ─────────
    // Prima passata: trova le lastKnown per team
    float sharedKnownX_t2 = 0, sharedKnownZ_t2 = 0; bool hasShared_t2 = false;
    float sharedKnownX_t1 = 0, sharedKnownZ_t1 = 0; bool hasShared_t1 = false;

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

        int losChecks = 0;
        for (size_t i = 0; i < targets.size(); ++i)
        {
            if (targets[i] == e) continue;
            const glm::vec3& tp = tgtPos[i];   // contiguo, niente hash lookup
            float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
            if (d2 >= ai->aggroRange * ai->aggroRange) continue;
            if (++losChecks > config::AI_MAX_LOS_CHECKS) break;   // Fase 4b: bounda la LOS
            if (!physics::hasLineOfSight(ePos, tp, world)) continue;

            // Qualcuno del myTeam ha visto un bersaglio!
            if (myTeam == 2) { sharedKnownX_t2 = tp.x; sharedKnownZ_t2 = tp.z; hasShared_t2 = true; }
            else             { sharedKnownX_t1 = tp.x; sharedKnownZ_t1 = tp.z; hasShared_t1 = true; }
            break;
        }
    }

    // ── Seconda passata: aggiorna ogni AI ────────────────────────────
    for (EntityId e : snap)
    {
        auto* ai   = world.getAi(e);   if (!ai) continue;
        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

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

        // Shared awareness: aggiorna lastKnown da chiunque nel team
        const bool teamHasShared = (myTeam == 2) ? hasShared_t2 : hasShared_t1;
        const float skX = (myTeam == 2) ? sharedKnownX_t2 : sharedKnownX_t1;
        const float skZ = (myTeam == 2) ? sharedKnownZ_t2 : sharedKnownZ_t1;

        if (teamHasShared)
        {
            ai->lastKnownX = skX;
            ai->lastKnownZ = skZ;
            ai->hasLastKnown = true;
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
            const float aggro2 = ai->aggroRange * ai->aggroRange;
            for (size_t i = 0; i < targets.size(); ++i)
            {
                if (targets[i] == e) continue;
                const glm::vec3& tp = tgtPos[i];   // contiguo, niente hash lookup
                const float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
                if (d2 >= aggro2) continue;
                if (kn == K && d2 >= kD2[K-1]) continue;   // più lontano dei K correnti
                int pos = (kn < K) ? kn : K - 1;           // inserimento ordinato
                while (pos > 0 && kD2[pos-1] > d2)
                { kD2[pos] = kD2[pos-1]; kIdx[pos] = kIdx[pos-1]; --pos; }
                kD2[pos] = d2; kIdx[pos] = (int)i;
                if (kn < K) ++kn;
            }
            nearest = 0;
            for (int j = 0; j < kn; ++j)
                if (physics::hasLineOfSight(ePos, tgtPos[kIdx[j]], world))
                { nearest = targets[kIdx[j]]; break; }

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
                    if (ft && fh && fh->current > 0.0f
                        && physics::hasLineOfSight(ePos, {ft->x, ft->y, ft->z}, world))
                        nearest = sqf->targetEntity;
                }
            ai->targetEntity = nearest;   // cache per i tick non-sensing
        }
        else
        {
            nearest = ai->targetEntity;   // riusa il bersaglio cachato
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
                enterHunt(*ai, *et);
        }
        else if (ai->state == AiState::Patrol && ai->hasLastKnown)
        {
            // Era in pattuglia ma il team ha condiviso una posizione: Hunt
            enterHunt(*ai, *et);
        }
        // Hunt e Search non hanno timeout — l'AI non "dimentica" MAI

        const bool patrolOk = !(ai->patrolAx==0 && ai->patrolAz==0 &&
                                 ai->patrolBx==0 && ai->patrolBz==0);

        // ── Anti-stuck ───────────────────────────────────────────────
        const float movedDist = std::abs(et->x - ai->prevX) + std::abs(et->z - ai->prevZ);
        if (movedDist < 0.05f && !ai->stationary)
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
            if (ai->state == AiState::Alert && nearest != 0)
            {
                // Ingaggio diretto: strafing + avanzamento
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
                        if (!covering && !ai->evading && aiRand01() < ai->coverPreference)
                        {
                            ai->evading = true;
                            ai->exposeTimer = aiRandRange(ai->hideMin, ai->hideMax);
                            // Copertura vera se la mappa la offre (doc 18):
                            // altrimenti resta lo strafe evasivo di fallback.
                            // Query al World Intelligence Layer (ADR-025).
                            const CoverPointDef* cov = world.activeMap
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

                    // ── Ritirata sotto soglia HP ─────────────────────
                    const auto* ehp = world.getHealth(e);
                    const float hpFrac = (ehp && ehp->max > 0.0f)
                                       ? ehp->current / ehp->max : 1.0f;
                    const bool retreating = ai->retreatHpThresh > 0.0f
                                         && hpFrac < ai->retreatHpThresh;

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
                }
            }
            else if (ai->state == AiState::Hunt && ai->hasLastKnown)
            {
                // Va verso l'ultima posizione nota (o, se sta fiancheggiando,
                // prima verso il punto laterale scelto in enterHunt).
                const float destX = ai->flankActive ? ai->flankX : ai->lastKnownX;
                const float destZ = ai->flankActive ? ai->flankZ : ai->lastKnownZ;
                moveDX = destX - et->x;
                moveDZ = destZ - et->z;
                float dist = norm2D(moveDX, moveDZ);
                moveDist = dist;

                if (ai->flankActive && (dist < 1.5f || isStuck))
                {
                    // Punto di fiancheggiamento raggiunto (o irraggiungibile):
                    // prosegui dritto sulla lastKnown.
                    ai->flankActive = false;
                    ai->stuckTimer = 0;
                }
                else if (dist < 1.0f)
                {
                    // Raggiunto lastKnown: passa a Search
                    ai->state = AiState::Search;
                    pickSearchPoint(*ai, et->x, et->z);
                }
                else if (isStuck)
                {
                    // Bloccato verso lastKnown: prova Search da un altro punto
                    ai->state = AiState::Search;
                    pickSearchPoint(*ai, et->x, et->z);
                    ai->stuckTimer = 0;
                }
                else
                {
                    moveSpeed = ai->seekSpeed;
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
                }
                moveDX = ai->searchX - et->x;
                moveDZ = ai->searchZ - et->z;
                float dist = norm2D(moveDX, moveDZ);
                moveDist = dist;

                if (dist < 1.5f || isStuck)
                {
                    // Raggiunto punto di ricerca o bloccato: scegli un nuovo punto
                    pickSearchPoint(*ai, et->x, et->z);
                    ai->stuckTimer = 0;
                }
                else
                {
                    moveSpeed = ai->seekSpeed * 0.8f;
                    et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI);
                }
            }
            else if (ai->state == AiState::Patrol
                     && (patrolOk || (world.enemyCommand.active && team->teamId == 2)))
            {
                // Pattuglia (prima del contatto). Se un comandante nemico è vivo
                // (ADR-024/doc 32), i droidi puntano il FOCUS strategico invece del
                // waypoint di rotta: è così che convergono sull'obiettivo scelto.
                // Ai waypoint (o al focus) SOSTA per patrolDwell secondi: è ciò che
                // permette di catturare i command post (presenza continuativa).
                const bool commanded = world.enemyCommand.active && team->teamId == 2;
                float wx, wz;
                if (commanded) { wx = world.enemyCommand.x; wz = world.enemyCommand.z; }
                else
                {
                    wx = ai->goingToB ? ai->patrolBx : ai->patrolAx;
                    wz = ai->goingToB ? ai->patrolBz : ai->patrolAz;
                }
                moveDX = wx - et->x; moveDZ = wz - et->z;

                if (ai->waitTimer > 0.0f)
                {
                    // In sosta: fermo, niente anti-stuck
                    ai->waitTimer -= dt;
                    ai->stuckTimer = 0.0f;
                    moveDX = 0; moveDZ = 0;
                    if (ai->waitTimer <= 0.0f)
                        ai->goingToB = !ai->goingToB;
                }
                else if ((moveDist = norm2D(moveDX, moveDZ)) < 0.6f)
                {
                    if (ai->patrolDwell > 0.0f)
                        ai->waitTimer = ai->patrolDwell;
                    else
                        ai->goingToB = !ai->goingToB;
                    moveDX = 0; moveDZ = 0;
                }
                else if (isStuck)
                { ai->goingToB = !ai->goingToB; ai->stuckTimer = 0; }
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
        if (sq && sq->hasActiveOrder() && !ai->stationary
            && sq->order != OrderType::FocusFire)
        {
            float ox = sq->targetX - et->x, oz = sq->targetZ - et->z;
            const float od = norm2D(ox, oz);   // normalizza ox/oz, ritorna la distanza
            const float leash = (sq->order == OrderType::Follow)       ? 8.0f
                              : (sq->order == OrderType::HoldPosition ||
                                 sq->order == OrderType::CoveringFire) ? 2.0f
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
                nv.requestMoveTarget(ai->crowdAgentIdx,
                                     {et->x + moveDX * moveDist, et->y,
                                      et->z + moveDZ * moveDist});
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
            telemetry::event(telemetry::Level::Info, "AI", "state change", d);
        }

        // ── Sparo (solo in Alert con LOS, mai in fase evasiva) ───────
        if (ai->state != AiState::Alert || nearest == 0) continue;
        if (ai->evading) continue; // peek/hide: in "hide" non spara
        if (ai->shootCooldown > 0.0f) { ai->shootCooldown -= dt; continue; }
        if (ai->overheated) continue; // arma surriscaldata: attende il cooling

        const auto* tt = world.getTransform(nearest);
        if (!tt) continue;
        // Il bersaglio è cachato per più frame (time-slicing): ri-verifica il
        // LOS al tiro, così non spara attraverso i muri se nel frattempo si è
        // coperto. Costa un LOS solo quando il cooldown è scaduto (raro), non
        // ogni frame → economico anche a scala.
        if (!physics::hasLineOfSight({et->x, et->y, et->z},
                                     {tt->x, tt->y, tt->z}, world)) continue;
        float dx = tt->x-et->x, dy = tt->y-et->y, dz = tt->z-et->z;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 0.001f) continue;

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
        world.addTransform(b, TransformComponent{.x=et->x,.y=et->y,.z=et->z,.sx=0.10f,.sy=0.10f,.sz=0.10f});
        world.addVelocity(b, {dx*inv, dy*inv, dz*inv});
        world.addTeam(b, {myTeam});
        world.addBullet(b, {ai->bulletDamage, ai->bulletLifetime, myTeam});
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
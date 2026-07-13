#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/Definitions.hpp"   // MapDef (18_AiMapConsumption)
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <tracy/Tracy.hpp>   // ADR-015: no-op se USE_TRACY_PROFILER=OFF
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

// Cerca il cover point più vicino (≤ maxDist) il cui fronte guarda verso il
// nemico (dot(facing, versoNemico) > 0). Ritorna false se non ce n'è.
static bool pickCover(const MapDef* map, float x, float z,
                      float enemyX, float enemyZ,
                      float& outX, float& outZ)
{
    if (!map || map->coverPoints.empty()) return false;
    const float maxDist2 = 12.0f * 12.0f;
    float best2 = maxDist2;
    bool found = false;
    for (const auto& c : map->coverPoints)
    {
        const float dx = c.x - x, dz = c.z - z;
        const float d2 = dx*dx + dz*dz;
        if (d2 >= best2) continue;
        // Il fronte della copertura deve guardare verso il nemico
        float ex = enemyX - c.x, ez = enemyZ - c.z;
        const float el = std::sqrt(ex*ex + ez*ez);
        if (el < 0.5f) continue;
        ex /= el; ez /= el;
        const float fr = c.facingDeg * (PI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        if (fx*ex + fz*ez <= 0.15f) continue;  // copre nella direzione sbagliata
        best2 = d2; outX = c.x; outZ = c.z; found = true;
    }
    return found;
}

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
            ai->stuckTimer = 0.0f;
        ai->prevX = et->x; ai->prevZ = et->z;
        const bool isStuck = ai->stuckTimer > config::AI_STUCK_TIME;

        // ── Movimento ────────────────────────────────────────────────
        float moveDX = 0, moveDZ = 0, moveSpeed = 0;

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
                        if (!ai->evading && aiRand01() < ai->coverPreference)
                        {
                            ai->evading = true;
                            ai->exposeTimer = aiRandRange(ai->hideMin, ai->hideMax);
                            // Copertura vera se la mappa la offre (doc 18):
                            // altrimenti resta lo strafe evasivo di fallback.
                            ai->hasCover = pickCover(world.activeMap,
                                                     et->x, et->z, tt->x, tt->z,
                                                     ai->coverX, ai->coverZ);

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
            else if (ai->state == AiState::Patrol && patrolOk)
            {
                // Pattuglia (prima del contatto). Ai waypoint SOSTA per
                // patrolDwell secondi: è ciò che permette di catturare i
                // command post (serve presenza continuativa nell'area).
                float wx = ai->goingToB ? ai->patrolBx : ai->patrolAx;
                float wz = ai->goingToB ? ai->patrolBz : ai->patrolAz;
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
                else if (norm2D(moveDX, moveDZ) < 0.6f)
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

        // Danger zone (doc 18): fuori dall'ingaggio il movimento evita le
        // aree marcate pericolose dall'autore della mappa.
        if (ai->state != AiState::Alert && moveSpeed > 0.0f)
            applyDangerRepulsion(world.activeMap, et->x, et->z, moveDX, moveDZ);

        // Cooldown abilità attive + scatto roll in corso (16 est.)
        if (auto* ab = world.getAbilities(e))
            for (auto& s : ab->states)
                if (s.cooldown > 0.0f) s.cooldown -= dt;

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

        // ── Salto anti-ostacolo (jump_enabled dal profilo AI) ─────────
        // Se sta provando a muoversi ma è ferma da metà del tempo anti-stuck
        // ed è a terra, tenta un salto PRIMA che scatti l'inversione di rotta:
        // supera casse/coperture basse invece di rimbalzare avanti-indietro.
        if (ai->jumpEnabled && !ai->stationary && moveSpeed > 0.0f
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
        const float spread = (1.0f - ai->accuracy) * config::AI_SPREAD_MAX;
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

        // Cadenza dall'arma se disponibile, altrimenti legacy dal profilo AI
        ai->shootCooldown = (ai->fireInterval > 0.0f) ? ai->fireInterval
                                                      : ai->shootInterval;

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
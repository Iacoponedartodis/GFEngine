#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <glm/glm.hpp>
#include <cmath>
#include <iostream>
#include <vector>

namespace mini
{

static constexpr float PI = 3.14159265f;

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

// Genera un punto di ricerca pseudo-random sulla mappa basato sulla posizione attuale
static void pickSearchPoint(AiComponent& ai, float x, float z)
{
    // Deterministico ma vario: usa la posizione come seed
    int ix = (int)(x * 73.0f) + (int)(z * 137.0f) + (int)(ai.stuckTimer * 31.0f);
    ai.searchX = -8.0f + (float)(((ix * 1103515245 + 12345) >> 8) % 160) * 0.1f; // -8 a +8
    ai.searchZ = -9.0f + (float)(((ix * 214013 + 2531011) >> 8) % 160) * 0.1f;    // -9 a +7
}

void AiSystem::update(World& world, float dt)
{
    const std::vector<EntityId> snap = world.getEntities();

    // ── Raccogli bersagli per team ───────────────────────────────────
    std::vector<EntityId> team1Tgts, team2Tgts;
    for (EntityId e : snap)
    {
        const auto* tm = world.getTeam(e);
        if (!tm || world.getBullet(e)) continue;
        if (tm->teamId == 1) team1Tgts.push_back(e);
        else if (tm->teamId == 2) team2Tgts.push_back(e);
    }

    // ── SHARED AWARENESS: se un qualsiasi nemico vede un bersaglio,
    //    TUTTI i nemici dello stesso team ricevono la lastKnown ─────────
    // Prima passata: trova le lastKnown per team
    float sharedKnownX_t2 = 0, sharedKnownZ_t2 = 0; bool hasShared_t2 = false;
    float sharedKnownX_t1 = 0, sharedKnownZ_t1 = 0; bool hasShared_t1 = false;

    for (EntityId e : snap)
    {
        auto* ai   = world.getAi(e);   if (!ai) continue;
        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

        const int myTeam = team->teamId;
        const auto& targets = (myTeam == 1) ? team2Tgts : team1Tgts;
        const glm::vec3 ePos = {et->x, et->y, et->z};

        for (EntityId tgt : targets)
        {
            if (tgt == e) continue;
            const auto* tt = world.getTransform(tgt);
            if (!tt) continue;
            const glm::vec3 tp = {tt->x, tt->y, tt->z};
            float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
            if (d2 >= ai->aggroRange * ai->aggroRange) continue;
            if (!physics::hasLineOfSight(ePos, tp, world)) continue;

            // Qualcuno del myTeam ha visto un bersaglio!
            if (myTeam == 2) { sharedKnownX_t2 = tt->x; sharedKnownZ_t2 = tt->z; hasShared_t2 = true; }
            else             { sharedKnownX_t1 = tt->x; sharedKnownZ_t1 = tt->z; hasShared_t1 = true; }
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
        EntityId nearest = 0;
        float minD2 = ai->aggroRange * ai->aggroRange;
        for (EntityId tgt : targets)
        {
            if (tgt == e) continue;
            const auto* tt = world.getTransform(tgt);
            if (!tt) continue;
            const glm::vec3 tp = {tt->x, tt->y, tt->z};
            float d2 = (tp.x-ePos.x)*(tp.x-ePos.x)+(tp.y-ePos.y)*(tp.y-ePos.y)+(tp.z-ePos.z)*(tp.z-ePos.z);
            if (d2 >= minD2) continue;
            if (!physics::hasLineOfSight(ePos, tp, world)) continue;
            minD2 = d2; nearest = tgt;
        }

        // ── Transizioni stato ────────────────────────────────────────
        if (nearest != 0)
        {
            // LOS diretto: Alert (spara)
            const auto* tt = world.getTransform(nearest);
            ai->lastKnownX = tt->x; ai->lastKnownZ = tt->z;
            ai->hasLastKnown = true;
            ai->state = AiState::Alert;
            ai->alertTimer = 3.0f;
        }
        else if (ai->state == AiState::Alert)
        {
            // Aveva LOS ma l'ha perso: attendi un po' poi Hunt
            ai->alertTimer -= dt;
            if (ai->alertTimer <= 0.0f)
                ai->state = AiState::Hunt;
        }
        else if (ai->state == AiState::Patrol && ai->hasLastKnown)
        {
            // Era in pattuglia ma il team ha condiviso una posizione: Hunt
            ai->state = AiState::Hunt;
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
                float advX = tt->x - et->x, advZ = tt->z - et->z;
                float dist = norm2D(advX, advZ);

                ai->strafeTimer -= dt;
                if (ai->strafeTimer <= 0.0f || isStuck)
                { ai->strafeSign = -ai->strafeSign; ai->strafeTimer = 1.4f; ai->stuckTimer = 0; }

                const float perpX = -advZ * ai->strafeSign;
                const float perpZ =  advX * ai->strafeSign;

                if (dist > 2.5f)
                { moveDX = advX*0.45f + perpX*0.65f; moveDZ = advZ*0.45f + perpZ*0.65f; moveSpeed = ai->seekSpeed*0.75f; }
                else
                { moveDX = perpX; moveDZ = perpZ; moveSpeed = ai->seekSpeed*0.55f; }

                et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);
            }
            else if (ai->state == AiState::Hunt && ai->hasLastKnown)
            {
                // Va verso l'ultima posizione nota
                moveDX = ai->lastKnownX - et->x;
                moveDZ = ai->lastKnownZ - et->z;
                float dist = norm2D(moveDX, moveDZ);

                if (dist < 1.0f)
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
                // Cerca in punti random sulla mappa finché non rivede il bersaglio
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
                // Pattuglia iniziale (prima del contatto)
                float wx = ai->goingToB ? ai->patrolBx : ai->patrolAx;
                float wz = ai->goingToB ? ai->patrolBz : ai->patrolAz;
                moveDX = wx - et->x; moveDZ = wz - et->z;
                if (norm2D(moveDX, moveDZ) < 0.2f)
                    ai->goingToB = !ai->goingToB;
                else if (isStuck)
                { ai->goingToB = !ai->goingToB; ai->stuckTimer = 0; }
                else
                { moveSpeed = ai->patrolSpeed; et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI); }
            }
        }
        else if (nearest != 0)
        {
            const auto* tt = world.getTransform(nearest);
            et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);
        }

        const float nx = et->x + moveDX * moveSpeed * dt;
        const float nz = et->z + moveDZ * moveSpeed * dt;
        aiMove(*et, nx, nz, *ai, dt, world);

        // ── Sparo (solo in Alert con LOS) ────────────────────────────
        if (ai->state != AiState::Alert || nearest == 0) continue;
        if (ai->shootCooldown > 0.0f) { ai->shootCooldown -= dt; continue; }

        const auto* tt = world.getTransform(nearest);
        if (!tt) continue;
        float dx = tt->x-et->x, dy = tt->y-et->y, dz = tt->z-et->z;
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 0.001f) continue;

        float inv = k_bulletSpeed / len;
        EntityId b = world.createEntity();
        world.addTransform(b, TransformComponent{.x=et->x,.y=et->y,.z=et->z,.sx=0.10f,.sy=0.10f,.sz=0.10f});
        world.addVelocity(b, {dx*inv, dy*inv, dz*inv});
        world.addTeam(b, {myTeam});
        world.addBullet(b, {k_bulletDmg, k_bulletLife, myTeam});
        if (ai->bulletMesh)
            world.addMeshRenderer(b, {ai->bulletMesh, ai->bulletTexture, ai->bulletR, ai->bulletG, ai->bulletB});

        ai->shootCooldown = ai->shootInterval;
    }
}

} // namespace mini
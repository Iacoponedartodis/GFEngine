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

// Slide + step-up + gravità per AI (usa il modulo condiviso)
static void aiMove(TransformComponent& et, float nx, float nz,
                   AiComponent& ai, float dt, World& world)
{
    const glm::vec3 H = config::aiHalf();

    // Orizzontale con step-up
    const glm::vec3 prev = {et.x, et.y, et.z};
    const glm::vec3 next = {nx, et.y, nz};
    const glm::vec3 result = physics::slideMoveWithStepUp(
        prev, next, H, world, config::STEP_HEIGHT);
    et.x = result.x;
    et.y = result.y; // step-up potrebbe averlo alzato
    et.z = result.z;

    // Gravità Y
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

// ── Update ───────────────────────────────────────────────────────────────
void AiSystem::update(World& world, float dt)
{
    const std::vector<EntityId> snap = world.getEntities();

    std::vector<EntityId> team1Tgts, team2Tgts;
    for (EntityId e : snap)
    {
        const auto* tm = world.getTeam(e);
        if (!tm || world.getBullet(e)) continue;
        if (tm->teamId == 1) team1Tgts.push_back(e);
        else if (tm->teamId == 2) team2Tgts.push_back(e);
    }

    for (EntityId e : snap)
    {
        auto* ai   = world.getAi(e);   if (!ai) continue;
        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

        const int myTeam = team->teamId;
        const auto& targets = (myTeam == 1) ? team2Tgts : team1Tgts;
        const glm::vec3 ePos = {et->x, et->y, et->z};

        // ── 1. Bersaglio con LOS (usa modulo condiviso) ──────────────
        EntityId nearest = 0;
        float    minD2   = ai->aggroRange * ai->aggroRange;
        for (EntityId tgt : targets)
        {
            if (tgt == e) continue;
            const auto* tt = world.getTransform(tgt);
            if (!tt) continue;
            const glm::vec3 tp = {tt->x, tt->y, tt->z};
            const float d2 = (tp.x-ePos.x)*(tp.x-ePos.x) +
                             (tp.y-ePos.y)*(tp.y-ePos.y) +
                             (tp.z-ePos.z)*(tp.z-ePos.z);
            if (d2 >= minD2) continue;
            if (!physics::hasLineOfSight(ePos, tp, world)) continue;
            minD2 = d2; nearest = tgt;
        }

        // ── 2. Transizioni stato ─────────────────────────────────────
        if (nearest != 0)
        {
            const auto* tt = world.getTransform(nearest);
            ai->lastKnownX = tt->x; ai->lastKnownZ = tt->z;
            ai->hasLastKnown = true;
            ai->state = AiState::Alert; ai->alertTimer = 2.0f; ai->seekTimer = 7.0f;
        }
        else if (ai->state == AiState::Alert)
        {
            ai->alertTimer -= dt;
            if (ai->alertTimer <= 0.0f)
                ai->state = ai->hasLastKnown ? AiState::Seek : AiState::Patrol;
        }
        else if (ai->state == AiState::Seek)
        {
            ai->seekTimer -= dt;
            if (ai->seekTimer <= 0.0f)
            { ai->state = AiState::Patrol; ai->hasLastKnown = false; }
        }

        const bool patrolOk = !(ai->patrolAx==0 && ai->patrolAz==0 &&
                                 ai->patrolBx==0 && ai->patrolBz==0);

        // ── 3. Anti-stuck detection ──────────────────────────────────
        const float movedDist = std::abs(et->x - ai->prevX) + std::abs(et->z - ai->prevZ);
        if (movedDist < 0.05f && !ai->stationary)
            ai->stuckTimer += dt;
        else
            ai->stuckTimer = 0.0f;
        ai->prevX = et->x; ai->prevZ = et->z;

        const bool isStuck = ai->stuckTimer > config::AI_STUCK_TIME;

        // ── 4. Movimento ─────────────────────────────────────────────
        float moveDX = 0, moveDZ = 0, moveSpeed = 0;

        if (!ai->stationary)
        {
            if (ai->state == AiState::Alert && nearest != 0)
            {
                const auto* tt = world.getTransform(nearest);
                float advX = tt->x - et->x, advZ = tt->z - et->z;
                float dist = norm2D(advX, advZ);

                ai->strafeTimer -= dt;
                if (ai->strafeTimer <= 0.0f || isStuck)
                {
                    ai->strafeSign = -ai->strafeSign;
                    ai->strafeTimer = 1.4f;
                    if (isStuck) ai->stuckTimer = 0.0f;
                }
                const float perpX = -advZ * ai->strafeSign;
                const float perpZ =  advX * ai->strafeSign;

                if (dist > 3.5f)
                { moveDX = advX*0.45f + perpX*0.65f; moveDZ = advZ*0.45f + perpZ*0.65f; moveSpeed = ai->seekSpeed*0.75f; }
                else
                { moveDX = perpX; moveDZ = perpZ; moveSpeed = ai->seekSpeed*0.55f; }

                et->ry = std::atan2(tt->x - et->x, tt->z - et->z) * (180.0f / PI);
            }
            else if (ai->state == AiState::Patrol && patrolOk)
            {
                float wx = ai->goingToB ? ai->patrolBx : ai->patrolAx;
                float wz = ai->goingToB ? ai->patrolBz : ai->patrolAz;
                moveDX = wx - et->x; moveDZ = wz - et->z;
                if (norm2D(moveDX, moveDZ) < 0.2f)
                    ai->goingToB = !ai->goingToB;
                else if (isStuck)
                { ai->goingToB = !ai->goingToB; ai->stuckTimer = 0.0f; }
                else
                { moveSpeed = ai->patrolSpeed; et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI); }
            }
            else if (ai->state == AiState::Seek && ai->hasLastKnown)
            {
                moveDX = ai->lastKnownX - et->x; moveDZ = ai->lastKnownZ - et->z;
                if (norm2D(moveDX, moveDZ) < 0.3f)
                { ai->state = AiState::Patrol; ai->hasLastKnown = false; }
                else if (isStuck)
                { ai->state = AiState::Patrol; ai->hasLastKnown = false; ai->stuckTimer = 0.0f; }
                else
                { moveSpeed = ai->seekSpeed; et->ry = std::atan2(moveDX, moveDZ) * (180.0f / PI); }
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

        // ── 5. Sparo ─────────────────────────────────────────────────
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
        std::cout << "[AI] Fuoco dal " << (myTeam==1?"alleato":"nemico") << "!" << std::endl;
    }
}

} // namespace mini
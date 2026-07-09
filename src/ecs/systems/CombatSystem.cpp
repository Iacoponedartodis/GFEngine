#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/core/Telemetry.hpp"

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cmath>

namespace mini
{

// ── Test a SEGMENTO (anti-tunneling) ─────────────────────────────────────
// I proiettili si muovono a step discreti (a 55 m/s sono ~0.9 m per tick):
// un test puntuale ATTRAVERSAVA le zone piccole (testa B1: 0.12x0.44x0.15)
// senza mai esserci "dentro" in un tick — il mirino (raycast continuo)
// diceva "colpibile" ma il proiettile mancava. Ora si testa il segmento
// percorso nel tick (posizione precedente → attuale).

// Distanza² tra un punto e un segmento [a,b].
static float segPointDistSq(const glm::vec3& a, const glm::vec3& b,
                            const glm::vec3& p)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    float t = (len2 > 1e-8f) ? glm::dot(p - a, ab) / len2 : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const glm::vec3 c = a + ab * t;
    return glm::dot(p - c, p - c);
}

// Segmento [a,b] vs AABB [mn,mx] (slab test, parametro t in [0,1]).
static bool segAABB(const glm::vec3& a, const glm::vec3& b,
                    const glm::vec3& mn, const glm::vec3& mx)
{
    const glm::vec3 d = b - a;
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(d[i]) < 1e-8f)
        {
            if (a[i] < mn[i] || a[i] > mx[i]) return false;
        }
        else
        {
            const float inv = 1.0f / d[i];
            float t0 = (mn[i] - a[i]) * inv;
            float t1 = (mx[i] - a[i]) * inv;
            if (t0 > t1) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmin > tmax) return false;
        }
    }
    return true;
}

// Trasforma una zona dal model space al world space dell'entità
// (scala uniforme, yaw, offset verticale mesh) e testa il segmento.
static bool segmentInZone(const glm::vec3& a, const glm::vec3& b,
                          const glm::vec3& entityPos,
                          float scale, float yawDeg, float meshOffY,
                          const HitZone& zone)
{
    glm::vec3 off = zone.offset * scale;
    if (yawDeg != 0.0f)
    {
        const float c = std::cos(glm::radians(yawDeg));
        const float s = std::sin(glm::radians(yawDeg));
        off = { c*off.x + s*off.z, off.y, -s*off.x + c*off.z };
    }
    const glm::vec3 center = entityPos + glm::vec3(0, meshOffY, 0) + off;
    const glm::vec3 he     = zone.halfExtents * scale;
    return segAABB(a, b, center - he, center + he);
}

struct HitResult { bool hit = false; float mult = 1.0f; std::string zone; };

static HitResult testHit(const glm::vec3& bulletPrev,
                          const glm::vec3& bulletPos,
                          const glm::vec3& entityPos,
                          float scale, float yawDeg, float meshOffY,
                          const HitboxComponent* hb)
{
    // ── 1. Broad test: distanza segmento-centro (O(1) early-out) ────────
    const float distSq = segPointDistSq(bulletPrev, bulletPos, entityPos);
    const float broadR = 2.5f;
    if (distSq >= broadR * broadR) return {false, 1.0f, ""};

    // ── 2. Zone test se disponibile ────────────────────────────────────
    if (hb && hb->profile && !hb->profile->zones.empty())
    {
        for (const auto& zone : hb->profile->zones)
        {
            if (segmentInZone(bulletPrev, bulletPos, entityPos,
                              scale, yawDeg, meshOffY, zone))
                return {true, zone.damageMultiplier, zone.name};
        }
        // Nessuna zona colpita → fallback di striscio sul corpo
        const float fallbackR = 0.7f;
        if (distSq < fallbackR * fallbackR)
            return {true, 0.5f, "glance"};
        return {false, 1.0f, ""};
    }

    // ── 3. Fallback sferico puro (nessun profilo hitbox) ───────────────
    if (distSq < CombatSystem::k_hitRadius * CombatSystem::k_hitRadius)
        return {true, 1.0f, ""};

    return {false, 1.0f, ""};
}

void CombatSystem::update(World& world, float dt)
{
    std::vector<EntityId> toDestroy;
    const std::vector<EntityId> entities = world.getEntities();

    for (EntityId bid : entities)
    {
        auto* bullet = world.getBullet(bid);
        if (!bullet) continue;

        bullet->lifetime -= dt;
        if (bullet->lifetime <= 0.0f) { toDestroy.push_back(bid); continue; }

        auto* bt = world.getTransform(bid);
        if (!bt) continue;
        const glm::vec3 bPos = {bt->x, bt->y, bt->z};

        // Posizione a inizio tick (per il test a segmento anti-tunneling)
        glm::vec3 bPrev = bPos;
        if (const auto* bv = world.getVelocity(bid))
            bPrev -= glm::vec3(bv->vx, bv->vy, bv->vz) * dt;

        for (EntityId eid : entities)
        {
            if (eid == bid || world.getBullet(eid)) continue;
            auto* team = world.getTeam(eid);
            if (!team || team->teamId == bullet->ownerTeam) continue;
            auto* et = world.getTransform(eid);
            auto* eh = world.getHealth(eid);
            if (!et || !eh || eh->current <= 0.0f) continue;

            const glm::vec3 ePos = {et->x, et->y, et->z};
            const auto* hb = world.getHitbox(eid);
            const auto* mr = world.getMeshRenderer(eid);
            const float scale   = (et->sx > 0.0001f) ? et->sx : 1.0f;
            const float meshOff = mr ? mr->meshOffsetY : 0.0f;
            auto result = testHit(bPrev, bPos, ePos, scale, et->ry, meshOff, hb);
            if (!result.hit) continue;

            const float dmg = bullet->damage * result.mult;
            eh->current -= dmg;

            // Feedback HUD (hitmarker) SOLO per i colpi del giocatore —
            // non per quelli degli alleati AI (stesso team).
            if (bullet->fromPlayer)
            {
                world.combatFeedback.team1Hit = true;
                if (eh->current <= 0.0f) world.combatFeedback.team1Kill = true;
            }

            // Telemetria: ogni hit nel log (con zona, danno, hp, team) —
            // "i nemici non muoiono" diventa diagnosticabile dal file.
            telemetry::logTrace("hit: team" + std::to_string(bullet->ownerTeam)
                + " -> entita' " + std::to_string(eid)
                + " zona=" + (result.zone.empty() ? "corpo" : result.zone)
                + " x" + std::to_string(result.mult)
                + " danno=" + std::to_string((int)dmg)
                + " hp=" + std::to_string((int)eh->current)
                + "/" + std::to_string((int)eh->max));

            if (!result.zone.empty() && result.zone != "glance")
                std::cout << "[Combat] " << result.zone << " x" << result.mult
                          << " — danno: " << (int)dmg
                          << " HP: " << (int)eh->current << "/" << (int)eh->max << "\n";
            else
                std::cout << "[Combat] Colpito! HP: "
                          << (int)eh->current << "/" << (int)eh->max << "\n";

            toDestroy.push_back(bid);
            if (eh->current <= 0.0f)
            {
                telemetry::logInfo("kill: entita' " + std::to_string(eid)
                    + " eliminata dal team " + std::to_string(bullet->ownerTeam));
                std::cout << "[Combat] Eliminato!\n";
                toDestroy.push_back(eid);
            }
            break;
        }
    }
    for (EntityId id : toDestroy) world.destroyEntity(id);
}

} // namespace mini
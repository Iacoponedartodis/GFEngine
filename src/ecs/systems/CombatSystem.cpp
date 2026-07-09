#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cmath>

namespace mini
{

// Trasforma un punto zona dal model space (autorato nell'editor, piedi a Y=0)
// al world space dell'entità: scala uniforme, yaw, offset verticale mesh.
// Le zone dei profili sono definite sul modello NON scalato: senza questa
// trasformazione le hitbox di modelli scalati (es. clone 0.011) erano
// completamente fuori posto, e per tutti mancava il meshOffsetY (-0.5).
static bool pointInZone(const glm::vec3& p, const glm::vec3& entityPos,
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
    return (std::abs(p.x - center.x) <= he.x &&
            std::abs(p.y - center.y) <= he.y &&
            std::abs(p.z - center.z) <= he.z);
}

struct HitResult { bool hit = false; float mult = 1.0f; std::string zone; };

static HitResult testHit(const glm::vec3& bulletPos,
                          const glm::vec3& entityPos,
                          float scale, float yawDeg, float meshOffY,
                          const HitboxComponent* hb)
{
    // ── 1. Broad sphere test (O(1) early-out) ──────────────────────────
    // Il raggio deve coprire anche le zone alte (testa a ~1.8 dal suolo):
    // 1.2 rigettava gli headshot prima ancora del test per-zona.
    const glm::vec3 d = entityPos - bulletPos;
    const float distSq = d.x*d.x + d.y*d.y + d.z*d.z;
    const float broadR = 2.5f;
    if (distSq >= broadR * broadR) return {false, 1.0f, ""};

    // ── 2. Zone test se disponibile ────────────────────────────────────
    if (hb && hb->profile && !hb->profile->zones.empty())
    {
        for (const auto& zone : hb->profile->zones)
        {
            if (pointInZone(bulletPos, entityPos, scale, yawDeg, meshOffY, zone))
                return {true, zone.damageMultiplier, zone.name};
        }
        // Nessuna zona colpita → usa fallback sferico con raggio ridotto
        // (il broad test è già passato: il proiettile è vicino ma non in zona)
        const float fallbackR = 0.7f;
        if (distSq < fallbackR * fallbackR)
            return {true, 0.5f, "glance"}; // colpo di striscio
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
            auto result = testHit(bPos, ePos, scale, et->ry, meshOff, hb);
            if (!result.hit) continue;

            const float dmg = bullet->damage * result.mult;
            eh->current -= dmg;

            if (!result.zone.empty() && result.zone != "glance")
                std::cout << "[Combat] " << result.zone << " x" << result.mult
                          << " — danno: " << (int)dmg
                          << " HP: " << (int)eh->current << "/" << (int)eh->max << "\n";
            else
                std::cout << "[Combat] Colpito! HP: "
                          << (int)eh->current << "/" << (int)eh->max << "\n";

            toDestroy.push_back(bid);
            if (eh->current <= 0.0f)
            { std::cout << "[Combat] Eliminato!\n"; toDestroy.push_back(eid); }
            break;
        }
    }
    for (EntityId id : toDestroy) world.destroyEntity(id);
}

} // namespace mini
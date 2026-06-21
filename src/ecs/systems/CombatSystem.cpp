#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cmath>

namespace mini
{

static bool pointInZone(const glm::vec3& p, const glm::vec3& entityPos, const HitZone& zone)
{
    const glm::vec3 center = entityPos + zone.offset;
    return (std::abs(p.x - center.x) <= zone.halfExtents.x &&
            std::abs(p.y - center.y) <= zone.halfExtents.y &&
            std::abs(p.z - center.z) <= zone.halfExtents.z);
}

struct HitResult { bool hit = false; float mult = 1.0f; std::string zone; };

static HitResult testHit(const glm::vec3& bulletPos,
                          const glm::vec3& entityPos,
                          const HitboxComponent* hb)
{
    // ── 1. Broad sphere test (O(1) early-out) ──────────────────────────
    const glm::vec3 d = entityPos - bulletPos;
    const float distSq = d.x*d.x + d.y*d.y + d.z*d.z;
    const float broadR = 1.2f; // leggermente più grande del corpo
    if (distSq >= broadR * broadR) return {false, 1.0f, ""};

    // ── 2. Zone test se disponibile ────────────────────────────────────
    if (hb && hb->profile && !hb->profile->zones.empty())
    {
        for (const auto& zone : hb->profile->zones)
        {
            if (pointInZone(bulletPos, entityPos, zone))
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
            auto result = testHit(bPos, ePos, hb);
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
#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <string>

namespace mini
{

// ── AABB zone hit test ────────────────────────────────────────────────────
// Testa se il punto 'p' (posizione proiettile) si trova dentro la zona
// 'zone' centrata su 'entityPos'.
static bool pointInZone(const glm::vec3& p,
                        const glm::vec3& entityPos,
                        const HitZone& zone)
{
    const glm::vec3 center = entityPos + zone.offset;
    return (std::abs(p.x - center.x) <= zone.halfExtents.x &&
            std::abs(p.y - center.y) <= zone.halfExtents.y &&
            std::abs(p.z - center.z) <= zone.halfExtents.z);
}

// ── Hit test completo: hitbox a zone o fallback sferico ──────────────────
struct HitResult
{
    bool  hit             = false;
    float damageMultiplier = 1.0f;
    std::string zoneName; // vuoto = hit fallback
};

static HitResult testHit(const glm::vec3& bulletPos,
                          const glm::vec3& entityPos,
                          const HitboxComponent* hb)
{
    if (hb && hb->profile && !hb->profile->zones.empty())
    {
        // Prova tutte le zone: la prima che contiene il punto è quella colpita
        for (const auto& zone : hb->profile->zones)
        {
            if (pointInZone(bulletPos, entityPos, zone))
            {
                return {true, zone.damageMultiplier, zone.name};
            }
        }
        return {false, 1.0f, ""};
    }

    // Fallback: sfera di raggio k_hitRadius
    const glm::vec3 d = entityPos - bulletPos;
    const float distSq = d.x*d.x + d.y*d.y + d.z*d.z;
    if (distSq < CombatSystem::k_hitRadius * CombatSystem::k_hitRadius)
        return {true, 1.0f, "fallback"};

    return {false, 1.0f, ""};
}

// ── Update ────────────────────────────────────────────────────────────────
void CombatSystem::update(World& world, float dt)
{
    std::vector<EntityId> toDestroy;
    const std::vector<EntityId> entities = world.getEntities();

    for (EntityId bid : entities)
    {
        auto* bullet = world.getBullet(bid);
        if (!bullet) continue;

        bullet->lifetime -= dt;
        if (bullet->lifetime <= 0.0f)
        { toDestroy.push_back(bid); continue; }

        auto* bt = world.getTransform(bid);
        if (!bt) continue;

        const glm::vec3 bulletPos = {bt->x, bt->y, bt->z};

        for (EntityId eid : entities)
        {
            if (eid == bid) continue;
            if (world.getBullet(eid)) continue;

            auto* team = world.getTeam(eid);
            if (!team || team->teamId == bullet->ownerTeam) continue;

            auto* et = world.getTransform(eid);
            auto* eh = world.getHealth(eid);
            if (!et || !eh || eh->current <= 0.0f) continue;

            const glm::vec3 entityPos = {et->x, et->y, et->z};
            const auto*     hb        = world.getHitbox(eid);

            auto result = testHit(bulletPos, entityPos, hb);
            if (!result.hit) continue;

            // Danno con moltiplicatore zona
            const float finalDamage = bullet->damage * result.damageMultiplier;
            eh->current -= finalDamage;

            if (!result.zoneName.empty() && result.zoneName != "fallback")
            {
                std::cout << "[Combat] Colpito: " << result.zoneName
                          << " x" << result.damageMultiplier
                          << " — danno: " << (int)finalDamage
                          << " | HP: " << (int)eh->current
                          << "/" << (int)eh->max << std::endl;
            }
            else
            {
                std::cout << "[Combat] Colpito! HP: "
                          << (int)eh->current << "/" << (int)eh->max << std::endl;
            }

            toDestroy.push_back(bid);

            if (eh->current <= 0.0f)
            {
                std::cout << "[Combat] Bersaglio eliminato!" << std::endl;
                toDestroy.push_back(eid);
            }
            break;
        }
    }

    for (EntityId id : toDestroy)
        world.destroyEntity(id);
}

} // namespace mini
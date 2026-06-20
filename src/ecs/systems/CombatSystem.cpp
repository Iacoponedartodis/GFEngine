#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"

#include <iostream>
#include <vector>

namespace mini
{

void CombatSystem::update(World& world, float dt)
{
    std::vector<EntityId> toDestroy;
    // Copia la lista entita' per evitare iterator invalidation
    const std::vector<EntityId> entities = world.getEntities();

    for (EntityId bid : entities)
    {
        auto* bullet = world.getBullet(bid);
        if (!bullet) continue;

        // Aggiorna lifetime
        bullet->lifetime -= dt;
        if (bullet->lifetime <= 0.0f)
        {
            toDestroy.push_back(bid);
            continue;
        }

        auto* bt = world.getTransform(bid);
        if (!bt) continue;

        // Controlla collisioni con potenziali bersagli
        for (EntityId eid : entities)
        {
            if (eid == bid) continue;
            if (world.getBullet(eid)) continue;  // non colpire altri proiettili

            auto* team = world.getTeam(eid);
            if (!team || team->teamId == bullet->ownerTeam) continue;  // stesso team: skip

            auto* et = world.getTransform(eid);
            auto* eh = world.getHealth(eid);
            if (!et || !eh) continue;

            // Distanza al quadrato (evita sqrt)
            const float dx = et->x - bt->x;
            const float dy = et->y - bt->y;
            const float dz = et->z - bt->z;
            const float distSq = dx*dx + dy*dy + dz*dz;

            if (distSq < k_hitRadius * k_hitRadius)
            {
                eh->current -= bullet->damage;
                std::cout << "[Combat] Colpito! HP rimasti: "
                          << eh->current << "/" << eh->max << std::endl;

                toDestroy.push_back(bid);  // distruggi il proiettile

                if (eh->current <= 0.0f)
                {
                    std::cout << "[Combat] Bersaglio eliminato!" << std::endl;
                    toDestroy.push_back(eid);
                }
                break;  // un proiettile colpisce un solo bersaglio
            }
        }
    }

    // Distruzione in batch per evitare invalidazione
    for (EntityId id : toDestroy)
        world.destroyEntity(id);
}

} // namespace mini
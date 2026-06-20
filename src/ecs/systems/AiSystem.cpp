#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace mini
{

void AiSystem::update(World& world, float dt)
{
    // Copia la lista entita' PRIMA di spawnar proiettili (evita iterator invalidation)
    const std::vector<EntityId> snapshot = world.getEntities();

    // Raccoglie tutti i bersagli di team 1 (il giocatore e alleati)
    std::vector<EntityId> targets;
    for (EntityId e : snapshot)
    {
        auto* t = world.getTeam(e);
        if (!t || t->teamId != 1) continue;
        if (world.getBullet(e))  continue; // salta i proiettili
        if (world.getAi(e))     continue; // salta altri AI
        targets.push_back(e);
    }
    if (targets.empty()) return;

    for (EntityId e : snapshot)
    {
        auto* ai   = world.getAi(e);
        if (!ai) continue;

        auto* et   = world.getTransform(e);
        auto* team = world.getTeam(e);
        if (!et || !team) continue;

        // Aggiorna cooldown
        if (ai->shootCooldown > 0.0f) { ai->shootCooldown -= dt; continue; }

        // Trova il bersaglio piu' vicino nel raggio di aggro
        EntityId nearest   = 0;
        float    minDistSq = ai->aggroRange * ai->aggroRange;

        for (EntityId t : targets)
        {
            auto* tt = world.getTransform(t);
            if (!tt) continue;
            const float dx = tt->x - et->x;
            const float dy = tt->y - et->y;
            const float dz = tt->z - et->z;
            const float dSq = dx*dx + dy*dy + dz*dz;
            if (dSq < minDistSq) { minDistSq = dSq; nearest = t; }
        }

        if (nearest == 0) continue; // nessun bersaglio in range

        auto* tt = world.getTransform(nearest);
        if (!tt) continue;

        // Calcola direzione verso il bersaglio
        const float dx  = tt->x - et->x;
        const float dy  = tt->y - et->y;
        const float dz  = tt->z - et->z;
        const float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 0.001f) continue;

        const float inv = k_bulletSpeed / len;

        // Spawna proiettile
        const EntityId bullet = world.createEntity();
        world.addTransform(bullet, TransformComponent{
            .x = et->x, .y = et->y, .z = et->z,
            .sx = 0.10f, .sy = 0.10f, .sz = 0.10f
        });
        world.addVelocity(bullet, {dx * inv, dy * inv, dz * inv});
        world.addTeam(bullet,   {team->teamId});
        world.addBullet(bullet, {k_bulletDmg, k_bulletLife, team->teamId});

        if (ai->bulletMesh)
            world.addMeshRenderer(bullet, {
                ai->bulletMesh, ai->bulletTexture,
                ai->bulletR, ai->bulletG, ai->bulletB
            });

        ai->shootCooldown = ai->shootInterval;
        std::cout << "[AI] Fuoco dal nemico!" << std::endl;
    }
}

} // namespace mini
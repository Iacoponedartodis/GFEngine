#include "mini/game/game_modes/ConquestMode.hpp"

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"
#include "mini/ecs/World.hpp"

#include <iostream>

namespace mini
{
    void ConquestMode::start(World& world)
    {
        std::cout << "[ConquestMode] Modalita avviata." << std::endl;
        world.initialize();

        const EntityId player = world.createEntity();
        world.addTransform(player, TransformComponent{0.0f, 0.0f});
        world.addTeam(player, TeamComponent{1});
        world.addVelocity(player, VelocityComponent{10.0f, 0.0f});

        const EntityId enemy = world.createEntity();
        world.addTransform(enemy, TransformComponent{25.0f, 10.0f});
        world.addTeam(enemy, TeamComponent{2});
        world.addVelocity(enemy, VelocityComponent{10.0f, 0.0f});

        std::cout << "[ConquestMode] Spawn iniziale completato." << std::endl;
    }

    void ConquestMode::update(World& world, float deltaTime)
    {
        std::cout << "[ConquestMode] Aggiornamento modalita." << std::endl;
        world.tick(deltaTime);
    }
}
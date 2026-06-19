#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"
#include "mini/ecs/World.hpp"

#include <iostream>

namespace mini
{

void ConquestMode::start(World& world, Mesh* mesh, Texture* tex)
{
    std::cout << "[ConquestMode] Modalita avviata." << std::endl;
    world.initialize();

    // --- Player (team 1) — blu, si muove verso +X ---
    const EntityId player = world.createEntity();
    world.addTransform(player, {-3.0f, 0.0f, 0.0f});
    world.addTeam(player,      {1});
    world.addVelocity(player,  {1.5f, 0.0f, 0.0f});
    world.addHealth(player,    {100.0f, 100.0f});
    world.addMeshRenderer(player, {mesh, tex, 0.4f, 0.65f, 1.0f}); // blu clone

    // --- Enemy (team 2) — rosso, statico ---
    const EntityId enemy = world.createEntity();
    world.addTransform(enemy, {3.0f, 0.0f, 2.0f, 0.0f, 45.0f, 0.0f}); // ruotato 45 gradi
    world.addTeam(enemy,      {2});
    world.addHealth(enemy,    {80.0f, 80.0f});
    world.addMeshRenderer(enemy, {mesh, tex, 1.0f, 0.25f, 0.15f}); // rosso nemico

    // --- Cubi ambiente (grigi, disposti in scena) ---
    const float positions[][3] = {
        { 0.0f, -1.5f,  0.0f},  // piattaforma centrale, appiattita
        {-2.0f,  0.0f, -3.0f},  // copertura sinistra
        { 2.0f,  0.0f, -3.0f},  // copertura destra
    };
    const float rotations[][3] = {
        {0, 0, 0}, {0, 20, 0}, {0, -20, 0}
    };
    const float scales[][3] = {
        {4, 0.2f, 4}, {1,1,1}, {1,1,1}
    };

    for (int i = 0; i < 3; ++i)
    {
        const EntityId cube = world.createEntity();
        world.addTransform(cube, {
            positions[i][0], positions[i][1], positions[i][2],
            rotations[i][0], rotations[i][1], rotations[i][2],
            scales[i][0],    scales[i][1],    scales[i][2]
        });
        world.addMeshRenderer(cube, {mesh, tex, 0.65f, 0.65f, 0.65f}); // grigio
    }

    std::cout << "[ConquestMode] Spawn completato." << std::endl;
}

void ConquestMode::update(World& world, float deltaTime)
{
    world.tick(deltaTime);
}

} // namespace mini
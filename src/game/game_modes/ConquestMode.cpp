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
 
    // Player (team 1) — posizione origine, si muove lungo X
    const EntityId player = world.createEntity();
    world.addTransform(player, TransformComponent{0.0f, 0.0f, 0.0f});
    world.addTeam(player,      TeamComponent{1});
    world.addVelocity(player,  VelocityComponent{10.0f, 0.0f, 0.0f});
    world.addHealth(player,    HealthComponent{100.0f, 100.0f});
 
    // Enemy (team 2) — posizionato a distanza, si muove verso il player
    const EntityId enemy = world.createEntity();
    world.addTransform(enemy, TransformComponent{25.0f, 0.0f, 10.0f});
    world.addTeam(enemy,      TeamComponent{2});
    world.addVelocity(enemy,  VelocityComponent{-5.0f, 0.0f, -2.0f});
    world.addHealth(enemy,    HealthComponent{80.0f, 80.0f});
 
    std::cout << "[ConquestMode] Spawn iniziale completato." << std::endl;
}
 
void ConquestMode::update(World& world, float deltaTime)
{
    // Nessun print qui: siamo in un loop a 60Hz,
    // stampare ogni frame sarebbe spam inutile.
    world.tick(deltaTime);
}
 
} // namespace mini
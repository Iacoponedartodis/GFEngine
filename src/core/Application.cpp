#include "mini/core/Application.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
 
#include <iostream>
#include <memory>
 
namespace mini
{
 
void Application::initialize()
{
    std::cout << "[Application] Inizializzazione GFEngine..." << std::endl;
}
 
void Application::shutdown()
{
    std::cout << "[Application] Arresto GFEngine." << std::endl;
}
 
void Application::run()
{
    initialize();
 
    World world;
    world.setDebugLogging(true);
 
    // I sistemi vengono registrati qui in Application, non dentro World.
    // World e' neutro: non sa quali sistemi esistono.
    world.registerSystem(std::make_unique<MovementSystem>());
 
    ConquestMode conquestMode;
    conquestMode.start(world);
 
    constexpr int   maxTicks  = 5;
    constexpr float deltaTime = 1.0f / 60.0f;
 
    for (int tick = 0; tick < maxTicks; ++tick)
    {
        std::cout << "[Application] Tick #" << tick << std::endl;
        conquestMode.update(world, deltaTime);
    }
 
    shutdown();
}
 
} // namespace mini
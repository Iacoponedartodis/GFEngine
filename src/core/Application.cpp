#include "mini/core/Application.hpp"

#include "mini/ecs/World.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"

#include <iostream>

namespace mini
{
    void Application::run()
    {
        std::cout << "[Application] Avvio GFEngine..." << std::endl;

        World world;
        ConquestMode conquestMode;

        conquestMode.start(world);

        constexpr int maxTicks = 5;

        for (int tick = 0; tick < maxTicks; ++tick)
        {
            std::cout << "[Application] Tick #" << tick << std::endl;
            conquestMode.update(world, 1.0f / 60.0f);
        }

        std::cout << "[Application] Arresto GFEngine." << std::endl;
    }
}
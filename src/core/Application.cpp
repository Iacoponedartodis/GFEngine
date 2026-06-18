#include "mini/core/Application.hpp"
#include "mini/core/Clock.hpp"
#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"

#include <SDL2/SDL.h>
#include <iostream>
#include <memory>

namespace mini
{

void Application::initialize()
{
    std::cout << "[Application] Inizializzazione GFEngine..." << std::endl;
    m_running = true;
}

void Application::shutdown()
{
    m_running = false;
    std::cout << "[Application] Arresto GFEngine." << std::endl;
}

void Application::requestShutdown()
{
    m_running = false;
}

void Application::processEvents(Window& window)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                window.close();
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    window.close();
                break;
            default:
                break;
        }
    }
}

void Application::run()
{
    initialize();

    Window   window({"GFEngine v0.1", 1280, 720, true});
    Renderer renderer(window);

    World world;
    world.registerSystem(std::make_unique<MovementSystem>());

    ConquestMode conquestMode;
    conquestMode.start(world);

    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator       = 0.0f;
    Clock clock;

    std::cout << "[Application] Game loop avviato. ESC o X per chiudere." << std::endl;

    while (m_running && window.isOpen())
    {
        // Input
        processEvents(window);

        // Update fisico (60 Hz deterministici)
        float elapsed = clock.restart();
        if (elapsed > 0.25f) elapsed = 0.25f;
        accumulator += elapsed;
        while (accumulator >= fixedDt)
        {
            conquestMode.update(world, fixedDt);
            accumulator -= fixedDt;
        }

        // Render
        renderer.beginFrame({0.04f, 0.04f, 0.10f, 1.0f});
        renderer.render();
        renderer.endFrame();
    }

    std::cout << "[Application] Tick totali: " << world.getTickCount() << std::endl;
    shutdown();
}

} // namespace mini
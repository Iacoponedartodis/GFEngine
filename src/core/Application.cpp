#include "mini/core/Application.hpp"
#include "mini/core/Clock.hpp"
#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

void Application::requestShutdown() { m_running = false; }

void Application::processEvents(Window& window)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT: window.close(); break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) window.close();
                if (event.key.keysym.sym == SDLK_TAB)
                    window.setMouseCaptured(!window.isMouseCaptured());
                break;
            default: break;
        }
    }
}

static glm::mat4 toModelMatrix(const TransformComponent& t)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(t.x, t.y, t.z));
    m = glm::rotate(m, glm::radians(t.rx), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(t.ry), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(t.rz), glm::vec3(0, 0, 1));
    m = glm::scale(m,  glm::vec3(t.sx, t.sy, t.sz));
    return m;
}

void Application::run()
{
    initialize();

    Window   window({"GFEngine v0.1", 1280, 720, true});
    Renderer renderer(window);
    window.setMouseCaptured(true);

    // ============================================================
    // Caricamento risorse
    // Tenta di caricare asset da file; se mancano usa fallback procedurali.
    // Metti i file in:  assets/textures/   e   assets/models/
    // ============================================================

    // Texture: prova a caricare da file, altrimenti usa scacchiera
    auto texOpt = Texture::loadFromFile(GFENGINE_ASSETS_DIR "/textures/default.png");
    auto albedo = texOpt
        ? std::make_unique<Texture>(std::move(*texOpt))
        : std::make_unique<Texture>(Texture::createCheckerboard(128, 16));

    // Mesh: prova a caricare un modello OBJ, altrimenti usa il cubo
    std::unique_ptr<Mesh> defaultMesh;
    auto modelOpt = Model::loadFromObj(GFENGINE_ASSETS_DIR "/models/default.obj");
    if (modelOpt && !modelOpt->isEmpty())
    {
        // Usa il primo sub-mesh del modello come mesh principale
        defaultMesh = std::make_unique<Mesh>(modelOpt->getMeshes()[0]);
        std::cout << "[Application] Usando modello OBJ." << std::endl;
    }
    else
    {
        defaultMesh = std::make_unique<Mesh>(Mesh::createCube({1.0f, 1.0f, 1.0f}));
        std::cout << "[Application] Usando mesh cubo (fallback)." << std::endl;
    }

    World world;
    world.registerSystem(std::make_unique<MovementSystem>());

    ConquestMode conquestMode;
    conquestMode.start(world, defaultMesh.get(), albedo.get());

    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator       = 0.0f;
    Clock clock;

    std::cout << "[Application] Game loop avviato." << std::endl;
    std::cout << "  WASD + mouse = FPS camera" << std::endl;
    std::cout << "  SPACE/CTRL   = su/giu'" << std::endl;
    std::cout << "  TAB          = libera/cattura mouse" << std::endl;
    std::cout << "  ESC          = esci" << std::endl;

    while (m_running && window.isOpen())
    {
        processEvents(window);

        float elapsed = clock.restart();
        if (elapsed > 0.25f) elapsed = 0.25f;
        accumulator += elapsed;
        while (accumulator >= fixedDt)
        {
            conquestMode.update(world, fixedDt);
            accumulator -= fixedDt;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Camera& cam = renderer.getCamera();
        cam.processKeyboard(
            static_cast<bool>(keys[SDL_SCANCODE_W]),
            static_cast<bool>(keys[SDL_SCANCODE_S]),
            static_cast<bool>(keys[SDL_SCANCODE_A]),
            static_cast<bool>(keys[SDL_SCANCODE_D]),
            static_cast<bool>(keys[SDL_SCANCODE_SPACE]),
            static_cast<bool>(keys[SDL_SCANCODE_LCTRL]),
            elapsed);

        int mdx = 0, mdy = 0;
        SDL_GetRelativeMouseState(&mdx, &mdy);
        if (window.isMouseCaptured())
            cam.processMouse(static_cast<float>(mdx), static_cast<float>(mdy));

        // Render ECS
        renderer.beginFrame({0.04f, 0.04f, 0.10f, 1.0f});
        for (EntityId id : world.getEntities())
        {
            const auto* t  = world.getTransform(id);
            const auto* mr = world.getMeshRenderer(id);
            if (!t || !mr || !mr->mesh || !mr->visible) continue;
            renderer.drawMesh(*mr->mesh, mr->texture,
                              toModelMatrix(*t), glm::vec3(mr->r, mr->g, mr->b));
        }
        renderer.endFrame();
    }

    std::cout << "[Application] Tick totali: " << world.getTickCount() << std::endl;
    shutdown();
}

} // namespace mini
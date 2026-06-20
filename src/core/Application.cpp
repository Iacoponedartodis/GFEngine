#include "mini/core/Application.hpp"
#include "mini/core/Clock.hpp"
#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/ecs/systems/CombatSystem.hpp"
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
void Application::shutdown()    { m_running = false; std::cout << "[Application] Arresto GFEngine." << std::endl; }
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
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT && window.isMouseCaptured())
                    m_shootRequested = true;
                break;
            default: break;
        }
    }
}

static glm::mat4 toModelMatrix(const TransformComponent& t)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(t.x, t.y, t.z));
    m = glm::rotate(m, glm::radians(t.rx), glm::vec3(1,0,0));
    m = glm::rotate(m, glm::radians(t.ry), glm::vec3(0,1,0));
    m = glm::rotate(m, glm::radians(t.rz), glm::vec3(0,0,1));
    m = glm::scale(m,  glm::vec3(t.sx, t.sy, t.sz));
    return m;
}

void Application::run()
{
    initialize();

    Window   window({"GFEngine v0.1", 1280, 720, true});
    Renderer renderer(window);
    window.setMouseCaptured(true);

    auto texOpt = Texture::loadFromFile("assets/textures/default.png");
    auto albedo = texOpt
        ? std::make_unique<Texture>(std::move(*texOpt))
        : std::make_unique<Texture>(Texture::createCheckerboard(128, 16));

    auto modelOpt = Model::loadFromObj("assets/models/default.obj");
    std::unique_ptr<Mesh> defaultMesh;
    if (modelOpt && !modelOpt->isEmpty())
        defaultMesh = std::make_unique<Mesh>(modelOpt->getMeshes()[0]);
    else
        defaultMesh = std::make_unique<Mesh>(Mesh::createCube({1.0f, 1.0f, 1.0f}));

    World world;
    // Registra MovementSystem e CombatSystem qui.
    // AiSystem viene registrato in ConquestMode::start() per evitare
    // dipendenze di include troppo profonde in Application.cpp.
    world.registerSystem(std::make_unique<MovementSystem>());
    world.registerSystem(std::make_unique<CombatSystem>());

    ConquestMode conquestMode;
    conquestMode.start(world, defaultMesh.get(), albedo.get());
    // Dopo start(): AiSystem e' gia' registrato da ConquestMode

    EntityId playerEntity = conquestMode.getPlayerEntity();
    bool     playerAlive  = true;

    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator       = 0.0f;
    Clock clock;

    std::cout << "[Application] Game loop avviato." << std::endl;
    std::cout << "  WASD + mouse = camera FPS | SPACE/CTRL = su/giu'" << std::endl;
    std::cout << "  TASTO SX     = spara!     | TAB = libera mouse | ESC = esci" << std::endl;
    std::cout << "  >> Avvicinati ai nemici (entro 14u) per farli sparare <<" << std::endl;

    while (m_running && window.isOpen())
    {
        m_shootRequested = false;
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

        // Sincronizza entita' giocatore con la camera (bersaglio per gli AI)
        if (playerAlive && world.isValidEntity(playerEntity))
        {
            auto* pt = world.getTransform(playerEntity);
            if (pt)
            {
                const glm::vec3& p = cam.getPosition();
                pt->x = p.x; pt->y = p.y; pt->z = p.z;
            }
        }
        else if (playerAlive)
        {
            playerAlive = false;
            std::cout << "\n[GAME OVER] Sei stato eliminato!" << std::endl;
            std::cout << "  (continua a volare, ESC per uscire)" << std::endl;
        }

        // Sparo giocatore
        if (m_shootRequested && playerAlive && world.isValidEntity(playerEntity))
        {
            const glm::vec3 origin  = cam.getPosition();
            const glm::vec3 forward = cam.getForward();
            constexpr float speed   = 18.0f;

            const EntityId bullet = world.createEntity();
            world.addTransform(bullet, TransformComponent{
                .x = origin.x, .y = origin.y, .z = origin.z,
                .sx = 0.12f, .sy = 0.12f, .sz = 0.12f
            });
            world.addVelocity(bullet, {forward.x*speed, forward.y*speed, forward.z*speed});
            world.addTeam(bullet,   {1});
            world.addBullet(bullet, {25.0f, 3.0f, 1});
            world.addMeshRenderer(bullet, {defaultMesh.get(), nullptr, 1.0f, 0.92f, 0.0f});
        }

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
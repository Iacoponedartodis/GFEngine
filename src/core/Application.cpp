#include "mini/core/Application.hpp"
#include "mini/core/Audio.hpp"
#include "mini/core/Clock.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/core/InputManager.hpp"
#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/Weapon.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/HUD.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <cmath>

namespace mini
{

static constexpr int ST_PAUSE = -2;
static constexpr int ST_FREE  = -1;
static constexpr int ST_PLAY  =  0;
static constexpr int ST_WIN   =  1;
static constexpr int ST_LOSE  =  2;

void Application::initialize()
{ std::cout << "[Application] Inizializzazione GFEngine..." << std::endl; m_running = true; }
void Application::shutdown()
{ m_running = false; std::cout << "[Application] Arresto GFEngine." << std::endl; }
void Application::requestShutdown() { m_running = false; }

static glm::mat4 toModelMatrix(const TransformComponent& t)
{
    glm::mat4 m = glm::translate(glm::mat4(1), {t.x, t.y, t.z});
    m = glm::rotate(m, glm::radians(t.rx), {1,0,0});
    m = glm::rotate(m, glm::radians(t.ry), {0,1,0});
    m = glm::rotate(m, glm::radians(t.rz), {0,0,1});
    return glm::scale(m, {t.sx, t.sy, t.sz});
}

static bool anyEnemyAlive(World& w)
{
    for (EntityId id : w.getEntities())
    {
        const auto* tm = w.getTeam(id);
        const auto* hp = w.getHealth(id);
        if (tm && tm->teamId == 2 && hp && hp->current > 0.0f) return true;
    }
    return false;
}

static const glm::vec3 FREE_POS  = {0.0f, 9.0f, 17.0f};
static const glm::vec3 FREE_LOOK = {0.0f, 0.5f, -1.0f};

void Application::processEvents(Window&) {}

void Application::run()
{
    using namespace config;
    initialize();

    constexpr int W = 1280, H = 720;
    Window   window({"GFEngine v0.1", W, H, true});
    Renderer renderer(window);
    window.setMouseCaptured(true);
    Audio audio;
    InputManager input;

    auto texOpt = Texture::loadFromFile("assets/textures/default.png");
    auto albedo = texOpt
        ? std::make_unique<Texture>(std::move(*texOpt))
        : std::make_unique<Texture>(Texture::createCheckerboard(128, 16));

    auto modelOpt = Model::loadFromObj("assets/models/default.obj");
    std::unique_ptr<Mesh> mesh;
    if (modelOpt && !modelOpt->isEmpty())
        mesh = std::make_unique<Mesh>(modelOpt->getMeshes()[0]);
    else
    {
        mesh = std::make_unique<Mesh>(Mesh::createCube({1,1,1}));
        std::cout << "[Application] Mesh cubo." << std::endl;
    }

    World world;
    world.registerSystem(std::make_unique<MovementSystem>());
    world.registerSystem(std::make_unique<CombatSystem>());
    world.registerSystem(std::make_unique<AiSystem>());

    HUD hud(W, H);
    ConquestMode conquestMode;
    conquestMode.start(world, mesh.get(), albedo.get());

    // ── Player state ─────────────────────────────────────────────────
    int      gameState    = ST_FREE;
    EntityId playerEntity = conquestMode.getPlayerEntity();
    float    prevHp       = 100.0f;
    float    playerVelY   = 0.0f;
    bool     onGround     = true;
    bool     stateChanged = false;

    // ── Arma giocatore ───────────────────────────────────────────────
    Weapon weapon = makeBlasterRifle();
    weapon.reset();

    const glm::vec3 PHALF = playerHalf();
    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;
    Clock clock;

    Camera& cam = renderer.getCamera();
    cam.setPosition(FREE_POS);
    cam.lookAt(FREE_LOOK);

    std::cout << "[Application] FreeRoam — WASD+mouse | ENTER=inizia | ESC=esci" << std::endl;

    auto startGame = [&]()
    {
        conquestMode.start(world, mesh.get(), albedo.get());
        playerEntity = conquestMode.getPlayerEntity();
        prevHp = 100.0f; playerVelY = 0.0f; onGround = true;
        weapon.reset();
        gameState = ST_PLAY; stateChanged = true;
        const glm::vec3 sp = conquestMode.getSpawnPos();
        cam.setPosition(sp);
        cam.lookAt({sp.x, sp.y, sp.z - 10.0f});
        window.setMouseCaptured(true);
        std::cout << "[Game] Partita iniziata — " << weapon.name << std::endl;
    };

    auto goFreeRoam = [&]()
    {
        conquestMode.start(world, mesh.get(), albedo.get());
        playerEntity = conquestMode.getPlayerEntity();
        playerVelY = 0.0f;
        gameState = ST_FREE; stateChanged = true;
        cam.setPosition(FREE_POS);
        cam.lookAt(FREE_LOOK);
        window.setMouseCaptured(true);
        std::cout << "[Game] Volo libero." << std::endl;
    };

    while (m_running && window.isOpen())
    {
        stateChanged = false;
        input.update();

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT) { window.close(); break; }
            input.processEvent(ev);
        }

        // ── Transizioni ──────────────────────────────────────────────
        if (input.isPressed(Action::Pause) && !stateChanged)
        {
            if (gameState == ST_FREE)
                window.close();
            else if (gameState == ST_PLAY)
            { gameState = ST_PAUSE; stateChanged = true; window.setMouseCaptured(false); }
            else if (gameState == ST_PAUSE)
            { gameState = ST_PLAY; stateChanged = true; window.setMouseCaptured(true); }
        }

        if (gameState == ST_PAUSE && SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q])
            window.close();

        if (gameState == ST_FREE && input.isPressed(Action::StartGame) && !stateChanged)
            startGame();

        // Selezione arma in FreeRoam (1-4)
        if (gameState == ST_FREE)
        {
            const Uint8* ks = SDL_GetKeyboardState(nullptr);
            if (ks[SDL_SCANCODE_1]) { weapon = makeBlasterRifle(); weapon.reset(); }
            if (ks[SDL_SCANCODE_2]) { weapon = makeBlasterPistol(); weapon.reset(); }
            if (ks[SDL_SCANCODE_3]) { weapon = makeHeavyBlaster(); weapon.reset(); }
            if (ks[SDL_SCANCODE_4]) { weapon = makeSniperRifle(); weapon.reset(); }
        }

        if ((gameState==ST_WIN||gameState==ST_LOSE||gameState==ST_PAUSE)
            && input.isPressed(Action::Restart) && !stateChanged)
            startGame();

        if ((gameState==ST_WIN||gameState==ST_LOSE||gameState==ST_PAUSE)
            && input.isPressed(Action::FreeRoam) && !stateChanged)
            goFreeRoam();

        // ── ECS tick ─────────────────────────────────────────────────
        float elapsed = clock.restart();
        if (elapsed > 0.25f) elapsed = 0.25f;

        if (gameState == ST_PLAY)
        {
            accumulator += elapsed;
            while (accumulator >= fixedDt)
            { conquestMode.update(world, fixedDt); accumulator -= fixedDt; }

            // Aggiorna arma (calore si raffredda anche se non spari)
            weapon.update(elapsed);
        }

        // ── Camera + Physics ─────────────────────────────────────────
        const glm::vec3 prevPos = cam.getPosition();

        if (gameState == ST_FREE || gameState == ST_PLAY)
        {
            if (window.isMouseCaptured())
                cam.processMouse((float)input.mouseDX(), (float)input.mouseDY());
        }

        if (gameState == ST_FREE)
        {
            cam.processKeyboard(
                input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
                input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
                input.isDown(Action::Jump),
                SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LCTRL], elapsed);
        }
        else if (gameState == ST_PLAY)
        {
            cam.processKeyboard(
                input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
                input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
                false, false, elapsed);
            cam.setPosition({cam.getPosition().x, prevPos.y, cam.getPosition().z});

            if (input.isDown(Action::Jump) && onGround)
            { playerVelY = JUMP_IMPULSE; onGround = false; }

            playerVelY += GRAVITY * elapsed;
            const glm::vec3 target = {cam.getPosition().x,
                                      cam.getPosition().y + playerVelY * elapsed,
                                      cam.getPosition().z};
            const glm::vec3 final_ = physics::slideMoveWithStepUp(
                prevPos, target, PHALF, world, STEP_HEIGHT);
            if (playerVelY < 0.0f && final_.y > target.y + 0.001f)
            { playerVelY = 0.0f; onGround = true; }
            cam.setPosition(final_);
        }

        // ── Game logic ───────────────────────────────────────────────
        if (gameState == ST_PLAY)
        {
            if (world.isValidEntity(playerEntity))
            {
                auto* pt = world.getTransform(playerEntity);
                if (pt)
                { const glm::vec3& p = cam.getPosition(); pt->x=p.x; pt->y=p.y; pt->z=p.z; }

                const auto* ph = world.getHealth(playerEntity);
                if (ph && ph->current < prevHp - 0.5f)
                { audio.playHit(); prevHp = ph->current; }
                else if (ph) prevHp = ph->current;
            }

            // ── Sparo: hold-to-fire con fire rate dell'arma ──────────
            // Controlla se il tasto sinistro del mouse è premuto (hold)
            const Uint32 mouseButtons = SDL_GetMouseState(nullptr, nullptr);
            const bool mouseHeld = (mouseButtons & SDL_BUTTON_LMASK) != 0;

            if (mouseHeld && window.isMouseCaptured()
                && world.isValidEntity(playerEntity))
            {
                if (weapon.tryFire())
                {
                    audio.playShoot();
                    const glm::vec3 org = cam.getPosition();
                    const glm::vec3 fwd = cam.getForward();
                    EntityId b = world.createEntity();
                    world.addTransform(b, TransformComponent{
                        .x=org.x, .y=org.y, .z=org.z,
                        .sx=weapon.bulletScale, .sy=weapon.bulletScale, .sz=weapon.bulletScale
                    });
                    world.addVelocity(b, {fwd.x*weapon.bulletSpeed,
                                          fwd.y*weapon.bulletSpeed,
                                          fwd.z*weapon.bulletSpeed});
                    world.addTeam(b, {1});
                    world.addBullet(b, {weapon.bulletDamage, weapon.bulletLifetime, 1});
                    world.addMeshRenderer(b, {mesh.get(), nullptr,
                                              weapon.bulletR, weapon.bulletG, weapon.bulletB});
                }
            }

            // Win: nemici a 0 ticket E nessun nemico vivo E nessuno in respawn
            if (world.getTickCount() > 10
                && conquestMode.getTeam2Tickets() <= 0
                && !anyEnemyAlive(world))
            {
                gameState = ST_WIN; audio.playVictory();
                std::cout << "\n[Game] VITTORIA! Tutti i rinforzi nemici esauriti." << std::endl;
            }
            else if (!world.isValidEntity(playerEntity))
            {
                gameState = ST_LOSE; audio.playGameOver();
                std::cout << "\n[Game] GAME OVER!" << std::endl;
            }
        }

        // ── Render ───────────────────────────────────────────────────
        renderer.beginFrame({0.05f, 0.06f, 0.12f, 1.0f});
        for (EntityId id : world.getEntities())
        {
            const auto* t  = world.getTransform(id);
            const auto* mr = world.getMeshRenderer(id);
            if (!t || !mr || !mr->mesh || !mr->visible) continue;
            renderer.drawMesh(*mr->mesh, mr->texture, toModelMatrix(*t), {mr->r, mr->g, mr->b});
        }

        float curHp = 100.0f, maxHp = 100.0f;
        if (world.isValidEntity(playerEntity))
        {
            const auto* ph = world.getHealth(playerEntity);
            if (ph) { curHp = ph->current; maxHp = ph->max; }
        }
        hud.render(curHp, maxHp, gameState,
                   weapon.heat, weapon.overheated, weapon.name,
                   conquestMode.getTeam1Tickets(),
                   conquestMode.getTeam2Tickets());

        renderer.endFrame();
    }

    std::cout << "[Application] Tick: " << world.getTickCount() << std::endl;
    shutdown();
}

} // namespace mini
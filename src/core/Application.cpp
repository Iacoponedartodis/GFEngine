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
#include "mini/game/MatchSettings.hpp"
#include "mini/game/Weapon.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/HUD.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/OptionsMenu.hpp"
#include "mini/render/PreMatchMenu.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <cmath>

namespace mini
{

static constexpr int ST_PREMATCH = -4;
static constexpr int ST_OPTIONS  = -3;
static constexpr int ST_PAUSE    = -2;
static constexpr int ST_FREE     = -1;
static constexpr int ST_PLAY     =  0;
static constexpr int ST_WIN      =  1;
static constexpr int ST_LOSE     =  2;

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

    HUD          hud(W, H);
    OptionsMenu  optMenu(W, H);
    PreMatchMenu preMatchMenu(W, H);
    MatchSettings currentSettings;

    ConquestMode conquestMode;
    conquestMode.applySettings(currentSettings);
    conquestMode.start(world, mesh.get(), albedo.get());

    int      gameState    = ST_FREE;
    int      prevState    = ST_FREE;
    EntityId playerEntity = conquestMode.getPlayerEntity();
    float    prevHp       = 100.0f;
    float    playerVelY   = 0.0f;
    bool     onGround     = true;
    float    airVelX      = 0.0f;
    float    airVelZ      = 0.0f;
    bool     stateChanged = false;

    float playerRespawnTimer = -1.0f;
    bool  playerIsDead       = false;

    Weapon weapon = makeBlasterRifle();
    weapon.reset();
    bool wasOverheated = false;

    const glm::vec3 PHALF = playerHalf();
    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;
    Clock clock;

    Camera& cam = renderer.getCamera();
    cam.setPosition(FREE_POS);
    cam.lookAt(FREE_LOOK);

    std::cout << "[Application] FreeRoam — WASD+mouse | ENTER=menu partita | O=opzioni | ESC=esci"
              << std::endl;

    auto startGame = [&]()
    {
        conquestMode.applySettings(currentSettings);
        conquestMode.start(world, mesh.get(), albedo.get());
        playerEntity = conquestMode.getPlayerEntity();
        prevHp = currentSettings.playerHp;
        playerVelY = 0.0f;
        onGround = true;
        playerRespawnTimer = -1.0f;
        playerIsDead = false;
        weapon.reset();
        gameState = ST_PLAY;
        stateChanged = true;
        const glm::vec3 sp = conquestMode.getSpawnPos();
        cam.setPosition(sp);
        cam.lookAt({sp.x, sp.y, sp.z - 10.0f});
        window.setMouseCaptured(true);
        std::cout << "[Game] Partita iniziata — " << weapon.name << std::endl;
    };

    auto goFreeRoam = [&]()
    {
        conquestMode.applySettings(currentSettings);
        conquestMode.start(world, mesh.get(), albedo.get());
        playerEntity = conquestMode.getPlayerEntity();
        playerVelY = 0.0f;
        playerRespawnTimer = -1.0f;
        playerIsDead = false;
        gameState = ST_FREE;
        stateChanged = true;
        cam.setPosition(FREE_POS);
        cam.lookAt(FREE_LOOK);
        window.setMouseCaptured(true);
        std::cout << "[Game] Volo libero." << std::endl;
    };

    // ── Game loop ─────────────────────────────────────────────────────
    while (m_running && window.isOpen())
    {
        stateChanged = false;
        input.update();

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT) { window.close(); break; }
            input.processEvent(ev);

            if (ev.type == SDL_TEXTINPUT)
            {
                if (gameState == ST_PREMATCH)
                    preMatchMenu.handleTextInput(ev.text.text);
                // OptionsMenu non usa text input (solo keybinding)
            }

            if (ev.type == SDL_KEYDOWN)
            {
                const int sc = ev.key.keysym.scancode;

                // ── Menu pre-partita ─────────────────────────────────
                if (gameState == ST_PREMATCH)
                {
                    auto res = preMatchMenu.handleKey(sc);
                    if (res == PreMatchMenu::Result::StartGame)
                    {
                        currentSettings = preMatchMenu.getSettings();
                        startGame();
                    }
                    else if (res == PreMatchMenu::Result::Back)
                    {
                        gameState = ST_FREE;
                        stateChanged = true;
                        window.setMouseCaptured(true);
                    }
                }
                // ── Menu opzioni (O) ─────────────────────────────────
                else if (gameState == ST_OPTIONS)
                {
                    auto res = optMenu.handleKey(sc, input);
                    if (res == OptionsMenu::Result::Back)
                    {
                        gameState = prevState;
                        stateChanged = true;
                        if (gameState == ST_PLAY || gameState == ST_FREE)
                            window.setMouseCaptured(true);
                    }
                }
                else
                {
                    // ENTER in FreeRoam → apri menu pre-partita
                    if (sc == SDL_SCANCODE_RETURN && gameState == ST_FREE && !stateChanged)
                    {
                        preMatchMenu.setSettings(currentSettings);
                        gameState = ST_PREMATCH;
                        stateChanged = true;
                        window.setMouseCaptured(false);
                    }

                    // O → opzioni
                    if (sc == SDL_SCANCODE_O && !stateChanged
                        && (gameState == ST_FREE || gameState == ST_PAUSE))
                    {
                        prevState = gameState;
                        gameState = ST_OPTIONS;
                        stateChanged = true;
                        window.setMouseCaptured(false);
                        std::cout << "[Options] Menu aperto." << std::endl;
                    }
                }
            }
        }

        // ── Transizioni stato ─────────────────────────────────────────
        if (gameState != ST_PREMATCH && gameState != ST_OPTIONS)
        {
            const Uint8* ks = SDL_GetKeyboardState(nullptr);

            if (input.isPressed(Action::Pause) && !stateChanged)
            {
                if (gameState == ST_FREE)
                    window.close();
                else if (gameState == ST_PLAY)
                { gameState = ST_PAUSE; stateChanged = true; window.setMouseCaptured(false); }
                else if (gameState == ST_PAUSE)
                { gameState = ST_PLAY; stateChanged = true; window.setMouseCaptured(true); }
            }

            if (gameState == ST_PAUSE && ks[SDL_SCANCODE_Q])
                window.close();

            if (gameState == ST_FREE)
            {
                if (ks[SDL_SCANCODE_1]) { weapon = makeBlasterRifle();  weapon.reset(); }
                if (ks[SDL_SCANCODE_2]) { weapon = makeBlasterPistol(); weapon.reset(); }
                if (ks[SDL_SCANCODE_3]) { weapon = makeHeavyBlaster();  weapon.reset(); }
                if (ks[SDL_SCANCODE_4]) { weapon = makeSniperRifle();   weapon.reset(); }
            }

            if ((gameState == ST_WIN || gameState == ST_LOSE || gameState == ST_PAUSE)
                && input.isPressed(Action::Restart) && !stateChanged)
                startGame();

            if ((gameState == ST_WIN || gameState == ST_LOSE || gameState == ST_PAUSE)
                && input.isPressed(Action::FreeRoam) && !stateChanged)
                goFreeRoam();
        }

        // ── ECS tick ──────────────────────────────────────────────────
        float elapsed = clock.restart();
        if (elapsed > 0.25f) elapsed = 0.25f;

        if (gameState == ST_PLAY)
        {
            accumulator += elapsed;
            while (accumulator >= fixedDt)
            { conquestMode.update(world, fixedDt); accumulator -= fixedDt; }
            weapon.update(elapsed);

            // Suono overheat (solo al momento in cui scatta)
            if (weapon.overheated && !wasOverheated)
                audio.playOverheat();
            wasOverheated = weapon.overheated;

            // Respawn timer giocatore
            if (playerRespawnTimer > 0.0f)
            {
                playerRespawnTimer -= elapsed;
                if (playerRespawnTimer <= 0.0f)
                {
                    playerRespawnTimer = -1.0f;
                    playerIsDead = false;
                    playerEntity = world.createEntity();
                    const glm::vec3 sp = conquestMode.getSpawnPos();
                    world.addTransform(playerEntity, {sp.x, sp.y, sp.z});
                    world.addTeam(playerEntity, {1});
                    world.addHealth(playerEntity, {currentSettings.playerHp, currentSettings.playerHp});
                    conquestMode.overridePlayerEntity(playerEntity);
                    prevHp = currentSettings.playerHp;
                    playerVelY = 0.0f;
                    onGround = true;
                    cam.setPosition(sp);
                    std::cout << "[Respawn] Giocatore rinato! Ticket team 1 rimasti: "
                              << conquestMode.getTeam1Tickets() << std::endl;
                }
            }
        }

        // ── Camera + Physics ──────────────────────────────────────────
        const glm::vec3 prevPos = cam.getPosition();

        if ((gameState == ST_FREE || gameState == ST_PLAY) && window.isMouseCaptured())
            cam.processMouse((float)input.mouseDX(), (float)input.mouseDY());

        if (gameState == ST_FREE)
        {
            cam.processKeyboard(
                input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
                input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
                input.isDown(Action::Jump),
                SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LCTRL], elapsed);
        }
        else if (gameState == ST_PLAY && !playerIsDead)
        {
            glm::vec3 movePos = prevPos;

            if (onGround)
            {
                // A terra: WASD controlla il movimento normalmente
                cam.processKeyboard(
                    input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
                    input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
                    false, false, elapsed);
                cam.setPosition({cam.getPosition().x, prevPos.y, cam.getPosition().z});
                movePos = cam.getPosition();

                // Salva la velocità orizzontale corrente per il salto
                if (elapsed > 0.0001f)
                {
                    airVelX = (movePos.x - prevPos.x) / elapsed;
                    airVelZ = (movePos.z - prevPos.z) / elapsed;
                }

                // Salto
                if (input.isDown(Action::Jump))
                {
                    playerVelY = JUMP_IMPULSE;
                    onGround = false;
                }
            }
            else
            {
                // In aria: mantiene la direzione del salto, niente WASD
                // (la rotazione della telecamera resta attiva via processMouse)
                movePos = {prevPos.x + airVelX * elapsed,
                           prevPos.y,
                           prevPos.z + airVelZ * elapsed};
                cam.setPosition(movePos);
            }

            // Gravità + sliding
            playerVelY += GRAVITY * elapsed;
            const glm::vec3 target = {cam.getPosition().x,
                                      cam.getPosition().y + playerVelY * elapsed,
                                      cam.getPosition().z};
            const glm::vec3 final_ = physics::slideMoveWithStepUp(
                prevPos, target, PHALF, world, STEP_HEIGHT);
            if (playerVelY < 0.0f && final_.y > target.y + 0.001f)
            {
                playerVelY = 0.0f;
                onGround = true;
            }
            cam.setPosition(final_);
        }

        // ── Game logic ────────────────────────────────────────────────
        if (gameState == ST_PLAY)
        {
            if (!playerIsDead && world.isValidEntity(playerEntity))
            {
                auto* pt = world.getTransform(playerEntity);
                if (pt)
                {
                    const glm::vec3& p = cam.getPosition();
                    pt->x = p.x; pt->y = p.y; pt->z = p.z;
                }

                const auto* hp = world.getHealth(playerEntity);
                if (hp)
                {
                    if (hp->current <= 0.0f && prevHp > 0.0f)
                    {
                        playerIsDead = true;
                        int t1 = conquestMode.getTeam1Tickets();
                        if (t1 > 0)
                        {
                            playerRespawnTimer = currentSettings.respawnDelay;
                            std::cout << "[Respawn] Giocatore eliminato. Ticket rimasti: "
                                      << t1 << " — respawn in "
                                      << currentSettings.respawnDelay << "s" << std::endl;
                        }
                        else
                        {
                            gameState = ST_LOSE;
                            stateChanged = true;
                            window.setMouseCaptured(false);
                            std::cout << "[Game] SCONFITTA!" << std::endl;
                        }
                    }
                    prevHp = hp->current;
                }
            }

            // Shooting — crea proiettili visibili (hold-to-fire)
            if (!playerIsDead && input.isDown(Action::Shoot) && window.isMouseCaptured())
            {
                if (weapon.tryFire())
                {
                    audio.playShoot();
                    const glm::vec3 org = cam.getPosition();
                    const glm::vec3 fwd = cam.getForward();

                    EntityId b = world.createEntity();
                    world.addTransform(b, TransformComponent{
                        .x = org.x, .y = org.y, .z = org.z,
                        .sx = weapon.bulletScale, .sy = weapon.bulletScale, .sz = weapon.bulletScale
                    });
                    world.addVelocity(b, {
                        fwd.x * weapon.bulletSpeed,
                        fwd.y * weapon.bulletSpeed,
                        fwd.z * weapon.bulletSpeed
                    });
                    world.addTeam(b, {1});
                    world.addBullet(b, {weapon.bulletDamage, weapon.bulletLifetime, 1});
                    world.addMeshRenderer(b, {mesh.get(), nullptr,
                                              weapon.bulletR, weapon.bulletG, weapon.bulletB});
                }
            }
        }

        // ── Win condition ─────────────────────────────────────────────
        if (gameState == ST_PLAY
            && world.getTickCount() > 10
            && conquestMode.getTeam2Tickets() <= 0
            && !anyEnemyAlive(world))
        {
            gameState = ST_WIN;
            stateChanged = true;
            window.setMouseCaptured(false);
            audio.playVictory();
            std::cout << "\n[Game] VITTORIA! Rinforzi nemici esauriti." << std::endl;
        }

        // ── Contatori vivi ────────────────────────────────────────────────
        int aliveAllies = 0, aliveEnemies = 0;
        if (gameState == ST_PLAY)
        {
            for (EntityId id : world.getEntities())
            {
                const auto* tm = world.getTeam(id);
                const auto* hp = world.getHealth(id);
                if (!tm || !hp || hp->current <= 0 || world.getBullet(id)) continue;
                if (tm->teamId == 1) ++aliveAllies;
                else if (tm->teamId == 2) ++aliveEnemies;
            }
        }

        // ── Render ───────────────────────────────────────────────────────
        renderer.beginFrame();

        for (EntityId id : world.getEntities())
        {
            const auto* tr = world.getTransform(id);
            const auto* mr = world.getMeshRenderer(id);
            if (!tr || !mr || !mr->visible || !mr->mesh) continue;

            const glm::mat4 model = toModelMatrix(*tr);
            renderer.drawMesh(*mr->mesh, mr->texture, model,
                              {mr->r, mr->g, mr->b});
        }

        if (gameState == ST_OPTIONS)
        {
            optMenu.render(input);
        }
        else if (gameState == ST_PREMATCH)
        {
            preMatchMenu.render();
        }
        else
        {
            hud.render(prevHp, currentSettings.playerHp, gameState,
                       weapon.heat, weapon.overheated, weapon.name,
                       conquestMode.getTeam1Tickets(), conquestMode.getTeam2Tickets(),
                       aliveAllies, aliveEnemies);
        }

        renderer.endFrame();
    } // fine while (m_running && window.isOpen())
} // fine Application::run   
} // namespace mini
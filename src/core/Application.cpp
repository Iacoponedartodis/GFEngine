#include "mini/core/Application.hpp"
#include "mini/core/Audio.hpp"
#include "mini/core/Clock.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/core/GameState.hpp"
#include "mini/core/InputManager.hpp"
#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/MovementSystem.hpp"
#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/systems/AiSystem.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/MatchSettings.hpp"
#include "mini/game/PlayerController.hpp"
#include "mini/game/Weapon.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/HUD.hpp"
#include "mini/render/LauncherScreen.hpp"
#include "mini/render/MainMenuScreen.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/OptionsMenu.hpp"
#include "mini/render/PreMatchMenu.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <memory>

// ── weaponFromDef inline ─────────────────────────────────────────────────────
namespace mini {
inline Weapon weaponFromDef(const WeaponDef& def)
{
    Weapon w;
    w.name            = def.name;
    w.fireRate        = def.fireRate;
    w.bulletSpeed     = def.bulletSpeed;
    w.bulletDamage    = def.damage;
    w.bulletLifetime  = def.bulletLifetime;
    w.bulletScale     = def.bulletScale;
    w.bulletR         = def.bulletColor[0];
    w.bulletG         = def.bulletColor[1];
    w.bulletB         = def.bulletColor[2];
    w.heatPerShot     = def.heatPerShot;
    w.cooldownRate    = def.cooldownRate;
    w.overheatPenalty = def.overheatPenalty;
    return w;
}
} // namespace mini


namespace mini
{

void Application::initialize()
{ std::cout << "[Application] Inizializzazione GFEngine..." << std::endl; m_running = true; }
void Application::shutdown()
{ m_running = false; std::cout << "[Application] Arresto GFEngine." << std::endl; }
void Application::requestShutdown() { m_running = false; }
void Application::processEvents(Window&) {}

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

void Application::run(bool directPreMatch)
{
    using namespace config;
    initialize();

    // ── Definition Registry ──────────────────────────────────────────
    DefinitionRegistry registry;
    registry.loadAll("data");

    // Popola la lista armi del PreMatchMenu dal registry (armi ordinate per nome)
    {
        std::vector<PreMatchMenu::WeaponEntry> wList;
        for (auto& [id, def] : registry.weapons())
            wList.push_back({id, def.name});
        // Ordina per nome per consistenza UI
        std::sort(wList.begin(), wList.end(),
            [](const auto& a, const auto& b){ return a.name < b.name; });
        preMatchMenu.setWeaponList(wList);
    }

    constexpr int W = 1280, H = 720;
    Window   window({"GFEngine v0.1", W, H, true});
    Renderer renderer(window);
    window.setMouseCaptured(false);
    Audio audio;
    InputManager input;

    // ── Risorse ──────────────────────────────────────────────────────
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

    // ── ECS ──────────────────────────────────────────────────────────
    World world;
    world.registerSystem(std::make_unique<MovementSystem>());
    world.registerSystem(std::make_unique<CombatSystem>());
    world.registerSystem(std::make_unique<AiSystem>());

    // ── Schermate ────────────────────────────────────────────────────
    LauncherScreen   launcher(W, H);
    MainMenuScreen   mainMenu(W, H);
    HUD              hud(W, H);
    OptionsMenu      optMenu(W, H);
    PreMatchMenu     preMatchMenu(W, H);

    // ── Game mode ────────────────────────────────────────────────────
    MatchSettings currentSettings;
    ConquestMode  conquestMode;
    bool worldReady = false;

    // ── Player controller ────────────────────────────────────────────
    PlayerController player;
    player.weapon = makeBlasterRifle();

    // ── Stato ────────────────────────────────────────────────────────
    GameState state     = directPreMatch ? GameState::PreMatch : GameState::Launcher;
    GameState prevState = state;
    bool      stateChanged  = false;
    bool      wasOverheated = false;

    constexpr float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;
    Clock clock;
    Camera& cam = renderer.getCamera();

    if (directPreMatch)
        std::cout << "[Application] Direct PreMatch mode." << std::endl;
    else
        std::cout << "[Application] Launcher avviato." << std::endl;

    // ── Lambda transizioni ───────────────────────────────────────────
    auto initWorld = [&]()
    {
        conquestMode.applySettings(currentSettings);
        conquestMode.start(world, mesh.get(), albedo.get(), &registry);
        player.reset(conquestMode.getPlayerEntity(), currentSettings.playerHp,
                     conquestMode.getSpawnPos(), cam);
        worldReady = true;
    };

    auto startGame = [&]()
    {
        // Usa l'ID dell'arma selezionata dal menu (lista dinamica dal registry)
        const std::string& selectedId = preMatchMenu.getSelectedWeaponId();
        const auto* wDef = registry.getWeapon(selectedId);
        if (wDef)
            player.weapon = weaponFromDef(*wDef);
        else
        {
            std::cerr << "[Game] Arma non trovata nel registry: " << selectedId
                      << " -- fallback blaster rifle\n";
            player.weapon = makeBlasterRifle();
        }
        initWorld();
        state = GameState::Playing;
        stateChanged = true;
        window.setMouseCaptured(true);
        std::cout << "[Game] Partita iniziata — " << player.weapon.name << std::endl;
    };

    auto goMainMenu = [&]()
    {
        state = GameState::MainMenu;
        stateChanged = true;
        window.setMouseCaptured(false);
    };

    // Respawn volontario: consuma un ticket e avvia il timer come se il giocatore fosse morto
    auto doVoluntaryRespawn = [&]()
    {
        if (player.isDead) return; // già morto, respawn già in corso
        int t1 = conquestMode.getTeam1Tickets();
        if (t1 <= 0)
        {
            // Nessun ticket: game over immediato
            state = GameState::Lose;
            stateChanged = true;
            window.setMouseCaptured(false);
            std::cout << "[Game] Nessun ticket per il respawn volontario. SCONFITTA!" << std::endl;
            return;
        }
        conquestMode.consumeTeam1Ticket();
        player.isDead = true;
        player.prevHp = 0.0f;
        player.respawnTimer = currentSettings.respawnDelay;
        // Invalida l'entità corrente per simulare la morte
        if (world.isValidEntity(player.entity))
        {
            auto* hp = world.getHealth(player.entity);
            if (hp) hp->current = 0.0f;
        }
        state = GameState::Playing;
        stateChanged = true;
        window.setMouseCaptured(true);
        std::cout << "[Respawn volontario] Ticket rimasti: "
                  << conquestMode.getTeam1Tickets()
                  << " — respawn in " << currentSettings.respawnDelay << "s" << std::endl;
    };

    // ═════════════════════════════════════════════════════════════════
    // MAIN LOOP
    // ═════════════════════════════════════════════════════════════════
    while (m_running && window.isOpen())
    {
        stateChanged = false;
        input.update();

        // ── 1. EVENTI SDL ────────────────────────────────────────────
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT) { window.close(); break; }
            input.processEvent(ev);

            if (ev.type == SDL_TEXTINPUT && state == GameState::PreMatch)
                preMatchMenu.handleTextInput(ev.text.text);

            if (ev.type == SDL_KEYDOWN)
            {
                const int sc = ev.key.keysym.scancode;

                // F11 globale
                if (sc == SDL_SCANCODE_F11) window.toggleFullscreen();
                // ── Launcher ─────────────────────────────────────────
                if (state == GameState::Launcher)
                {
                    auto res = launcher.handleKey(sc);
                    if (res == LauncherScreen::Result::Launch)
                        goMainMenu();
                    else if (res == LauncherScreen::Result::Quit)
                        window.close();
                }
                // ── Main Menu ────────────────────────────────────────
                else if (state == GameState::MainMenu)
                {
                    auto res = mainMenu.handleKey(sc);
                    if (res == MainMenuScreen::Result::NewGame)
                    {
                        preMatchMenu.setSettings(currentSettings);
                        state = GameState::PreMatch; stateChanged = true;
                    }
                    else if (res == MainMenuScreen::Result::Options)
                    {
                        prevState = GameState::MainMenu;
                        state = GameState::Options; stateChanged = true;
                    }
                    else if (res == MainMenuScreen::Result::Quit)
                        window.close();
                }
                // ── PreMatch ─────────────────────────────────────────
                else if (state == GameState::PreMatch)
                {
                    auto res = preMatchMenu.handleKey(sc);
                    if (res == PreMatchMenu::Result::StartGame)
                    { currentSettings = preMatchMenu.getSettings(); startGame(); }
                    else if (res == PreMatchMenu::Result::Back)
                    { goMainMenu(); }
                }
                // ── Options ──────────────────────────────────────────
                else if (state == GameState::Options)
                {
                    auto res = optMenu.handleKey(sc, input);
                    if (res == OptionsMenu::Result::Back)
                    {
                        state = prevState; stateChanged = true;
                        if (state == GameState::Playing) window.setMouseCaptured(true);
                    }
                }
                // ── Paused: K = respawn volontario ───────────────────
                else if (state == GameState::Paused)
                {
                    if (sc == SDL_SCANCODE_K && !stateChanged)
                        doVoluntaryRespawn();
                }
            }
        }

        // ── 2. TRANSIZIONI STATO (gameplay) ──────────────────────────
        if (!isMenuState(state))
        {
            const Uint8* ks = SDL_GetKeyboardState(nullptr);

            if (input.isPressed(Action::Pause) && !stateChanged)
            {
                if (state == GameState::Playing)
                { state = GameState::Paused; stateChanged = true; window.setMouseCaptured(false); }
                else if (state == GameState::Paused)
                { state = GameState::Playing; stateChanged = true; window.setMouseCaptured(true); }
            }

            if (state == GameState::Paused)
            {
                if (ks[SDL_SCANCODE_Q]) goMainMenu();
                if (ks[SDL_SCANCODE_O] && !stateChanged)
                {
                    prevState = GameState::Paused;
                    state = GameState::Options; stateChanged = true;
                    window.setMouseCaptured(false);
                }
            }

            if (isOverlayState(state) && input.isPressed(Action::Restart) && !stateChanged)
                startGame();

            if ((state == GameState::Win || state == GameState::Lose) && ks[SDL_SCANCODE_Q])
                goMainMenu();
        }

        // ── 3. FIXED UPDATE ──────────────────────────────────────────
        float elapsed = clock.restart();
        if (elapsed > 0.25f) elapsed = 0.25f;

        if (state == GameState::Playing)
        {
            accumulator += elapsed;
            while (accumulator >= fixedDt)
            { conquestMode.update(world, fixedDt); world.tick(fixedDt); accumulator -= fixedDt; }

            player.weapon.update(elapsed);
            if (player.weapon.overheated && !wasOverheated) audio.playOverheat();
            wasOverheated = player.weapon.overheated;

            if (player.respawnTimer > 0.0f)
            {
                player.respawnTimer -= elapsed;
                if (player.respawnTimer <= 0.0f)
                {
                    player.updateRespawn(world, cam, currentSettings.respawnDelay,
                                          conquestMode.getSpawnPos(), currentSettings.playerHp);
                    conquestMode.overridePlayerEntity(player.entity);
                }
            }
        }

        // ── 4. CAMERA + PHYSICS ──────────────────────────────────────
        if (state == GameState::Playing && window.isMouseCaptured())
            cam.processMouse((float)input.mouseDX(), (float)input.mouseDY());

        if (state == GameState::Playing)
            player.updateMovement(cam, input, world, elapsed);

        // ── 5. GAME LOGIC ────────────────────────────────────────────
        if (state == GameState::Playing)
        {
            if (!player.isDead && world.isValidEntity(player.entity))
            {
                auto* pt = world.getTransform(player.entity);
                if (pt)
                { const glm::vec3& p = cam.getPosition(); pt->x=p.x; pt->y=p.y; pt->z=p.z; }
            }

            if (!player.isDead && !world.isValidEntity(player.entity))
            {
                player.isDead = true;
                player.prevHp = 0.0f;
                int t1 = conquestMode.getTeam1Tickets();
                if (t1 > 0)
                {
                    conquestMode.consumeTeam1Ticket();
                    player.respawnTimer = currentSettings.respawnDelay;
                    std::cout << "[Game] Eliminato! Respawn in "
                              << currentSettings.respawnDelay << "s" << std::endl;
                }
                else
                {
                    state = GameState::Lose; stateChanged = true;
                    window.setMouseCaptured(false);
                    std::cout << "[Game] SCONFITTA!" << std::endl;
                }
            }

            if (player.updateHealth(world, audio))
            {
                int t1 = conquestMode.getTeam1Tickets();
                if (t1 > 0)
                {
                    conquestMode.consumeTeam1Ticket();
                    player.respawnTimer = currentSettings.respawnDelay;
                }
                else
                {
                    state = GameState::Lose; stateChanged = true;
                    window.setMouseCaptured(false);
                }
            }

            player.updateShooting(world, cam, input, audio,
                                   mesh.get(), window.isMouseCaptured());

            if (world.getTickCount() > 10
                && conquestMode.getTeam2Tickets() <= 0
                && !anyEnemyAlive(world))
            {
                state = GameState::Win; stateChanged = true;
                window.setMouseCaptured(false);
                audio.playVictory();
                std::cout << "\n[Game] VITTORIA!" << std::endl;
            }
        }

        // ── 6. RENDER ────────────────────────────────────────────────
        renderer.beginFrame();

        if (worldReady && state == GameState::Playing)
        {
            for (EntityId id : world.getEntities())
            {
                const auto* tr = world.getTransform(id);
                const auto* mr = world.getMeshRenderer(id);
                if (!tr || !mr || !mr->visible || !mr->mesh) continue;
                renderer.drawMesh(*mr->mesh, mr->texture, toModelMatrix(*tr),
                                  {mr->r, mr->g, mr->b});
            }
        }

        if (state == GameState::Launcher)
            launcher.render();
        else if (state == GameState::MainMenu)
            mainMenu.render();
        else if (state == GameState::Options)
            optMenu.render(input);
        else if (state == GameState::PreMatch)
            preMatchMenu.render();
        else
        {
            int aliveAllies = 0, aliveEnemies = 0;
            if (worldReady)
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
            hud.render(player.prevHp, currentSettings.playerHp, (int)state,
                       player.weapon.heat, player.weapon.overheated, player.weapon.name.c_str(),
                       conquestMode.getTeam1Tickets(), conquestMode.getTeam2Tickets(),
                       aliveAllies, aliveEnemies);
        }

        renderer.endFrame();
    }
}

} // namespace mini
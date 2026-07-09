#include "mini/core/Application.hpp"
#include "mini/core/Telemetry.hpp"
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
#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/CommandPosts.hpp"
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
#include <nlohmann/json.hpp>
#include <filesystem>
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

// Restituisce il percorso alla cartella data/ sorgente del progetto.
// Prima prova 3 livelli su dall'exe (build/config/Debug -> project root),
// identico alla logica usata dal Balance Editor.
// Fallback: data/ accanto all'exe (copia da CMake).
static std::string getDataPath()
{
    char* base = SDL_GetBasePath();
    std::filesystem::path exeDir = base ? base : ".";
    SDL_free(base);

    std::error_code ec;
    std::filesystem::path sourceData = std::filesystem::canonical(exeDir / "../../../data", ec);
    if (!ec && std::filesystem::exists(sourceData / "enemies", ec))
        return sourceData.string();

    return (exeDir / "data").string();
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

// Ray vs AABB (slab test) per il feedback di mira sul mirino.
static bool rayAABB(const glm::vec3& o, const glm::vec3& d,
                    const glm::vec3& mn, const glm::vec3& mx, float maxT)
{
    float tmin = 0.0f, tmax = maxT;
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(d[i]) < 1e-6f)
        {
            if (o[i] < mn[i] || o[i] > mx[i]) return false;
        }
        else
        {
            float inv = 1.0f / d[i];
            float t0 = (mn[i] - o[i]) * inv;
            float t1 = (mx[i] - o[i]) * inv;
            if (t0 > t1) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmin > tmax) return false;
        }
    }
    return true;
}

// (la verifica "nemici vivi" ora vive nelle regole dei mode — ADR-014)

void Application::run(bool directPreMatch, bool sandbox)
{
    using namespace config;
    initialize();

    // ── Telemetria (ADR-013): logger + crash net + input recorder ─────
    telemetry::init("GFEngine");
    telemetry::logInfo(std::string("flag avvio: directPreMatch=")
        + (directPreMatch ? "si" : "no") + " sandbox=" + (sandbox ? "si" : "no"));

    // ── Definition Registry ──────────────────────────────────────────
    // Usa percorso assoluto basato sull'exe, non CWD
    DefinitionRegistry registry;
    const std::string dataPath = getDataPath();
    registry.loadAll(dataPath);
    telemetry::logInfo("registry caricato da '" + dataPath + "': "
        + std::to_string(registry.weapons().size()) + " armi, "
        + std::to_string(registry.enemies().size()) + " nemici, "
        + std::to_string(registry.allies().size()) + " alleati, "
        + std::to_string(registry.maps().size()) + " mappe");

    // Popola la lista armi del PreMatchMenu — solo Republic e Neutral (non Separatist)
    std::vector<PreMatchMenu::WeaponEntry> wList;
    for (auto& [id, def] : registry.weapons())
        if (def.faction != mini::Faction::Separatist)
            wList.push_back({id, def.name});
    std::sort(wList.begin(), wList.end(),
        [](const auto& a, const auto& b){ return a.name < b.name; });

    // Sandbox: lista COMPLETA delle armi (anche separatiste) per il banco di
    // prova — tasti 1-9 in gioco (Todo #0).
    std::vector<std::pair<std::string, std::string>> testWeapons; // id, name
    for (auto& [id, def] : registry.weapons())
        testWeapons.push_back({id, def.name});
    std::sort(testWeapons.begin(), testWeapons.end(),
        [](const auto& a, const auto& b){ return a.second < b.second; });

    std::vector<PreMatchMenu::AbilityEntry> aList;
    for (auto& [id, def] : registry.abilities())
        aList.push_back({id, def.name, def.type});
    std::sort(aList.begin(), aList.end(),
        [](const auto& a, const auto& b){ return a.name < b.name; });

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
        mesh = std::make_unique<Mesh>(*modelOpt->merged());
    else
    {
        mesh = std::make_unique<Mesh>(Mesh::createCube({1,1,1}));
        std::cout << "[Application] Mesh cubo." << std::endl;
    }

    // ── Mesh cache per modelli nemici/alleati ────────────────────────
    // Supporta .obj, .gltf e .glb — sceglie il loader in base all'estensione.
    // I puntatori sono stabili per tutta la vita di Application::run().
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshStore;
    MeshCache meshCache;

    auto loadMeshIntoCache = [&](const std::string& path)
    {
        if (path.empty() || meshCache.count(path)) return;
        std::optional<Model> mOpt;
        const bool isGltf = (path.size() >= 5 && path.substr(path.size()-5) == ".gltf")
                          || (path.size() >= 4 && path.substr(path.size()-4) == ".glb");
        if (isGltf)
            mOpt = Model::loadFromGltf(path.c_str());
        else
            mOpt = Model::loadFromObj(path.c_str());

        if (mOpt && !mOpt->isEmpty())
        {
            auto m = std::make_unique<Mesh>(*mOpt->merged());
            meshCache[path] = m.get();
            meshStore[path] = std::move(m);
        }
        else
        {
            std::cerr << "[MeshCache] Impossibile caricare: " << path << "\n";
        }
    };

    for (auto& [id, enemy] : registry.enemies()) loadMeshIntoCache(enemy.meshPath);
    for (auto& [id, ally]  : registry.allies())  loadMeshIntoCache(ally.meshPath);
    for (auto& [id, wpn]   : registry.weapons()) loadMeshIntoCache(wpn.meshPath);

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
    preMatchMenu.setWeaponList(wList);
    preMatchMenu.setAbilityList(aList);

    // ── Game mode (ADR-008: Application parla solo con IGameMode) ─────
    MatchSettings currentSettings;
    std::unique_ptr<IGameMode> mode =
        createGameMode(sandbox ? "sandbox" : "conquest");
    telemetry::logInfo(std::string("game mode creato: ")
        + (sandbox ? "sandbox" : "conquest"));
    bool worldReady = false;

    // Spike ADR-011: F9 attiva una seconda vista della stessa scena
    // (metà destra, camera offset). Solo verifica di fattibilità.
    bool splitSpike = false;

    // ── Player controller ────────────────────────────────────────────
    PlayerController player;
    player.weapon = makeBlasterRifle();

    // ── Stato ────────────────────────────────────────────────────────
    GameState state     = sandbox        ? GameState::Playing
                        : directPreMatch ? GameState::PreMatch
                                         : GameState::Launcher;
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
        mode->applySettings(currentSettings);
        mode->start(world, mesh.get(), albedo.get(), &registry, &meshCache);
        player.reset(mode->getPlayerEntity(), currentSettings.playerHp,
                     mode->getSpawnPos(), cam);
        worldReady = true;
    };

    auto startGame = [&]()
    {
        // Modalità scelta nel PreMatch (ADR-014): Conquista/Assalto/Difesa.
        // Ricreata SEMPRE qui (il mode del processo può essere sandbox).
        const char* modeId = matchModeId(currentSettings.modeIndex);
        mode = createGameMode(modeId);
        telemetry::logInfo(std::string("game mode da PreMatch: ") + modeId);

        // ── Arma primaria ─────────────────────────────────────────────
        const std::string& primaryId = preMatchMenu.getSelectedWeaponId();
        const auto* wDef = registry.getWeapon(primaryId);
        player.weapons[0] = wDef ? weaponFromDef(*wDef) : makeBlasterRifle();
        if (!wDef)
            std::cerr << "[Game] Arma primaria non trovata: " << primaryId << " — fallback\n";

        // ── Arma secondaria ───────────────────────────────────────────
        const std::string& secId = preMatchMenu.getSettings().secondaryWeaponId;
        const auto* wDef2 = secId.empty() ? nullptr : registry.getWeapon(secId);
        player.weapons[1] = wDef2 ? weaponFromDef(*wDef2) : Weapon{};

        player.activeWeapon = 0;
        player.weapon = player.weapons[0];

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

    auto doVoluntaryRespawn = [&]()
    {
        if (player.isDead) return;
        int t1 = mode->getTeam1Tickets();
        if (t1 <= 0)
        {
            state = GameState::Lose;
            stateChanged = true;
            window.setMouseCaptured(false);
            std::cout << "[Game] Nessun ticket per il respawn volontario. SCONFITTA!" << std::endl;
            return;
        }
        mode->consumeTeam1Ticket();
        player.isDead = true;
        player.prevHp = 0.0f;
        player.respawnTimer = currentSettings.respawnDelay;
        if (world.isValidEntity(player.entity))
        {
            auto* hp = world.getHealth(player.entity);
            if (hp) hp->current = 0.0f;
        }
        state = GameState::Playing;
        stateChanged = true;
        window.setMouseCaptured(true);
        std::cout << "[Respawn volontario] Ticket rimasti: "
                  << mode->getTeam1Tickets()
                  << " — respawn in " << currentSettings.respawnDelay << "s" << std::endl;
    };

    // ── Bootstrap Sandbox: salta i menu, entra subito in gioco ────────
    if (sandbox)
    {
        // Arma di default (nessun PreMatch in sandbox)
        const auto* wDef = registry.getWeapon(preMatchMenu.getSelectedWeaponId());
        if (!wDef && !wList.empty()) wDef = registry.getWeapon(wList[0].id);
        player.weapons[0] = wDef ? weaponFromDef(*wDef) : makeBlasterRifle();
        player.weapons[1] = Weapon{};
        player.activeWeapon = 0;
        player.weapon = player.weapons[0];

        initWorld();
        window.setMouseCaptured(true);
        hud.toast("Sandbox: tasti 1-9 per provare le armi", 5.0f);
        std::cout << "[Game] Sandbox avviata — " << player.weapon.name << std::endl;
    }

    // ═════════════════════════════════════════════════════════════════
    // MAIN LOOP
    // ═════════════════════════════════════════════════════════════════
    while (m_running && window.isOpen())
    {
        telemetry::beginFrame();
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

                if (sc == SDL_SCANCODE_F11) window.toggleFullscreen();

                // ── F12: dump completo dello stato (ADR-013) ──────────
                if (sc == SDL_SCANCODE_F12)
                {
                    const glm::vec3 cp = cam.getPosition();
                    const glm::vec3 cf = cam.getForward();
                    nlohmann::json js;
                    js["app"]            = "GFEngine";
                    js["game_state"]     = (int)state;
                    js["world_ready"]    = worldReady;
                    js["entity_count"]   = worldReady
                                           ? (int)world.getEntities().size() : 0;
                    js["camera"]["pos"]     = {cp.x, cp.y, cp.z};
                    js["camera"]["forward"] = {cf.x, cf.y, cf.z};
                    js["player"]["hp"]      = player.prevHp;
                    js["player"]["dead"]    = player.isDead;
                    js["player"]["weapon"]  = player.weapon.name;
                    js["player"]["heat"]    = player.weapon.heat;
                    js["tickets"]["team1"]  = mode->getTeam1Tickets();
                    js["tickets"]["team2"]  = mode->getTeam2Tickets();
                    js["mouse_captured"]    = window.isMouseCaptured();
                    telemetry::dumpGameState(js);
                    hud.toast("F12: stato salvato in _telemetry_data/game_state.json");
                }

                // ── F9: spike split-screen (ADR-011, solo verifica) ───
                if (sc == SDL_SCANCODE_F9 && state == GameState::Playing)
                {
                    splitSpike = !splitSpike;
                    hud.toast(splitSpike ? "Split-screen spike: ON (F9)"
                                         : "Split-screen spike: OFF");
                    telemetry::logInfo(std::string("split spike: ")
                                       + (splitSpike ? "ON" : "OFF"));
                }

                if (state == GameState::Launcher)
                {
                    auto res = launcher.handleKey(sc);
                    if (res == LauncherScreen::Result::Launch)
                        goMainMenu();
                    else if (res == LauncherScreen::Result::Quit)
                        window.close();
                }
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
                else if (state == GameState::PreMatch)
                {
                    auto res = preMatchMenu.handleKey(sc);
                    if (res == PreMatchMenu::Result::StartGame)
                    { currentSettings = preMatchMenu.getSettings(); startGame(); }
                    else if (res == PreMatchMenu::Result::Back)
                    { goMainMenu(); }
                }
                else if (state == GameState::Options)
                {
                    auto res = optMenu.handleKey(sc, input);
                    if (res == OptionsMenu::Result::Back)
                    {
                        state = prevState; stateChanged = true;
                        if (state == GameState::Playing) window.setMouseCaptured(true);
                    }
                }
                else if (state == GameState::Paused)
                {
                    if (sc == SDL_SCANCODE_K && !stateChanged)
                        doVoluntaryRespawn();
                }
                else if (state == GameState::Playing)
                {
                    if (sc == SDL_SCANCODE_V)
                        player.toggleThirdPerson(cam);

                    // ── Sandbox: 1-9 = equipaggia arma dal registry ────
                    if (sandbox
                        && sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
                    {
                        const int idx = sc - SDL_SCANCODE_1;
                        if (idx < (int)testWeapons.size())
                        {
                            const auto* wd = registry.getWeapon(testWeapons[idx].first);
                            if (wd)
                            {
                                player.weapons[0]   = weaponFromDef(*wd);
                                player.activeWeapon = 0;
                                player.weapon       = player.weapons[0];
                                hud.toast("Arma " + std::to_string(idx + 1) + ": "
                                          + wd->name);
                                telemetry::logInfo("sandbox: arma cambiata -> "
                                                   + wd->name);
                            }
                        }
                    }
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
        hud.tick(elapsed);

        if (state == GameState::Playing)
        {
            accumulator += elapsed;
            while (accumulator >= fixedDt)
            { mode->update(world, fixedDt); world.tick(fixedDt); accumulator -= fixedDt; }

            // Feedback colpi a segno del giocatore → hitmarker sul mirino
            if (world.combatFeedback.team1Kill)     hud.hitmarker(true);
            else if (world.combatFeedback.team1Hit) hud.hitmarker(false);
            world.combatFeedback.reset();

            player.weapon.update(elapsed);
            if (player.weapon.overheated && !wasOverheated) audio.playOverheat();
            wasOverheated = player.weapon.overheated;

            if (player.respawnTimer > 0.0f)
            {
                player.respawnTimer -= elapsed;
                if (player.respawnTimer <= 0.0f)
                {
                    player.updateRespawn(world, cam, currentSettings.respawnDelay,
                                          mode->getSpawnPos(), currentSettings.playerHp);
                    mode->overridePlayerEntity(player.entity);
                }
            }
        }

        // ── 4. CAMERA + PHYSICS ──────────────────────────────────────
        if (state == GameState::Playing && window.isMouseCaptured())
            player.processMouse(cam, (float)input.mouseDX(), (float)input.mouseDY());

        if (state == GameState::Playing)
            player.updateMovement(cam, input, world, elapsed);

        // ── 5. GAME LOGIC ────────────────────────────────────────────
        if (state == GameState::Playing)
        {
            if (!player.isDead && world.isValidEntity(player.entity))
            {
                auto* pt = world.getTransform(player.entity);
                if (pt)
                {
                    // In TPS la posizione del player è in tpsPlayerPos, non nella camera
                    const glm::vec3& p = player.thirdPerson
                                         ? player.tpsPlayerPos
                                         : cam.getPosition();
                    pt->x = p.x; pt->y = p.y; pt->z = p.z;
                }
            }

            if (!player.isDead && !world.isValidEntity(player.entity))
            {
                player.isDead = true;
                player.prevHp = 0.0f;
                int t1 = mode->getTeam1Tickets();
                if (t1 > 0)
                {
                    mode->consumeTeam1Ticket();
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
                int t1 = mode->getTeam1Tickets();
                if (t1 > 0)
                {
                    mode->consumeTeam1Ticket();
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

            // ── Feedback di mira: il mirino diventa rosso se punta una
            //    hitbox nemica reale (stesse trasformazioni del CombatSystem:
            //    scala, yaw, offset mesh). Fallback: sfera sul corpo. ───────
            {
                bool aimOn = false;
                const glm::vec3 ro = cam.getPosition();
                const glm::vec3 rd = cam.getForward();
                for (EntityId id : world.getEntities())
                {
                    const auto* tm2 = world.getTeam(id);
                    const auto* eh2 = world.getHealth(id);
                    const auto* tr2 = world.getTransform(id);
                    if (!tm2 || tm2->teamId == 1 || !eh2 || eh2->current <= 0.0f
                        || !tr2 || world.getBullet(id)) continue;

                    const auto* hb2 = world.getHitbox(id);
                    const auto* mr2 = world.getMeshRenderer(id);
                    const float sc  = (tr2->sx > 0.0001f) ? tr2->sx : 1.0f;
                    const float mo  = mr2 ? mr2->meshOffsetY : 0.0f;
                    const glm::vec3 ep{tr2->x, tr2->y, tr2->z};

                    if (hb2 && hb2->profile && !hb2->profile->zones.empty())
                    {
                        const float cy2 = std::cos(glm::radians(tr2->ry));
                        const float sy2 = std::sin(glm::radians(tr2->ry));
                        for (const auto& z : hb2->profile->zones)
                        {
                            glm::vec3 off = z.offset * sc;
                            glm::vec3 roff{ cy2*off.x + sy2*off.z, off.y,
                                           -sy2*off.x + cy2*off.z };
                            const glm::vec3 ctr = ep + glm::vec3(0, mo, 0) + roff;
                            const glm::vec3 he  = z.halfExtents * sc;
                            if (rayAABB(ro, rd, ctr - he, ctr + he, 80.0f))
                            { aimOn = true; break; }
                        }
                    }
                    else
                    {
                        const glm::vec3 to = ep - ro;
                        const float t = glm::dot(to, rd);
                        if (t > 0.0f && t < 80.0f
                            && glm::length(ep - (ro + rd * t)) < 0.7f)
                            aimOn = true;
                    }
                    if (aimOn) break;
                }
                hud.setAimOnTarget(aimOn);
            }

            // ── Stato command post → HUD (ADR-014/#6) ─────────────────
            if (const CommandPosts* cps = mode->commandPosts())
            {
                std::vector<HUD::PostStatus> ps;
                for (const auto& s : cps->status())
                    ps.push_back({s.label, s.owner, s.capturingTeam, s.progress01});
                hud.setPosts(ps);
            }

            // ── Esito: deciso dal MODE (ADR-014) ─────────────────────
            const MatchOutcome oc = mode->outcome(world);
            if (oc == MatchOutcome::Team1Win)
            {
                state = GameState::Win; stateChanged = true;
                window.setMouseCaptured(false);
                audio.playVictory();
                telemetry::logInfo("esito: VITTORIA (regole della modalita')");
                std::cout << "\n[Game] VITTORIA!" << std::endl;
            }
            else if (oc == MatchOutcome::Team2Win)
            {
                state = GameState::Lose; stateChanged = true;
                window.setMouseCaptured(false);
                telemetry::logInfo("esito: SCONFITTA (regole della modalita')");
                std::cout << "\n[Game] SCONFITTA (obiettivo perso)!" << std::endl;
            }
        }

        // ── 6. RENDER ────────────────────────────────────────────────
        renderer.beginFrame();

        if (worldReady && state == GameState::Playing)
        {
            auto drawScene = [&](const Camera& viewCam)
            {
                for (EntityId id : world.getEntities())
                {
                    const auto* tr = world.getTransform(id);
                    const auto* mr = world.getMeshRenderer(id);
                    if (!tr || !mr || !mr->visible || !mr->mesh) continue;
                    // meshOffsetY sposta verticalmente la mesh rispetto al centro
                    // fisico dell'entità (es. per appoggiare i piedi a terra).
                    glm::mat4 model = toModelMatrix(*tr);
                    if (mr->meshOffsetY != 0.0f)
                        model = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(0.0f, mr->meshOffsetY, 0.0f)) * model;
                    renderer.drawMeshFrom(viewCam, *mr->mesh, mr->texture, model,
                                          {mr->r, mr->g, mr->b});

                    // Arma in mano (o altro modello agganciato)
                    if (mr->attachMesh)
                        renderer.drawMeshFrom(viewCam, *mr->attachMesh, mr->texture,
                                              model * mr->attachLocal,
                                              {0.55f, 0.55f, 0.58f});
                }
            };

            if (!splitSpike)
            {
                drawScene(cam);
            }
            else
            {
                // Spike ADR-011: stessa scena in due viewport affiancati.
                int dw = 0, dh = 0;
                renderer.getDrawableSize(dw, dh);
                const float halfAspect = (dh > 0)
                    ? (float)(dw / 2) / (float)dh : 1.0f;

                // Sinistra: vista del giocatore
                Camera camL = cam;
                camL.setAspect(halfAspect);
                renderer.setViewportRect(0, 0, dw / 2, dh);
                drawScene(camL);

                // Destra: seconda camera (offset laterale, stessa direzione)
                Camera camR = cam;
                camR.setAspect(halfAspect);
                const glm::vec3 right =
                    glm::normalize(glm::cross(cam.getForward(), {0, 1, 0}));
                camR.setPosition(cam.getPosition() + right * 2.5f
                                 + glm::vec3(0, 0.5f, 0));
                renderer.setViewportRect(dw / 2, 0, dw - dw / 2, dh);
                drawScene(camR);

                // Ripristina il viewport pieno per HUD/menu
                renderer.setViewportRect(0, 0, dw, dh);
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
                       mode->getTeam1Tickets(), mode->getTeam2Tickets(),
                       aliveAllies, aliveEnemies);
        }

        renderer.endFrame();
    }

    telemetry::shutdown();
}

} // namespace mini
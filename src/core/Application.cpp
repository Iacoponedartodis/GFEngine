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
#include "mini/ecs/systems/SquadSystem.hpp"
#include "mini/ecs/systems/ObjectiveSystem.hpp"
#include "mini/game/data/ContentValidation.hpp"   // gate contenuti (ADR-018)
#include "mini/ecs/systems/CrowdSystem.hpp"
#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/CommandPosts.hpp"
#include "mini/game/MatchSettings.hpp"
#include "mini/game/PlayerController.hpp"
#include "mini/game/Weapon.hpp"
#include "mini/game/VehicleDrive.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/nav/NavManager.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/physics/HitTest.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/HUD.hpp"
#include "mini/render/LauncherScreen.hpp"
#include "mini/render/MainMenuScreen.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/OptionsMenu.hpp"
#include "mini/render/PreMatchMenu.hpp"
#include "mini/render/SandboxMenu.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>   // ADR-015: no-op se USE_TRACY_PROFILER=OFF
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
    w.meshPath        = def.meshPath;
    w.meshScale       = def.meshScale;
    w.meshRotY        = def.meshRotY;
    w.baseSpread      = def.baseSpread;
    w.adsSpread       = def.adsSpread;
    w.moveSpread      = def.moveSpread;
    w.sprintSpread    = def.sprintSpread;
    w.jumpSpread      = def.jumpSpread;
    w.effectiveRange  = def.effectiveRange;
    return w;
}

// Restituisce il percorso alla cartella data/ sorgente del progetto.
// Prima prova 3 livelli su dall'exe (build/config/Debug -> project root),
// identico alla logica usata dal Balance Editor.
// Fallback: data/ accanto all'exe (copia da CMake).
// Esposta (ADR-018): la usa anche `--validate` in main.cpp. Duplicarla
// significherebbe che il gate valida una cartella diversa da quella caricata.
std::string getDataPath()
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

// Il feedback di mira usa il test OBB condiviso in physics/HitTest.hpp
// (rimosso il rayAABB locale: KI #13, mirino = proiettili per costruzione).

// (la verifica "nemici vivi" ora vive nelle regole dei mode — ADR-014)

void Application::run(bool directPreMatch, bool sandbox, bool autoSim,
                      const std::string& mapOverride, int stressAiCount,
                      const std::string& missionId,
                      const std::string& classId)
{
    using namespace config;
    initialize();

    // ── Telemetria (ADR-013): logger + crash net + input recorder ─────
    telemetry::init("GFEngine");
    telemetry::logInfo(std::string("flag avvio: directPreMatch=")
        + (directPreMatch ? "si" : "no") + " sandbox=" + (sandbox ? "si" : "no"));
    // Evento strutturato di avvio sessione (ADR-016): prima riga di session_latest.jsonl
    telemetry::event(telemetry::Level::Info, "Engine", "session start",
        { {"direct_prematch", directPreMatch}, {"sandbox", sandbox},
          {"auto_sim", autoSim}, {"stress_ai", stressAiCount} });

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

    // ── Gate di validazione contenuti (ADR-018, doc 24) ──────────────
    // Gira DOPO loadAll(), sullo stesso registry, con le STESSE regole di
    // `--validate` e dell'editor. Un Error è contenuto critico invalido: si
    // blocca con diagnostica azionabile invece di degradare in silenzio — è
    // l'intera ragione per cui questo gate esiste (KI #7/#24/#25/#26).
    {
        const Diagnostics diags = validateContent(registry, dataPath);
        const bool critical = reportDiagnostics(diags, /*printToStdout=*/true);
        telemetry::event(critical ? telemetry::Level::Error : telemetry::Level::Info,
                         "Content", "content validated",
                         {{"errors",   countBy(diags, telemetry::Level::Error)},
                          {"warnings", countBy(diags, telemetry::Level::Warn)}});
        if (critical)
        {
            std::cerr << "\n[Content] Avvio BLOCCATO: contenuto critico invalido.\n"
                         "[Content] Correggi gli errori sopra, oppure esegui "
                         "GFEngine.exe --validate per rivederli.\n";
            telemetry::flushEvents();
            return;
        }
    }

    // ── Missione attiva (ADR-019, doc 25) ────────────────────────────
    // Risolta UNA volta dal registry. Un id inesistente non deve avviare una
    // partita "senza missione" in silenzio: chi ha chiesto una missione deve
    // sapere che non c'è (stessa disciplina degli ordini di squadra).
    // `--mission <id>` SEMINA soltanto la scelta: la missione vera si risolve in
    // initWorld da `currentSettings.missionId`, perché dal 2026-07-16 può cambiare
    // anche dal PreMatch. Risolverla una volta all'avvio la congelerebbe al flag.
    const MissionDef* cliMission = nullptr;
    if (!missionId.empty())
    {
        cliMission = registry.getMission(missionId);
        if (!cliMission)
        {
            std::cerr << "[Mission] id sconosciuto: '" << missionId
                      << "' — nessuna missione attiva\n";
            telemetry::logWarn("mission id sconosciuto: " + missionId);
        }
        else
            telemetry::logInfo("missione da CLI: " + cliMission->id);
    }

    // La missione IMPONE la sua mappa (MissionDef.mapId, doc 25). Gli obiettivi
    // sono coordinate in quella mappa: giocarli altrove li renderebbe assurdi
    // (es. "raggiungi (12,0)" su una mappa dove quel punto è dentro un muro).
    // Un `--map` esplicito che contraddice la missione è un errore dell'utente:
    // si segnala, non si sceglie in silenzio. (Dal PreMatch il problema non si
    // pone: scegliendo la missione il menu aggiorna a vista la riga Mappa.)
    std::string requestedMapId = mapOverride;
    if (cliMission && !cliMission->mapId.empty())
    {
        if (!requestedMapId.empty() && requestedMapId != cliMission->mapId)
            std::cerr << "[Mission] --map '" << requestedMapId << "' ignorato: la missione '"
                      << cliMission->id << "' impone la mappa '"
                      << cliMission->mapId << "'\n";
        requestedMapId = cliMission->mapId;
    }

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

    // Mappe selezionabili nel PreMatch (R3: niente più mappa hardcoded)
    std::vector<PreMatchMenu::MapEntry> mList;
    for (auto& [id, def] : registry.maps())
        mList.push_back({id, def.name.empty() ? id : def.name});
    std::sort(mList.begin(), mList.end(),
        [](const auto& a, const auto& b){ return a.id < b.id; });

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

    for (auto& [id, enemy] : registry.enemies())  loadMeshIntoCache(enemy.meshPath);
    for (auto& [id, ally]  : registry.allies())   loadMeshIntoCache(ally.meshPath);
    for (auto& [id, wpn]   : registry.weapons())  loadMeshIntoCache(wpn.meshPath);
    for (auto& [id, veh]   : registry.vehicles()) loadMeshIntoCache(veh.meshPath);

    // ── ECS ──────────────────────────────────────────────────────────
    World world;
    world.registerSystem(std::make_unique<MovementSystem>());
    world.registerSystem(std::make_unique<CombatSystem>());
    world.registerSystem(std::make_unique<SquadSystem>());   // ADR-020: PRIMA di AiSystem
    world.registerSystem(std::make_unique<AiSystem>());      // (l'ordine è un vincolo,
    world.registerSystem(std::make_unique<CrowdSystem>());   //  non un override)
    // ADR-019: DOPO Ai/Crowd — gli obiettivi valutano lo stato quando le unità
    // si sono già mosse in questo tick. Application tiene un puntatore NON
    // proprietario per leggerne esito e stato (HUD + fine partita): i sistemi
    // sopravvivono a World::initialize(), quindi resta valido per tutta la run.
    // Senza questo il framework obiettivi sarebbe un sistema ISOLATO — vietato
    // dal GDD 21.2 — e `outcome()` resterebbe codice morto.
    auto objectiveSystemOwned = std::make_unique<ObjectiveSystem>();
    ObjectiveSystem* objectives = objectiveSystemOwned.get();
    world.registerSystem(std::move(objectiveSystemOwned));

    // ── Schermate ────────────────────────────────────────────────────
    LauncherScreen   launcher(W, H);
    MainMenuScreen   mainMenu(W, H);
    HUD              hud(W, H);
    OptionsMenu      optMenu(W, H);
    PreMatchMenu     preMatchMenu(W, H);
    SandboxMenu      sbMenu(W, H);        // menu banco di prova (17_SandboxTools)
    bool sbMenuOpen  = false;             // overlay aperto (solo sandbox)
    bool sbMouseFreed = false;            // cursore liberato mentre il menu sandbox è aperto
    bool observerFly = false;             // volo libero osservatore (sim AI)
    EntityId drivenVehicle = 0;           // veicolo guidato (19_Vehicles)
    // Ruota di comando (doc 26): stato che vive fra i frame mentre il tasto è
    // tenuto. `wheelDirX/Y` accumula il movimento mouse per scegliere il settore.
    bool  wheelOpen = false; float wheelDirX = 0.0f, wheelDirY = 0.0f; int wheelSel = -1;
    // Selezione del punto di respawn mentre si è a terra (mappa top-down, doc
    // 30). `respawnSel` = indice in mode->availableSpawns() (0 = spawn base);
    // `deathPos` = dove si è caduti (marker di orientamento sulla mappa).
    int       respawnSel = 0;
    glm::vec3 deathPos{0.0f, 0.0f, 0.0f};
    float vehPrevR = 0, vehPrevG = 0, vehPrevB = 0; // colore pre-mount
    int   vehTraceCnt = 0;                // throttle telemetria guida
    sbMenu.setWeapons(testWeapons);
    {
        std::vector<std::pair<std::string, std::string>> sbMaps;
        for (const auto& me : mList) sbMaps.push_back({me.id, me.name});
        sbMenu.setMaps(sbMaps);
    }
    preMatchMenu.setWeaponList(wList);
    preMatchMenu.setAbilityList(aList);
    preMatchMenu.setMapList(mList);

    // ── Missioni (ADR-019) e classi (doc 14) nel PreMatch ─────────────
    //    Senza questo il giocatore non può SCEGLIERE una missione: il sistema
    //    obiettivi resterebbe raggiungibile solo da `--mission`, cioè invisibile
    //    in partita normale (GDD 23.1 lo mette fra i sistemi Core).
    //    La missione porta con sé mappa e modalità: sono sue e il menu le mostra.
    {
        std::vector<PreMatchMenu::MissionEntry> misList;
        for (const auto& [id, m] : registry.missions())
            misList.push_back({id, m.name.empty() ? id : m.name, m.mapId, m.modeId});
        std::sort(misList.begin(), misList.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
        preMatchMenu.setMissionList(misList);
        // Nessuna lista di classi: il menu non offre una scelta di classe
        // (GDD 11.3, ADR-022). Il loadout sono le righe Arma primaria/secondaria.
    }

    // ── Game mode (ADR-008: Application parla solo con IGameMode) ─────
    MatchSettings currentSettings;

    // ── Personaggio attivo (KI #35) ──────────────────────────────────
    // PlayerDef era autorato e mai letto: le stat dell'editor non avevano effetto.
    // Ora vengono applicate. Con UN solo personaggio autorato non c'è nulla da
    // scegliere — quello È il giocatore, e il pannello dell'editor diventa vivo
    // senza bisogno di UI. Con più personaggi la scelta diventa ambigua: NON si
    // indovina (sceglierne uno a caso è come i fallback hardcoded di ADR-007),
    // si dichiara che serve una selezione esplicita.
    if (currentSettings.characterId.empty())
    {
        const auto& chars = registry.playerDefs();
        if (chars.size() == 1)
            currentSettings.characterId = chars.begin()->first;
        else if (chars.size() > 1)
            telemetry::logWarn("piu' personaggi autorati (" + std::to_string(chars.size())
                + ") ma nessun selettore nel PreMatch: si usano le stat di default. "
                  "Serve la selezione esplicita (14_ClassSystem Phase B).");
    }

    // HP di default dal personaggio: `PlayerDef.hp` SEMINA lo slider "HP
    // giocatore" del PreMatch come default (fatto UNA volta all'avvio). Da lì
    // in poi lo slider è l'autorità unica sugli HP del giocatore — usata
    // identica per spawn iniziale E respawn. Prima `initWorld` risovrascriveva
    // `playerHp` col PlayerDef DOPO che il mode aveva già creato l'entità con
    // il valore dello slider: gli HP del primo spawn erano quelli scelti, ma
    // al respawn tornavano a quelli del PlayerDef (bug 2026-07-18).
    if (!currentSettings.characterId.empty())
        if (const PlayerDef* pd = registry.getPlayerDef(currentSettings.characterId))
            currentSettings.playerHp = pd->hp;

    // ── Missione iniziale + override classe da CLI ───────────────────
    // La missione si sceglie anche dal PreMatch: il flag SEMINA il valore di
    // partenza. `--class` invece è un **override di TEST**, non una scelta di
    // gioco: dal 2026-07-17 il menu non offre più le classi (GDD 11.3, ADR-022:
    // il giocatore non sceglie una classe, la livella giocando). Serve a provare
    // rapidamente un loadout senza passare dalle righe Arma; sovrascrive quelle
    // righe, e per questo lo dice a voce (telemetria + stderr).
    // Un id sconosciuto non degrada in silenzio.
    if (!classId.empty())
    {
        if (registry.getClass(classId))
        {
            currentSettings.classId = classId;
            telemetry::logInfo("classe iniziale (override di test --class): " + classId);
        }
        else
            std::cerr << "[Class] id sconosciuto: '" << classId
                      << "' — loadout manuale\n";
    }
    if (cliMission) currentSettings.missionId = cliMission->id;
    // Il PreMatch deve PARTIRE da questi valori, altrimenti li azzera al primo
    // avvio: possiede lui missione e classe, e getSettings() sovrascriverebbe
    // ciò che il CLI ha seminato (è la stessa trappola di KI #36).
    preMatchMenu.setSettings(currentSettings);
    // Mappa: --map <id> (test/debug, R3) oppure quella imposta dalla missione.
    if (!requestedMapId.empty())
    {
        if (registry.getMap(requestedMapId))
            currentSettings.mapId = requestedMapId;
        else
            std::cerr << "[Map] id sconosciuto: '" << requestedMapId
                      << "' — resta '" << currentSettings.mapId << "'\n";
    }
    sbMenu.allyCount    = std::max(1, currentSettings.team1AiCount);
    sbMenu.enemyCount   = std::max(1, currentSettings.team2AiCount);
    sbMenu.team1Tickets = currentSettings.team1Tickets;
    sbMenu.team2Tickets = currentSettings.team2Tickets;
    sbMenu.respawnDelay = currentSettings.respawnDelay;
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
    player.weapons[0] = makeBlasterRifle();

    // ── Stato ────────────────────────────────────────────────────────
    GameState state     = sandbox        ? GameState::Playing
                        : directPreMatch ? GameState::PreMatch
                                         : GameState::Launcher;
    GameState prevState = state;
    bool      stateChanged  = false;
    bool      wasOverheated = false;

    // ── Frame pacing (Fase 2 ottimizzazione) ─────────────────────────
    // Timestep fisso con accumulatore in DOPPIA precisione; il tempo viene
    // dal contatore hardware SDL_GetPerformanceCounter (sub-ms), non da
    // std::chrono/SDL_GetTicks. L'accumulatore float accumulava errore su
    // sessioni lunghe. La simulazione riceve comunque fixedDt (float)
    // invariato → il gameplay resta identico a 60 Hz.
    constexpr float  fixedDt         = 1.0f / 60.0f;  // dt passato ai sistemi (invariato)
    constexpr double SIMULATION_STEP = 1.0 / 60.0;    // passo dell'accumulatore (double)
    double       accumulator = 0.0;
    const double perfFreq    = (double)SDL_GetPerformanceFrequency();
    Uint64       prevCounter = SDL_GetPerformanceCounter();
    Camera& cam = renderer.getCamera();

    if (directPreMatch)
        std::cout << "[Application] Direct PreMatch mode." << std::endl;
    else
        std::cout << "[Application] Launcher avviato." << std::endl;

    // ── Lambda transizioni ───────────────────────────────────────────
    NavManager nav;          // ADR-017: navmesh + crowd, ricostruiti al load mappa
    world.nav = &nav;        // AiSystem/CrowdSystem lo usano via world (Phase B)
    auto initWorld = [&]()
    {
        mode->applySettings(currentSettings);
        mode->start(world, mesh.get(), albedo.get(), &registry, &meshCache);

        // ── Personaggio (KI #35): le stat base diventano DATI ──────────
        // Sta in initWorld e NON in startGame perché vale per OGNI percorso
        // (partita e sandbox): il giocatore non può comportarsi diversamente a
        // seconda di come è entrato. PlayerDef era autorato dal BalanceEditor e
        // letto da nessuno — ogni stat regolata lì non aveva effetto. I default
        // di PlayerController sono identici alle vecchie costanti, quindi senza
        // personaggio il comportamento è invariato per costruzione.
        if (!currentSettings.characterId.empty())
        {
            if (const PlayerDef* pd = registry.getPlayerDef(currentSettings.characterId))
            {
                player.moveSpeed   = pd->moveSpeed;
                player.jumpMult    = pd->jumpHeight;
                player.sprintMult  = pd->sprintMult;
                player.armorRating = pd->armorRating;
                // NB: gli HP NON si risovrascrivono qui. `PlayerDef.hp` ha già
                // seminato lo slider "HP giocatore" all'avvio; da lì lo slider
                // (currentSettings.playerHp) è l'autorità, identica per spawn
                // iniziale e respawn. Sovrascrivere qui li faceva divergere.
                telemetry::event(telemetry::Level::Info, "Content", "character equipped",
                                 {{"character", currentSettings.characterId},
                                  {"hp", pd->hp}, {"move_speed", pd->moveSpeed},
                                  {"sprint_mult", pd->sprintMult},
                                  {"armor", pd->armorRating}});
            }
            else
            {
                std::cerr << "[Character] id sconosciuto: "
                          << currentSettings.characterId << " — stat di default\n";
                telemetry::logWarn("personaggio sconosciuto: " + currentSettings.characterId);
                currentSettings.characterId.clear();
            }
        }

        player.reset(mode->getPlayerEntity(), currentSettings.playerHp,
                     mode->getSpawnPos(), cam);
        // Armatura del personaggio → HealthComponent (KI #35). Il mode ha appena
        // creato l'entità: l'armatura è l'unica stat che non vive sul controller,
        // perché il danno lo applica CombatSystem. 1.0 = nessuna riduzione.
        if (auto* ph = world.getHealth(mode->getPlayerEntity()))
            ph->armor = player.armorRating;
        drivenVehicle = 0;   // ogni restart parte a piedi
        // Mailbox squadra (ADR-020): il SquadSystem usa il giocatore come leader
        // degli alleati quando è un'entità valida di team 1 (in sim è neutro).
        world.playerEntity = mode->getPlayerEntity();
        // Mailbox missione (ADR-019): il mode ha appena fatto start() e World
        // ::initialize() ha azzerato le mailbox, quindi si (ri)collega qui.
        // Nessuna missione → ObjectiveSystem inerte, i mode restano identici.
        // Risolta QUI (non all'avvio) perché la missione può venire dal PreMatch,
        // da un preset o dal flag CLI: il campo autoritativo è uno solo.
        const MissionDef* mission = currentSettings.missionId.empty()
                                  ? nullptr
                                  : registry.getMission(currentSettings.missionId);
        if (!mission && !currentSettings.missionId.empty())
        {
            std::cerr << "[Mission] id sconosciuto: '" << currentSettings.missionId
                      << "' — partita libera\n";
            currentSettings.missionId.clear();
        }
        world.activeMission = mission;
        world.objectiveDefs = mission ? &registry : nullptr;
        worldReady = true;

        // NavMesh (ADR-017 Phase A): costruito dai box collider della mappa;
        // validato via telemetria JSONL. NON muove ancora l'AI (Phase B).
        if (const MapDef* nm = registry.getMap(currentSettings.mapId))
        {
            const NavBuildStats st = nav.build(*nm);
            telemetry::event(st.ok ? telemetry::Level::Info : telemetry::Level::Error,
                "Nav", "navmesh built",
                {{"ok", st.ok}, {"map", currentSettings.mapId},
                 {"input_tris", st.inputTris}, {"polys", st.polyCount},
                 {"verts", st.vertCount},
                 {"danger_polys", st.dangerPolys}, {"cover_polys", st.coverPolys},
                 {"bmin", {st.bmin.x, st.bmin.y, st.bmin.z}},
                 {"bmax", {st.bmax.x, st.bmax.y, st.bmax.z}}});
            if (st.ok)   // path di esempio spawn1→spawn2: valida il pathfinding
            {
                std::vector<glm::vec3> wp;
                const glm::vec3 a = mode->getSpawnPos();
                const glm::vec3 b = {nm->spawnTeam2[0], nm->spawnTeam2[1], nm->spawnTeam2[2]};
                const bool found = nav.findPath(a, b, wp);
                float len = 0.0f;
                for (size_t i = 1; i < wp.size(); ++i) len += glm::length(wp[i] - wp[i-1]);
                telemetry::event(telemetry::Level::Info, "Nav", "sample path",
                    {{"found", found}, {"waypoints", (int)wp.size()}, {"length", len},
                     {"from", {a.x, a.y, a.z}}, {"to", {b.x, b.y, b.z}}});
            }
        }
    };

    // ── Dump stato completo (ADR-016 Phase 4) ─────────────────────────
    // Snapshot JSON di OGNI entità attiva (pos/team/HP/goal-stato-AI), oltre a
    // camera/player/ticket. Riusato da F12, fine partita e crash net. Scritto
    // in game_state.json con "dump_reason" per distinguere il trigger.
    auto buildStateDump = [&](const char* reason) -> nlohmann::json
    {
        const glm::vec3 cp = cam.getPosition();
        const glm::vec3 cf = cam.getForward();
        nlohmann::json js;
        js["app"]         = "GFEngine";
        js["dump_reason"] = reason;
        js["game_state"]  = (int)state;
        js["world_ready"] = worldReady;
        js["camera"]["pos"]     = {cp.x, cp.y, cp.z};
        js["camera"]["forward"] = {cf.x, cf.y, cf.z};
        js["player"]["hp"]     = player.prevHp;
        js["player"]["dead"]   = player.isDead;
        js["player"]["weapon"] = player.weapon().name;
        js["player"]["heat"]   = player.weapon().heat;
        js["tickets"]["team1"] = mode ? mode->getTeam1Tickets() : 0;
        js["tickets"]["team2"] = mode ? mode->getTeam2Tickets() : 0;
        auto& ents = js["entities"] = nlohmann::json::array();
        if (worldReady)
            for (EntityId id : world.getEntities())
            {
                const auto* tr = world.getTransform(id);
                if (!tr) continue;
                nlohmann::json ent;
                ent["id"]  = id;
                ent["pos"] = {tr->x, tr->y, tr->z};
                if (const auto* tm = world.getTeam(id))   ent["team"] = tm->teamId;
                if (const auto* hp = world.getHealth(id)) { ent["hp"] = hp->current; ent["hp_max"] = hp->max; }
                if (const auto* ai = world.getAi(id))
                {
                    switch (ai->state) {
                        case AiState::Patrol: ent["ai_state"] = "Patrol"; break;
                        case AiState::Alert:  ent["ai_state"] = "Alert";  break;
                        case AiState::Hunt:   ent["ai_state"] = "Hunt";   break;
                        case AiState::Search: ent["ai_state"] = "Search"; break;
                    }
                    if (ai->hasLastKnown) ent["goal"] = {ai->lastKnownX, ai->lastKnownZ};
                }
                if (world.getBullet(id))  ent["kind"] = "bullet";
                if (world.getVehicle(id)) ent["kind"] = "vehicle";
                ents.push_back(std::move(ent));
            }
        js["entity_count"] = (int)ents.size();
        return js;
    };
    // Crash net (Phase 4): su crash, dump best-effort dello stato completo.
    telemetry::setStateDumpCallback([&]() { telemetry::dumpGameState(buildStateDump("crash")); });
    GameState endDumpState = GameState::Playing;   // per il dump one-shot a fine partita

    auto startGame = [&]()
    {
        // Modalità scelta nel PreMatch (ADR-014): Conquista/Assalto/Difesa.
        // Ricreata SEMPRE qui (il mode del processo può essere sandbox).
        const char* modeId = matchModeId(currentSettings.modeIndex);
        currentSettings.mapId = preMatchMenu.getSelectedMapId();   // R3
        mode = createGameMode(modeId);
        telemetry::logInfo(std::string("game mode da PreMatch: ") + modeId
                           + " su mappa '" + currentSettings.mapId + "'");

        // ── Classe (14_ClassSystem): un loadout confezionato ───────────
        // Se è impostata, la classe DECIDE le armi; altrimenti vale la scelta
        // manuale del PreMatch. Additivo: senza classe il comportamento è
        // identico a prima. La risoluzione avviene qui, cioè nello stesso punto
        // in cui l'arma viene già scelta oggi (14_ClassSystem, Integration).
        std::string primaryId = preMatchMenu.getSelectedWeaponId();
        std::string secId     = preMatchMenu.getSettings().secondaryWeaponId;
        if (!currentSettings.classId.empty())
        {
            if (const ClassDef* cls = registry.getClass(currentSettings.classId))
            {
                primaryId = cls->primaryWeaponId;
                secId     = cls->secondaryWeaponId;
                currentSettings.abilityIds = cls->abilityIds;
                telemetry::event(telemetry::Level::Info, "Content", "class equipped",
                                 {{"class", cls->id}, {"primary", primaryId},
                                  {"secondary", secId}});
            }
            else
            {
                // Il gate ADR-018 non può vederlo (la classe arriva da un preset
                // salvato, non dai dati): non degradare in silenzio.
                std::cerr << "[Game] Classe sconosciuta: " << currentSettings.classId
                          << " — loadout manuale\n";
                telemetry::logWarn("classe sconosciuta: " + currentSettings.classId);
                currentSettings.classId.clear();
            }
        }

        // ── Arma primaria ─────────────────────────────────────────────
        const auto* wDef = registry.getWeapon(primaryId);
        player.weapons[0] = wDef ? weaponFromDef(*wDef) : makeBlasterRifle();
        if (!wDef)
            std::cerr << "[Game] Arma primaria non trovata: " << primaryId << " — fallback\n";

        // ── Arma secondaria ───────────────────────────────────────────
        const auto* wDef2 = secId.empty() ? nullptr : registry.getWeapon(secId);
        player.weapons[1] = wDef2 ? weaponFromDef(*wDef2) : Weapon{};

        player.activeWeapon = 0;

        // Reset dello stato sandbox/osservatore: una partita vera avviata
        // dopo una simulazione NON deve ereditare volo libero o menu aperto.
        observerFly       = false;
        sbMenu.simRunning = false;
        sbMenuOpen        = false;

        initWorld();
        state = GameState::Playing;
        stateChanged = true;
        window.setMouseCaptured(true);
        std::cout << "[Game] Partita iniziata — " << player.weapon().name << std::endl;
    };

    // Avvio partita DAL PreMatch — punto UNICO, di proposito.
    // Il PreMatch NON possiede personaggio e classe (non ha ancora i selettori):
    // assegnare la sua struct intera li azzererebbe in silenzio. È la stessa
    // modalità di guasto che ha prodotto la regola READ-MODIFY-WRITE (ADR-010) —
    // costruire un oggetto nuovo e sovrascrivere invece di modificare solo i
    // propri campi — qui in memoria invece che su file. Il bug era reale: fino al
    // 2026-07-16 né la classe né le stat del personaggio arrivavano in partita.
    // Se il PreMatch HA un valore (es. da un preset caricato, che serializza
    // "class") vince lui; mai il contrario. Quando il PreMatch avrà i selettori,
    // questi campi diventeranno suoi e questa funzione sparirà.
    auto startFromPreMatch = [&]()
    {
        MatchSettings s = preMatchMenu.getSettings();
        // Il PreMatch possiede la MISSIONE: non va ripristinata da currentSettings —
        // se il giocatore sceglie "(nessuna)" dopo un `--mission`, la sua scelta
        // esplicita deve vincere. Preservare a forza il valore vecchio sarebbe la
        // toppa che distrugge una decisione.
        // `characterId` e `classId` NON sono nel menu (nessun selettore: il
        // giocatore non sceglie una classe, GDD 11.3): quelli vanno preservati,
        // altrimenti un `--class` verrebbe azzerato passando dal menu (KI #36).
        if (s.characterId.empty()) s.characterId = currentSettings.characterId;
        if (s.classId.empty())     s.classId     = currentSettings.classId;
        currentSettings = s;
        startGame();
    };

    // ── Simulazione AI-vs-AI (17_SandboxTools): condivisa tra il menu
    //    sandbox (ToggleSim) e il flag CLI --sim ─────────────────────────
    auto startSimulation = [&]()
    {
        currentSettings.team1AiCount = std::max(1, sbMenu.allyCount);
        currentSettings.team2AiCount = std::max(1, sbMenu.enemyCount);
        currentSettings.team1Tickets = sbMenu.team1Tickets;
        currentSettings.team2Tickets = sbMenu.team2Tickets;
        currentSettings.respawnDelay = sbMenu.respawnDelay;
        currentSettings.mapId        = sbMenu.selectedMapId();
        const char* simMode = matchModeId(sbMenu.simModeIndex);
        mode = createGameMode(simMode);
        initWorld();
        if (auto* tm = world.getTeam(player.entity))
            tm->teamId = 0;   // neutro: le AI lo ignorano
        // Parcheggia l'entità del giocatore FUORI dal campo: da neutrale
        // fermava comunque i proiettili vaganti (e la loro morte innescava
        // il respawn → teletrasporto della camera + ritorno a team 1).
        if (auto* pt = world.getTransform(player.entity))
            pt->y = -100.0f;
        observerFly = true;
        sbMenu.simRunning = true;
        hud.pushFeed(std::string("SIMULAZIONE AI avviata (") + simMode
                     + ", osservatore in volo)");
        telemetry::logInfo(std::string("sandbox: simulazione AI avviata (")
                           + simMode + ")");
    };
    auto stopSimulation = [&]()
    {
        mode = createGameMode("sandbox");
        observerFly = false;
        sbMenu.simRunning = false;
        initWorld();
        hud.pushFeed("SIMULAZIONE terminata: sandbox normale");
        telemetry::logInfo("sandbox: simulazione AI terminata");
    };

    auto goMainMenu = [&]()
    {
        state = GameState::MainMenu;
        stateChanged = true;
        window.setMouseCaptured(false);
    };

    // ── Regola della morte del giocatore: UN SOLO POSTO ──────────────────
    // I ticket sono la RISERVA DI RINFORZI della squadra — le truppe che entrano
    // man mano che quelle in campo cadono (il campo ha un cap di AI). NON sono le
    // vite del giocatore: morendo non ne consuma. Si perde solo cadendo quando non
    // resta né un alleato vivo né un rinforzo in arrivo: lì la squadra non esiste
    // più e non arriverà nessuno.
    // Prima: ogni morte del giocatore bruciava un rinforzo della squadra, e a
    // ticket 0 la morte era sconfitta secca anche con la squadra intatta.
    // Vale anche per il respawn volontario: è una morte come le altre.
    auto onPlayerDeath = [&]()
    {
        ++world.missionStats.playerDeaths;   // costo della missione (doc 25)
        int alliedAiAlive = 0;
        for (EntityId id : world.getEntities())
        {
            if (id == player.entity) continue;   // sta morendo: non conta
            const auto* tm = world.getTeam(id);
            const auto* hp = world.getHealth(id);
            if (tm && tm->teamId == 1 && hp && hp->current > 0.0f
                && !world.getBullet(id)) { ++alliedAiAlive; break; }
        }
        const int reinforcements = mode->getTeam1Tickets();
        if (alliedAiAlive > 0 || reinforcements > 0)
        {
            player.respawnTimer = currentSettings.respawnDelay;
            respawnSel = 0;   // ogni caduta riparte dallo spawn base (doc 25)
            deathPos   = cam.getPosition();   // marker "caduto" sulla mappa (doc 30)
            std::cout << "[Game] Eliminato! Respawn in "
                      << currentSettings.respawnDelay << "s (alleati vivi: "
                      << alliedAiAlive << ", rinforzi: " << reinforcements << ")"
                      << std::endl;
        }
        else
        {
            state = GameState::Lose; stateChanged = true;
            window.setMouseCaptured(false);
            telemetry::event(telemetry::Level::Info, "GameMode", "last stand lost",
                             {{"allied_ai_alive", 0}, {"reinforcements", 0}});
            std::cout << "[Game] SCONFITTA: squadra annientata e nessun rinforzo."
                      << std::endl;
        }
    };

    auto doVoluntaryRespawn = [&]()
    {
        if (player.isDead) return;
        player.isDead = true;
        player.prevHp = 0.0f;
        if (world.isValidEntity(player.entity))
        {
            auto* hp = world.getHealth(player.entity);
            if (hp) hp->current = 0.0f;
        }
        onPlayerDeath();   // stessa regola delle altre morti: mai due criteri
        // onPlayerDeath può aver deciso la sconfitta: tornare a Playing la
        // cancellerebbe. Suicidarsi da ultimo superstite senza rinforzi È perdere.
        if (state == GameState::Lose) return;
        state = GameState::Playing;
        stateChanged = true;
        window.setMouseCaptured(true);
    };

    // ── Schieramento dal punto scelto (doc 30) ────────────────────────
    // Il respawn del giocatore NON è automatico allo scadere del timer (quando
    // c'è una scelta): il timer è solo l'ATTESA MINIMA (che permette di tenere
    // respawnDelay basso per far rientrare in fretta AI e nemici), poi il rientro
    // avviene solo quando il giocatore CONFERMA (click sulla mappa/Invio), così
    // la scelta del punto ha sempre la precedenza. La posizione scelta viene
    // spinta fuori da eventuali collider (`nudgeOutOfColliders`): rientrare dentro
    // la geometria di un command post non incastra più — generale, non per-mappa.
    auto deployPlayerRespawn = [&]()
    {
        if (observerFly || !player.isDead || player.respawnTimer > 0.0f) return;
        const auto spawns = mode->availableSpawns();
        if (respawnSel < 0 || respawnSel >= (int)spawns.size()) respawnSel = 0;
        glm::vec3 spawnPos = spawns.empty() ? mode->getSpawnPos()
                                            : spawns[respawnSel].pos;
        spawnPos = physics::nudgeOutOfColliders(spawnPos, config::playerHalf(), world);
        player.updateRespawn(world, cam, currentSettings.respawnDelay,
                             spawnPos, currentSettings.playerHp);
        mode->overridePlayerEntity(player.entity);
        world.playerEntity = player.entity;   // respawn = entità NUOVA (ADR-020)
        hud.setRespawnMap({});                 // chiudi la mappa
        window.setMouseCaptured(true);         // torna in gioco: cattura il mouse
    };

    // ── Costruzione della mappa top-down di respawn (doc 30) ──────────
    // Proietta i dati mondo (bounds dai box geometria, pareti, punti disponibili,
    // luogo di morte) in una `HUD::RespawnMap`. I bounds partono dalla geometria
    // e si allargano a marker/morte così tutto è sempre in-frame.
    auto buildRespawnMap = [&](const std::vector<IGameMode::SpawnPoint>& spawns)
    {
        HUD::RespawnMap m;
        m.active      = true;
        m.sel         = respawnSel;
        m.secondsLeft = player.respawnTimer;

        float minX = 1e9f, minZ = 1e9f, maxX = -1e9f, maxZ = -1e9f;
        auto grow = [&](float x, float z) {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
        };
        if (const MapDef* md = world.activeMap)
            for (const auto& b : md->geometry)
            {
                m.walls.push_back({b.x, b.z, b.sx, b.sz});
                grow(b.x - b.sx * 0.5f, b.z - b.sz * 0.5f);
                grow(b.x + b.sx * 0.5f, b.z + b.sz * 0.5f);
            }
        for (const auto& sp : spawns) { m.markers.push_back({sp.label, sp.pos.x, sp.pos.z}); grow(sp.pos.x, sp.pos.z); }
        m.hasDeath = true; m.deathX = deathPos.x; m.deathZ = deathPos.z; grow(deathPos.x, deathPos.z);
        if (minX > maxX) { minX = -10; maxX = 10; minZ = -10; maxZ = 10; }   // fallback
        const float pad = 3.0f;
        m.minX = minX - pad; m.maxX = maxX + pad;
        m.minZ = minZ - pad; m.maxZ = maxZ + pad;

        int mx = 0, my = 0; SDL_GetMouseState(&mx, &my);
        m.mouseX = (float)mx; m.mouseY = (float)my;
        return m;
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

        initWorld();
        window.setMouseCaptured(true);
        hud.toast("Sandbox: TAB menu prova, P partita (PreMatch), L log eventi", 6.0f);
        std::cout << "[Game] Sandbox avviata — " << player.weapon().name << std::endl;

        // --stress N: forza N AI per team nel sim (profiling a scala con Tracy).
        // Clampato al cap globale; startSimulation legge sbMenu.ally/enemyCount.
        if (stressAiCount > 0)
        {
            const int n = std::min(stressAiCount, config::MAX_AI_PER_TEAM);
            sbMenu.allyCount = sbMenu.enemyCount = n;
            std::cout << "[Stress] " << n << " AI per team (sim)\n";
        }

        // --sim: entra direttamente nella simulazione AI-vs-AI (test/debug)
        if (autoSim) startSimulation();
    }

    // ── Gestione dei Result dei menu, condivisa fra tastiera e mouse ──
    // Estratta in lambda così i due percorsi di input non divergono mai (una
    // sola definizione dell'azione per ogni esito del menu).
    auto applyPreMatchResult = [&](PreMatchMenu::Result res)
    {
        if      (res == PreMatchMenu::Result::StartGame) startFromPreMatch();
        else if (res == PreMatchMenu::Result::Back)      goMainMenu();
    };
    auto applySandboxResult = [&](SandboxMenu::Result res)
    {
        if (res == SandboxMenu::Result::Close) sbMenuOpen = false;
        else if (res == SandboxMenu::Result::EquipWeapon)
        {
            const auto* wd = registry.getWeapon(sbMenu.selectedWeaponId());
            if (wd)
            {
                const int slot = sbMenu.weaponSlot();
                player.weapons[slot] = weaponFromDef(*wd);
                if (slot == 0 || player.activeWeapon == slot) player.activeWeapon = slot;
                const char* slotName = slot == 0 ? "primaria" : "secondaria";
                hud.toast(std::string("Arma ") + slotName + ": " + wd->name);
                hud.pushFeed(std::string("ARMA ") + slotName + ": " + wd->name);
                telemetry::logInfo("sandbox: arma cambiata -> " + wd->name);
            }
            sbMenuOpen = false;
        }
        else if (res == SandboxMenu::Result::ToggleSim)
        {
            if (!sbMenu.simRunning) startSimulation();
            else                    stopSimulation();
            sbMenuOpen = false;
        }
        else if (res == SandboxMenu::Result::RestartSandbox)
        {
            currentSettings.mapId = sbMenu.selectedMapId();
            mode = createGameMode("sandbox");
            observerFly = false; sbMenu.simRunning = false;
            initWorld();
            sbMenuOpen = false;
            hud.toast("Sandbox su '" + currentSettings.mapId + "'");
            hud.pushFeed("SANDBOX riavviata su " + currentSettings.mapId);
            telemetry::logInfo("sandbox: riavvio su mappa '" + currentSettings.mapId + "'");
        }
    };

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

            // ── Keybinding: cattura di input NON da tastiera ──────────────
            //    Mentre le opzioni aspettano un nuovo binding, rotella e pulsanti
            //    del mouse valgono come input assegnabili (l'ESC per annullare e i
            //    tasti passano dal ramo SDL_KEYDOWN sotto). È così che l'utente può
            //    mettere "cambia arma sulla rotella" o "ordini sul tasto centrale".
            if (state == GameState::Options && optMenu.isAwaitingKey())
            {
                if (ev.type == SDL_MOUSEWHEEL && ev.wheel.y != 0)
                    optMenu.assignAwaited(ev.wheel.y > 0 ? InputBinding::wheelUp()
                                                         : InputBinding::wheelDown(), input);
                else if (ev.type == SDL_MOUSEBUTTONDOWN)
                    optMenu.assignAwaited(InputBinding::mouseButton(ev.button.button), input);
            }

            // ── Mouse nel menu principale: hover evidenzia, click attiva ──
            if (state == GameState::MainMenu &&
                (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN))
            {
                const bool clicked = (ev.type == SDL_MOUSEBUTTONDOWN &&
                                      ev.button.button == SDL_BUTTON_LEFT);
                const float mx = (float)(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x);
                const float my = (float)(ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y);
                auto res = mainMenu.handleMouse(mx, my, clicked);
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

            // ── Mouse negli altri menu (PreMatch, Opzioni, Sandbox) ───────────
            //    Stesso schema del menu principale: hover evidenzia, click attiva.
            //    Le azioni passano dalle stesse lambda del ramo tastiera.
            if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN)
            {
                const bool clicked = (ev.type == SDL_MOUSEBUTTONDOWN &&
                                      ev.button.button == SDL_BUTTON_LEFT);
                const float mx = (float)(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x);
                const float my = (float)(ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y);

                if (state == GameState::Launcher)
                {
                    auto res = launcher.handleMouse(mx, my, clicked);
                    if (res == LauncherScreen::Result::Launch) goMainMenu();
                    else if (res == LauncherScreen::Result::Quit) window.close();
                }
                else if (state == GameState::PreMatch)
                    applyPreMatchResult(preMatchMenu.handleMouse(mx, my, clicked));
                else if (state == GameState::Options && !optMenu.isAwaitingKey())
                {
                    if (optMenu.handleMouse(mx, my, clicked) == OptionsMenu::Result::Back)
                    {
                        state = prevState; stateChanged = true;
                        if (state == GameState::Playing) window.setMouseCaptured(true);
                    }
                }
                else if (state == GameState::Playing && sbMenuOpen)
                    applySandboxResult(sbMenu.handleMouse(mx, my, clicked));

                // Click sui bottoni degli overlay Pausa / Fine partita
                else if (clicked && (state == GameState::Paused
                                  || state == GameState::Win || state == GameState::Lose))
                {
                    const int st = (state == GameState::Paused) ? -1
                                 : (state == GameState::Win) ? 1 : 2;
                    switch (hud.overlayPick(mx, my, st))
                    {
                    case HUD::OverlayAction::Resume:
                        state = GameState::Playing; stateChanged = true;
                        window.setMouseCaptured(true); break;
                    case HUD::OverlayAction::Restart:  startGame(); break;
                    case HUD::OverlayAction::Respawn:  doVoluntaryRespawn(); break;
                    case HUD::OverlayAction::Options:
                        prevState = GameState::Paused; state = GameState::Options;
                        stateChanged = true; window.setMouseCaptured(false); break;
                    case HUD::OverlayAction::MainMenu: goMainMenu(); break;
                    case HUD::OverlayAction::None: break;
                    }
                }
            }

            // ── Click sinistro sulla mappa di respawn (doc 30) ────────────────
            //    Da vivi il click spara (updateShooting); da morti seleziona il
            //    punto sotto il cursore e schiera (deployPlayerRespawn no-op se
            //    non ancora pronto). Click nel vuoto: nessun effetto.
            if (ev.type == SDL_MOUSEBUTTONDOWN
                && ev.button.button == SDL_BUTTON_LEFT
                && state == GameState::Playing && !sbMenuOpen
                && player.isDead)
            {
                const int idx = hud.respawnMapPick((float)ev.button.x, (float)ev.button.y);
                if (idx >= 0) { respawnSel = idx; deployPlayerRespawn(); }
            }

            if (ev.type == SDL_KEYDOWN)
            {
                const int sc = ev.key.keysym.scancode;

                if (sc == SDL_SCANCODE_F11) window.toggleFullscreen();

                // ── F12: dump completo dello stato (ADR-013 + Phase 4) ─
                if (sc == SDL_SCANCODE_F12)
                {
                    telemetry::dumpGameState(buildStateDump("f12"));
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
                    // StartGame → startFromPreMatch (NON setSettings intera:
                    // azzererebbe personaggio/classe che il PreMatch non possiede
                    // ancora — stessa classe di guasto di ADR-010, qui in memoria;
                    // sparirà quando il PreMatch avrà i selettori). Back → menu.
                    // Stessa lambda del percorso mouse: i due input non divergono.
                    applyPreMatchResult(preMatchMenu.handleKey(sc));
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
                else if (state == GameState::Playing && sbMenuOpen)
                {
                    // Menu sandbox aperto: TUTTI i tasti vanno a lui. Esiti
                    // gestiti dalla stessa lambda del percorso mouse.
                    applySandboxResult(sbMenu.handleKey(sc));
                }
                else if (state == GameState::Playing)
                {
                    if (sc == SDL_SCANCODE_V)
                        player.toggleThirdPerson(cam);

                    // ── Scelta e schieramento del punto di respawn mentre si è
                    //    a terra (doc 25, base della futura mappa tattica): A/D o
                    //    frecce scorrono i punti (Base + command post alleati);
                    //    Invio/Spazio (o click, gestito sotto) SCHIERA al punto
                    //    scelto quando l'attesa minima è finita. Da morto il
                    //    movimento è inerte, quindi A/D non confligge.
                    if (!observerFly && player.isDead)
                    {
                        const int n = (int)mode->availableSpawns().size();
                        if (n > 1)
                        {
                            if (sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_RIGHT)
                                respawnSel = (respawnSel + 1) % n;
                            if (sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_LEFT)
                                respawnSel = (respawnSel + n - 1) % n;
                        }
                        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER
                            || sc == SDL_SCANCODE_SPACE)
                            deployPlayerRespawn();   // no-op finché non è pronto
                    }

                    // ── Menu sandbox (17_SandboxTools): TAB apre l'overlay
                    if (sandbox && sc == SDL_SCANCODE_TAB)
                        sbMenuOpen = true;

                    // ── P: scorciatoia dalla sandbox al PreMatch classico
                    //    (la partita vera si configura/avvia da lì) ──────
                    if (sandbox && sc == SDL_SCANCODE_P)
                    {
                        preMatchMenu.setSettings(currentSettings);
                        state = GameState::PreMatch;
                        stateChanged = true;
                        window.setMouseCaptured(false);
                        telemetry::logInfo("sandbox: scorciatoia P -> PreMatch");
                    }

                    // ── E: sali/scendi dal veicolo (19_Vehicles) ───────
                    if (sc == SDL_SCANCODE_E && !observerFly && !player.isDead)
                    {
                        if (drivenVehicle != 0)
                        {
                            // Scendi: primo lato LIBERO attorno al veicolo
                            // (scendere alla cieca poteva mettere il giocatore
                            // dentro un muro → "incastrato").
                            if (auto* vt = world.getTransform(drivenVehicle))
                            {
                                const float yr = glm::radians(vt->ry);
                                const glm::vec3 right = { std::cos(yr), 0, -std::sin(yr)};
                                const glm::vec3 fwd   = { std::sin(yr), 0,  std::cos(yr)};
                                const glm::vec3 base  = {vt->x, vt->y + 0.4f, vt->z};
                                const glm::vec3 half  = {config::PLAYER_HALF_X,
                                                         config::PLAYER_HALF_Y,
                                                         config::PLAYER_HALF_Z};
                                const glm::vec3 tries[4] = {
                                    base + right * 2.2f,  base - right * 2.2f,
                                    base - fwd * 3.0f,    base + fwd * 3.0f };
                                glm::vec3 out = tries[0];
                                for (const auto& t : tries)
                                    if (!physics::hasCollision(t, half, world))
                                    { out = t; break; }
                                cam.setPosition({out.x, out.y + 0.6f, out.z});
                            }
                            if (auto* vc = world.getVehicle(drivenVehicle))
                                vc->driver = 0;
                            if (auto* tm = world.getTeam(drivenVehicle))
                                tm->teamId = 0;   // di nuovo neutro
                            if (auto* mr = world.getMeshRenderer(drivenVehicle))
                            { mr->r = vehPrevR; mr->g = vehPrevG; mr->b = vehPrevB; }
                            drivenVehicle = 0;
                            hud.toast("Sei sceso dal veicolo");
                            hud.pushFeed("VEICOLO: giocatore a piedi");
                        }
                        else
                        {
                            // Sali: veicolo libero più vicino entro il raggio
                            // In TPS la posizione reale è tpsPlayerPos, non la camera
                            const glm::vec3 pp = player.thirdPerson
                                               ? player.tpsPlayerPos
                                               : cam.getPosition();
                            EntityId best = 0;
                            float bestD2 = config::VEHICLE_MOUNT_RANGE
                                         * config::VEHICLE_MOUNT_RANGE;
                            float nearest2 = 1e9f;
                            for (EntityId id : world.getEntities())
                            {
                                const auto* vc2 = world.getVehicle(id);
                                if (!vc2 || vc2->driver != 0) continue;
                                const auto* vt2 = world.getTransform(id);
                                if (!vt2) continue;
                                const float dx = vt2->x - pp.x, dz = vt2->z - pp.z;
                                const float d2 = dx*dx + dz*dz;
                                if (d2 < nearest2) nearest2 = d2;
                                if (d2 < bestD2) { bestD2 = d2; best = id; }
                            }
                            if (best == 0)
                                telemetry::logTrace("veicolo: E premuto, nessun mezzo in raggio (min "
                                    + std::to_string(std::sqrt(nearest2)) + "m)");
                            if (best != 0)
                            {
                                drivenVehicle = best;
                                world.getVehicle(best)->driver = player.entity;
                                if (auto* tm = world.getTeam(best))
                                    tm->teamId = 1;   // ora bersaglio dei nemici
                                // Feedback visivo: leggera tinta azzurra sul
                                // veicolo guidato (moltiplica, non sovrascrive,
                                // così i colori del modello restano leggibili)
                                if (auto* mr = world.getMeshRenderer(best))
                                {
                                    vehPrevR = mr->r; vehPrevG = mr->g; vehPrevB = mr->b;
                                    mr->r = vehPrevR * 0.6f;
                                    mr->g = vehPrevG * 0.75f;
                                    mr->b = std::min(1.0f, vehPrevB * 1.2f + 0.2f);
                                }
                                hud.toast("Alla guida: W/S accelera, A/D sterza, E scendi");
                                hud.pushFeed("VEICOLO: giocatore alla guida");
                                telemetry::logInfo("veicolo: giocatore alla guida");
                            }
                        }
                    }

                    // ── Log chat: L apre/chiude, PAGSU/PAGGIU scorre ───
                    if (sc == SDL_SCANCODE_L)
                        hud.toggleFeed();
                    if (sc == SDL_SCANCODE_PAGEUP)   hud.scrollFeed(+6);
                    if (sc == SDL_SCANCODE_PAGEDOWN) hud.scrollFeed(-6);

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

        // Cursore assoluto all'HUD per l'hover dei bottoni Pausa/Fine partita.
        { int msx = 0, msy = 0; SDL_GetMouseState(&msx, &msy);
          hud.setMousePos((float)msx, (float)msy); }

        // Il menu sandbox libera il cursore per poterci cliccare; alla chiusura
        // lo riprende. Sincronizzato qui per coprire TUTTE le vie di chiusura.
        if (sbMenuOpen && !sbMouseFreed)
        { window.setMouseCaptured(false); sbMouseFreed = true; }
        else if (!sbMenuOpen && sbMouseFreed)
        { if (state == GameState::Playing) window.setMouseCaptured(true); sbMouseFreed = false; }

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
        const Uint64 nowCounter = SDL_GetPerformanceCounter();
        double frameDt = (double)(nowCounter - prevCounter) / perfFreq;
        prevCounter = nowCounter;
        if (frameDt > 0.25) frameDt = 0.25;         // anti spiral-of-death (invariato)
        const float elapsed = (float)frameDt;        // i sistemi non-simulazione vogliono float
        hud.tick(elapsed);

        if (state == GameState::Playing)
        {
            // Ruota di comando aperta → RALLENTA il tempo di gioco (non pausa):
            // si alimenta l'accumulatore con meno tempo reale, quindi la
            // simulazione avanza meno passi al secondo. Il timestep fisso
            // (fixedDt) resta invariato → fisica/AI deterministiche, solo più
            // lente. Camera e selezione della ruota girano fuori da qui, a
            // velocità reale. `wheelOpen` è del frame precedente (la ruota si
            // gestisce più sotto): scarto di 1 frame, impercettibile.
            const float timeScale = wheelOpen ? config::WHEEL_TIME_SCALE : 1.0f;
            accumulator += frameDt * timeScale;
            while (accumulator >= SIMULATION_STEP)
            {
                mode->update(world, fixedDt);
                // Stati dei command post → mailbox, FRA il mode (che li aggiorna)
                // e i sistemi (che li leggono): ObjectiveSystem vede lo stato di
                // QUESTO tick, non di quello prima. È ciò che permette a
                // CaptureZone/DefendZone di avvolgere ADR-009 senza duplicarne
                // la logica di cattura (doc 25); `ecs/` non può includere
                // CommandPosts, che vive nel game mode.
                world.commandPostStates.clear();
                if (const CommandPosts* cps = mode->commandPosts())
                    for (const auto& s : cps->status())
                        world.commandPostStates.push_back({s.label, s.owner, s.progress01});
                world.tick(fixedDt);
                accumulator -= SIMULATION_STEP;
            }

            // Feedback colpi a segno del giocatore → hitmarker sul mirino
            if (world.combatFeedback.team1Kill)     hud.hitmarker(true);
            else if (world.combatFeedback.team1Hit) hud.hitmarker(false);
            world.combatFeedback.reset();
            world.killedThisTick.clear();   // mailbox per-tick (ADR-020)

            // Log chat: drena gli eventi dei sistemi verso la HUD
            for (auto& msg : world.eventFeed) hud.pushFeed(msg);
            world.eventFeed.clear();

            player.weapon().update(elapsed);
            if (player.weapon().overheated && !wasOverheated) audio.playOverheat();
            wasOverheated = player.weapon().overheated;

            // In osservatore NIENTE respawn: updateRespawn teletrasporterebbe
            // la camera allo spawn e ricreerebbe il player a team 1 (bug:
            // "osservo dall'alto e i droidi iniziano a spararmi").
            //
            // Il respawn NON è automatico QUANDO c'è una scelta: qui si scala
            // solo l'attesa minima e si alimenta l'overlay. Con 2+ punti il
            // rientro avviene in `deployPlayerRespawn` alla CONFERMA del
            // giocatore (click/Invio) — così la scelta ha la precedenza anche con
            // respawnDelay basso (KI #56). Se invece c'è UN solo punto (nessun
            // post catturato) non c'è nulla da scegliere: si torna al respawn
            // automatico di prima, per non aggiungere attrito a chi gioca con
            // respawn brevi.
            if (!observerFly && player.isDead)
            {
                if (player.respawnTimer > 0.0f)
                    player.respawnTimer -= elapsed;

                // La lista può cambiare mentre si aspetta (un post cade): si
                // riclampa l'indice per non uscire dai limiti.
                const auto spawns = mode->availableSpawns();
                if (respawnSel < 0 || respawnSel >= (int)spawns.size())
                    respawnSel = 0;

                if (spawns.size() <= 1)
                {
                    // Un solo punto: nessuna scelta → respawn automatico allo
                    // scadere del timer, nessuna mappa (niente attrito, KI #56).
                    hud.setRespawnMap({});
                    if (player.respawnTimer <= 0.0f) deployPlayerRespawn();
                }
                else
                {
                    // 2+ punti: mappa top-down cliccabile (doc 30). Rilascia il
                    // cursore per poter cliccare i marker; lo riprende il deploy.
                    if (window.isMouseCaptured()) window.setMouseCaptured(false);
                    hud.setRespawnMap(buildRespawnMap(spawns));
                }
            }
            else
                hud.setRespawnMap({});
        }

        // ── Ruota di comando (doc 26, livello 2) ─────────────────────
        //    Tenuto il tasto: la camera si CONGELA e il movimento del mouse
        //    sceglie il settore (Regroup/Hold/Advance); al rilascio si impartisce
        //    l'ordine di squadra. Un ordine che non ferma l'azione è il requisito
        //    del doc — qui l'azione si mette in pausa solo mentre si sceglie.
        {
            const bool wheelWasOpen = wheelOpen;
            const bool canWheel = state == GameState::Playing && !observerFly
                                && !sbMenuOpen && drivenVehicle == 0
                                && window.isMouseCaptured();
            wheelOpen = canWheel && input.isDown(Action::CommandWheel);
            if (wheelOpen)
            {
                if (!wheelWasOpen) { wheelDirX = wheelDirY = 0.0f; wheelSel = -1; }
                wheelDirX += (float)input.mouseDX();
                wheelDirY += (float)input.mouseDY();
                const float mag = std::sqrt(wheelDirX*wheelDirX + wheelDirY*wheelDirY);
                if (mag > 24.0f)   // dead zone: un micromovimento non seleziona
                {
                    const float ang = std::atan2(wheelDirY, wheelDirX);
                    // Centri: Regroup basso-sx, Hold basso-dx, Advance in alto
                    // (Y schermo verso il basso → Advance = -90°).
                    const float c[3] = { 2.356f, 0.785f, -1.5708f };
                    int best = 0; float bestD = 1e9f;
                    for (int i = 0; i < 3; ++i)
                    {
                        const float dd = std::fabs(std::atan2(std::sin(ang-c[i]),
                                                              std::cos(ang-c[i])));
                        if (dd < bestD) { bestD = dd; best = i; }
                    }
                    wheelSel = best;
                }
                else wheelSel = -1;
            }
            else if (wheelWasOpen && wheelSel >= 0)
            {
                // Rilascio con un settore scelto → ordine di squadra.
                SquadOrderRequest req; req.pending = true;
                const auto* pt = world.getTransform(player.entity);
                const glm::vec3 pp = pt ? glm::vec3{pt->x, pt->y, pt->z} : cam.getPosition();
                if (wheelSel == 0)        // REGROUP: raduna sul leader (giocatore)
                { req.order = OrderType::MoveTo; req.targetX = pp.x; req.targetZ = pp.z; }
                else if (wheelSel == 1)   // HOLD: ognuno tiene la propria posizione
                { req.order = OrderType::HoldPosition; }
                else                      // ADVANCE: avanza nella direzione di mira
                {
                    glm::vec3 fwd = cam.getForward(); fwd.y = 0.0f;
                    const float fl = glm::length(fwd);
                    fwd = (fl > 0.001f) ? fwd / fl : glm::vec3{0,0,1};
                    req.order = OrderType::MoveTo;
                    req.targetX = pp.x + fwd.x * 15.0f;
                    req.targetZ = pp.z + fwd.z * 15.0f;
                }
                world.squadOrder = req;
                hud.toast(wheelSel==0 ? "Squadra: REGROUP"
                        : wheelSel==1 ? "Squadra: HOLD"
                                      : "Squadra: ADVANCE");
                wheelSel = -1;
            }
            hud.setCommandWheel(wheelOpen, wheelSel);
        }

        // ── 4. CAMERA + PHYSICS ──────────────────────────────────────
        // Alla guida la camera segue il veicolo (VehicleDrive): niente
        // mouse-look, altrimenti lo sterzo sembra invertito. La ruota di comando
        // CONGELA la camera mentre si sceglie il settore (mouse = selezione).
        if (state == GameState::Playing && window.isMouseCaptured()
            && !sbMenuOpen && drivenVehicle == 0 && !wheelOpen)
            player.processMouse(cam, (float)input.mouseDX(), (float)input.mouseDY());

        if (state == GameState::Playing)
        {
            if (observerFly)
            {
                // Volo libero osservatore (17_SandboxTools): camera sganciata
                // dal PlayerController, WASD + SPAZIO/CTRL, velocità alta.
                const Uint8* kb = SDL_GetKeyboardState(nullptr);
                cam.setSpeed(14.0f);
                cam.processKeyboard(kb[SDL_SCANCODE_W] != 0, kb[SDL_SCANCODE_S] != 0,
                                    kb[SDL_SCANCODE_A] != 0, kb[SDL_SCANCODE_D] != 0,
                                    kb[SDL_SCANCODE_SPACE] != 0,
                                    kb[SDL_SCANCODE_LCTRL] != 0, elapsed);
            }
            else if (drivenVehicle != 0)
            {
                // ── Guida veicolo (19_Vehicles): fisica+camera estratte in
                //    game/VehicleDrive.hpp (R2) ────────────────────────────
                if (!vehicledrive::update(world, drivenVehicle, cam, elapsed,
                                          player.thirdPerson, vehTraceCnt))
                {
                    drivenVehicle = 0;
                    hud.toast("Veicolo distrutto!");
                    hud.pushFeed("VEICOLO distrutto sotto il giocatore");
                }
            }
            else if (!sbMenuOpen)
                player.updateMovement(cam, input, world, elapsed);
        }

        // ── 5. GAME LOGIC ────────────────────────────────────────────
        if (state == GameState::Playing)
        {
            if (!observerFly && !player.isDead && world.isValidEntity(player.entity))
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

            // In osservatore il giocatore non partecipa: niente morte,
            // ticket o respawn (l'entità parcheggiata può prendere colpi
            // vaganti — vanno ignorati).
            if (!observerFly && !player.isDead && !world.isValidEntity(player.entity))
            {
                player.isDead = true;
                player.prevHp = 0.0f;
                onPlayerDeath();
            }
            // updateHealth() imposta già isDead/prevHp da sé quando rileva la morte.
            if (!observerFly && player.updateHealth(world, audio))
                onPlayerDeath();

            // Alla guida non si spara (19_Vehicles Fase A: niente armi di bordo)
            if (!observerFly && !sbMenuOpen && drivenVehicle == 0)
                player.updateShooting(world, cam, input, audio,
                                       mesh.get(), window.isMouseCaptured());

            // ── Feedback di mira: il mirino diventa rosso se punta una
            //    hitbox nemica reale. USA LO STESSO test OBB dei proiettili
            //    (physics/HitTest.hpp, KI #13): il raggio è un segmento di
            //    80m — mirino e colpi concordano per costruzione. ──────────
            EntityId aimEntity = 0;   // NEMICO inquadrato (0 = nessuno) → crosshair + FocusFire
            EntityId aimAlly   = 0;   // COMPAGNO inquadrato (anche a terra) → Revive/CoveringFire
            {
                bool aimOn = false;
                // Entità inquadrata: la risolve GIÀ questo loop: riusarla per
                // l'ordine contestuale (ADR-020 Phase B) garantisce che ciò che
                // il mirino segna sia ciò che la squadra riceve come bersaglio.
                // Ora distingue nemico e COMPAGNO: gli ordini che puntano un
                // alleato (rianima / fuoco di copertura) hanno bisogno del secondo.
                aimEntity = 0;
                const glm::vec3 ro = cam.getPosition();
                const glm::vec3 rd = cam.getForward();
                const glm::vec3 re = ro + rd * 80.0f;
                for (EntityId id : world.getEntities())
                {
                    if (id == player.entity) continue;   // non ci si mira da soli
                    const auto* tm2 = world.getTeam(id);
                    const auto* eh2 = world.getHealth(id);
                    const auto* tr2 = world.getTransform(id);
                    if (!tm2 || !tr2 || world.getBullet(id)) continue;
                    const bool ally = (tm2->teamId == 1);
                    // Nemico: dev'essere vivo (i morti spariscono). Compagno: anche
                    // A TERRA (hp 0) resta mirabile — è il caso del comando Revive.
                    if (!ally && (!eh2 || eh2->current <= 0.0f)) continue;

                    const auto* hb2 = world.getHitbox(id);
                    const auto* mr2 = world.getMeshRenderer(id);
                    const float sc  = (tr2->sx > 0.0001f) ? tr2->sx : 1.0f;
                    const float mo  = mr2 ? mr2->meshOffsetY : 0.0f;
                    const glm::vec3 ep{tr2->x, tr2->y, tr2->z};

                    if (const auto* vc2 = world.getVehicle(id))
                    {
                        // Veicolo: stesso volume di danno OBB del CombatSystem,
                        // così il mirino diventa rosso su tutta la carrozzeria.
                        HitZone box;
                        box.offset      = {0.0f, vc2->hitOffsetY, 0.0f};
                        box.halfExtents = {vc2->hitHalfX, vc2->hitHalfY, vc2->hitHalfZ};
                        if (hittest::segmentInZone(ro, re, ep, 1.0f, tr2->ry, 0.0f, box))
                            aimOn = true;
                    }
                    else if (hb2 && hb2->profile && !hb2->profile->zones.empty())
                    {
                        for (const auto& z : hb2->profile->zones)
                            if (hittest::segmentInZone(ro, re, ep, sc,
                                                       tr2->ry, mo, z))
                            { aimOn = true; break; }
                    }
                    else
                    {
                        const glm::vec3 to = ep - ro;
                        const float t = glm::dot(to, rd);
                        if (t > 0.0f && t < 80.0f
                            && glm::length(ep - (ro + rd * t)) < 0.7f)
                            aimOn = true;
                    }
                    if (aimOn)
                    {
                        if (ally) aimAlly = id;   // compagno (Revive/CoveringFire)
                        else      aimEntity = id; // nemico (crosshair + FocusFire)
                        break;
                    }
                }
                hud.setAimOnTarget(aimEntity != 0);   // crosshair rosso solo sui nemici
                hud.setAimOnAlly(aimAlly != 0);        // verde sui compagni (feedback comandi)
            }

            // ── Ordine contestuale alla squadra (ADR-020 Phase B, doc 26) ──
            //    Un tasto, nessun menu: la tattica sta DENTRO il flusso
            //    dell'azione (un comando che obbliga a fermarsi ha già fallito
            //    il requisito di design). Il contesto lo decide il mirino:
            //    nemico → FocusFire; cover point vicino → TakeCover; else MoveTo.
            //    L'intenzione va in una MAILBOX sul World: `ecs/` non conosce
            //    l'input (ADR-002/doc 10). ────────────────────────────────────
            if (!observerFly && !sbMenuOpen && drivenVehicle == 0
                && window.isMouseCaptured() && input.isPressed(Action::SquadOrder))
            {
                SquadOrderRequest req;
                if (aimEntity != 0)
                {
                    req.order        = OrderType::FocusFire;   // nemico inquadrato
                    req.targetEntity = aimEntity;
                    req.pending      = true;
                }
                else if (aimAlly != 0)
                {
                    const auto* asq = world.getSquad(aimAlly);
                    if (asq && asq->downed)
                    {
                        // Compagno A TERRA → manda il membro vivo più vicino a
                        // rianimarlo (comando esplicito, oltre all'auto-soccorso).
                        const auto* dt = world.getTransform(aimAlly);
                        EntityId reviver = 0; float best2 = 1e30f;
                        if (dt)
                            for (EntityId o : world.getEntities())
                            {
                                const auto* osq = world.getSquad(o);
                                if (!osq || osq->squadId == 0 || osq->isLeader || osq->downed) continue;
                                if (o == aimAlly) continue;
                                const auto* ot = world.getTransform(o);
                                if (!ot) continue;
                                const float ddx = ot->x - dt->x, ddz = ot->z - dt->z;
                                const float d2 = ddx*ddx + ddz*ddz;
                                if (d2 < best2) { best2 = d2; reviver = o; }
                            }
                        if (reviver != 0)
                        {
                            req.order          = OrderType::Revive;
                            req.targetEntity   = aimAlly;
                            req.directedMember = reviver;
                            req.pending        = true;
                            hud.toast("Rianimazione: compagno in soccorso");
                        }
                        else
                            hud.toast("Nessun compagno disponibile per la rianimazione");
                    }
                    else
                    {
                        // Compagno vivo → fuoco di copertura: tiene la posizione
                        // (dove si trova ora) e spara di supporto.
                        req.order          = OrderType::CoveringFire;
                        req.directedMember = aimAlly;
                        if (const auto* at = world.getTransform(aimAlly))
                        { req.targetX = at->x; req.targetZ = at->z; }
                        req.pending        = true;
                        hud.toast("Fuoco di copertura");
                    }
                }
                else
                {
                    // Punto mirato a terra: interseca il raggio col piano
                    // orizzontale dei piedi del giocatore (il terreno su cui la
                    // squadra cammina), non con y=0 — il pavimento non è a 0.
                    const glm::vec3 ro = cam.getPosition();
                    const glm::vec3 rd = cam.getForward();
                    const auto* ptr = world.getTransform(player.entity);
                    const float planeY = ptr ? (ptr->y - config::PLAYER_HALF_Y) : 0.0f;
                    if (rd.y < -0.05f)   // deve puntare verso il basso
                    {
                        const float t = (planeY - ro.y) / rd.y;
                        const glm::vec3 gp = ro + rd * t;
                        req.targetX = gp.x; req.targetZ = gp.z;

                        // Cover point REALE del MapDef entro 4m dal punto mirato
                        // (doc 15/18) → l'intenzione diventa TakeCover.
                        req.order = OrderType::MoveTo;
                        if (const MapDef* md = world.activeMap)
                        {
                            float best2 = 4.0f * 4.0f;
                            for (const auto& c : md->coverPoints)
                            {
                                const float dx = c.x - gp.x, dz = c.z - gp.z;
                                const float d2 = dx*dx + dz*dz;
                                if (d2 < best2)
                                { best2 = d2; req.targetX = c.x; req.targetZ = c.z;
                                  req.order = OrderType::TakeCover; }
                            }
                        }
                        req.pending = true;
                    }
                    else
                        hud.toast("Ordine: punta una posizione a terra");
                }

                // Raggiungibilità PRIMA di impartire: findPath restituisce un path
                // PARZIALE se il bersaglio è irraggiungibile (non fallisce), quindi
                // si confronta l'arrivo col punto chiesto. Senza questo si possono
                // ordinare mete impossibili — es. la "Collina Centrale" di firebase,
                // 1m > agentClimb, scollegata dal pavimento (KI #34).
                // Il pre-check di raggiungibilità vale solo per gli ordini di
                // MOVIMENTO verso un punto (MoveTo/TakeCover). FocusFire vincola un
                // bersaglio; Revive insegue un compagno (meta dinamica); CoveringFire
                // tiene la posizione dove il compagno è già → sempre raggiungibile.
                if (req.pending && nav.crowdReady()
                    && req.order != OrderType::FocusFire
                    && req.order != OrderType::Revive
                    && req.order != OrderType::CoveringFire)
                {
                    const auto* ptr = world.getTransform(player.entity);
                    std::vector<glm::vec3> path;
                    const glm::vec3 from = ptr ? glm::vec3{ptr->x, ptr->y, ptr->z}
                                               : cam.getPosition();
                    const bool ok = nav.findPath(from, {req.targetX, from.y, req.targetZ}, path)
                        && !path.empty()
                        && glm::length(glm::vec2{path.back().x - req.targetX,
                                                 path.back().z - req.targetZ}) < 2.0f;
                    if (!ok)
                    {
                        req.pending = false;
                        hud.toast("Ordine rifiutato: posizione irraggiungibile");
                        world.pushEvent("Ordine rifiutato: posizione irraggiungibile");
                    }
                }
                if (req.pending) world.squadOrder = req;
            }

            // ── Debrief di fine missione (doc 25 / GDD 9.6) ───────────────
            //    "Il risultato è narrativo, non un semplice voto": qui NON si
            //    calcola un punteggio — si mostra l'INSIEME dei fattori, che è
            //    ciò che racconta com'è andata. I pesi (→ esperienza) sono
            //    progressione (doc 27) e vanno decisi dal design, non qui.
            //    Si mostrano solo i fatti REALI: niente riga per ciò che non è
            //    successo, e niente statistiche che non sappiamo misurare.
            {
                const auto& st = world.missionStats;
                std::vector<std::string> lines;
                char b[96];
                if (world.activeMission)
                {
                    std::snprintf(b, sizeof(b), "Obiettivi: %d completati, %d falliti",
                                  st.objectivesDone, st.objectivesFailed);
                    lines.emplace_back(b);
                    const int mm = (int)(st.missionTime / 60.0f);
                    const int ss = (int)st.missionTime % 60;
                    std::snprintf(b, sizeof(b), "Tempo: %d:%02d", mm, ss);
                    lines.emplace_back(b);
                }
                std::snprintf(b, sizeof(b), "Nemici eliminati: %d  (tuoi: %d)",
                              st.teamKills, st.playerKills);
                lines.emplace_back(b);
                std::snprintf(b, sizeof(b), "Alleati persi: %d", st.alliesLost);
                lines.emplace_back(b);
                std::snprintf(b, sizeof(b), "Tue cadute: %d", st.playerDeaths);
                lines.emplace_back(b);
                hud.setDebrief(std::move(lines));
            }

            // ── Obiettivi → HUD (ADR-019) ─────────────────────────────────
            //    Letti dallo STATO REALE del sistema: l'HUD non deve mai mostrare
            //    un obiettivo che il runtime non ha davvero. Gli inattivi (non
            //    ancora sbloccati) non si mostrano: rivelerebbero la struttura
            //    della missione prima del tempo.
            {
                std::vector<HUD::ObjectiveLine> lines;
                for (const auto& r : objectives->objectives())
                {
                    if (!r.def || r.state == ObjectiveSystem::State::Inactive) continue;
                    HUD::ObjectiveLine l;
                    l.primary = (r.def->tier == ObjectiveTier::Primary);
                    l.state   = (r.state == ObjectiveSystem::State::Completed) ? 1
                              : (r.state == ObjectiveSystem::State::Failed)    ? 2 : 0;
                    // Il progresso si mostra solo dove ESISTE davvero un conteggio:
                    // inventarne uno per gli altri tipi sarebbe un numero falso.
                    char buf[96];
                    if (r.def->type == ObjectiveType::EliminateTarget)
                        std::snprintf(buf, sizeof(buf), "%s  %d/%d",
                                      r.def->name.c_str(), r.progress, r.def->count);
                    else if (r.def->type == ObjectiveType::HoldAreaForDuration
                             && r.state == ObjectiveSystem::State::Active)
                        std::snprintf(buf, sizeof(buf), "%s  %.0f/%.0fs",
                                      r.def->name.c_str(), r.holdTime, r.def->holdSeconds);
                    else
                        std::snprintf(buf, sizeof(buf), "%s", r.def->name.c_str());
                    l.label = buf;
                    lines.push_back(std::move(l));
                }
                hud.setObjectives(std::move(lines));
            }

            // ── Pannello squadra → HUD (doc 26: ordine, stato, distanza) ───
            //    Legge lo stato reale dei membri: la HUD non deve MAI mostrare
            //    un ordine che i membri non hanno davvero.
            {
                int members = 0;
                bool found = false;
                OrderType ord = OrderType::None;
                float dist = 0.0f;
                int   downed = 0;
                float mostUrgentBleed = -1.0f;   // -1 = nessuno a terra
                for (EntityId id : world.getEntities())
                {
                    const auto* sq = world.getSquad(id);
                    if (!sq || sq->squadId == 0 || sq->isLeader) continue;
                    ++members;
                    // A terra (Phase C): conta e traccia il timer più urgente.
                    if (sq->downed)
                    {
                        ++downed;
                        if (mostUrgentBleed < 0.0f || sq->bleedoutRemaining < mostUrgentBleed)
                            mostUrgentBleed = sq->bleedoutRemaining;
                        continue;   // un a-terra non ha un ordine da mostrare
                    }
                    if (found || !sq->hasActiveOrder()) continue;
                    ord = sq->order; found = true;
                    if (const auto* t = world.getTransform(id))
                        dist = glm::length(glm::vec2{sq->targetX - t->x,
                                                     sq->targetZ - t->z});
                }
                if (members == 0) hud.setSquadOrder("");
                else
                {
                    char sb[128];
                    char downTag[48] = "";
                    // Lo stato "a terra" è la cosa più urgente: va davanti, con il
                    // conto alla rovescia della perdita più imminente.
                    if (downed > 0)
                        std::snprintf(downTag, sizeof(downTag), "  [A TERRA %d — %.0fs]",
                                      downed, mostUrgentBleed);
                    // FocusFire non ha una destinazione: mostrarne una distanza
                    // sarebbe un numero inventato.
                    if (found && ord != OrderType::FocusFire)
                        std::snprintf(sb, sizeof(sb), "SQUADRA (%d)  %s  %.0fm%s",
                                      members, orderName(ord), dist, downTag);
                    else if (found)
                        std::snprintf(sb, sizeof(sb), "SQUADRA (%d)  %s%s",
                                      members, orderName(ord), downTag);
                    else
                        std::snprintf(sb, sizeof(sb), "SQUADRA (%d)%s", members, downTag);
                    hud.setSquadOrder(sb);
                }
            }

            // ── Stato command post → HUD (ADR-014/#6) ─────────────────
            if (const CommandPosts* cps = mode->commandPosts())
            {
                std::vector<HUD::PostStatus> ps;
                for (const auto& s : cps->status())
                    ps.push_back({s.label, s.owner, s.capturingTeam, s.progress01});
                hud.setPosts(ps);
            }

            // ── Esito: MODE (ADR-014) + MISSIONE (ADR-019) ────────────
            //    In osservazione la partita non finisce mai (si guarda la
            //    battaglia AI). Divisione di doc 25: il mode decide le REGOLE
            //    (ticket), gli obiettivi decidono COSA fare — quindi entrambi
            //    possono chiudere la partita. Il mode ha la precedenza: se ha
            //    già deciso (ticket esauriti) la missione non lo ribalta;
            //    altrimenti l'esito della missione È l'esito della partita.
            //    Senza questo, `ObjectiveSystem::outcome()` era codice morto e
            //    completare una missione non faceva assolutamente nulla.
            MatchOutcome oc = observerFly ? MatchOutcome::Ongoing
                                          : mode->outcome(world);
            if (!observerFly && oc == MatchOutcome::Ongoing)
            {
                switch (objectives->outcome())
                {
                case ObjectiveSystem::Outcome::Success: oc = MatchOutcome::Team1Win; break;
                case ObjectiveSystem::Outcome::Failure: oc = MatchOutcome::Team2Win; break;
                default: break;
                }
            }
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

        // Fine partita (Phase 4): al PRIMO ingresso in Win/Lose, dump completo
        // dello stato (qualunque path l'abbia deciso). Reset a Playing per la
        // partita successiva.
        if (state == GameState::Win || state == GameState::Lose)
        {
            if (endDumpState != state)
            {
                endDumpState = state;
                telemetry::dumpGameState(buildStateDump(
                    state == GameState::Win ? "match_win" : "match_lose"));
                // Debrief anche su JSONL (doc 21): il "giudizio" è un insieme di
                // fattori, e deve essere leggibile da un tool/LLM senza guardare
                // lo schermo — è da qui che la progressione (doc 27) prenderà
                // l'esperienza quando esisterà.
                const auto& st = world.missionStats;
                telemetry::event(telemetry::Level::Info, "GameMode", "match end",
                    {{"outcome", state == GameState::Win ? "win" : "lose"},
                     {"mission", world.activeMission ? currentSettings.missionId : ""},
                     {"objectives_done",   st.objectivesDone},
                     {"objectives_failed", st.objectivesFailed},
                     {"mission_time",      st.missionTime},
                     {"team_kills",        st.teamKills},
                     {"player_kills",      st.playerKills},
                     {"allies_lost",       st.alliesLost},
                     {"player_deaths",     st.playerDeaths}});
            }
        }
        else endDumpState = GameState::Playing;

        // ── 6. RENDER ────────────────────────────────────────────────
        renderer.beginFrame();

        if (worldReady && state == GameState::Playing)
        {
            auto drawScene = [&](const Camera& viewCam)
            {
                ZoneScopedN("render.drawScene");   // ADR-015 (equiv. Application::render)
                for (EntityId id : world.getEntities())
                {
                    const auto* tr = world.getTransform(id);
                    const auto* mr = world.getMeshRenderer(id);
                    if (!tr || !mr || !mr->visible || !mr->mesh) continue;
                    // meshOffsetY sposta verticalmente la mesh rispetto al centro
                    // fisico dell'entità (es. per appoggiare i piedi a terra).
                    glm::mat4 model = toModelMatrix(*tr);
                    // Rotazione visiva extra (veicoli): raddrizza il muso senza
                    // toccare la direzione di marcia (transform.ry).
                    if (mr->yawOffsetDeg != 0.0f)
                        model = model * glm::rotate(glm::mat4(1.0f),
                                    glm::radians(mr->yawOffsetDeg), glm::vec3(0,1,0));
                    if (mr->meshOffsetY != 0.0f)
                        model = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(0.0f, mr->meshOffsetY, 0.0f)) * model;
                    // Indicatore "a terra" (Phase C): manca una posa prone, quindi
                    // il segnale è un TINT ROSSO — dice a colpo d'occhio QUALE clone
                    // è a terra (l'HUD dice quanti/quanto). Riutilizzabile poi per
                    // un HUD dei cloni più ricco.
                    glm::vec3 tint = {mr->r, mr->g, mr->b};
                    if (const auto* sqd = world.getSquad(id); sqd && sqd->downed)
                        tint = {0.85f, 0.12f, 0.12f};
                    renderer.drawMeshFrom(viewCam, *mr->mesh, mr->texture, model, tint);

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

            // ── Viewmodel arma (Todo #11): arma visibile in prima persona.
            //    Non in osservatore, non alla guida, non in TPS. ───────────
            if (!observerFly && drivenVehicle == 0 && !player.thirdPerson
                && !player.isDead && !player.weapon().meshPath.empty())
            {
                auto itW = meshCache.find(player.weapon().meshPath);
                if (itW != meshCache.end() && itW->second)
                {
                    const glm::vec3 f = cam.getForward();
                    glm::vec3 r = glm::cross(f, glm::vec3(0, 1, 0));
                    if (glm::length(r) > 0.001f) r = glm::normalize(r);
                    const glm::vec3 u = glm::cross(r, f);
                    const glm::vec3 p = cam.getPosition()
                                      + f * 0.55f + r * 0.30f - u * 0.26f;
                    glm::mat4 basis(1.0f);
                    basis[0] = glm::vec4(r, 0.0f);
                    basis[1] = glm::vec4(u, 0.0f);
                    basis[2] = glm::vec4(f, 0.0f);
                    // Convenzione GLB lungo +X (yaw 90 = forward) più il
                    // raddrizzamento per-arma mesh_rot_y dal Weapon Editor.
                    const glm::mat4 model =
                        glm::translate(glm::mat4(1.0f), p) * basis
                        * glm::rotate(glm::mat4(1.0f),
                                      glm::radians(90.0f + player.weapon().meshRotY),
                                      glm::vec3(0, 1, 0))
                        * glm::scale(glm::mat4(1.0f),
                                     glm::vec3(player.weapon().meshScale));
                    renderer.drawMesh(*itW->second, nullptr, model,
                                      {0.75f, 0.78f, 0.85f});
                }
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
                       player.weapon().heat, player.weapon().overheated, player.weapon().name.c_str(),
                       mode->getTeam1Tickets(), mode->getTeam2Tickets(),
                       aliveAllies, aliveEnemies);

            if (sbMenuOpen && state == GameState::Playing)
                sbMenu.render();
        }

        renderer.endFrame();

        // Fine frame renderizzato (dopo lo swap in endFrame → SDL_GL_SwapWindow):
        // delimita il frame per Tracy. No-op se il profiler è disabilitato.
        FrameMark;

        // Frame-cap di sicurezza SOLO se la VSync è spenta (swap interval 0):
        // senza pacing GPU il loop girerebbe a migliaia di FPS. Sleep IBRIDO
        // (SDL_Delay grossolano + busy-wait finale) perché lo Sleep di Windows
        // ha risoluzione ~15ms e da solo darebbe frame-time irregolari. Con
        // VSync ON (default) questo blocco è inerte (un solo check per frame).
        if (SDL_GL_GetSwapInterval() == 0)
        {
            const double target = 1.0 / (double)config::MAX_UNCAPPED_FPS;
            const double soFar  = (double)(SDL_GetPerformanceCounter() - prevCounter) / perfFreq;
            const double remain = target - soFar;
            if (remain > 0.002)
                SDL_Delay((Uint32)((remain - 0.002) * 1000.0));   // dormi fino a ~2ms dal target
            while (((double)(SDL_GetPerformanceCounter() - prevCounter) / perfFreq) < target)
            { /* busy-wait sub-ms finale */ }
        }

        // Fine frame: svuota il buffer del log JSONL su disco (ADR-016), così
        // session_latest.jsonl è sempre leggibile a valle di ogni frame.
        telemetry::flushEvents();
    }

    telemetry::setStateDumpCallback(nullptr);   // evita dangling dopo il loop
    telemetry::shutdown();
}

} // namespace mini
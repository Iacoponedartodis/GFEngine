#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/MapQuery.hpp"
#include "mini/game/WeaponAttach.hpp"
#include "mini/game/ClassResolve.hpp"   // ADR-022: unica fonte di "la classe vince"
#include "mini/game/VehicleSpawn.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <nlohmann/json.hpp>   // event() data (ADR-016)

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

namespace mini
{

static constexpr float SPAWN_Y  = 0.86f;
static constexpr float AI_GND_Y = 0.50f;
static constexpr float AI_PLT_Y = 2.50f;
static constexpr float SPAWN_Z  = 8.0f;

namespace
{
struct ResolvedEnemyArchetype
{
    std::string enemyId;
    std::string hitboxProfileId;
    std::string meshPath;
    std::string weaponId;
    std::string aiProfileId;
    std::vector<std::string> abilityIds;

    float meshRotX  = 0.0f;
    float meshRotY  = 0.0f;
    float meshScale = 1.0f;

    float hp        = 80.0f;
    float moveSpeed = 2.5f;
    float interval  = 2.2f;
    float range     = 18.0f;

    float mr = 0.70f, mg = 0.10f, mb = 0.10f;
    float br = 1.00f, bg = 0.50f, bb = 0.00f;

    // Stats arma primaria (da WeaponDef)
    float bulletSpeed    = 8.0f;
    float bulletDamage   = 20.0f;
    float bulletLifetime = 5.0f;
};

// Risolve un archetipo unità (nemico O alleato: stessa EnemyDef, cambia il
// registry di provenienza e i default). Prima il path alleati re-implementava
// questa logica a mano con stats proiettile hardcoded 8/20/5 (Todo A7).
static ResolvedEnemyArchetype resolveUnitArchetype(const DefinitionRegistry* registry,
                                                   const std::string& unitId, int team)
{
    const bool ally = (team == 1);
    ResolvedEnemyArchetype out;
    out.enemyId = unitId;
    if (ally)
    {   // default alleato (colori/stats storici del path alleati)
        out.mr = 0.25f; out.mg = 0.45f; out.mb = 1.0f;
        out.br = 0.30f; out.bg = 0.60f; out.bb = 1.0f;
        out.hp = 60.0f; out.moveSpeed = 1.8f;
        out.interval = 3.5f; out.range = 14.0f;
    }

    const EnemyDef* enemy = registry
        ? (ally ? registry->getAlly(unitId) : registry->getEnemy(unitId))
        : nullptr;
    if (!enemy)
    {
        std::cerr << "[ConquestMode] Unita' '" << unitId
                  << "' non trovata nel registry (team " << team
                  << "). Uso fallback.\n";
        out.hitboxProfileId = unitId;
        return out;
    }

    out.hp = enemy->hp;
    out.moveSpeed = enemy->moveSpeed;
    // Colore direttamente dall'EnemyDef
    out.mr = enemy->color[0];
    out.mg = enemy->color[1];
    out.mb = enemy->color[2];
    out.br = enemy->bulletColor[0];
    out.bg = enemy->bulletColor[1];
    out.bb = enemy->bulletColor[2];
    out.hitboxProfileId = enemy->hitboxProfileId.empty() ? enemy->id : enemy->hitboxProfileId;
    out.meshPath  = enemy->meshPath;
    out.meshRotX  = enemy->meshRotX;
    out.meshRotY  = enemy->meshRotY;
    out.meshScale = enemy->meshScale;
    out.weaponId  = enemy->primaryWeaponId();
    out.aiProfileId = enemy->aiProfileId;
    out.abilityIds  = enemy->abilityIds;

    // ── Classe (ADR-022, metà NPC) ───────────────────────────────────
    // La classe è una PROFESSIONE: fornisce loadout, comportamento e abilità
    // all'unità che la referenzia — è ciò che rende una squadra Trooper+Heavy+
    // Recon diversa da una monoclasse (GDD 12.3). Ogni campo della classe vince
    // solo se è VALORIZZATO: così un'unità può referenziare una classe e tenersi
    // comunque una particolarità. Nessuna classe → tutto come prima (additivo).
    // Arma e profilo passano da `classres`, NON da una copia locale della regola:
    // averla scritta solo qui è ciò che ha lasciato divergere il modello in mano
    // e i bullet stats (2026-07-17). Le abilità restano qui perché nessun altro
    // consumatore le legge grezze.
    if (registry)
    {
        out.weaponId    = classres::primaryWeaponId(*registry, *enemy);
        out.aiProfileId = classres::aiProfileId(*registry, *enemy);
        if (!enemy->classId.empty())
        {
            if (const ClassDef* cls = registry->getClass(enemy->classId))
            {
                if (!cls->abilityIds.empty()) out.abilityIds = cls->abilityIds;
            }
            else
                std::cerr << "[ConquestMode] Classe '" << enemy->classId
                          << "' non trovata per unita' '" << enemy->id
                          << "'. Uso i campi propri dell'unita'.\n";
        }
    }

    if (registry)
    {
        const AiProfileDef* ai = registry->getAiProfile(out.aiProfileId);
        if (ai)
        {
            out.moveSpeed = ai->patrolSpeed;
            out.interval  = ai->shootInterval;
            out.range     = ai->sightRange;
        }
        else
        {
            // out.aiProfileId, non enemy->aiProfileId: col classId impostato il
            // profilo viene dalla CLASSE, e stampare l'altro indicherebbe il
            // dato sbagliato da correggere.
            std::cerr << "[ConquestMode] AI profile '" << out.aiProfileId
                      << "' non trovato per enemy '" << enemy->id
                      << "'. Uso fallback AI.\n";
        }

        // Bullet stats dall'arma primaria EFFETTIVA (`out.weaponId`, già risolta
        // dalla classe) — NON da `enemy->primaryWeaponId()`: leggere il campo
        // grezzo qui faceva sparare all'unità l'arma della classe con i danni,
        // la velocità e il colore del proiettile dell'arma dell'entità.
        const WeaponDef* wpn = registry->getWeapon(out.weaponId);
        if (wpn)
        {
            out.bulletSpeed    = wpn->bulletSpeed;
            out.bulletDamage   = wpn->damage;
            out.bulletLifetime = wpn->bulletLifetime;
            out.br = wpn->bulletColor[0];
            out.bg = wpn->bulletColor[1];
            out.bb = wpn->bulletColor[2];
        }
    }

    // `weapon` e `class` nella diagnostica: la risoluzione della classe era
    // invisibile: si poteva leggere hp/move/range e non accorgersi che l'arma
    // effettiva divergeva da quella impugnata (bug 2026-07-17). Un valore che
    // nessun log mostra è un valore che nessuno controlla.
    std::cout << "[ConquestMode] Enemy resolved: " << enemy->id
              << " class=" << (enemy->classId.empty() ? "(nessuna)" : enemy->classId)
              << " weapon=" << (out.weaponId.empty() ? "(nessuna)" : out.weaponId)
              << " ai=" << out.aiProfileId
              << " hp=" << out.hp
              << " move=" << out.moveSpeed
              << " interval=" << out.interval
              << " range=" << out.range
              << " color=(" << out.mr << ", " << out.mg << ", " << out.mb << ")\n";

    // Telemetria solo per le unità CON una classe (ADR-016: eventi discreti, e
    // così non fa rumore per le classless). È il canale che sopravvive a un run
    // interrotto: `std::cout` è bufferizzato e un kill lo perde, quindi la riga
    // qui sopra non è verificabile in un run headless (10_ProjectMemory).
    if (registry && !enemy->classId.empty())
        telemetry::event(telemetry::Level::Info, "Content", "unit class resolved",
                         {{"unit",   enemy->id},
                          {"class",  enemy->classId},
                          {"weapon", out.weaponId},
                          {"ai",     out.aiProfileId},
                          // `damage` è il campo che RENDE VISIBILE il bug del
                          // 2026-07-17: prima veniva dall'arma dell'entità mentre
                          // `weapon` veniva dalla classe. Due campi incoerenti nello
                          // stesso evento sono la sua firma.
                          {"damage", out.bulletDamage}});

    return out;
}

static std::vector<std::string> buildEnemySpawnList(const DefinitionRegistry* registry,
                                                    const std::string& mapId, int count)
{
    std::vector<std::string> result;
    if (count <= 0) return result;

    std::vector<std::string> preferredIds;

    if (registry)
    {
        const MapDef* map = registry->getMap(mapId);
        if (map && !map->enemyTypes.empty())
        {
            preferredIds = map->enemyTypes;
            std::cout << "[ConquestMode] Uso enemy_types da map '" << mapId
                      << "' (" << preferredIds.size() << " tipi).\n";
        }
    }

    // Fallback: nessun enemy_types nella mappa → usa gli id realmente
    // registrati (l'id è il nome file, mai stringhe hardcoded — ADR-001).
    if (preferredIds.empty() && registry)
    {
        for (auto& [id, def] : registry->enemies())
            preferredIds.push_back(id);
        std::sort(preferredIds.begin(), preferredIds.end());
        if (!preferredIds.empty())
            std::cout << "[ConquestMode] enemy_types assenti: fallback su "
                      << preferredIds.size() << " nemici dal registry.\n";
    }

    if (preferredIds.empty())
    {
        std::cerr << "[ConquestMode] ERRORE: nessun nemico disponibile "
                     "(ne' enemy_types nella mappa ne' file in data/enemies/). "
                     "Nessuno spawn nemico.\n";
        return result;
    }

    for (int i = 0; i < count; ++i)
        result.push_back(preferredIds[i % (int)preferredIds.size()]);

    return result;
}
} // namespace

void ConquestMode::spawnUnit(World& world, const RespawnEntry& info)
{
    EntityId e = world.createEntity();

    // ── unlock_spawn (doc 25): i RINFORZI alleati arrivano al post conquistato ──
    // Conseguenza di un obiettivo CaptureZone: `battleState.allySpawnPost` nomina
    // il command post da cui la squadra rinasce, invece che dallo spawn di mappa.
    // Vale solo per gli ALLEATI (team 1) e solo se un obiettivo l'ha deciso (vuoto
    // = comportamento invariato). Piccolo spread per non impilarli sul punto.
    // Senza questo, la conseguenza scriveva un valore che nessuno leggeva (gap).
    float sx = info.x, sz = info.z;
    if (info.teamId == 1 && m_map && !world.battleState.allySpawnPost.empty())
    {
        for (const auto& cp : m_map->commandPosts)
            if (cp.label == world.battleState.allySpawnPost)
            {
                m_spawnSpread = (m_spawnSpread + 1) % 8;
                const float ang = m_spawnSpread * 0.785398f;   // 8 direzioni
                sx = cp.x + std::cos(ang) * 2.0f;
                sz = cp.z + std::sin(ang) * 2.0f;
                telemetry::event(telemetry::Level::Info, "Objective",
                                 "reinforcement at unlocked spawn",
                                 {{"post", cp.label}, {"x", sx}, {"z", sz}});
                break;
            }
    }

    // A livello del suolo REALE della mappa (il pavimento può non essere a
    // y=0: firebase ha il top a +0.1 — prima le unità affondavano di 0.1).
    const float ground = mapquery::groundHeightAt(m_map, sx, sz);
    float yPos = ground + AI_GND_Y;

    // Trasformazione modello dall'EnemyDef, solo per mesh custom:
    // il cubo placeholder resta a scala 1 (altrimenti sparirebbe).
    const bool  hasMesh = (info.entityMesh != nullptr);
    const float mrx = hasMesh ? info.meshRotX  : 0.0f;
    const float mry = hasMesh ? info.meshRotY  : 0.0f;
    const float msc = hasMesh ? info.meshScale : 1.0f;

    // Fuori dalle ENTITÀ solide (veicoli): la decollisione findFreeSpot vede
    // solo la geometria della mappa, non i mezzi già spawnati.
    const glm::vec3 freePos = physics::nudgeOutOfColliders(
        {sx, yPos, sz}, config::aiHalf(), world);

    world.addTransform(e, {freePos.x, yPos, freePos.z, mrx, mry, 0, msc, msc, msc});
    world.addTeam(e, {info.teamId});
    world.addHealth(e, {info.hp, info.hp});
    Mesh* useMesh = (info.entityMesh ? info.entityMesh : m_mesh);
    MeshRendererComponent mrc;
    mrc.mesh    = useMesh;
    mrc.texture = m_tex;
    mrc.r = info.mr; mrc.g = info.mg; mrc.b = info.mb;
    // I modelli GLB hanno i piedi a Y=0: abbassa la mesh fino ai piedi
    // dell'entità (mezza altezza fisica sotto il centro).
    mrc.meshOffsetY = (hasMesh ? -AI_GND_Y : 0.0f);
    // Arma visibile in mano (risolta a monte dai metadata dell'editor)
    if (hasMesh && info.weaponMesh)
    {
        mrc.attachMesh  = info.weaponMesh;
        mrc.attachLocal = info.weaponLocal;
    }
    world.addMeshRenderer(e, mrc);

    AiComponent aic{
        .shootInterval  = info.interval,
        .aggroRange     = info.range,
        .bulletMesh     = m_mesh,
        .bulletR        = info.br,
        .bulletG        = info.bg,
        .bulletB        = info.bb,
        .bulletSpeed    = info.bulletSpeed,
        .bulletDamage   = info.bulletDamage,
        .bulletLifetime = info.bulletLifetime,
        .patrolAx       = info.pax,
        .patrolAz       = info.paz,
        .patrolBx       = info.pbx,
        .patrolBz       = info.pbz,
        .patrolSpeed    = info.patSpd,
        .seekSpeed      = info.patSpd + 1.5f,
        .strafeTimer    = 1.4f,
        .strafeSign     = 1.0f,
        .stationary     = info.stationary
    };

    // Sosta ai waypoint: con i command post in mappa, l'AI resta nell'area
    // abbastanza a lungo da completare la cattura (dwell > capture_time).
    if (m_map && !m_map->commandPosts.empty())
        aic.patrolDwell = 12.0f;

    // ── Profilo AI: seekSpeed reale + salto/precisione/reazione ──────────
    if (m_registry && !info.aiProfileId.empty())
    {
        if (const AiProfileDef* ap = m_registry->getAiProfile(info.aiProfileId))
        {
            aic.seekSpeed    = ap->seekSpeed;
            aic.jumpEnabled  = ap->jumpEnabled;
            aic.accuracy     = ap->accuracy;
            aic.reactionTime = ap->reactionTime;

            // Comportamento tattico (16_AiBehavior)
            aic.aggression      = ap->aggression;
            aic.retreatHpThresh = ap->retreatHpThresh;
            aic.coverPreference = ap->coverPreference;
            aic.peekMin = ap->peekDurationMin;  aic.peekMax = ap->peekDurationMax;
            aic.hideMin = ap->hideDurationMin;  aic.hideMax = ap->hideDurationMax;
            aic.flankChance     = ap->flankChance;
        }
    }

    // ── Arma reale: cadenza + surriscaldamento + proiettile dal WeaponDef ─
    // L'AI spara a una frazione della cadenza dell'arma (bilanciamento):
    // il calore la costringe comunque a raffiche + pause come il giocatore.
    if (m_registry && !info.weaponId.empty())
    {
        if (const WeaponDef* wd = m_registry->getWeapon(info.weaponId))
        {
            constexpr float AI_FIRE_RATE_SCALE = 0.35f;
            if (wd->fireRate > 0.01f)
                aic.fireInterval = 1.0f / (wd->fireRate * AI_FIRE_RATE_SCALE);
            aic.heatPerShot     = wd->heatPerShot;
            aic.cooldownRate    = wd->cooldownRate;
            aic.overheatPenalty = wd->overheatPenalty;
            aic.bulletSpeed     = wd->bulletSpeed;
            aic.bulletDamage    = wd->damage;
            aic.bulletLifetime  = wd->bulletLifetime;
            aic.bulletR = wd->bulletColor[0];
            aic.bulletG = wd->bulletColor[1];
            aic.bulletB = wd->bulletColor[2];
        }
    }
    world.addAi(e, aic);

    // ── Abilità (16_AiBehavior): shield passivo + abilità ATTIVE ─────────
    if (m_registry)
    {
        AbilityComponent ac;
        bool hasShield = false;
        for (const auto& abId : info.abilityIds)
        {
            const AbilityDef* ab = m_registry->getAbility(abId);
            if (!ab) continue;
            if (ab->type == "shield" && !hasShield)
            {
                ShieldComponent sh;
                sh.max = sh.current = ab->param1;
                sh.regenRate  = ab->param2;
                sh.regenDelay = ab->param3;
                world.addShield(e, sh);
                hasShield = true;   // un solo scudo per entità
            }
            else if (ab->type == "roll")
            {
                AbilityState st;
                st.abilityId   = ab->id;
                st.type        = ab->type;
                st.param1      = ab->param1;   // velocità scatto (m/s)
                st.param2      = ab->param2;   // durata scatto (s)
                st.cooldownMax = ab->cooldown;
                ac.states.push_back(std::move(st));
            }
        }
        if (!ac.states.empty())
            world.addAbilities(e, ac);
    }

    // Profilo hitbox: sempre risolto a monte (id entità come fallback,
    // ADR-006/007). Nessun id hardcoded: se manca, l'unità usa il fallback
    // sferico del CombatSystem.
    if (m_registry && !info.hitboxProfileId.empty())
    {
        const auto* hp = m_registry->getHitboxProfile(info.hitboxProfileId);
        if (hp) world.addHitbox(e, HitboxComponent{hp});
    }

    // Template di respawn = copia integrale dello spawn spec (Todo A4):
    // nessuna lista di campi da tenere allineata a mano.
    m_trackedUnits.push_back({e, info});
}

void ConquestMode::checkDeaths(World& world)
{
    // Bersagli strategici (DestroyTarget): non respawnano (una torre distrutta
    // resta distrutta), quindi non sono in m_trackedUnits. Segnala la distruzione
    // una volta sola (entity azzerata dopo il report). L'obiettivo l'ha già
    // rilevata nel tick della morte via killedThisTick; questo è solo osservabilità.
    for (auto& st : world.strategicTargets)
        if (st.entity != 0 && !world.isValidEntity(st.entity))
        {
            telemetry::event(telemetry::Level::Info, "Objective", "strategic target destroyed",
                             {{"label", st.label}});
            world.pushEvent("BERSAGLIO DISTRUTTO: " + st.label);
            st.entity = 0;
        }

    auto it = m_trackedUnits.begin();
    while (it != m_trackedUnits.end())
    {
        if (!world.isValidEntity(it->first))
        {
            const auto& tpl = it->second;
            int& tickets = (tpl.teamId == 1) ? m_team1Tickets : m_team2Tickets;

            // Conseguenza di un obiettivo (doc 25): se la squadra ha tagliato i
            // rinforzi nemici (es. presa la base d'atterraggio), il team 2 non
            // rimpiazza più le perdite — le sue riserve restano a terra.
            // Il dato arriva da `battleState`: qui non si sa QUALE obiettivo
            // l'abbia deciso, e non deve saperlo.
            if (tpl.teamId == 2 && world.battleState.enemyReinforcementsBlocked)
            {
                std::cout << "[Respawn] Nemico eliminato. RINFORZI INTERROTTI — "
                             "nessun rimpiazzo.\n";
                it = m_trackedUnits.erase(it);
                continue;
            }

            if (tickets > 0)
            {
                --tickets;
                // Copia integrale dello spawn spec, solo il timer cambia (A4)
                RespawnEntry entry = tpl;
                // Controllo dei command post (direttiva utente 07-18): ogni post
                // posseduto dal NEMICO rallenta il rientro di questa unità. È il
                // vantaggio della conquista — non svuota le riserve avversarie
                // (i ticket restano), le fa rientrare più piano. Con `unlock_spawn`
                // (rinforzi al fronte) è l'altra faccia del controllo territoriale.
                const int foePosts = m_commandPosts.countOwnedBy(tpl.teamId == 1 ? 2 : 1);
                entry.timer = respawnDelay * (1.0f + config::POST_RESPAWN_SLOW * foePosts);
                m_respawnQueue.push_back(entry);

                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team << " eliminato. Ticket rimasti: "
                          << tickets << " — respawn in " << entry.timer
                          << "s (post nemici: " << foePosts << ")\n";
            }
            else
            {
                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team
                          << " eliminato. NESSUN ticket — morte permanente.\n";
            }

            it = m_trackedUnits.erase(it);
        }
        else { ++it; }
    }
}

void ConquestMode::applySettings(const MatchSettings& s)
{
    m_mapId             = s.mapId.empty() ? "firebase" : s.mapId;
    initialTeam1Tickets = s.team1Tickets;
    initialTeam2Tickets = s.team2Tickets;
    team1AiCount        = s.team1AiCount;
    team2AiCount        = s.team2AiCount;
    respawnDelay        = s.respawnDelay;
    playerHp            = s.playerHp;
}

void ConquestMode::start(World& world, Mesh* mesh, Texture* tex,
                         const DefinitionRegistry* registry,
                         const MeshCache* meshCache)
{
    std::cout << "[ConquestMode] Firebase — caricamento...\n";
    world.initialize();
    m_mesh      = mesh;
    m_tex       = tex;
    m_registry  = registry;
    m_meshCache = meshCache;
    m_map       = registry ? registry->getMap(m_mapId) : nullptr;
    world.activeMap = m_map;   // canale metadata per l'AiSystem (doc 18)

    // Spawn del giocatore dal MapDef (team1), altrimenti default.
    // Posato a terra (data-driven): sul suolo reale della mappa e fuori dagli
    // ostacoli, non a Y fissa (altrimenti nasce incastrato nel pavimento a
    // top>0 e lo step-up lo lancia in aria). Y-occhi = suolo + PLAYER_HALF_Y.
    float playerX = 0.0f, playerZ = SPAWN_Z;
    if (m_map)
    { playerX = m_map->spawnTeam1[0]; playerZ = m_map->spawnTeam1[2]; }
    m_spawnPos  = m_map
        ? mapquery::groundedSpawn(m_map, playerX, playerZ,
                                  config::PLAYER_HALF_X, config::PLAYER_HALF_Y,
                                  config::PLAYER_HALF_Z, config::PLAYER_HALF_Y,
                                  0.0f, (playerZ > 0.0f ? -1.0f : 1.0f))
        : glm::vec3{playerX, SPAWN_Y, playerZ};

    m_team1Tickets = initialTeam1Tickets;
    m_team2Tickets = initialTeam2Tickets;
    m_respawnQueue.clear();
    m_trackedUnits.clear();

    // ── Giocatore ────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {m_spawnPos.x, m_spawnPos.y, m_spawnPos.z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Lista nemici da mappa/registry ───────────────────────────────────
    int nEnemies = std::min(team2AiCount, config::MAX_AI_PER_TEAM);
    std::vector<std::string> enemyIds = buildEnemySpawnList(registry, m_mapId, nEnemies);
    if ((int)enemyIds.size() < nEnemies)
        nEnemies = (int)enemyIds.size(); // registry vuoto: niente spawn ciechi

    // ── Lambda helper spawn ───────────────────────────────────────────────
    auto mkUnit = [&](float x, float z, int team,
                      float mr, float mg, float mb,
                      float br, float bg, float bb,
                      float hp,
                      float pax, float paz, float pbx, float pbz,
                      float pspd, float intv, float range,
                      const std::string& hitboxProfile = "",
                      bool stat = false,
                      float bspd = 8.0f, float bdmg = 20.0f, float blife = 5.0f)
    {
        RespawnEntry info;
        info.timer           = 0;
        info.x = x; info.z = z;
        info.teamId          = team;
        info.mr = mr; info.mg = mg; info.mb = mb;
        info.br = br; info.bg = bg; info.bb = bb;
        info.hp              = hp;
        info.pax = pax; info.paz = paz; info.pbx = pbx; info.pbz = pbz;
        info.patSpd          = pspd;
        info.interval        = intv;
        info.range           = range;
        info.hitboxProfileId = hitboxProfile;
        info.stationary      = stat;
        info.bulletSpeed     = bspd;
        info.bulletDamage    = bdmg;
        info.bulletLifetime  = blife;
        info.entityMesh      = nullptr;
        spawnUnit(world, info);
    };

    // Versione con mesh custom (per nemici con meshPath)
    auto mkUnitWithMesh = [&](float x, float z, int team,
                              float mr, float mg, float mb,
                              float br, float bg, float bb,
                              float hp,
                              float pax, float paz, float pbx, float pbz,
                              float pspd, float intv, float range,
                              const std::string& hitboxProfile,
                              bool stat,
                              float bspd, float bdmg, float blife,
                              const std::string& meshPath,
                              float meshRotX = 0.0f, float meshRotY = 0.0f,
                              float meshScale = 1.0f,
                              const std::string& weaponId = "",
                              Mesh* weaponMesh = nullptr,
                              const glm::mat4& weaponLocal = glm::mat4(1.0f),
                              const std::string& aiProfileId = "",
                              const std::vector<std::string>& abilityIds = {})
    {
        RespawnEntry info;
        info.timer           = 0;
        info.x = x; info.z = z;
        info.teamId          = team;
        info.mr = mr; info.mg = mg; info.mb = mb;
        info.br = br; info.bg = bg; info.bb = bb;
        info.hp              = hp;
        info.pax = pax; info.paz = paz; info.pbx = pbx; info.pbz = pbz;
        info.patSpd          = pspd;
        info.interval        = intv;
        info.range           = range;
        info.hitboxProfileId = hitboxProfile;
        info.stationary      = stat;
        info.bulletSpeed     = bspd;
        info.bulletDamage    = bdmg;
        info.bulletLifetime  = blife;

        // Risolve mesh dal cache se disponibile
        info.entityMesh = nullptr;
        if (m_meshCache && !meshPath.empty())
        {
            auto it = m_meshCache->find(meshPath);
            if (it != m_meshCache->end())
                info.entityMesh = it->second;
        }
        info.meshRotX  = meshRotX;
        info.meshRotY  = meshRotY;
        info.meshScale = meshScale;
        info.weaponId  = weaponId;
        info.weaponMesh  = weaponMesh;
        info.weaponLocal = weaponLocal;
        info.aiProfileId = aiProfileId;
        info.abilityIds  = abilityIds;
        spawnUnit(world, info);
    };

    // ── Posizioni generate attorno agli spawn point della mappa ───────────
    struct UnitPos { float x, z, pax, paz, pbx, pbz; bool stat; };

    // Base spawn: dal MapDef se disponibile, altrimenti default.
    float enemyBaseX = 0.0f, enemyBaseZ = -SPAWN_Z;
    float allyBaseX  = 0.0f, allyBaseZ  =  SPAWN_Z;
    if (const MapDef* md = registry ? registry->getMap(m_mapId) : nullptr)
    {
        enemyBaseX = md->spawnTeam2[0]; enemyBaseZ = md->spawnTeam2[2];
        allyBaseX  = md->spawnTeam1[0]; allyBaseZ  = md->spawnTeam1[2];
    }

    // Genera N posizioni in file davanti allo spawn, avanzando verso il
    // centro (dirZ). Ogni posizione viene spinta fuori dagli ostacoli della
    // mappa (prima le file finivano DENTRO le barricate davanti agli spawn).
    // Patrol: dallo spawn verso il command post assegnato (round-robin) —
    // così l'AI marcia sugli obiettivi invece di fare avanti-indietro.
    auto genPositions = [this](float baseX, float baseZ, float dirZ, int count)
    {
        std::vector<UnitPos> out;
        const int   perRow = 5;
        const float dx = 3.5f, dz = 3.0f;
        const int   nPosts = m_map ? (int)m_map->commandPosts.size() : 0;

        // Patrol route autorate (18_AiMapConsumption): se la mappa ne ha,
        // le unità pattugliano segmenti consecutivi delle route (round-robin)
        // invece dei waypoint procedurali verso i post. Limite documentato:
        // AiComponent ha due waypoint → un segmento per unità.
        struct Seg { float ax, az, bx, bz; };
        std::vector<Seg> routeSegs;
        if (m_map)
            for (const auto& r : m_map->patrolRoutes)
                for (size_t k = 0; k + 1 < r.points.size(); ++k)
                    routeSegs.push_back({r.points[k][0],   r.points[k][2],
                                         r.points[k+1][0], r.points[k+1][2]});

        for (int i = 0; i < count; ++i)
        {
            int row = i / perRow, col = i % perRow;
            float x = baseX + (col - (perRow - 1) * 0.5f) * dx;
            float z = baseZ + dirZ * (3.0f + row * dz);

            // Fuori dagli ostacoli (muri/barricate/casse), avanzando in campo
            mapquery::findFreeSpot(m_map, x, z, 0.0f, dirZ, 0.45f, 0.5f, 0.45f);

            UnitPos p;
            p.x = x;  p.z = z;
            if (!routeSegs.empty())
            {
                const Seg& s = routeSegs[i % (int)routeSegs.size()];
                p.pax = s.ax; p.paz = s.az;
                p.pbx = s.bx; p.pbz = s.bz;
            }
            else if (nPosts > 0)
            {
                const auto& cp = m_map->commandPosts[i % nPosts];
                p.pax = x;
                p.paz = z;
                // Punto di arrivo attorno al post (sparso, non impilato)
                p.pbx = cp.x + (float)((i % 3) - 1) * 2.0f;
                p.pbz = cp.z + ((i % 2) ? 1.8f : -1.8f);
            }
            else
            {
                p.pax = x - 1.5f; p.paz = z;
                p.pbx = x + 1.5f; p.pbz = z;
            }
            p.stat = false;
            out.push_back(p);
        }
        return out;
    };

    std::vector<UnitPos> enemyPos = genPositions(enemyBaseX, enemyBaseZ, +1.0f, nEnemies);

    // ── Spawn nemici dai JSON realmente selezionati ──────────────────────
    for (int i = 0; i < nEnemies; ++i)
    {
        const auto& p = enemyPos[i];
        const std::string enemyId = enemyIds[i];
        const ResolvedEnemyArchetype resolved = resolveUnitArchetype(registry, enemyId, 2);

        // Arma in mano dai metadata dell'editor
        auto wa = weaponattach::resolve(registry, m_meshCache,
                                        registry ? registry->getEnemy(enemyId) : nullptr);

        mkUnitWithMesh(p.x, p.z, 2,
               resolved.mr, resolved.mg, resolved.mb,
               resolved.br, resolved.bg, resolved.bb,
               resolved.hp,
               p.pax, p.paz, p.pbx, p.pbz,
               resolved.moveSpeed, resolved.interval, resolved.range,
               resolved.hitboxProfileId, p.stat,
               resolved.bulletSpeed, resolved.bulletDamage, resolved.bulletLifetime,
               resolved.meshPath,
               resolved.meshRotX, resolved.meshRotY, resolved.meshScale,
               resolved.weaponId, wa.mesh, wa.local, resolved.aiProfileId,
               resolved.abilityIds);
    }

    // ── Lista alleati da mappa/registry ─────────────────────────────────────
    std::vector<std::string> allyIds;
    if (registry)
    {
        const MapDef* map = registry->getMap(m_mapId);
        if (map && !map->allyTypes.empty())
            allyIds = map->allyTypes;
    }
    // Fallback: id realmente registrati, mai stringhe hardcoded (ADR-001/007).
    if (allyIds.empty() && registry)
    {
        for (auto& [id, def] : registry->allies())
            allyIds.push_back(id);
        std::sort(allyIds.begin(), allyIds.end());
    }

    // ── Spawn alleati AI (data-driven) ───────────────────────────────────────
    int nAllies = std::min(team1AiCount, config::MAX_AI_PER_TEAM);
    if (allyIds.empty()) nAllies = 0; // nessun alleato registrato: niente spawn ciechi
    std::vector<UnitPos> allyPos = genPositions(allyBaseX, allyBaseZ, -1.0f, nAllies);
    for (int i = 0; i < nAllies; ++i)
    {
        const auto& p = allyPos[i];
        const std::string allyId = allyIds[i % (int)allyIds.size()];
        // Stesso resolve dei nemici (A7): stats proiettile dall'arma vera,
        // non più 8/20/5 hardcoded.
        const ResolvedEnemyArchetype resolved = resolveUnitArchetype(registry, allyId, 1);

        auto wa = weaponattach::resolve(registry, m_meshCache,
                                        registry ? registry->getAlly(allyId) : nullptr);
        mkUnitWithMesh(p.x, p.z, 1,
                       resolved.mr, resolved.mg, resolved.mb,
                       resolved.br, resolved.bg, resolved.bb,
                       resolved.hp,
                       p.pax, p.paz, p.pbx, p.pbz,
                       resolved.moveSpeed, resolved.interval, resolved.range,
                       resolved.hitboxProfileId, p.stat,
                       resolved.bulletSpeed, resolved.bulletDamage, resolved.bulletLifetime,
                       resolved.meshPath,
                       resolved.meshRotX, resolved.meshRotY, resolved.meshScale,
                       resolved.weaponId, wa.mesh, wa.local, resolved.aiProfileId,
                       resolved.abilityIds);
    }

    // ── Geometria ─────────────────────────────────────────────────────────
    auto addBox = [&](float x, float yc, float z, float ry,
                      float sx, float sy, float sz,
                      float cr, float cg, float cb, bool collider = true)
    {
        EntityId c = world.createEntity();
        world.addTransform(c, {x, yc, z, 0, ry, 0, sx, sy, sz});
        world.addMeshRenderer(c, {mesh, tex, cr, cg, cb});
        if (collider)
            world.addCollider(c, {sx * 0.5f, sy * 0.5f, sz * 0.5f});
    };

    // Geometria autorata nel Map Editor (data/maps/firebase.json "geometry").
    // Se presente, ha priorità sulla geometria hardcoded di fallback.
    const MapDef* map = registry ? registry->getMap(m_mapId) : nullptr;
    if (map && !map->geometry.empty())
    {
        for (const auto& gb : map->geometry)
            addBox(gb.x, gb.y, gb.z, gb.ry, gb.sx, gb.sy, gb.sz,
                   gb.r, gb.g, gb.b, gb.collider);
        std::cout << "[ConquestMode] Geometria da JSON: "
                  << map->geometry.size() << " box.\n";
    }
    else
    {
        addBox(0,-0.1f,-1, 0, 24,0.2f,20, 0.42f,0.38f,0.32f);
        addBox( 0,0.7f,-10.5f, 0, 24,1.4f,0.4f, 0.30f,0.28f,0.26f);
        addBox(-11.2f,0.7f,-5.5f, 0, 0.4f,1.4f,10, 0.28f,0.26f,0.24f);
        addBox( 11.2f,0.7f,-5.5f, 0, 0.4f,1.4f,10, 0.28f,0.26f,0.24f);
        addBox(-5.5f,0.7f,-3, 0, 5,1.4f,0.4f, 0.35f,0.33f,0.30f);
        addBox( 5.5f,0.7f,-3, 0, 5,1.4f,0.4f, 0.35f,0.33f,0.30f);
        addBox( 8,1.0f,-8, 0, 4,2,5, 0.28f,0.25f,0.22f);
        addBox(-8,1.0f,-8, 0, 4,2,5, 0.28f,0.25f,0.22f);
        for (int i = 1; i <= 5; ++i)
        {
            float top = i * 0.4f, yc = top * 0.5f;
            float zc  = -3.575f - (i - 1) * 0.55f;
            addBox( 8, yc, zc, 0, 3, top, 0.5f, 0.36f,0.32f,0.28f);
            addBox(-8, yc, zc, 0, 3, top, 0.5f, 0.36f,0.32f,0.28f);
        }
        addBox(  6,0.7f,-3.5f, 0, 0.4f,1.4f,2.5f, 0.30f,0.28f,0.26f);
        addBox( -6,0.7f,-3.5f, 0, 0.4f,1.4f,2.5f, 0.30f,0.28f,0.26f);
        addBox(  0,0.7f,-5,    0, 3.5f,1.4f,0.4f, 0.32f,0.30f,0.28f);
        std::cout << "[ConquestMode] Geometria hardcoded (nessuna geometry nel JSON).\n";
    }

    // ── Command post (ADR-009) ────────────────────────────────────────────
    m_bleedTimer = 0.0f;   // usato da Assalto/Difesa (ObjectiveModes); Conquest no
    if (map)
        m_commandPosts.init(world, map->commandPosts, mesh, tex);

    // ── Bersagli strategici distruttibili (doc 25, DestroyTarget) ─────────
    // Struttura statica di team 2: colpibile da giocatore e alleati, niente AI
    // (non spara). Distruggerla completa un obiettivo DestroyTarget e ne scatena
    // la conseguenza. La mailbox entità→label lascia l'ObjectiveSystem agnostico.
    world.strategicTargets.clear();
    if (map && registry)
    {
        for (const auto& t : map->strategicTargets)
        {
            const float gy = mapquery::groundHeightAt(map, t.x, t.z);
            EntityId e = world.createEntity();

            Mesh* useMesh = mesh;   // box di fallback
            if (m_meshCache && !t.meshPath.empty())
            {
                auto it = m_meshCache->find(t.meshPath);
                if (it != m_meshCache->end()) useMesh = it->second;
            }
            const bool box = (useMesh == mesh);
            // Fallback box: dimensione FISSA visibile (2.5 m), scala uniforme così
            // la hitbox (che usa sx) resta coerente. Mesh reale: usa meshScale.
            const float sc = box ? 2.5f
                                 : ((t.meshScale > 0.0001f) ? t.meshScale : 1.0f);
            world.addTransform(e, {t.x, gy, t.z, 0, 0, 0, sc, sc, sc});
            world.addTeam(e, {2});
            world.addHealth(e, {t.hp, t.hp});

            MeshRendererComponent mrc;
            mrc.mesh = useMesh; mrc.texture = tex;
            mrc.r = t.color[0]; mrc.g = t.color[1]; mrc.b = t.color[2];
            // Grounding: il cubo di fallback è centrato (±0.5), quindi va alzato di
            // mezza altezza (0.5·scala) perché la base tocchi il suolo — NON floti.
            // La hitbox sintetica usa lo stesso meshOffsetY → resta allineata.
            // Mesh reale (base a Y=0, come i GLB): nessun offset.
            mrc.meshOffsetY = box ? (0.5f * sc) : 0.0f;
            world.addMeshRenderer(e, mrc);

            if (const auto* hp = registry->getHitboxProfile("__strategic_target"))
                world.addHitbox(e, HitboxComponent{hp});

            world.strategicTargets.push_back({e, t.label});
            std::cout << "[ConquestMode] Bersaglio strategico '" << t.label
                      << "' (hp " << (int)t.hp << ") a (" << t.x << ", " << t.z << ")\n";
            telemetry::event(telemetry::Level::Info, "Objective", "strategic target spawned",
                             {{"label", t.label}, {"hp", t.hp},
                              {"x", t.x}, {"z", t.z}});
        }
    }

    // ── Veicoli in mappa (19_Vehicles, helper condiviso — R6) ────────────
    // Il tracker li fa respawnare al loro spawn quando distrutti (Fase B).
    vehiclespawn::spawnFromMap(world, map, registry, m_meshCache, mesh, tex,
                               &m_vehicleTracker);

    std::cout << "[ConquestMode] Spawn completato: "
              << nEnemies << " nemici, " << nAllies << " alleati AI, "
              << m_commandPosts.count() << " command post.\n";
    telemetry::logInfo("[Conquest] spawn: " + std::to_string(nEnemies)
        + " nemici, " + std::to_string(nAllies) + " alleati AI, "
        + std::to_string(m_commandPosts.count()) + " post, entita' totali "
        + std::to_string(world.getEntities().size()));
}

// Regole obiettivo di Conquista: nessuna regola per-tick (il vantaggio dei post
// e' il respawn-slow in checkDeaths, non un drenaggio a tempo). Gancio vuoto —
// le modalità derivate (Assalto/Difesa) lo sostituiscono col proprio bleed.
void ConquestMode::updateObjectiveRules(World& /*world*/, float /*dt*/)
{
    // Il "ticket bleed a tempo" (chi ha più post drenava i ticket avversari) è
    // stato RIMOSSO (direttiva utente 07-18): consumare le riserve nemiche era
    // punitivo e poco leggibile. Ora il controllo dei post agisce sul RITMO dei
    // rinforzi, non sul loro numero — vedi `checkDeaths` (respawn più lento per
    // il team con meno post) e `spawnUnit` (unlock_spawn: rinforzi al fronte).
    // Funzione lasciata come gancio per future regole per-tick della modalità.
}

// Punti di respawn selezionabili: lo spawn base (indice 0, coincide con
// getSpawnPos come richiede il contratto IGameMode) + ogni command post
// posseduto dagli alleati. Conquistare un post avanza quindi anche il punto
// da cui si può rientrare — base della futura mappa tattica (doc 25).
std::vector<IGameMode::SpawnPoint> ConquestMode::availableSpawns() const
{
    std::vector<SpawnPoint> out;
    out.push_back({"Base", m_spawnPos});
    for (const auto& op : m_commandPosts.ownedByTeam(1))
        out.push_back({op.label, glm::vec3(op.x, m_spawnPos.y, op.z)});
    return out;
}

// Esito Conquista: vittoria quando i nemici non hanno più ticket né unità.
MatchOutcome ConquestMode::outcome(const World& world) const
{
    if (world.getTickCount() <= 10) return MatchOutcome::Ongoing;
    if (m_team2Tickets <= 0)
    {
        bool enemyAlive = false;
        for (EntityId id : world.getEntities())
        {
            const auto* tm = world.getTeam(id);
            const auto* hp = world.getHealth(id);
            if (tm && tm->teamId == 2 && hp && hp->current > 0.0f
                && !world.getBullet(id))
            { enemyAlive = true; break; }
        }
        if (!enemyAlive) return MatchOutcome::Team1Win;
    }
    return MatchOutcome::Ongoing;
}

void ConquestMode::update(World& world, float dt)
{
    // Riserve extra da una conseguenza (doc 25): il mode possiede i ticket, quindi
    // è lui a consumare il delta e ad azzerarlo — chi lo ha prodotto non conosce
    // il numero attuale delle riserve.
    if (world.battleState.pendingAllyReinforcements != 0)
    {
        m_team1Tickets = std::max(0, m_team1Tickets
                                   + world.battleState.pendingAllyReinforcements);
        world.battleState.pendingAllyReinforcements = 0;
    }

    checkDeaths(world);

    // Respawn dei veicoli distrutti (19_Vehicles Fase B)
    m_vehicleTracker.tick(world, m_map, m_registry, m_meshCache, m_mesh, m_tex, dt);

    // ── Command post: cattura + regole obiettivo della modalità ──────────
    m_commandPosts.update(world, dt);
    updateObjectiveRules(world, dt);

    for (auto it = m_respawnQueue.begin(); it != m_respawnQueue.end(); )
    {
        it->timer -= dt;
        if (it->timer <= 0.0f)
        {
            spawnUnit(world, *it);
            it = m_respawnQueue.erase(it);
        }
        else { ++it; }
    }
}

} // namespace mini
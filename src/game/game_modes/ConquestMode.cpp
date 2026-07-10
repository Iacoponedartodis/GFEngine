#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/MapQuery.hpp"
#include "mini/game/WeaponAttach.hpp"
#include "mini/game/VehicleSpawn.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

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

static ResolvedEnemyArchetype resolveEnemyArchetype(const DefinitionRegistry* registry,
                                                    const std::string& enemyId)
{
    ResolvedEnemyArchetype out;
    out.enemyId = enemyId;

    const EnemyDef* enemy = registry ? registry->getEnemy(enemyId) : nullptr;
    if (!enemy)
    {
        std::cerr << "[ConquestMode] Enemy '" << enemyId
                  << "' non trovato nel registry. Uso fallback.\n";
        out.hitboxProfileId = enemyId;
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

    if (registry)
    {
        const AiProfileDef* ai = registry->getAiProfile(enemy->aiProfileId);
        if (ai)
        {
            out.moveSpeed = ai->patrolSpeed;
            out.interval  = ai->shootInterval;
            out.range     = ai->sightRange;
        }
        else
        {
            std::cerr << "[ConquestMode] AI profile '" << enemy->aiProfileId
                      << "' non trovato per enemy '" << enemy->id
                      << "'. Uso fallback AI.\n";
        }

        // Bullet stats dall'arma primaria
        const WeaponDef* wpn = registry->getWeapon(enemy->primaryWeaponId());
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

    std::cout << "[ConquestMode] Enemy resolved: " << enemy->id
              << " hp=" << out.hp
              << " move=" << out.moveSpeed
              << " interval=" << out.interval
              << " range=" << out.range
              << " color=(" << out.mr << ", " << out.mg << ", " << out.mb << ")\n";

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
    // A livello del suolo REALE della mappa (il pavimento può non essere a
    // y=0: firebase ha il top a +0.1 — prima le unità affondavano di 0.1).
    const float ground = mapquery::groundHeightAt(m_map, info.x, info.z);
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
        {info.x, yPos, info.z}, config::aiHalf(), world);

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

    UnitTemplate tpl = {
        info.x, info.z, yPos, info.teamId,
        info.mr, info.mg, info.mb,
        info.br, info.bg, info.bb,
        info.hp, info.pax, info.paz, info.pbx, info.pbz,
        info.patSpd, info.interval, info.range, info.stationary,
        info.hitboxProfileId
    };
    // Campi non coperti dall'aggregate: senza questi il respawn perdeva
    // mesh custom, trasformazione e stats proiettile (tornava un cubo).
    tpl.bulletSpeed    = info.bulletSpeed;
    tpl.bulletDamage   = info.bulletDamage;
    tpl.bulletLifetime = info.bulletLifetime;
    tpl.entityMesh     = info.entityMesh;
    tpl.meshRotX       = info.meshRotX;
    tpl.meshRotY       = info.meshRotY;
    tpl.meshScale      = info.meshScale;
    tpl.weaponId       = info.weaponId;
    tpl.aiProfileId    = info.aiProfileId;
    tpl.abilityIds     = info.abilityIds;
    tpl.weaponMesh     = info.weaponMesh;
    tpl.weaponLocal    = info.weaponLocal;

    // Profilo hitbox: sempre risolto a monte (id entità come fallback,
    // ADR-006/007). Nessun id hardcoded: se manca, l'unità usa il fallback
    // sferico del CombatSystem.
    if (m_registry && !info.hitboxProfileId.empty())
    {
        const auto* hp = m_registry->getHitboxProfile(info.hitboxProfileId);
        if (hp) world.addHitbox(e, HitboxComponent{hp});
    }

    m_trackedUnits.push_back({e, tpl});
}

void ConquestMode::checkDeaths(World& world)
{
    auto it = m_trackedUnits.begin();
    while (it != m_trackedUnits.end())
    {
        if (!world.isValidEntity(it->first))
        {
            const auto& tpl = it->second;
            int& tickets = (tpl.teamId == 1) ? m_team1Tickets : m_team2Tickets;

            if (tickets > 0)
            {
                --tickets;
                RespawnEntry entry;
                entry.timer           = respawnDelay;
                entry.x               = tpl.x;
                entry.z               = tpl.z;
                entry.teamId          = tpl.teamId;
                entry.mr = tpl.mr; entry.mg = tpl.mg; entry.mb = tpl.mb;
                entry.br = tpl.br; entry.bg = tpl.bg; entry.bb = tpl.bb;
                entry.hp              = tpl.hp;
                entry.pax = tpl.pax; entry.paz = tpl.paz;
                entry.pbx = tpl.pbx; entry.pbz = tpl.pbz;
                entry.patSpd          = tpl.patSpd;
                entry.interval        = tpl.interval;
                entry.range           = tpl.range;
                entry.stationary      = tpl.stationary;
                entry.hitboxProfileId = tpl.hitboxProfileId;
                entry.bulletSpeed     = tpl.bulletSpeed;
                entry.bulletDamage    = tpl.bulletDamage;
                entry.bulletLifetime  = tpl.bulletLifetime;
                entry.entityMesh      = tpl.entityMesh;
                entry.meshRotX        = tpl.meshRotX;
                entry.meshRotY        = tpl.meshRotY;
                entry.meshScale       = tpl.meshScale;
                entry.weaponId        = tpl.weaponId;
                entry.aiProfileId     = tpl.aiProfileId;
                entry.abilityIds      = tpl.abilityIds;
                entry.weaponMesh      = tpl.weaponMesh;
                entry.weaponLocal     = tpl.weaponLocal;
                m_respawnQueue.push_back(entry);

                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team << " eliminato. Ticket rimasti: "
                          << tickets << " — respawn in " << respawnDelay << "s\n";
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
    float playerX = 0.0f, playerZ = SPAWN_Z;
    if (m_map)
    { playerX = m_map->spawnTeam1[0]; playerZ = m_map->spawnTeam1[2]; }
    m_spawnPos  = {playerX, SPAWN_Y, playerZ};

    m_team1Tickets = initialTeam1Tickets;
    m_team2Tickets = initialTeam2Tickets;
    m_respawnQueue.clear();
    m_trackedUnits.clear();

    // ── Giocatore ────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {playerX, SPAWN_Y, playerZ});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Lista nemici da mappa/registry ───────────────────────────────────
    int nEnemies = std::min(team2AiCount, 20);
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
        const ResolvedEnemyArchetype resolved = resolveEnemyArchetype(registry, enemyId);

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
    int nAllies = std::min(team1AiCount, 10);
    if (allyIds.empty()) nAllies = 0; // nessun alleato registrato: niente spawn ciechi
    std::vector<UnitPos> allyPos = genPositions(allyBaseX, allyBaseZ, -1.0f, nAllies);
    for (int i = 0; i < nAllies; ++i)
    {
        const auto& p = allyPos[i];
        const std::string allyId = allyIds[i % (int)allyIds.size()];

        // Cerca nel registry allies
        const EnemyDef* allyDef = registry ? registry->getAlly(allyId) : nullptr;

        float mr=0.25f, mg=0.45f, mb=1.0f;
        float br=0.30f, bg=0.60f, bb=1.0f;
        float hp=60.0f, pspd=1.8f, intv=3.5f, rng=14.0f;
        float arx=0.0f, ary=0.0f, asc=1.0f;
        std::string hitboxId;
        std::string meshPath;

        if (allyDef)
        {
            mr = allyDef->color[0]; mg = allyDef->color[1]; mb = allyDef->color[2];
            hp = allyDef->hp;
            hitboxId = allyDef->hitboxProfileId.empty() ? allyDef->id
                                                        : allyDef->hitboxProfileId;
            meshPath = allyDef->meshPath;
            arx = allyDef->meshRotX;
            ary = allyDef->meshRotY;
            asc = allyDef->meshScale;

            if (registry)
            {
                const AiProfileDef* ai = registry->getAiProfile(allyDef->aiProfileId);
                if (ai) { pspd = ai->patrolSpeed; intv = ai->shootInterval; rng = ai->sightRange; }

                const WeaponDef* wpn = registry->getWeapon(allyDef->primaryWeaponId());
                if (wpn) { br = wpn->bulletColor[0]; bg = wpn->bulletColor[1]; bb = wpn->bulletColor[2]; }
                else { br = allyDef->bulletColor[0]; bg = allyDef->bulletColor[1]; bb = allyDef->bulletColor[2]; }
            }
        }

        auto wa = weaponattach::resolve(registry, m_meshCache, allyDef);
        mkUnitWithMesh(p.x, p.z, 1,
                       mr, mg, mb, br, bg, bb, hp,
                       p.pax, p.paz, p.pbx, p.pbz,
                       pspd, intv, rng,
                       hitboxId, p.stat,
                       8.0f, 20.0f, 5.0f,
                       meshPath, arx, ary, asc,
                       allyDef ? allyDef->primaryWeaponId() : std::string(),
                       wa.mesh, wa.local,
                       allyDef ? allyDef->aiProfileId : std::string(),
                       allyDef ? allyDef->abilityIds : std::vector<std::string>{});
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
    m_bleedTimer = 0.0f;
    if (map)
        m_commandPosts.init(world, map->commandPosts, mesh, tex);

    // ── Veicoli in mappa (19_Vehicles, helper condiviso — R6) ────────────
    // Il tracker li fa respawnare al loro spawn quando distrutti (Fase B).
    vehiclespawn::spawnFromMap(world, map, registry, mesh, tex,
                               &m_vehicleTracker);

    std::cout << "[ConquestMode] Spawn completato: "
              << nEnemies << " nemici, " << nAllies << " alleati AI, "
              << m_commandPosts.count() << " command post.\n";
    telemetry::logInfo("[Conquest] spawn: " + std::to_string(nEnemies)
        + " nemici, " + std::to_string(nAllies) + " alleati AI, "
        + std::to_string(m_commandPosts.count()) + " post, entita' totali "
        + std::to_string(world.getEntities().size()));
}

// Regole obiettivo di Conquista: maggioranza dei post → drena i ticket
// avversari. Le modalità derivate (Assalto/Difesa) sostituiscono questo hook.
void ConquestMode::updateObjectiveRules(World& /*world*/, float dt)
{
    if (m_commandPosts.count() == 0) return;

    m_bleedTimer += dt;
    if (m_bleedTimer < m_bleedInterval) return;
    m_bleedTimer -= m_bleedInterval;

    const int own1 = m_commandPosts.countOwnedBy(1);
    const int own2 = m_commandPosts.countOwnedBy(2);
    if (own1 > own2 && m_team2Tickets > 0)
    {
        --m_team2Tickets;
        std::cout << "[Conquest] Maggioranza post alleata: ticket nemici -> "
                  << m_team2Tickets << "\n";
    }
    else if (own2 > own1 && m_team1Tickets > 0)
    {
        --m_team1Tickets;
        std::cout << "[Conquest] Maggioranza post nemica: ticket alleati -> "
                  << m_team1Tickets << "\n";
    }
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
    checkDeaths(world);

    // Respawn dei veicoli distrutti (19_Vehicles Fase B)
    m_vehicleTracker.tick(world, m_map, m_registry, m_mesh, m_tex, dt);

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
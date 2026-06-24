#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"

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
    out.meshPath = enemy->meshPath;

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

static std::vector<std::string> buildEnemySpawnList(const DefinitionRegistry* registry, int count)
{
    std::vector<std::string> result;
    if (count <= 0) return result;

    std::vector<std::string> preferredIds;

    if (registry)
    {
        const MapDef* map = registry->getMap("firebase");
        if (map && !map->enemyTypes.empty())
        {
            preferredIds = map->enemyTypes;
            std::cout << "[ConquestMode] Uso enemy_types da map 'firebase' ("
                      << preferredIds.size() << " tipi).\n";
        }
    }

    if (preferredIds.empty())
    {
        preferredIds = {"grunt", "heavy", "sniper"};
        std::cout << "[ConquestMode] enemy_types non trovati: uso fallback grunt/heavy/sniper.\n";
    }

    for (int i = 0; i < count; ++i)
        result.push_back(preferredIds[i % (int)preferredIds.size()]);

    return result;
}
} // namespace

void ConquestMode::spawnUnit(World& world, const RespawnEntry& info)
{
    EntityId e = world.createEntity();
    float yPos = info.stationary ? AI_PLT_Y : AI_GND_Y;

    world.addTransform(e, {info.x, yPos, info.z});
    world.addTeam(e, {info.teamId});
    world.addHealth(e, {info.hp, info.hp});
    Mesh* useMesh = (info.entityMesh ? info.entityMesh : m_mesh);
    world.addMeshRenderer(e, {useMesh, m_tex, info.mr, info.mg, info.mb});

    world.addAi(e, AiComponent{
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
    });

    UnitTemplate tpl = {
        info.x, info.z, yPos, info.teamId,
        info.mr, info.mg, info.mb,
        info.br, info.bg, info.bb,
        info.hp, info.pax, info.paz, info.pbx, info.pbz,
        info.patSpd, info.interval, info.range, info.stationary,
        info.hitboxProfileId
    };

    if (m_registry)
    {
        const std::string profileId = info.hitboxProfileId.empty()
                                      ? (info.teamId == 2 ? "grunt" : "")
                                      : info.hitboxProfileId;
        if (!profileId.empty())
        {
            const auto* hp = m_registry->getHitboxProfile(profileId);
            if (hp) world.addHitbox(e, HitboxComponent{hp});
        }
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
    m_spawnPos  = {0, SPAWN_Y, SPAWN_Z};
    m_mesh      = mesh;
    m_tex       = tex;
    m_registry  = registry;
    m_meshCache = meshCache;

    m_team1Tickets = initialTeam1Tickets;
    m_team2Tickets = initialTeam2Tickets;
    m_respawnQueue.clear();
    m_trackedUnits.clear();

    // ── Giocatore ────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {0, SPAWN_Y, SPAWN_Z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Lista nemici da mappa/registry ───────────────────────────────────
    const int nEnemies = std::min(team2AiCount, 20);
    std::vector<std::string> enemyIds = buildEnemySpawnList(registry, nEnemies);

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
                              const std::string& meshPath)
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
        spawnUnit(world, info);
    };

    // ── Posizioni predefinite ─────────────────────────────────────────────
    struct UnitPos { float x, z, pax, paz, pbx, pbz; bool stat; };

    static const UnitPos enemyPos[20] = {
        {  7,-2,   5,-2,   9,-2, false },
        { -7,-2,  -9,-2,  -5,-2, false },
        {  3,-2,   1,-2,   5,-2, false },
        { -3,-2,  -5,-2,  -1,-2, false },
        {  8,-8,   0, 0,   0, 0, true  },
        { -8,-8,   0, 0,   0, 0, true  },
        {  0,-5,  -2,-5,   2,-5, false },
        {  4,-4,   3,-3,   5,-5, false },
        { -4,-4,  -5,-3,  -3,-5, false },
        {  2,-6,   1,-5,   3,-7, false },
        { -2,-6,  -3,-5,  -1,-7, false },
        {  0,-9,  -1,-8,   1,-9.5f, false },
        {  4,-9,   3,-8,   5,-9.5f, false },
        { -4,-9,  -5,-8,  -3,-9.5f, false },
        {  9,-2,   8,-1,  10,-3, false },
        { -9,-2, -10,-1,  -8,-3, false },
        {  9,-6,   8,-5,  10,-7, false },
        { -9,-6, -10,-5,  -8,-7, false },
        {  6,-1,   5, 0,   7,-2, false },
        { -6,-1,  -7, 0,  -5,-2, false },
    };

    static const UnitPos allyPos[10] = {
        { -7, 4, -7, 2, -7, 6, false },
        {  7, 4,  7, 2,  7, 6, false },
        { -4, 5, -4, 3, -4, 7, false },
        {  4, 5,  4, 3,  4, 7, false },
        {  0, 6,  0, 4,  0, 8, false },
        { -6, 6, -6, 4, -6, 8, false },
        {  6, 6,  6, 4,  6, 8, false },
        { -2, 7, -2, 5, -2, 9, false },
        {  2, 7,  2, 5,  2, 9, false },
        {  0, 3,  0, 1,  0, 5, false },
    };

    // ── Spawn nemici dai JSON realmente selezionati ──────────────────────
    for (int i = 0; i < nEnemies; ++i)
    {
        const auto& p = enemyPos[i];
        const std::string enemyId = enemyIds[i];
        const ResolvedEnemyArchetype resolved = resolveEnemyArchetype(registry, enemyId);

        mkUnitWithMesh(p.x, p.z, 2,
               resolved.mr, resolved.mg, resolved.mb,
               resolved.br, resolved.bg, resolved.bb,
               resolved.hp,
               p.pax, p.paz, p.pbx, p.pbz,
               resolved.moveSpeed, resolved.interval, resolved.range,
               resolved.hitboxProfileId, p.stat,
               resolved.bulletSpeed, resolved.bulletDamage, resolved.bulletLifetime,
               resolved.meshPath);
    }

    // ── Lista alleati da mappa/registry ─────────────────────────────────────
    std::vector<std::string> allyIds;
    if (registry)
    {
        const MapDef* map = registry->getMap("firebase");
        if (map && !map->allyTypes.empty())
            allyIds = map->allyTypes;
    }
    if (allyIds.empty())
        allyIds = {"clone_trooper"};

    // ── Spawn alleati AI (data-driven) ───────────────────────────────────────
    int nAllies = std::min(team1AiCount, 10);
    for (int i = 0; i < nAllies; ++i)
    {
        const auto& p = allyPos[i];
        const std::string allyId = allyIds[i % (int)allyIds.size()];

        // Cerca nel registry allies
        const EnemyDef* allyDef = registry ? registry->getAlly(allyId) : nullptr;

        float mr=0.25f, mg=0.45f, mb=1.0f;
        float br=0.30f, bg=0.60f, bb=1.0f;
        float hp=60.0f, pspd=1.8f, intv=3.5f, rng=14.0f;
        std::string hitboxId;
        std::string meshPath;

        if (allyDef)
        {
            mr = allyDef->color[0]; mg = allyDef->color[1]; mb = allyDef->color[2];
            hp = allyDef->hp;
            hitboxId = allyDef->hitboxProfileId;
            meshPath = allyDef->meshPath;

            if (registry)
            {
                const AiProfileDef* ai = registry->getAiProfile(allyDef->aiProfileId);
                if (ai) { pspd = ai->patrolSpeed; intv = ai->shootInterval; rng = ai->sightRange; }

                const WeaponDef* wpn = registry->getWeapon(allyDef->primaryWeaponId());
                if (wpn) { br = wpn->bulletColor[0]; bg = wpn->bulletColor[1]; bb = wpn->bulletColor[2]; }
                else { br = allyDef->bulletColor[0]; bg = allyDef->bulletColor[1]; bb = allyDef->bulletColor[2]; }
            }
        }

        mkUnitWithMesh(p.x, p.z, 1,
                       mr, mg, mb, br, bg, bb, hp,
                       p.pax, p.paz, p.pbx, p.pbz,
                       pspd, intv, rng,
                       hitboxId, p.stat,
                       8.0f, 20.0f, 5.0f,
                       meshPath);
    }

    // ── Geometria ─────────────────────────────────────────────────────────
    auto addBox = [&](float x, float yc, float z, float ry,
                      float sx, float sy, float sz,
                      float cr, float cg, float cb)
    {
        EntityId c = world.createEntity();
        world.addTransform(c, {x, yc, z, 0, ry, 0, sx, sy, sz});
        world.addMeshRenderer(c, {mesh, tex, cr, cg, cb});
        world.addCollider(c, {sx * 0.5f, sy * 0.5f, sz * 0.5f});
    };

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

    std::cout << "[ConquestMode] Spawn completato: "
              << nEnemies << " nemici, " << nAllies << " alleati AI.\n";
}

void ConquestMode::update(World& world, float dt)
{
    checkDeaths(world);

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
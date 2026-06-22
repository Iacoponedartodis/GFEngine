#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"

#include <iostream>
#include <algorithm>

namespace mini
{

static constexpr float SPAWN_Y  = 0.86f;
static constexpr float AI_GND_Y = 0.50f;
static constexpr float AI_PLT_Y = 2.50f;
static constexpr float SPAWN_Z  = 8.0f;

void ConquestMode::spawnUnit(World& world, const RespawnEntry& info)
{
    EntityId e = world.createEntity();
    float yPos = info.stationary ? AI_PLT_Y : AI_GND_Y;
    world.addTransform(e, {info.x, yPos, info.z});
    world.addTeam(e, {info.teamId});
    world.addHealth(e, {info.hp, info.hp});
    world.addMeshRenderer(e, {m_mesh, m_tex, info.mr, info.mg, info.mb});
    world.addAi(e, AiComponent{
        .shootInterval = info.interval, .aggroRange = info.range,
        .bulletMesh = m_mesh, .bulletR = info.br, .bulletG = info.bg, .bulletB = info.bb,
        .patrolAx = info.pax, .patrolAz = info.paz,
        .patrolBx = info.pbx, .patrolBz = info.pbz,
        .patrolSpeed = info.patSpd, .seekSpeed = info.patSpd + 1.5f,
        .strafeTimer = 1.4f, .strafeSign = 1.0f,
        .stationary = info.stationary
    });

    UnitTemplate tpl = {info.x, info.z, yPos, info.teamId,
                        info.mr, info.mg, info.mb,
                        info.br, info.bg, info.bb,
                        info.hp, info.pax, info.paz, info.pbx, info.pbz,
                        info.patSpd, info.interval, info.range, info.stationary,
                        info.hitboxProfileId};

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
                entry.timer            = respawnDelay;
                entry.x                = tpl.x;
                entry.z                = tpl.z;
                entry.teamId           = tpl.teamId;
                entry.mr=tpl.mr; entry.mg=tpl.mg; entry.mb=tpl.mb;
                entry.br=tpl.br; entry.bg=tpl.bg; entry.bb=tpl.bb;
                entry.hp               = tpl.hp;
                entry.pax=tpl.pax; entry.paz=tpl.paz;
                entry.pbx=tpl.pbx; entry.pbz=tpl.pbz;
                entry.patSpd           = tpl.patSpd;
                entry.interval         = tpl.interval;
                entry.range            = tpl.range;
                entry.stationary       = tpl.stationary;
                entry.hitboxProfileId  = tpl.hitboxProfileId;
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
                          const DefinitionRegistry* registry)
{
    std::cout << "[ConquestMode] Firebase — caricamento...\n";
    world.initialize();
    m_spawnPos = {0, SPAWN_Y, SPAWN_Z};
    m_mesh     = mesh;
    m_tex      = tex;
    m_registry = registry;

    m_team1Tickets = initialTeam1Tickets;
    m_team2Tickets = initialTeam2Tickets;
    m_respawnQueue.clear();
    m_trackedUnits.clear();

    // ── Giocatore ────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {0, SPAWN_Y, SPAWN_Z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Legge parametri grunt dal registry (fallback ai valori hardcoded) ─
    float enemyHp       = 80.0f;
    float enemyMoveSpd  = 2.5f;
    float enemyInterval = 2.2f;
    float enemyRange    = 18.0f;
    float enemyMR=0.70f, enemyMG=0.65f, enemyMB=0.50f;
    float enemyBR=1.00f, enemyBG=0.50f, enemyBB=0.00f;
    std::string enemyHitboxProfile = "grunt";

    if (registry)
    {
        const EnemyDef* grunt = registry->getEnemy("grunt");
        if (grunt)
        {
            enemyHp    = grunt->hp;
            enemyMoveSpd = grunt->moveSpeed;
            enemyMR=grunt->color[0]; enemyMG=grunt->color[1]; enemyMB=grunt->color[2];
            enemyBR=grunt->bulletColor[0]; enemyBG=grunt->bulletColor[1]; enemyBB=grunt->bulletColor[2];
            enemyHitboxProfile = grunt->hitboxProfileId;
            const AiProfileDef* ai = registry->getAiProfile(grunt->aiProfileId);
            if (ai) { enemyMoveSpd=ai->patrolSpeed; enemyInterval=ai->shootInterval; enemyRange=ai->sightRange; }
            std::cout << "[ConquestMode] Grunt dal registry: hp=" << enemyHp
                      << " spd=" << enemyMoveSpd << " interval=" << enemyInterval << "\n";
        }
    }

    // ── Legge parametri heavy dal registry ────────────────────────────────
    float heavyHp=140.0f, heavyMoveSpd=2.0f, heavyInterval=1.5f, heavyRange=16.0f;
    float heavyMR=0.50f, heavyMG=0.45f, heavyMB=0.35f;
    float heavyBR=1.00f, heavyBG=0.40f, heavyBB=0.00f;
    std::string heavyHitboxProfile = "heavy";

    if (registry)
    {
        const EnemyDef* heavy = registry->getEnemy("heavy");
        if (heavy)
        {
            heavyHp=heavy->hp; heavyMoveSpd=heavy->moveSpeed;
            heavyMR=heavy->color[0]; heavyMG=heavy->color[1]; heavyMB=heavy->color[2];
            heavyBR=heavy->bulletColor[0]; heavyBG=heavy->bulletColor[1]; heavyBB=heavy->bulletColor[2];
            heavyHitboxProfile=heavy->hitboxProfileId;
            const AiProfileDef* ai = registry->getAiProfile(heavy->aiProfileId);
            if (ai) { heavyMoveSpd=ai->patrolSpeed; heavyInterval=ai->shootInterval; heavyRange=ai->sightRange; }
        }
    }

    // ── Legge parametri sniper dal registry ───────────────────────────────
    float sniperHp=55.0f, sniperMoveSpd=1.8f, sniperInterval=4.0f, sniperRange=30.0f;
    float sniperMR=0.35f, sniperMG=0.30f, sniperMB=0.25f;
    float sniperBR=0.20f, sniperBG=0.50f, sniperBB=1.00f;
    std::string sniperHitboxProfile = "sniper";

    if (registry)
    {
        const EnemyDef* sniper = registry->getEnemy("sniper");
        if (sniper)
        {
            sniperHp=sniper->hp; sniperMoveSpd=sniper->moveSpeed;
            sniperMR=sniper->color[0]; sniperMG=sniper->color[1]; sniperMB=sniper->color[2];
            sniperBR=sniper->bulletColor[0]; sniperBG=sniper->bulletColor[1]; sniperBB=sniper->bulletColor[2];
            sniperHitboxProfile=sniper->hitboxProfileId;
            const AiProfileDef* ai = registry->getAiProfile(sniper->aiProfileId);
            if (ai) { sniperMoveSpd=ai->patrolSpeed; sniperInterval=ai->shootInterval; sniperRange=ai->sightRange; }
        }
    }

    // ── Lambda helper spawn ───────────────────────────────────────────────
    auto mkUnit = [&](float x, float z, int team,
                      float mr, float mg, float mb,
                      float br, float bg, float bb,
                      float hp,
                      float pax, float paz, float pbx, float pbz,
                      float pspd, float intv, float range,
                      const std::string& hitboxProfile = "",
                      bool stat = false)
    {
        RespawnEntry info;
        info.timer           = 0;
        info.x=x; info.z=z;
        info.teamId          = team;
        info.mr=mr; info.mg=mg; info.mb=mb;
        info.br=br; info.bg=bg; info.bb=bb;
        info.hp              = hp;
        info.pax=pax; info.paz=paz; info.pbx=pbx; info.pbz=pbz;
        info.patSpd          = pspd;
        info.interval        = intv;
        info.range           = range;
        info.hitboxProfileId = hitboxProfile;
        info.stationary      = stat;
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

    // ── Spawn nemici ──────────────────────────────────────────────────────
    int nEnemies = std::min(team2AiCount, 20);
    for (int i = 0; i < nEnemies; ++i)
    {
        const auto& p = enemyPos[i];
        if (i == 4 || i == 5)
        {
            mkUnit(p.x, p.z, 2,
                   sniperMR, sniperMG, sniperMB,
                   sniperBR, sniperBG, sniperBB,
                   sniperHp,
                   p.pax, p.paz, p.pbx, p.pbz,
                   sniperMoveSpd, sniperInterval, sniperRange,
                   sniperHitboxProfile, true);
        }
        else
        {
            mkUnit(p.x, p.z, 2,
                   enemyMR, enemyMG, enemyMB,
                   enemyBR, enemyBG, enemyBB,
                   enemyHp,
                   p.pax, p.paz, p.pbx, p.pbz,
                   enemyMoveSpd, enemyInterval, enemyRange,
                   enemyHitboxProfile, p.stat);
        }
    }

    // ── Spawn alleati AI ──────────────────────────────────────────────────
    int nAllies = std::min(team1AiCount, 10);
    for (int i = 0; i < nAllies; ++i)
    {
        const auto& p = allyPos[i];
        mkUnit(p.x, p.z, 1,
               0.25f, 0.45f, 1.0f,
               0.30f, 0.60f, 1.0f,
               60.0f,
               p.pax, p.paz, p.pbx, p.pbz,
               1.8f, 3.5f, 14.0f,
               "", p.stat);
    }

    // ── Geometria ─────────────────────────────────────────────────────────
    auto addBox = [&](float x, float yc, float z, float ry,
                      float sx, float sy, float sz,
                      float cr, float cg, float cb)
    {
        EntityId c = world.createEntity();
        world.addTransform(c, {x, yc, z, 0, ry, 0, sx, sy, sz});
        world.addMeshRenderer(c, {mesh, tex, cr, cg, cb});
        world.addCollider(c, {sx*0.5f, sy*0.5f, sz*0.5f});
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
        float top = i*0.4f, yc = top*0.5f;
        float zc  = -3.575f - (i-1)*0.55f;
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

    // Processa la coda di respawn: ogni entry ha il suo timer interno
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
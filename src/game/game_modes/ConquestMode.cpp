#include "mini/game/game_modes/ConquestMode.hpp"
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

struct UnitTemplate
{
    float x, z, yPos;
    int   teamId;
    float mr, mg, mb;
    float br, bg, bb;
    float hp;
    float pax, paz, pbx, pbz;
    float patSpd, interval, range;
    bool  stationary;
};

static std::vector<std::pair<EntityId, UnitTemplate>> s_trackedUnits;

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
                        info.patSpd, info.interval, info.range, info.stationary};
    s_trackedUnits.push_back({e, tpl});
}

void ConquestMode::checkDeaths(World& world)
{
    auto it = s_trackedUnits.begin();
    while (it != s_trackedUnits.end())
    {
        if (!world.isValidEntity(it->first))
        {
            const auto& tpl = it->second;
            int& tickets = (tpl.teamId == 1) ? m_team1Tickets : m_team2Tickets;

            if (tickets > 0)
            {
                --tickets;
                RespawnEntry entry;
                entry.timer      = respawnDelay;
                entry.x          = tpl.x;
                entry.z          = tpl.z;
                entry.teamId     = tpl.teamId;
                entry.mr         = tpl.mr;
                entry.mg         = tpl.mg;
                entry.mb         = tpl.mb;
                entry.br         = tpl.br;
                entry.bg         = tpl.bg;
                entry.bb         = tpl.bb;
                entry.hp         = tpl.hp;
                entry.pax        = tpl.pax;
                entry.paz        = tpl.paz;
                entry.pbx        = tpl.pbx;
                entry.pbz        = tpl.pbz;
                entry.patSpd     = tpl.patSpd;
                entry.interval   = tpl.interval;
                entry.range      = tpl.range;
                entry.stationary = tpl.stationary;
                m_respawnQueue.push_back(entry);

                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team << " eliminato. Ticket rimasti: "
                          << tickets << " — respawn in " << respawnDelay << "s" << std::endl;
            }
            else
            {
                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team
                          << " eliminato. NESSUN ticket — morte permanente." << std::endl;
            }

            it = s_trackedUnits.erase(it);
        }
        else
        {
            ++it;
        }
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

void ConquestMode::start(World& world, Mesh* mesh, Texture* tex)
{
    std::cout << "[ConquestMode] Firebase — caricamento..." << std::endl;
    world.initialize();
    m_spawnPos = {0, SPAWN_Y, SPAWN_Z};
    m_mesh = mesh;
    m_tex  = tex;

    m_team1Tickets = initialTeam1Tickets;
    m_team2Tickets = initialTeam2Tickets;
    m_respawnQueue.clear();
    s_trackedUnits.clear();

    // Giocatore
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {0, SPAWN_Y, SPAWN_Z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    auto mkUnit = [&](float x, float z, int team,
                      float mr, float mg, float mb,
                      float br, float bg, float bb,
                      float hp,
                      float pax, float paz, float pbx, float pbz,
                      float pspd, float intv, float range,
                      bool stat = false)
    {
        RespawnEntry info;
        info.timer      = 0;
        info.x          = x;
        info.z          = z;
        info.teamId     = team;
        info.mr         = mr; info.mg = mg; info.mb = mb;
        info.br         = br; info.bg = bg; info.bb = bb;
        info.hp         = hp;
        info.pax        = pax; info.paz = paz;
        info.pbx        = pbx; info.pbz = pbz;
        info.patSpd     = pspd;
        info.interval   = intv;
        info.range      = range;
        info.stationary = stat;
        spawnUnit(world, info);
    };

    // Posizioni predefinite nemici (fino a 20 slot)
    struct UnitPos { float x, z, pax, paz, pbx, pbz; bool stat; };
    static const UnitPos enemyPos[20] = {
        {  7,-2,   5,-2,  9,-2, false },
        { -7,-2,  -9,-2, -5,-2, false },
        {  0,-5,  -3,-5,  3,-5, false },
        {  5,-4,   4,-2,  6,-6, false },
        {  8,-8,   0, 0,  0, 0, true  },
        { -8,-8,   0, 0,  0, 0, true  },
        {  4,-3,   2,-1,  6,-5, false },
        { -4,-3,  -6,-1, -2,-5, false },
        {  0,-9,  -2,-7,  2,-9, false },
        {  9,-5,   7,-3, 11,-7, false },
        { -9,-5, -11,-3, -7,-7, false },
        {  3,-7,   1,-5,  5,-9, false },
        { -3,-7,  -5,-5, -1,-9, false },
        {  6,-6,   4,-4,  8,-8, false },
        { -6,-6,  -8,-4, -4,-8, false },
        {  2,-4,   0,-2,  4,-6, false },
        { -2,-4,  -4,-2,  0,-6, false },
        { 10,-3,   8,-1, 12,-5, false },
        {-10,-3, -12,-1, -8,-5, false },
        {  0,-3,  -2,-1,  2,-5, false },
    };

    // Posizioni predefinite alleati (fino a 10 slot)
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

    // Spawn nemici
    int nEnemies = std::min(team2AiCount, 20);
    for (int i = 0; i < nEnemies; ++i)
    {
        const auto& p = enemyPos[i];
        float r = 0.6f + (i % 3) * 0.2f;
        float g = 0.1f + (i % 4) * 0.1f;
        float b = 0.05f;
        mkUnit(p.x, p.z, 2,
               r, g, b,
               1.0f, 0.6f, 0.0f,
               80.0f,
               p.pax, p.paz, p.pbx, p.pbz,
               2.5f, 2.2f, 18.0f, p.stat);
    }

    // Spawn alleati AI
    int nAllies = std::min(team1AiCount, 10);
    for (int i = 0; i < nAllies; ++i)
    {
        const auto& p = allyPos[i];
        mkUnit(p.x, p.z, 1,
               0.25f, 0.45f, 1.0f,
               0.3f,  0.6f,  1.0f,
               60.0f,
               p.pax, p.paz, p.pbx, p.pbz,
               1.8f, 3.5f, 14.0f, p.stat);
    }

    // Geometria
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
        float zc = -3.575f - (i-1)*0.55f;
        addBox( 8, yc, zc, 0, 3, top, 0.5f, 0.36f,0.32f,0.28f);
        addBox(-8, yc, zc, 0, 3, top, 0.5f, 0.36f,0.32f,0.28f);
    }
    addBox( 0, 0.25f, 0, 0, 3,0.5f,2, 0.38f,0.35f,0.30f);
    addBox(-5,0.7f,-1, 0, 0.4f,1.4f,3, 0.40f,0.38f,0.34f);
    addBox( 5,0.7f,-1, 0, 0.4f,1.4f,3, 0.40f,0.38f,0.34f);
    addBox(-3,0.7f, 3, 90, 0.4f,1.4f,2.5f, 0.44f,0.42f,0.38f);
    addBox( 3,0.7f, 3, 90, 0.4f,1.4f,2.5f, 0.44f,0.42f,0.38f);

    std::cout << "[ConquestMode] Firebase pronto." << std::endl;
    std::cout << "  Nemici: " << m_team2Tickets << " ticket | " << nEnemies << " AI attive" << std::endl;
    std::cout << "  Alleati: " << m_team1Tickets << " ticket | " << nAllies << " AI attive" << std::endl;
}

void ConquestMode::update(World& world, float dt)
{
    world.tick(dt);
    checkDeaths(world);

    auto it = m_respawnQueue.begin();
    while (it != m_respawnQueue.end())
    {
        it->timer -= dt;
        if (it->timer <= 0.0f)
        {
            spawnUnit(world, *it);
            const char* team = (it->teamId == 1) ? "alleato" : "nemico";
            std::cout << "[Respawn] " << team << " rinato!" << std::endl;
            it = m_respawnQueue.erase(it);
        }
        else { ++it; }
    }
}

} // namespace mini
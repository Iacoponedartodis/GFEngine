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

// Info per poter respawnare un'unità identica
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

// Tiene traccia delle unità vive per rilevare le morti
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

    // Traccia per rilevare morte
    UnitTemplate tpl = {info.x, info.z, yPos, info.teamId,
                        info.mr, info.mg, info.mb,
                        info.br, info.bg, info.bb,
                        info.hp, info.pax, info.paz, info.pbx, info.pbz,
                        info.patSpd, info.interval, info.range, info.stationary};
    s_trackedUnits.push_back({e, tpl});
}

void ConquestMode::checkDeaths(World& world)
{
    // Controlla se unità tracciate sono state distrutte (da CombatSystem)
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
                // Aggiungi alla coda di respawn
                m_respawnQueue.push_back(RespawnEntry{
                    .timer = respawnDelay,
                    .x = tpl.x, .z = tpl.z,
                    .teamId = tpl.teamId,
                    .mr = tpl.mr, .mg = tpl.mg, .mb = tpl.mb,
                    .br = tpl.br, .bg = tpl.bg, .bb = tpl.bb,
                    .hp = tpl.hp,
                    .pax = tpl.pax, .paz = tpl.paz,
                    .pbx = tpl.pbx, .pbz = tpl.pbz,
                    .patSpd = tpl.patSpd, .interval = tpl.interval,
                    .range = tpl.range,
                    .stationary = tpl.stationary
                });

                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team << " eliminato. Ticket rimasti: "
                          << tickets << " — respawn in " << respawnDelay << "s" << std::endl;
            }
            else
            {
                const char* team = (tpl.teamId == 1) ? "Alleato" : "Nemico";
                std::cout << "[Respawn] " << team << " eliminato. NESSUN ticket — morte permanente." << std::endl;
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
    respawnDelay        = s.respawnDelay;
    aiSpeed             = s.aiSpeed;
    aiFireInterval      = s.aiFireInterval;
    aiRange             = s.aiRange;
    playerHp            = s.playerHp;
    playerSpeed         = s.playerSpeed;
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

    // ── Giocatore (NON tracciato per respawn — gestito da Application) ─
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {0, SPAWN_Y, SPAWN_Z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {100.0f, 100.0f});

    // Helper per creare e tracciare unità
    auto mkUnit = [&](float x, float z, int team,
                      float mr, float mg, float mb,
                      float br, float bg, float bb,
                      float hp,
                      float pax, float paz, float pbx, float pbz,
                      float pspd, float intv, float range,
                      bool stat = false)
    {
        RespawnEntry info = {
            .timer = 0, .x = x, .z = z, .teamId = team,
            .mr = mr, .mg = mg, .mb = mb,
            .br = br, .bg = bg, .bb = bb,
            .hp = hp, .pax = pax, .paz = paz, .pbx = pbx, .pbz = pbz,
            .patSpd = pspd, .interval = intv, .range = range,
            .stationary = stat
        };
        spawnUnit(world, info);
    };

    // ═══ NEMICI ═══════════════════════════════════════════════════════
    mkUnit( 7,-2, 2, 1.0f,0.18f,0.08f, 1.0f,0.5f,0.0f, 80,
             5,-2, 9,-2, 2.5f,2.2f,18);
    mkUnit(-7,-2, 2, 0.7f,0.10f,0.85f, 0.8f,0.2f,0.9f, 80,
            -9,-2,-5,-2, 2.0f,2.8f,18);
    mkUnit( 0,-5, 2, 1.0f,0.50f,0.00f, 1.0f,0.7f,0.0f, 70,
            -3,-5, 3,-5, 2.8f,3.0f,16);
    mkUnit( 5,-4, 2, 0.2f,0.75f,0.20f, 0.3f,0.9f,0.3f, 70,
             4,-2, 6,-6, 2.0f,2.5f,16);
    mkUnit( 8,-8, 2, 1.0f,0.85f,0.00f, 1.0f,0.9f,0.1f, 60,
             0,0,0,0, 0,4.0f,24, true);
    mkUnit(-8,-8, 2, 0.1f,0.75f,0.85f, 0.2f,0.8f,1.0f, 60,
             0,0,0,0, 0,4.0f,24, true);

    // ═══ ALLEATO ══════════════════════════════════════════════════════
    mkUnit(-7, 4, 1, 0.25f,0.45f,1.0f, 0.3f,0.6f,1.0f, 60,
            -7, 2,-7, 6, 1.8f,3.5f,14);

    // ═══ GEOMETRIA ════════════════════════════════════════════════════
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
    std::cout << "  Nemici: " << m_team2Tickets << " ticket | Alleati: "
              << m_team1Tickets << " ticket" << std::endl;
}

void ConquestMode::update(World& world, float dt)
{
    world.tick(dt);

    // Controlla morti e metti in coda respawn
    checkDeaths(world);

    // Processa coda respawn
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
        else
        {
            ++it;
        }
    }
}

} // namespace mini
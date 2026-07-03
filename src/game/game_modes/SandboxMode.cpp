#include "mini/game/game_modes/SandboxMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/GameConfig.hpp"

#include <iostream>
#include <string>

namespace mini
{

static constexpr float SPAWN_Y  = 0.86f;
static constexpr float GND_Y    = 0.0f;   // suolo piatto a Y=0

void SandboxMode::start(World& world, Mesh* mesh, Texture* tex,
                        const DefinitionRegistry* registry,
                        const MeshCache* meshCache)
{
    std::cout << "[SandboxMode] Avvio — mappa firebase...\n";
    world.initialize();
    m_mesh      = mesh;
    m_tex       = tex;
    m_registry  = registry;
    m_meshCache = meshCache;
    m_dummies.clear();
    m_respawnQueue.clear();

    // ── Spawn point dalla mappa firebase ──────────────────────────────────
    float p1x = 0.0f, p1z = 8.0f;     // giocatore (team1)
    float p2x = 0.0f, p2z = -8.0f;    // manichini  (team2)
    const MapDef* map = registry ? registry->getMap("firebase") : nullptr;
    if (map)
    {
        p1x = map->spawnTeam1[0]; p1z = map->spawnTeam1[2];
        p2x = map->spawnTeam2[0]; p2z = map->spawnTeam2[2];
    }
    m_spawnPos = {p1x, SPAWN_Y, p1z};

    // ── Giocatore ─────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {p1x, SPAWN_Y, p1z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Geometria della mappa (firebase) o arena di fallback ──────────────
    if (map && !map->geometry.empty())
        buildMapGeometry(world);
    else
        buildArena(world);

    // ── Manichini sullo spawn nemici (per controllare gli spawn point) ────
    std::string dummyId = "B1 Battle Droid";
    if (registry && !registry->enemies().empty())
        dummyId = registry->enemies().begin()->first;

    // Una fila di manichini centrata sullo spawn team2.
    for (int i = -2; i <= 2; ++i)
        spawnDummy(world, { p2x + i * 3.0f, p2z, dummyId });
}

EntityId SandboxMode::spawnDummy(World& world, const DummyInfo& info)
{
    const EnemyDef* def = m_registry ? m_registry->getEnemy(info.id) : nullptr;

    Mesh* useMesh = m_mesh;
    float rx = 0, ry = 0, scale = 1;
    float footY = 0;
    float cr = 0.80f, cg = 0.15f, cb = 0.15f;

    if (def)
    {
        rx    = def->meshRotX;
        ry    = def->meshRotY;
        scale = def->meshScale;
        footY = def->footY();
        cr    = def->color[0];
        cg    = def->color[1];
        cb    = def->color[2];

        if (m_meshCache && !def->meshPath.empty())
        {
            auto it = m_meshCache->find(def->meshPath);
            if (it != m_meshCache->end()) useMesh = it->second;
        }
    }

    EntityId e = world.createEntity();
    const float spawnY = GND_Y + config::AI_HALF_Y;  // centro fisico su suolo
    world.addTransform(e, {info.x, spawnY, info.z, rx, ry, 0, scale, scale, scale});
    world.addTeam(e, {2});
    world.addHealth(e, {100.0f, 100.0f});  // muore e respawna

    const bool hasRealMesh = (useMesh != m_mesh);
    MeshRendererComponent mrc;
    mrc.mesh        = useMesh;
    mrc.texture     = m_tex;
    mrc.r           = cr;
    mrc.g           = cg;
    mrc.b           = cb;
    // Modello GLB: piedi a Y=0, abbassa fino al suolo. Cubo: centrato.
    mrc.meshOffsetY = hasRealMesh ? (-footY - config::AI_HALF_Y) : 0.0f;
    world.addMeshRenderer(e, mrc);

    // Hitbox (così i colpi alla testa contano) se il profilo esiste.
    // Nessun AI component → resta fermo, non spara.

    m_dummies.push_back({e, info});
    return e;
}

void SandboxMode::buildMapGeometry(World& world)
{
    const MapDef* map = m_registry ? m_registry->getMap("firebase") : nullptr;
    if (!map) return;

    for (const auto& gb : map->geometry)
    {
        EntityId c = world.createEntity();
        world.addTransform(c, {gb.x, gb.y, gb.z, 0, gb.ry, 0, gb.sx, gb.sy, gb.sz});
        world.addMeshRenderer(c, {m_mesh, m_tex, gb.r, gb.g, gb.b});
        if (gb.collider)
            world.addCollider(c, {gb.sx * 0.5f, gb.sy * 0.5f, gb.sz * 0.5f});
    }
    std::cout << "[Sandbox] Geometria firebase: " << map->geometry.size() << " box.\n";
}

void SandboxMode::buildArena(World& world)
{
    auto addBox = [&](float x, float yc, float z, float sx, float sy, float sz,
                      float cr, float cg, float cb)
    {
        EntityId c = world.createEntity();
        world.addTransform(c, {x, yc, z, 0, 0, 0, sx, sy, sz});
        world.addMeshRenderer(c, {m_mesh, m_tex, cr, cg, cb});
        world.addCollider(c, {sx * 0.5f, sy * 0.5f, sz * 0.5f});
    };

    addBox(0, -0.1f, 0,  30, 0.2f, 30,  0.38f, 0.34f, 0.28f);
    addBox(  0, 1.0f, -15, 30, 2.0f, 0.4f, 0.25f, 0.22f, 0.20f);
    addBox(  0, 1.0f,  15, 30, 2.0f, 0.4f, 0.25f, 0.22f, 0.20f);
    addBox(-15, 1.0f,   0, 0.4f, 2.0f, 30, 0.25f, 0.22f, 0.20f);
    addBox( 15, 1.0f,   0, 0.4f, 2.0f, 30, 0.25f, 0.22f, 0.20f);
    std::cout << "[Sandbox] Arena di fallback costruita.\n";
}

void SandboxMode::update(World& world, float dt)
{
    // Manichini eliminati → in coda per il respawn
    auto it = m_dummies.begin();
    while (it != m_dummies.end())
    {
        if (!world.isValidEntity(it->first))
        {
            m_respawnQueue.push_back({m_respawnDelay, it->second});
            std::cout << "[Sandbox] Manichino eliminato — respawn in "
                      << m_respawnDelay << "s\n";
            it = m_dummies.erase(it);
        }
        else ++it;
    }

    // Timer di respawn
    for (auto rq = m_respawnQueue.begin(); rq != m_respawnQueue.end(); )
    {
        rq->first -= dt;
        if (rq->first <= 0.0f)
        {
            spawnDummy(world, rq->second);
            rq = m_respawnQueue.erase(rq);
        }
        else ++rq;
    }
}

} // namespace mini

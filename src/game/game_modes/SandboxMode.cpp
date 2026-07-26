#include "mini/game/game_modes/SandboxMode.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/MapQuery.hpp"
#include "mini/game/WeaponAttach.hpp"
#include "mini/game/ClassResolve.hpp"   // effectiveUnit (ADR-023): classi come tipo-unità
#include "mini/game/VehicleSpawn.hpp"
#include "mini/game/StrategicTargets.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/physics/Collision.hpp"

#include <algorithm>
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
    const MapDef* map = registry ? registry->getMap(m_mapId) : nullptr;
    world.activeMap = map;   // canale metadata per l'AiSystem (doc 18)
    if (map)
    {
        p1x = map->spawnTeam1[0]; p1z = map->spawnTeam1[2];
        p2x = map->spawnTeam2[0]; p2z = map->spawnTeam2[2];
    }
    // Spawn posato a terra (data-driven): groundedSpawn interroga i collider
    // della MapDef per mettere il giocatore SUL suolo reale (non a Y fissa) e
    // fuori dagli ostacoli. Prima nasceva a Y=0.86 coi piedi a 0 mentre il
    // "Pavimento" ha il top a y>0 -> incastrato -> lo step-up lo lanciava in
    // aria ("respawn sospeso sopra un muro"). Y-occhi = suolo + PLAYER_HALF_Y.
    m_spawnPos = map
        ? mapquery::groundedSpawn(map, p1x, p1z,
                                  config::PLAYER_HALF_X, config::PLAYER_HALF_Y,
                                  config::PLAYER_HALF_Z, config::PLAYER_HALF_Y,
                                  0.0f, (p1z > 0.0f ? -1.0f : 1.0f))
        : glm::vec3{p1x, SPAWN_Y, p1z};

    // ── Giocatore ─────────────────────────────────────────────────────────
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {m_spawnPos.x, m_spawnPos.y, m_spawnPos.z});
    world.addTeam(m_playerEntity, {1});
    world.addHealth(m_playerEntity, {playerHp, playerHp});

    // ── Geometria della mappa (firebase) o arena di fallback ──────────────
    if (map && !map->geometry.empty())
        buildMapGeometry(world);
    else
        buildArena(world);

    // ── Command post: visibili e catturabili anche in sandbox ─────────────
    if (map)
        m_commandPosts.init(world, map->commandPosts, mesh, tex);

    // ── Strutture strategiche (doc 25/34/35, helper condiviso) ────────────
    // Mancavano del tutto: le torri erano dati della mappa ma in sandbox non
    // comparivano, quindi non erano né provabili né osservabili lì dove si prova
    // tutto il resto (segnalato dall'utente).
    structures::spawnAll(world, map, registry, mesh, tex, meshCache);

    // ── Veicoli in mappa (19_Vehicles, helper condiviso — R6) ─────────────
    // Il tracker li fa respawnare al loro spawn quando distrutti (Fase B).
    vehiclespawn::spawnFromMap(world, map, registry, meshCache, mesh, tex,
                               &m_vehicleTracker);

    // ── Manichini nemici sullo spawn team2 ────────────────────────────────
    // Almeno un manichino per OGNI definizione registrata (round-robin):
    // così ogni nemico/alleato autorato è subito testabile in sandbox.
    // Nessun id hardcoded (ADR-001/007, KI #24): registry vuoto = nessuno
    // spawn + log errore, come in ConquestMode.
    std::vector<std::string> enemyIds;
    if (registry)
    {
        for (auto& [id, def] : registry->enemies()) enemyIds.push_back(id);
        // + le CLASSI istanziabili il cui corpo è un NEMICO (ADR-023): così la
        //   sandbox testa sia i corpi sia le professioni (es. B1 Heavy Battle Droid).
        for (auto& [cid, c] : registry->classes())
            if (!c.baseEntityId.empty() && registry->enemies().count(c.baseEntityId))
                enemyIds.push_back(cid);
    }
    std::sort(enemyIds.begin(), enemyIds.end());
    if (enemyIds.empty())
        std::cerr << "[Sandbox] ERRORE: nessun nemico in data/enemies/ — "
                     "nessun manichino nemico.\n";

    // Formazione a GRIGLIA (non fila singola): con conteggi alti da stress test
    // (fino a config::MAX_AI_PER_TEAM) una fila si estenderebbe fuori mappa. La
    // griglia resta nei limiti; findFreeSpot spinge ogni cella fuori dagli
    // ostacoli, avanzando verso il centro (come genPositions in ConquestMode).
    auto gridSpawn = [&](int n, float baseX, float baseZ, float dirZ,
                         const std::vector<std::string>& ids, int team)
    {
        const int   perRow = 10;
        const float gx = 2.8f, gz = 2.5f;
        // Guarda verso il nemico: `dirZ` punta al campo avversario (convenzione
        // ry = atan2(dx,dz) in gradi, come AiSystem). Prima i manichini restavano
        // a ry=0 → clone e droidi guardavano tutti nella stessa direzione.
        const float facing = (dirZ > 0.0f) ? 0.0f : 180.0f;
        for (int i = 0; i < n; ++i)
        {
            const int row = i / perRow, col = i % perRow;
            float x = baseX + ((float)col - (float)(perRow - 1) * 0.5f) * gx;
            float z = baseZ + dirZ * (2.5f + (float)row * gz);
            mapquery::findFreeSpot(map, x, z, 0.0f, dirZ, 0.45f, 0.5f, 0.45f);
            spawnDummy(world, { x, z, ids[i % (int)ids.size()], team, facing });
        }
    };

    // Multi-spawn: distribuisce n unità sui punti (se presenti), altrimenti tutte sul
    // punto base (retrocompat). I primi n%m punti prendono un'unità in più.
    auto spawnDistributed = [&](int n, const std::vector<std::array<float,3>>& points,
                                float baseX, float baseZ, float dirZ,
                                const std::vector<std::string>& ids, int team)
    {
        if (points.empty()) { gridSpawn(n, baseX, baseZ, dirZ, ids, team); return; }
        const int m = (int)points.size();
        for (int p = 0; p < m; ++p)
        {
            const int cnt = n / m + (p < n % m ? 1 : 0);
            if (cnt > 0) gridSpawn(cnt, points[p][0], points[p][2], dirZ, ids, team);
        }
    };
    const std::vector<std::array<float,3>> noPts;
    const auto& spawnPts1 = map ? map->spawnPointsTeam1 : noPts;
    const auto& spawnPts2 = map ? map->spawnPointsTeam2 : noPts;

    const int nEnemies = enemyIds.empty() ? 0
                       : std::max(enemyCount, (int)enemyIds.size());
    spawnDistributed(nEnemies, spawnPts2, p2x, p2z, (p1z > p2z ? 1.0f : -1.0f), enemyIds, 2);

    // ── Manichini alleati vicino allo spawn team1 (verso il centro) ───────
    std::vector<std::string> allyIds;
    if (registry)
    {
        for (auto& [id, def] : registry->allies()) allyIds.push_back(id);
        for (auto& [cid, c] : registry->classes())   // classi con corpo ALLEATO (ADR-023)
            if (!c.baseEntityId.empty() && registry->allies().count(c.baseEntityId))
                allyIds.push_back(cid);
    }
    std::sort(allyIds.begin(), allyIds.end());
    if (allyIds.empty())
        std::cerr << "[Sandbox] ERRORE: nessun alleato in data/allies/ — "
                     "nessun manichino alleato.\n";

    const float dirZ = (p2z > p1z) ? 1.0f : -1.0f; // verso il campo
    const int nAllies = allyIds.empty() ? 0
                      : std::max(allyCount, (int)allyIds.size());
    spawnDistributed(nAllies, spawnPts1, p1x, p1z, dirZ, allyIds, 1);
}

EntityId SandboxMode::spawnDummy(World& world, const DummyInfo& info)
{
    // team 1 → alleato, team 2 → nemico. `effectiveUnit` (ADR-023) risolve sia
    // un'ENTITÀ (corpo) sia una CLASSE (→ corpo + loadout/abilità della classe),
    // così un manichino-classe usa il modello del corpo e l'arma/abilità della
    // professione — coerente col gioco.
    EnemyDef dummyStorage;
    const EnemyDef* def = nullptr;
    if (m_registry)
        def = classres::effectiveUnit(*m_registry, info.id, info.team == 1, dummyStorage);

    Mesh* useMesh = m_mesh;
    float rx = 0, ry = info.facing, scale = 1;
    float footY = 0;
    float cr = 0.80f, cg = 0.15f, cb = 0.15f;
    if (info.team == 1) { cr = 0.25f; cg = 0.45f; cb = 1.0f; }

    if (def)
    {
        rx    = def->meshRotX;
        ry    = info.facing + def->meshRotY;   // guarda il nemico (+ correzione mesh)
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
    // Suolo reale della mappa in (x,z): il pavimento firebase ha top a +0.1,
    // non a 0 — prima i manichini affondavano di quella differenza.
    const MapDef* map = m_registry ? m_registry->getMap(m_mapId) : nullptr;
    const float ground = mapquery::groundHeightAt(map, info.x, info.z);
    const float spawnY = ground + config::AI_HALF_Y;  // centro fisico su suolo
    // Fuori dalle ENTITÀ solide (veicoli): la decollisione mapquery vede
    // solo la geometria della mappa, non i mezzi già spawnati.
    const glm::vec3 freePos = physics::nudgeOutOfColliders(
        {info.x, spawnY, info.z}, config::aiHalf(), world);
    world.addTransform(e, {freePos.x, spawnY, freePos.z, rx, ry, 0, scale, scale, scale});
    world.addTeam(e, {info.team});
    world.addHealth(e, {100.0f, 100.0f});  // muore e respawna

    const bool hasRealMesh = (useMesh != m_mesh);
    MeshRendererComponent mrc;
    mrc.mesh        = useMesh;
    mrc.texture     = m_tex;
    mrc.r           = cr;
    mrc.g           = cg;
    mrc.b           = cb;
    // Modello GLB: piedi a Y=0, abbassa fino al suolo. Cubo: centrato.
    // footY è in model space → va scalato come il modello.
    mrc.meshOffsetY = hasRealMesh ? (-footY * scale - config::AI_HALF_Y) : 0.0f;
    // Arma in mano dai metadata dell'editor
    if (hasRealMesh)
    {
        auto wa = weaponattach::resolve(m_registry, m_meshCache, def);
        if (wa.mesh) { mrc.attachMesh = wa.mesh; mrc.attachLocal = wa.local; }
    }
    world.addMeshRenderer(e, mrc);

    // Hitbox dal profilo (così testa/zone contano anche in sandbox).
    // Nessun AI component → resta fermo, non spara.
    if (def && m_registry)
    {
        const std::string profId = def->hitboxProfileId.empty()
                                   ? def->id : def->hitboxProfileId;
        if (const auto* hp = m_registry->getHitboxProfile(profId))
            world.addHitbox(e, HitboxComponent{hp});

        // Abilità (16_AiBehavior): lo shield conta anche sui manichini,
        // così è testabile in sandbox come tutto il resto del combat.
        for (const auto& abId : def->abilityIds)
        {
            const AbilityDef* ab = m_registry->getAbility(abId);
            if (!ab || ab->type != "shield") continue;
            ShieldComponent sh;
            sh.max = sh.current = ab->param1;
            sh.regenRate  = ab->param2;
            sh.regenDelay = ab->param3;
            world.addShield(e, sh);
            break;
        }
    }

    m_dummies.push_back({e, info});
    return e;
}

void SandboxMode::buildMapGeometry(World& world)
{
    const MapDef* map = m_registry ? m_registry->getMap(m_mapId) : nullptr;
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
    // Respawn dei veicoli distrutti (19_Vehicles Fase B)
    const MapDef* map = m_registry ? m_registry->getMap(m_mapId) : nullptr;
    m_vehicleTracker.tick(world, map, m_registry, m_meshCache, m_mesh, m_tex, dt);

    // Command post catturabili (nessuna conseguenza: solo test visivo)
    m_commandPosts.update(world, dt);

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

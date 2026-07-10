#pragma once
// VehicleSpawn.hpp — spawn dei veicoli di una mappa (19_Vehicles, Fase A).
// Helper CONDIVISO tra i game mode (R6: prima Conquest e Sandbox avevano
// due copie identiche). Header-only, nessuna dipendenza dall'editor.

#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/MapQuery.hpp"
#include "mini/core/Telemetry.hpp"

#include <string>

namespace mini::vehiclespawn
{

// Spawna UN veicolo dal suo VehicleSpawnDef. Ritorna 0 se il def manca.
inline EntityId spawnOne(World& world, const MapDef* map,
                         const DefinitionRegistry* registry,
                         Mesh* mesh, Texture* tex, const VehicleSpawnDef& vs)
{
    const VehicleDef* vd = registry ? registry->getVehicle(vs.vehicleId) : nullptr;
    if (!vd) return 0;

    // Fuori dagli ostacoli (uno spawn dentro un muro = mezzo incastrato),
    // spinto verso il centro campo.
    float sx = vs.x, sz = vs.z;
    const float pushZ = (sz > 0.0f) ? -1.0f : 1.0f;
    mapquery::findFreeSpot(map, sx, sz, 0.0f, pushZ,
                           vd->halfX, vd->halfY, vd->halfZ);
    const float gy = mapquery::groundHeightAt(map, sx, sz);

    EntityId v = world.createEntity();
    world.addTransform(v, {sx, gy + vd->halfY, sz, 0, vs.ry, 0,
                           vd->halfX * 2.0f, vd->halfY * 2.0f,
                           vd->halfZ * 2.0f});
    world.addHealth(v, {vd->hp, vd->hp});
    world.addTeam(v, {0});   // neutro finché nessuno guida
    world.addCollider(v, {vd->halfX, vd->halfY, vd->halfZ}); // solido
    world.addMeshRenderer(v, {mesh, tex,
                              vd->color[0], vd->color[1], vd->color[2]});

    VehicleComponent vc;
    vc.maxSpeed    = vd->maxSpeed;
    vc.accel       = vd->accel;
    vc.turnRateDeg = vd->turnRateDeg;
    vc.halfX = vd->halfX; vc.halfY = vd->halfY; vc.halfZ = vd->halfZ;
    world.addVehicle(v, vc);

    telemetry::logInfo("veicolo '" + vd->id + "' spawnato ("
        + std::to_string((int)sx) + "," + std::to_string((int)sz) + ")");
    return v;
}

// Tracker di respawn (Fase B): i mezzi distrutti tornano al loro spawn
// dopo `respawnDelay` secondi. I mode ne tengono uno e chiamano tick().
struct RespawnTracker
{
    std::vector<std::pair<EntityId, VehicleSpawnDef>> active;
    std::vector<std::pair<float,    VehicleSpawnDef>> queue;
    float respawnDelay = 15.0f;

    void reset() { active.clear(); queue.clear(); }

    void tick(World& world, const MapDef* map,
              const DefinitionRegistry* registry,
              Mesh* mesh, Texture* tex, float dt)
    {
        // Mezzi distrutti → coda di respawn
        for (auto it = active.begin(); it != active.end(); )
        {
            if (!world.isValidEntity(it->first))
            {
                queue.push_back({respawnDelay, it->second});
                world.pushEvent("VEICOLO distrutto: torna tra "
                    + std::to_string((int)respawnDelay) + "s");
                it = active.erase(it);
            }
            else ++it;
        }
        // Coda → nuovo spawn
        for (auto it = queue.begin(); it != queue.end(); )
        {
            it->first -= dt;
            if (it->first <= 0.0f)
            {
                const EntityId v = spawnOne(world, map, registry,
                                            mesh, tex, it->second);
                if (v != 0) active.push_back({v, it->second});
                it = queue.erase(it);
            }
            else ++it;
        }
    }
};

// Spawna tutti i veicoli dichiarati in map->vehicleSpawns e li registra
// nel tracker (se fornito) per il respawn automatico.
inline void spawnFromMap(World& world, const MapDef* map,
                         const DefinitionRegistry* registry,
                         Mesh* mesh, Texture* tex,
                         RespawnTracker* tracker = nullptr)
{
    if (!map || !registry) return;
    if (tracker) tracker->reset();
    for (const auto& vs : map->vehicleSpawns)
    {
        const EntityId v = spawnOne(world, map, registry, mesh, tex, vs);
        if (v != 0 && tracker) tracker->active.push_back({v, vs});
    }
}

} // namespace mini::vehiclespawn

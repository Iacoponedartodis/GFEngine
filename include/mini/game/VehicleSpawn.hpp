#pragma once
// VehicleSpawn.hpp — spawn dei veicoli di una mappa (19_Vehicles, Fase A).
// Helper CONDIVISO tra i game mode (R6: prima Conquest e Sandbox avevano
// due copie identiche). Header-only, nessuna dipendenza dall'editor.

#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/game_modes/IGameMode.hpp"   // MeshCache
#include "mini/game/MapQuery.hpp"
#include "mini/core/Telemetry.hpp"

#include <string>

namespace mini::vehiclespawn
{

// Spawna UN veicolo dal suo VehicleSpawnDef. Ritorna 0 se il def manca.
// meshCache: se il veicolo ha una mesh custom caricata, la usa (altrimenti
// box colorato di fallback).
inline EntityId spawnOne(World& world, const MapDef* map,
                         const DefinitionRegistry* registry,
                         const MeshCache* meshCache,
                         Mesh* boxMesh, Texture* tex, const VehicleSpawnDef& vs)
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

    // Mesh custom dal cache (come nemici/alleati); altrimenti box.
    Mesh* useMesh = boxMesh;
    if (meshCache && !vd->meshPath.empty())
    {
        auto it = meshCache->find(vd->meshPath);
        if (it != meshCache->end() && it->second) useMesh = it->second;
    }
    const bool hasCustom = (useMesh != boxMesh);

    EntityId v = world.createEntity();
    // transform.ry = direzione di MARCIA (vs.ry): NON include meshRotY, così
    // ruotare il modello nell'editor NON inverte la guida. La correzione
    // visiva del muso va nel yawOffsetDeg del MeshRenderer.
    if (hasCustom)
    {
        world.addTransform(v, {sx, gy + vd->halfY, sz,
                               vd->meshRotX, vs.ry, 0,
                               vd->meshScale, vd->meshScale, vd->meshScale});
    }
    else
    {
        // Box di fallback: scala = dimensioni del box di collisione.
        world.addTransform(v, {sx, gy + vd->halfY, sz, 0, vs.ry, 0,
                               vd->halfX * 2.0f, vd->halfY * 2.0f,
                               vd->halfZ * 2.0f});
    }
    world.addHealth(v, {vd->hp, vd->hp});
    world.addTeam(v, {0});   // neutro finché nessuno guida
    world.addCollider(v, {vd->halfX, vd->halfY, vd->halfZ}); // solido
    MeshRendererComponent mrc;
    mrc.mesh = useMesh; mrc.texture = tex;
    // Mesh custom: tint BIANCO per mostrare i colori reali del modello.
    // vd->color tinge solo il box di fallback (era un bug: il modello vero
    // veniva moltiplicato per il colore del box).
    if (hasCustom) { mrc.r = mrc.g = mrc.b = 1.0f; }
    else { mrc.r = vd->color[0]; mrc.g = vd->color[1]; mrc.b = vd->color[2]; }
    mrc.meshOffsetY  = hasCustom ? vd->meshOffsetY : 0.0f;
    mrc.yawOffsetDeg = hasCustom ? vd->meshRotY : 0.0f;  // raddrizza il muso
    world.addMeshRenderer(v, mrc);

    VehicleComponent vc;
    vc.maxSpeed    = vd->maxSpeed;
    vc.accel       = vd->accel;
    vc.turnRateDeg = vd->turnRateDeg;
    vc.halfX = vd->halfX; vc.halfY = vd->halfY; vc.halfZ = vd->halfZ;
    // Volume di danno: 0 = usa la collisione (retrocompatibile).
    vc.hitOffsetY = vd->hitOffsetY;
    vc.hitHalfX = (vd->hitHalfX > 0.0f) ? vd->hitHalfX : vd->halfX;
    vc.hitHalfY = (vd->hitHalfY > 0.0f) ? vd->hitHalfY : vd->halfY;
    vc.hitHalfZ = (vd->hitHalfZ > 0.0f) ? vd->hitHalfZ : vd->halfZ;
    world.addVehicle(v, vc);

    telemetry::logInfo("veicolo '" + vd->id + "' spawnato ("
        + std::to_string((int)sx) + "," + std::to_string((int)sz) + ")"
        + (hasCustom ? " [mesh]" : " [box]"));
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
              const MeshCache* meshCache,
              Mesh* boxMesh, Texture* tex, float dt)
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
                                            meshCache, boxMesh, tex, it->second);
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
                         const MeshCache* meshCache,
                         Mesh* boxMesh, Texture* tex,
                         RespawnTracker* tracker = nullptr)
{
    if (!map || !registry) return;
    if (tracker) tracker->reset();
    for (const auto& vs : map->vehicleSpawns)
    {
        const EntityId v = spawnOne(world, map, registry,
                                    meshCache, boxMesh, tex, vs);
        if (v != 0 && tracker) tracker->active.push_back({v, vs});
    }
}

} // namespace mini::vehiclespawn

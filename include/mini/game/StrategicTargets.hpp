#pragma once
#include "mini/ecs/World.hpp"
#include "mini/game/data/Definitions.hpp"
#include "mini/game/MapQuery.hpp"
#include "mini/game/game_modes/IGameMode.hpp"   // MeshCache
#include "mini/core/Telemetry.hpp"
#include <string>

namespace mini::structures
{

// ── Spawn delle strutture strategiche (doc 25/34/35) ─────────────────────────
// Helper CONDIVISO fra i game mode. Prima viveva solo dentro ConquestMode:
// risultato, in sandbox le torri della mappa non comparivano affatto pur essendo
// dati della mappa (segnalato dall'utente). Un'unica implementazione evita che il
// prossimo mode ripeta lo stesso buco.

// Le derivazioni geometriche (scala visiva, semiassi solidi) vivono in
// `StrategicTargetDef` — una sola fonte condivisa con navmesh ed editor.

// Spawna tutte le strutture della mappa e popola `world.strategicTargets`
// (sorgente unica di intel, doc 35) e `world.comms` (doc 34).
inline void spawnAll(World& world, const MapDef* map,
                     const DefinitionRegistry* registry,
                     Mesh* boxMesh, Texture* tex,
                     const MeshCache* meshCache)
{
    world.strategicTargets.clear();
    if (!map || !registry) return;

    for (const auto& t : map->strategicTargets)
    {
        // `y` autorato = altezza SOPRA il suolo (0 = a terra, retro-compatibile).
        // Una struttura statica resta dove la si mette (un'unità cadrebbe).
        const float gy = mapquery::groundHeightAt(map, t.x, t.z) + t.y;
        EntityId e = world.createEntity();

        Mesh* useMesh = boxMesh;   // box di fallback
        if (meshCache && !t.meshPath.empty())
        {
            auto it = meshCache->find(t.meshPath);
            if (it != meshCache->end()) useMesh = it->second;
        }
        const bool box = (useMesh == boxMesh);
        const float sc = t.visualScale();

        world.addTransform(e, {t.x, gy, t.z, 0, t.ry, 0, sc, sc, sc});
        world.addTeam(e, {t.team});   // autorato: serve la torre anche ai CLONI
        world.addHealth(e, {t.hp, t.hp});

        {
            ColliderComponent col;
            t.solidHalfExtents(sc, col.hx, col.hy, col.hz);
            world.addCollider(e, col);
        }

        MeshRendererComponent mrc;
        mrc.mesh = useMesh; mrc.texture = tex;
        mrc.r = t.color[0]; mrc.g = t.color[1]; mrc.b = t.color[2];
        // Grounding: il cubo di fallback è centrato (±0.5) → va alzato di mezza
        // altezza perché la base tocchi il suolo. Mesh reale (base a Y=0): niente.
        mrc.meshOffsetY = box ? (0.5f * sc) : 0.0f;
        world.addMeshRenderer(e, mrc);

        if (const auto* hp = registry->getHitboxProfile("__strategic_target"))
            world.addHitbox(e, HitboxComponent{hp});

        const bool isComms   = (t.role == "comms");
        const bool isControl = (t.role == "control");
        world.strategicTargets.push_back({e, t.label, t.team, isComms, isControl,
                                          t.x, t.z, t.priority, t.engageRadius});
        // Rete di comunicazione (doc 34): chi possiede una torre parte connesso.
        // `hadTower` resta true anche dopo la distruzione — distingue "l'ha persa"
        // da "non l'ha mai avuta".
        if (isComms && (t.team == 1 || t.team == 2))
        {
            world.comms[t.team].hadTower   = true;
            world.comms[t.team].towerAlive = true;
        }
        telemetry::event(telemetry::Level::Info, "Objective", "strategic target spawned",
                         {{"label", t.label}, {"hp", t.hp}, {"team", t.team},
                          {"role", t.role}, {"x", t.x}, {"z", t.z},
                          {"y", gy}});   // altezza mondo effettiva (suolo + y autorato)
    }
}

} // namespace mini::structures

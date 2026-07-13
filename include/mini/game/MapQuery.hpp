#pragma once
// MapQuery — interrogazioni geometriche sulla geometria di mappa (MapDef).
// Usato dai game mode per posizionare unità in modo data-driven invece di
// assumere suolo a y=0 o posizioni libere.

#include "mini/game/data/Definitions.hpp"
#include <glm/glm.hpp>
#include <cmath>

namespace mini::mapquery
{

// Altezza del suolo calpestabile in (x,z): il top più alto tra i collider
// "bassi" (top <= maxWalkableTop) che coprono quel punto. Esclude muri e
// strutture alte. Fallback 0 se nessun collider copre il punto.
inline float groundHeightAt(const MapDef* map, float x, float z,
                            float maxWalkableTop = 1.6f)
{
    if (!map) return 0.0f;
    float best = 0.0f;
    for (const auto& b : map->geometry)
    {
        if (!b.collider) continue;
        const float top = b.y + b.sy * 0.5f;
        if (top > maxWalkableTop) continue;      // muro/struttura, non suolo
        if (std::abs(x - b.x) > b.sx * 0.5f) continue;
        if (std::abs(z - b.z) > b.sz * 0.5f) continue;
        if (top > best) best = top;
    }
    return best;
}

// True se un box unitario centrato in (x, groundY+halfY, z) compenetra un
// collider "ostacolo" (top sopra il suolo locale: muri, coperture, casse).
inline bool overlapsObstacle(const MapDef* map, float x, float z,
                             float halfX, float halfY, float halfZ)
{
    if (!map) return false;
    const float ground = groundHeightAt(map, x, z);
    const float cy     = ground + halfY;
    for (const auto& b : map->geometry)
    {
        if (!b.collider) continue;
        const float top = b.y + b.sy * 0.5f;
        if (top <= ground + 0.05f) continue;     // è il suolo stesso
        if (std::abs(x - b.x)  > b.sx * 0.5f + halfX) continue;
        if (std::abs(z - b.z)  > b.sz * 0.5f + halfZ) continue;
        const float bBot = b.y - b.sy * 0.5f;
        if (cy + halfY <= bBot || cy - halfY >= top) continue;
        return true;
    }
    return false;
}

// Sposta (x,z) lungo (dirX,dirZ) a passi di `step` finché la posizione è
// libera da ostacoli (max maxSteps tentativi). Ritorna la posizione trovata.
inline void findFreeSpot(const MapDef* map, float& x, float& z,
                         float dirX, float dirZ,
                         float halfX, float halfY, float halfZ,
                         float step = 1.0f, int maxSteps = 10)
{
    for (int i = 0; i < maxSteps; ++i)
    {
        if (!overlapsObstacle(map, x, z, halfX, halfY, halfZ)) return;
        x += dirX * step;
        z += dirZ * step;
    }
}

// Posizione di spawn "a terra" per un'entità con mezze-estensioni date:
// (x,z) spostato fuori dagli ostacoli e Y all'altezza degli occhi/centro
// SOPRA il suolo reale della mappa. Evita che il giocatore nasca incastrato
// nel pavimento (top del "Pavimento" a y>0 → step-up spurio che lo lancia in
// aria) o dentro un muro. `eyeHeight` = distanza centro/occhi dal suolo
// (= PLAYER_HALF_Y per il giocatore). push(X,Z) = verso in cui liberarsi
// dagli ostacoli (di default verso il centro campo lungo -Z).
inline glm::vec3 groundedSpawn(const MapDef* map, float x, float z,
                               float halfX, float halfY, float halfZ,
                               float eyeHeight,
                               float pushX = 0.0f, float pushZ = -1.0f)
{
    findFreeSpot(map, x, z, pushX, pushZ, halfX, halfY, halfZ);
    return { x, groundHeightAt(map, x, z) + eyeHeight, z };
}

} // namespace mini::mapquery

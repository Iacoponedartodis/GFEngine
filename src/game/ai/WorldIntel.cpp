#include "mini/game/ai/WorldIntel.hpp"
#include "mini/game/data/Definitions.hpp"

#include <cmath>

namespace mini
{
namespace worldintel
{
namespace
{
constexpr float kPI = 3.14159265358979323846f;
}

const CoverPointDef* bestCoverToward(const MapDef& map,
                                     float x, float z,
                                     float towardX, float towardZ,
                                     float maxDist)
{
    if (map.coverPoints.empty()) return nullptr;
    const float maxDist2 = maxDist * maxDist;
    float bestScore = -1e30f;
    const CoverPointDef* best = nullptr;
    for (const auto& c : map.coverPoints)
    {
        const float dx = c.x - x, dz = c.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 >= maxDist2) continue;
        // Il fronte della copertura deve guardare verso il bersaglio.
        float ex = towardX - c.x, ez = towardZ - c.z;
        const float el = std::sqrt(ex * ex + ez * ez);
        if (el < 0.5f) continue;
        ex /= el; ez /= el;
        const float fr = c.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        if (fx * ex + fz * ez <= 0.15f) continue;   // copre nella direzione sbagliata
        // Punteggio (ADR-026): premia la PROTEZIONE, penalizza la distanza. Con
        // protezione uniforme (0.5 ovunque) vince la più vicina — retrocompatibile.
        const float score = c.protection - 0.5f * (d2 / maxDist2);
        if (score > bestScore) { bestScore = score; best = &c; }
    }
    return best;
}

float dangerAt(const MapDef& map, float x, float z)
{
    float total = 0.0f;
    for (const auto& d : map.dangerZones)
    {
        const float dx = x - d.x, dz = z - d.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= d.radius) continue;
        total += d.dangerLevel * (1.0f - dist / d.radius);
    }
    return total;
}

const TacticalPointDef* nearestTacticalPoint(const MapDef& map, float x, float z,
                                             const char* type, float maxDist)
{
    if (map.tacticalPoints.empty()) return nullptr;
    const bool anyType = (type == nullptr || type[0] == '\0');
    float best2 = maxDist * maxDist;
    const TacticalPointDef* best = nullptr;
    for (const auto& t : map.tacticalPoints)
    {
        if (!anyType && t.type != type) continue;
        const float dx = t.x - x, dz = t.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best2) { best2 = d2; best = &t; }
    }
    return best;
}

} // namespace worldintel
} // namespace mini

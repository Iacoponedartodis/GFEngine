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

const TacticalPositionDef* bestCoverToward(const MapDef& map,
                                           float x, float z,
                                           float towardX, float towardZ,
                                           float maxDist)
{
    if (map.tacticalPositions.empty()) return nullptr;
    const float maxDist2 = maxDist * maxDist;
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;
    for (const auto& c : map.tacticalPositions)
    {
        if (c.protection <= 0.0f) continue;   // non ripara: non è una copertura
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

bool hasLineOfFire(const MapDef& map,
                   float ax, float ay, float az,
                   float bx, float by, float bz)
{
    const float dx = bx - ax, dy = by - ay, dz = bz - az;
    for (const auto& b : map.geometry)
    {
        if (!b.collider) continue;

        // Porta origine e direzione nel frame LOCALE del box (rotazione -ry),
        // dove il test torna a essere un semplice slab test su un AABB.
        const float rr = -b.ry * (kPI / 180.0f);
        const float cs = std::cos(rr), sn = std::sin(rr);
        const float ox = ax - b.x, oz = az - b.z;
        const float lox = ox * cs - oz * sn;
        const float loz = ox * sn + oz * cs;
        const float ldx = dx * cs - dz * sn;
        const float ldz = dx * sn + dz * cs;

        const float o[3] = {lox, ay - b.y, loz};
        const float d[3] = {ldx, dy,       ldz};
        const float h[3] = {b.sx * 0.5f, b.sy * 0.5f, b.sz * 0.5f};

        float tMin = 0.0f, tMax = 1.0f;   // segmento: t in [0,1]
        bool  hit  = true;
        for (int k = 0; k < 3 && hit; ++k)
        {
            if (std::fabs(d[k]) < 1e-6f)
            {
                if (o[k] < -h[k] || o[k] > h[k]) hit = false;   // parallelo e fuori
            }
            else
            {
                float t1 = (-h[k] - o[k]) / d[k];
                float t2 = ( h[k] - o[k]) / d[k];
                if (t1 > t2) { const float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) hit = false;
            }
        }
        if (hit) return false;   // un box blocca il segmento
    }
    return true;
}

void buildTacticalLinks(MapDef& map)
{
    const size_t n = map.tacticalPositions.size();
    map.positionCovers.assign(n, {});
    if (n == 0) return;

    // Altezza di riferimento per la linea di tiro: circa il petto di un'unità in
    // posizione. Non si usa la y del pavimento, o ogni muretto bloccherebbe tutto.
    constexpr float kEyeH = 1.2f;

    for (size_t i = 0; i < n; ++i)
    {
        const auto& a = map.tacticalPositions[i];
        if (!a.canShoot) continue;
        const float fr = a.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        const float cosHalf = std::cos((a.fireArcDeg * 0.5f) * (kPI / 180.0f));

        for (size_t k = 0; k < n; ++k)
        {
            if (k == i) continue;
            const auto& b = map.tacticalPositions[k];
            float tx = b.x - a.x, tz = b.z - a.z;
            const float len = std::sqrt(tx * tx + tz * tz);
            if (len > a.fireRange || len < 0.01f) continue;
            tx /= len; tz /= len;
            if (fx * tx + fz * tz < cosHalf) continue;          // fuori settore
            if (!hasLineOfFire(map, a.x, a.y + kEyeH, a.z,
                                    b.x, b.y + kEyeH, b.z)) continue;
            map.positionCovers[i].push_back((int)k);
        }
    }

    // Esposizione (ADR-033): si INVERTE il grafo appena costruito — quante
    // posizioni possono battere la posizione k. Costo nullo, nessun dato nuovo.
    std::vector<int> hitBy(n, 0);
    for (size_t i = 0; i < n; ++i)
        for (int k : map.positionCovers[i]) ++hitBy[(size_t)k];

    map.positionExposure.assign(n, 0.0f);
    const float denom = (n > 1) ? (float)(n - 1) : 1.0f;
    for (size_t k = 0; k < n; ++k)
        map.positionExposure[k] = (float)hitBy[k] / denom;
}

const TacticalPositionDef* bestFlankingPosition(const MapDef& map,
                                                float fromX, float fromZ,
                                                float targetX, float targetZ,
                                                float threatX, float threatZ,
                                                float maxDist)
{
    if (map.tacticalPositions.empty()) return nullptr;

    // Direzione da cui il bersaglio è GIÀ ingaggiato: aggirare vuol dire colpirlo
    // da un'altra parte, quindi si premia l'angolo rispetto a questa.
    float thx = threatX - targetX, thz = threatZ - targetZ;
    const float thLen = std::sqrt(thx * thx + thz * thz);
    if (thLen < 0.01f) return nullptr;
    thx /= thLen; thz /= thLen;

    const float maxDist2 = maxDist * maxDist;
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;

    for (size_t i = 0; i < map.tacticalPositions.size(); ++i)
    {
        const auto& p = map.tacticalPositions[i];
        if (!p.canShoot) continue;

        const float mx = p.x - fromX, mz = p.z - fromZ;
        const float myD2 = mx * mx + mz * mz;
        if (myD2 >= maxDist2) continue;

        // Deve poter battere il bersaglio: gittata, settore, linea di tiro.
        float tx = targetX - p.x, tz = targetZ - p.z;
        const float tLen = std::sqrt(tx * tx + tz * tz);
        if (tLen > p.fireRange || tLen < 0.01f) continue;
        tx /= tLen; tz /= tLen;
        const float fr = p.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        if (fx * tx + fz * tz < std::cos((p.fireArcDeg * 0.5f) * (kPI / 180.0f))) continue;
        if (!hasLineOfFire(map, p.x, p.y + 1.2f, p.z, targetX, p.y + 1.2f, targetZ)) continue;

        // Angolo di FIANCO: direzione bersaglio→posizione contro bersaglio→minaccia.
        // dot = 1 → stessa direzione (frontale, nessun aggiramento); -1 → alle spalle.
        const float px = -tx, pz = -tz;              // dal bersaglio verso la posizione
        const float dot = px * thx + pz * thz;
        const float flankBonus = (1.0f - dot) * 0.5f;   // 0 = frontale, 1 = opposto

        const float exposure = (i < map.positionExposure.size())
                             ? map.positionExposure[i] : 0.0f;

        const float score = flankBonus * 1.5f          // aggirare è lo scopo
                          + p.protection * 0.6f
                          + (1.0f - exposure) * 0.6f   // preferisci il coperto
                          - 0.4f * (myD2 / maxDist2);  // ma non attraversare la mappa
        if (score > bestScore) { bestScore = score; best = &p; }
    }
    return best;
}

const TacticalPositionDef* bestOverwatchFor(const MapDef& map,
                                            float fromX, float fromZ,
                                            float advanceX, float advanceZ,
                                            float maxDist)
{
    // "Da dove posso coprire un compagno che avanza LÌ": una posizione di tiro
    // raggiungibile che batte il punto d'avanzata. Riusa la stessa regola della
    // scelta di tiro, col bersaglio = punto verso cui il compagno si muove.
    return bestFiringPosition(map, fromX, fromZ, advanceX, advanceZ, maxDist);
}

const TacticalPositionDef* bestOverwatchForPosition(const MapDef& map,
                                                    float fromX, float fromZ,
                                                    int coveredIdx, float maxDist,
                                                    int* outIdx)
{
    const int n = (int)map.tacticalPositions.size();
    if (coveredIdx < 0 || coveredIdx >= n) return nullptr;
    if ((int)map.positionCovers.size() != n) return nullptr;   // grafo non costruito

    const float maxDist2 = maxDist * maxDist;
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;
    for (int i = 0; i < n; ++i)
    {
        // i copre coveredIdx? (lettura diretta del grafo `positionCovers`)
        bool covers = false;
        for (int c : map.positionCovers[(size_t)i])
            if (c == coveredIdx) { covers = true; break; }
        if (!covers) continue;

        const auto& p = map.tacticalPositions[(size_t)i];
        if (!p.canShoot) continue;
        const float dx = p.x - fromX, dz = p.z - fromZ;
        const float d2 = dx * dx + dz * dz;
        if (d2 > maxDist2) continue;
        // Vicina, protetta, poco esposta: buona posizione da cui coprire l'avanzata.
        const float expo = (i < (int)map.positionExposure.size())
                         ? map.positionExposure[(size_t)i] : 0.0f;
        const float score = p.protection * 2.0f + p.importance
                          - std::sqrt(d2) * 0.08f - expo;
        if (score > bestScore) { bestScore = score; best = &p; if (outIdx) *outIdx = i; }
    }
    return best;
}

const TacticalPositionDef* bestFiringPosition(const MapDef& map,
                                              float x, float z,
                                              float targetX, float targetZ,
                                              float maxDist)
{
    if (map.tacticalPositions.empty()) return nullptr;
    const float maxDist2 = maxDist * maxDist;
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;
    for (const auto& p : map.tacticalPositions)
    {
        if (!p.canShoot) continue;                     // da qui non si fa fuoco

        // Raggiungibile da chi cerca?
        const float mx = p.x - x, mz = p.z - z;
        const float myD2 = mx * mx + mz * mz;
        if (myD2 >= maxDist2) continue;

        // Il bersaglio è nella gittata utile della posizione?
        float tx = targetX - p.x, tz = targetZ - p.z;
        const float tLen = std::sqrt(tx * tx + tz * tz);
        if (tLen > p.fireRange || tLen < 0.01f) continue;
        tx /= tLen; tz /= tLen;

        // ...ed è DENTRO il settore di tiro (arco centrato sul fronte)?
        const float fr = p.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        const float cosHalf = std::cos((p.fireArcDeg * 0.5f) * (kPI / 180.0f));
        if (fx * tx + fz * tz < cosHalf) continue;     // fuori settore

        // Linea di tiro davvero libera (ADR-032): toglie il limite geometrico di
        // ADR-031 — non si "batte" più un bersaglio attraverso un muro.
        if (!hasLineOfFire(map, p.x, p.y + 1.2f, p.z, targetX, p.y + 1.2f, targetZ))
            continue;

        // Premia la protezione, penalizza la distanza da percorrere.
        const float score = p.protection - 0.5f * (myD2 / maxDist2);
        if (score > bestScore) { bestScore = score; best = &p; }
    }
    return best;
}

bool nearCommandPost(const MapDef& map, float x, float z, float slack)
{
    for (const auto& cp : map.commandPosts)
    {
        const float dx = x - cp.x, dz = z - cp.z;
        const float r  = cp.radius + slack;
        if (dx * dx + dz * dz <= r * r) return true;
    }
    return false;
}

const TacticalPositionDef* nearestPositionByRole(const MapDef& map, float x, float z,
                                                 const char* role, float maxDist)
{
    if (map.tacticalPositions.empty()) return nullptr;
    const bool anyRole = (role == nullptr || role[0] == '\0');
    float best2 = maxDist * maxDist;
    const TacticalPositionDef* best = nullptr;
    for (const auto& t : map.tacticalPositions)
    {
        if (!anyRole && t.role != role) continue;
        const float dx = t.x - x, dz = t.z - z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < best2) { best2 = d2; best = &t; }
    }
    return best;
}

} // namespace worldintel
} // namespace mini

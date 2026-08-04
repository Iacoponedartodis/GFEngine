#include "mini/game/ai/WorldIntel.hpp"
#include "mini/game/data/Definitions.hpp"
#include "mini/core/GameConfig.hpp"   // OVERHEAD_PROBE_HEIGHT (doc 41 B3)
#include "mini/game/ai/AiUtility.hpp"  // pesi delle decisioni tattiche (doc 40 A5)

#include <cmath>
#include <algorithm>

namespace mini
{
namespace worldintel
{
namespace
{
constexpr float kPI = 3.14159265358979323846f;
// Distanza di "peek" per la LOS di tiro: l'unità si sporge di ~1.5 m dalla cover
// verso il bersaglio prima di sparare, così la PROPRIA copertura non blocca il tiro
// (una cover ripara proprio perché blocca la visuale dal suo centro).
constexpr float kPeek = 1.5f;
}

float lateralCoord(const MapDef& map, float x, float z)
{
    // Asse d'attacco = spawnTeam1 → spawnTeam2; la corsia è la posizione lungo la
    // PERPENDICOLARE, con origine a metà fra i due spawn.
    const float ax = map.spawnTeam1[0] - map.spawnTeam2[0];
    const float az = map.spawnTeam1[2] - map.spawnTeam2[2];
    const float alen = std::sqrt(ax * ax + az * az);
    if (alen < 0.001f) return x;                       // spawn coincidenti → fallback X
    const float px = -az / alen, pz = ax / alen;        // perpendicolare normalizzata
    const float cx = (map.spawnTeam1[0] + map.spawnTeam2[0]) * 0.5f;
    const float cz = (map.spawnTeam1[2] + map.spawnTeam2[2]) * 0.5f;
    return (x - cx) * px + (z - cz) * pz;
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
        // Punteggio (ADR-026): premia la PROTEZIONE, penalizza la distanza, e
        // (ADR-046/C3) EVITA le danger zone: una copertura dentro un'area
        // pericolosa autorata non è un buon riparo. Fa parlare cover ↔ danger.
        const float score = c.protection * aiutility::kCover.protection
                          - aiutility::kCover.distance * (d2 / maxDist2)
                          - aiutility::kCover.danger * dangerAt(map, c.x, c.z);
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

bool hasOverheadCover(const MapDef& map, float x, float y, float z, float probeHeight)
{
    // Si parte SOPRA la testa (non dai piedi): partire dal suolo farebbe intersecare
    // il pavimento su cui si sta e restituirebbe "coperto" ovunque.
    const float from = y + 1.8f;
    if (probeHeight <= from - y) return false;
    return !hasLineOfFire(map, x, from, z, x, y + probeHeight, z);
}

void buildTacticalLinks(MapDef& map)
{
    const size_t n = map.tacticalPositions.size();
    map.positionCovers.assign(n, {});
    if (n == 0) return;

    // Copertura dall'alto (doc 41 B3): dato DERIVATO, calcolato qui insieme agli altri
    // così vive e muore con la geometria — un raycast per posizione, costo trascurabile.
    for (auto& t : map.tacticalPositions)
        t.hasOverhead = hasOverheadCover(map, t.x, t.y, t.z,
                                         config::OVERHEAD_PROBE_HEIGHT);

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
                                                float targetX, float targetY, float targetZ,
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
        // Arco come PREFERENZA, non esclusione (l'unità si gira per mirare).
        const float fr = p.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        const float cosHalf = std::cos((p.fireArcDeg * 0.5f) * (kPI / 180.0f));
        const float aim = fx * tx + fz * tz;
        const float arcPref = (aim >= cosHalf) ? 1.0f
                            : std::max(0.0f, (aim + 1.0f) / (cosHalf + 1.0f));
        // Peek: LOS dal punto avanti verso il bersaglio alla sua quota REALE (targetY),
        // non un piano orizzontale (KI #82) → tiri in salita/discesa valutati bene.
        if (!hasLineOfFire(map, p.x + tx * kPeek, p.y + 1.2f, p.z + tz * kPeek,
                                targetX, targetY + 1.0f, targetZ)) continue;

        // Angolo di FIANCO: direzione bersaglio→posizione contro bersaglio→minaccia.
        // dot = 1 → stessa direzione (frontale, nessun aggiramento); -1 → alle spalle.
        const float px = -tx, pz = -tz;              // dal bersaglio verso la posizione
        const float dot = px * thx + pz * thz;
        const float flankBonus = (1.0f - dot) * 0.5f;   // 0 = frontale, 1 = opposto

        const float exposure = (i < map.positionExposure.size())
                             ? map.positionExposure[i] : 0.0f;

        const float score = flankBonus * aiutility::kFlank.flankBonus
                          + p.protection * aiutility::kFlank.protection
                          + arcPref * aiutility::kFlank.arc
                          + (1.0f - aiutility::riskPenalty(exposure)) * aiutility::kFlank.unexposed
                          - aiutility::kFlank.distance * (myD2 / maxDist2);
        if (score > bestScore) { bestScore = score; best = &p; }
    }
    return best;
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
        const float score = p.protection * aiutility::kOverwatch.protection
                          + p.importance * aiutility::kOverwatch.importance
                          - std::sqrt(d2) * aiutility::kOverwatch.distance
                          - aiutility::riskPenalty(expo) * aiutility::kOverwatch.exposure;
        if (score > bestScore) { bestScore = score; best = &p; if (outIdx) *outIdx = i; }
    }
    return best;
}

const TacticalPositionDef* bestFiringPosition(const MapDef& map,
                                              float x, float z,
                                              float targetX, float targetY, float targetZ,
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

        // Orientamento verso il bersaglio. Prima una posizione FUORI dall'arco
        // autorato veniva ESCLUSA; ma un'unità in copertura si GIRA per mirare,
        // quindi ora l'arco è una PREFERENZA: chi è già orientato espone meno e vale
        // di più, ma le altre (vantage elevate/laterali orientate "male") restano
        // usabili. La LOS impedisce comunque di sparare attraverso la propria cover.
        const float fr = p.facingDeg * (kPI / 180.0f);
        const float fx = std::sin(fr), fz = std::cos(fr);
        const float cosHalf = std::cos((p.fireArcDeg * 0.5f) * (kPI / 180.0f));
        const float aim = fx * tx + fz * tz;           // 1 frontale … -1 alle spalle
        const float arcPref = (aim >= cosHalf) ? 1.0f  // dentro l'arco: pieno
                            : std::max(0.0f, (aim + 1.0f) / (cosHalf + 1.0f));

        // Linea di tiro: parte da un punto di PEEK, avanti verso il bersaglio, non
        // dal centro dietro la copertura. Una cover BLOCCA per definizione parte
        // della visuale dal suo centro (altrimenti non ripara): l'unità si SPORGE
        // per sparare. Testare dal centro la scarterebbe sempre (segnalato
        // dall'utente). Così la PROPRIA cover (dietro il peek) non blocca, ma un
        // muro/edificio DAVANTI (fra te e il bersaglio) sì.
        // Origine: occhio del tiratore SULLA posizione (p.y + 1.2). Bersaglio: la sua
        // quota REALE (targetY + ~1 m corpo) → valuta il tiro in salita/discesa, non
        // un piano orizzontale (KI #82: era il motivo per cui non si sparava dall'alto).
        const float peekX = p.x + tx * kPeek, peekZ = p.z + tz * kPeek;
        if (!hasLineOfFire(map, peekX, p.y + 1.2f, peekZ, targetX, targetY + 1.0f, targetZ))
            continue;

        // Protezione + orientamento (preferenza) + IMPORTANZA autorata (l'autore
        // marca il buon terreno, anche elevato → ora conta anche in combattimento) −
        // distanza − pericolo (ADR-046/C3).
        const float score = p.protection * aiutility::kFiring.protection
                          + arcPref * aiutility::kFiring.arc
                          + p.importance * aiutility::kFiring.importance
                          - aiutility::kFiring.distance * (myD2 / maxDist2)
                          - dangerAt(map, p.x, p.z);
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

const TacticalPositionDef* bestHoldPosition(const MapDef& map, float x, float z,
                                            float areaX, float areaZ, float areaRadius)
{
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;
    const float ar2 = areaRadius * areaRadius;
    for (const auto& p : map.tacticalPositions)
    {
        // I due ruoli "da presidiare": una posizione difensiva o un chokepoint.
        if (p.role != "defensive" && p.role != "chokepoint") continue;
        // Se è indicata un'area (settore-obiettivo), la posizione deve starci dentro.
        if (areaRadius > 0.0f)
        {
            const float ax = p.x - areaX, az = p.z - areaZ;
            if (ax * ax + az * az > ar2) continue;
        }
        const float dx = p.x - x, dz = p.z - z;
        const float d2 = dx * dx + dz * dz;
        // Protetta + importante, vicina, e fuori dal pericolo (ADR-046/C3).
        const float score = p.protection * aiutility::kHold.protection
                          + p.importance * aiutility::kHold.importance
                          - std::sqrt(d2) * aiutility::kHold.distance
                          - aiutility::kHold.danger * dangerAt(map, p.x, p.z);
        if (score > bestScore) { bestScore = score; best = &p; }
    }
    return best;
}

const TacticalPositionDef* bestAdvantageInArea(const MapDef& map, float x, float z,
                                               float areaX, float areaZ, float areaRadius)
{
    float bestScore = -1e30f;
    const TacticalPositionDef* best = nullptr;
    const float ar2 = areaRadius * areaRadius;
    for (const auto& p : map.tacticalPositions)
    {
        // Qualunque ruolo da tenere/coprire (incl. observation): set condiviso.
        if (!isTacticalHoldRole(p.role)) continue;
        if (areaRadius > 0.0f)
        {
            const float ax = p.x - areaX, az = p.z - areaZ;
            if (ax * ax + az * az > ar2) continue;
        }
        const float dx = p.x - x, dz = p.z - z;
        const float d2 = dx * dx + dz * dz;
        // IMPORTANZA pesata (l'autore marca il buon terreno, anche elevato, con
        // importanza alta) + protezione, vicina, fuori dal pericolo (ADR-046/C3).
        const float score = p.importance * aiutility::kAdvantage.importance
                          + p.protection * aiutility::kAdvantage.protection
                          - std::sqrt(d2) * aiutility::kAdvantage.distance
                          - aiutility::kAdvantage.danger * dangerAt(map, p.x, p.z);
        if (score > bestScore) { bestScore = score; best = &p; }
    }
    return best;
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

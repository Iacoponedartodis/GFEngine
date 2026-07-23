#include "mini/physics/Collision.hpp"
#include "mini/ecs/World.hpp"

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace mini::physics
{

static constexpr float PI = 3.14159265f;

// ── AABB ─────────────────────────────────────────────────────────────────

AABB computeWorldAABB(const TransformComponent& t, const ColliderComponent& c)
{
    const float ry = t.ry * PI / 180.0f;
    const float ca = std::abs(std::cos(ry));
    const float sa = std::abs(std::sin(ry));
    const float wx = c.hx * ca + c.hz * sa;
    const float wz = c.hx * sa + c.hz * ca;
    return {
        {t.x - wx, t.y - c.hy, t.z - wz},
        {t.x + wx, t.y + c.hy, t.z + wz}
    };
}

// Test ESATTO query-box (allineato agli assi) vs collider ruotato attorno
// a Y: SAT 2D sul piano XZ (assi mondo + assi locali del collider) più
// l'intervallo Y. Prima il collider ruotato veniva gonfiato nel suo AABB
// avvolgente: il movimento urtava "aria" agli angoli dei muri diagonali
// mentre i proiettili (test OBB vero) morivano sul bordo reale (KI #23).
static bool boxIntersectsRotatedCollider(const glm::vec3& pos, const glm::vec3& half,
                                         const TransformComponent& t,
                                         const ColliderComponent& c)
{
    // Intervallo Y (la rotazione è solo attorno a Y)
    if (pos.y - half.y >= t.y + c.hy || pos.y + half.y <= t.y - c.hy)
        return false;

    const float ry = t.ry * PI / 180.0f;
    const float co = std::cos(ry), si = std::sin(ry);
    // Assi locali del collider nel piano XZ (convenzione glm rotate-Y,
    // identica a toModelMatrix/HitTest): localX -> (co, -si), localZ -> (si, co)
    const float ax[2] = {co, -si};
    const float az[2] = {si,  co};
    const float dx = t.x - pos.x, dz = t.z - pos.z;

    // 4 assi di separazione: X mondo, Z mondo, X locale, Z locale
    const float axesX[4] = {1, 0, ax[0], az[0]};
    const float axesZ[4] = {0, 1, ax[1], az[1]};
    for (int i = 0; i < 4; ++i)
    {
        const float aX = axesX[i], aZ = axesZ[i];
        // proiezione query box (allineato al mondo)
        const float rq = half.x * std::abs(aX) + half.z * std::abs(aZ);
        // proiezione collider (assi locali ax/az con half hx/hz)
        const float rc = c.hx * std::abs(aX * ax[0] + aZ * ax[1])
                       + c.hz * std::abs(aX * az[0] + aZ * az[1]);
        const float dist = std::abs(aX * dx + aZ * dz);
        if (dist >= rq + rc) return false;   // asse separatore trovato
    }
    return true;
}

// Test ESATTO OBB query (ruotato di queryYaw attorno a Y) vs collider OBB.
// Generalizza boxIntersectsRotatedCollider: SAT 2D con gli assi locali di
// ENTRAMBI i box (4 assi) + intervallo Y. Serve ai box lunghi che ruotano
// (veicoli, KI #29): l'AABB avvolgente li faceva urtare l'aria ai lati.
static bool obbIntersectsRotatedCollider(const glm::vec3& pos, const glm::vec3& half,
                                         float queryYaw,
                                         const TransformComponent& t,
                                         const ColliderComponent& c)
{
    if (pos.y - half.y >= t.y + c.hy || pos.y + half.y <= t.y - c.hy)
        return false;

    const float qc = std::cos(queryYaw), qs = std::sin(queryYaw);
    const float cr = t.ry * PI / 180.0f;
    const float cc = std::cos(cr), cs = std::sin(cr);
    // Assi locali (convenzione glm rotate-Y: localX=(co,-si), localZ=(si,co))
    const float qx[2] = {qc, -qs}, qz[2] = {qs, qc};   // query
    const float cx[2] = {cc, -cs}, cz[2] = {cs, cc};   // collider
    const float dx = t.x - pos.x, dz = t.z - pos.z;

    const float axesX[4] = {qx[0], qz[0], cx[0], cz[0]};
    const float axesZ[4] = {qx[1], qz[1], cx[1], cz[1]};
    for (int i = 0; i < 4; ++i)
    {
        const float aX = axesX[i], aZ = axesZ[i];
        const float rq = half.x * std::abs(aX * qx[0] + aZ * qx[1])
                       + half.z * std::abs(aX * qz[0] + aZ * qz[1]);
        const float rc = c.hx * std::abs(aX * cx[0] + aZ * cx[1])
                       + c.hz * std::abs(aX * cz[0] + aZ * cz[1]);
        const float dist = std::abs(aX * dx + aZ * dz);
        if (dist >= rq + rc) return false;
    }
    return true;
}

// ── Indice spaziale dei collider (ADR-015: ottimizzazione) ─────────────────────
// hasCollision/hasLineOfSight passavano da O(TUTTE le entità) per chiamata: su una
// mappa grande (Training Ground: 175 box + unità) il costo del sensing/movimento
// scalava con la geometria (lag già con ~25 AI, segnalato dall'utente). La griglia
// uniforme sul piano XZ riduce a O(celle vicine) e precalcola le AABB mondo una
// volta per tick. Snapshot PER VALORE dei componenti → nessun puntatore pendente
// se un'entità viene distrutta a metà tick. File-static: il sim è single-thread;
// (world, tick) distingue mondi diversi (gioco/editor) e ricostruisce a ogni tick.
namespace {

struct GridItem { EntityId id; AABB aabb; TransformComponent t; ColliderComponent c; };

struct ColliderGrid
{
    static constexpr float kCell = 5.0f;
    float minX = 0.0f, minZ = 0.0f;
    int   nx = 0, nz = 0;
    std::vector<GridItem>          items;
    std::vector<std::vector<int>>  cells;
    std::vector<std::uint32_t>     stamp;     // dedup: un item testato una volta per query
    std::uint32_t                  qid = 0;

    int cxi(float x) const { int i = (int)((x - minX) / kCell); return i < 0 ? 0 : (i >= nx ? nx - 1 : i); }
    int czi(float z) const { int i = (int)((z - minZ) / kCell); return i < 0 ? 0 : (i >= nz ? nz - 1 : i); }

    void rebuild(World& world)
    {
        items.clear();
        float mnx = 1e30f, mnz = 1e30f, mxx = -1e30f, mxz = -1e30f;
        for (EntityId id : world.getEntities())
        {
            const auto* col = world.getCollider(id);
            const auto* tr  = world.getTransform(id);
            if (!col || !tr) continue;
            const AABB b = computeWorldAABB(*tr, *col);
            items.push_back({id, b, *tr, *col});
            if (b.min.x < mnx) mnx = b.min.x;  if (b.min.z < mnz) mnz = b.min.z;
            if (b.max.x > mxx) mxx = b.max.x;  if (b.max.z > mxz) mxz = b.max.z;
        }
        stamp.assign(items.size(), 0);
        qid = 0;
        cells.clear();
        if (items.empty()) { nx = nz = 0; return; }
        minX = mnx; minZ = mnz;
        nx = (int)((mxx - mnx) / kCell) + 1;  if (nx < 1) nx = 1;
        nz = (int)((mxz - mnz) / kCell) + 1;  if (nz < 1) nz = 1;
        cells.assign((size_t)nx * nz, {});
        for (int i = 0; i < (int)items.size(); ++i)
        {
            const AABB& b = items[i].aabb;
            const int x0 = cxi(b.min.x), x1 = cxi(b.max.x);
            const int z0 = czi(b.min.z), z1 = czi(b.max.z);
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    cells[(size_t)z * nx + x].push_back(i);
        }
    }

    // Itera i candidati le cui celle coprono l'inviluppo [mn,mx] (XZ). `fn` ritorna
    // true per fermarsi (collisione trovata). Ritorna true se `fn` ha fermato.
    template <class F>
    bool forRange(const glm::vec3& mn, const glm::vec3& mx, F&& fn)
    {
        if (items.empty()) return false;
        ++qid;
        const int x0 = cxi(mn.x), x1 = cxi(mx.x);
        const int z0 = czi(mn.z), z1 = czi(mx.z);
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                for (int i : cells[(size_t)z * nx + x])
                {
                    if (stamp[(size_t)i] == qid) continue;
                    stamp[(size_t)i] = qid;
                    if (fn(items[(size_t)i])) return true;
                }
        return false;
    }
};

ColliderGrid       g_grid;
const World*       g_gridWorld = nullptr;
std::uint64_t      g_gridTick  = ~0ull;

ColliderGrid& ensureGrid(World& world)
{
    const std::uint64_t t = world.getTickCount();
    if (g_gridWorld != &world || g_gridTick != t)
    {
        g_gridWorld = &world;
        g_gridTick  = t;
        g_grid.rebuild(world);
    }
    return g_grid;
}

} // namespace

bool hasCollision(const glm::vec3& pos, const glm::vec3& half, World& world,
                  EntityId excludeId, float queryYawRad)
{
    // Broad test sull'AABB avvolgente del box query. A yaw=0 è `half`; a yaw≠0
    // è l'inviluppo della sagoma ruotata (conservativo → nessun falso negativo).
    glm::vec3 qhalf = half;
    if (queryYawRad != 0.0f)
    {
        const float ca = std::abs(std::cos(queryYawRad));
        const float sa = std::abs(std::sin(queryYawRad));
        qhalf = {half.x * ca + half.z * sa, half.y, half.x * sa + half.z * ca};
    }
    const glm::vec3 pn = pos - qhalf;
    const glm::vec3 px = pos + qhalf;

    // Candidati dalla sola area vicina (indice spaziale) invece di tutte le entità.
    return ensureGrid(world).forRange(pn, px, [&](const GridItem& it) -> bool
    {
        if (it.id == excludeId) return false;   // es. veicolo che muove sé stesso

        // Broad test sull'AABB avvolgente (precalcolata: conservativo, veloce)
        const AABB& b = it.aabb;
        if (pn.x >= b.max.x || px.x <= b.min.x ||
            pn.y >= b.max.y || px.y <= b.min.y ||
            pn.z >= b.max.z || px.z <= b.min.z)
            return false;

        // Query ruotato (veicolo): test OBB-vs-OBB esatto, salta il fast path.
        if (queryYawRad != 0.0f)
            return obbIntersectsRotatedCollider(pos, half, queryYawRad, it.t, it.c);

        // Collider non ruotato: il broad test è già esatto
        const float ryMod = std::fmod(std::abs(it.t.ry), 180.0f);
        if (ryMod < 0.01f || std::abs(ryMod - 90.0f) < 0.01f
            || std::abs(ryMod - 180.0f) < 0.01f)
            return true;

        return boxIntersectsRotatedCollider(pos, half, it.t, it.c);
    });
}

// ── Slide semplice ───────────────────────────────────────────────────────

glm::vec3 slideMove(const glm::vec3& prev, const glm::vec3& next,
                    const glm::vec3& half, World& world,
                    EntityId excludeId, float queryYawRad)
{
    glm::vec3 r = prev;
    if (!hasCollision({next.x, r.y, r.z}, half, world, excludeId, queryYawRad)) r.x = next.x;
    if (!hasCollision({r.x, next.y, r.z}, half, world, excludeId, queryYawRad)) r.y = next.y;
    if (!hasCollision({r.x, r.y, next.z}, half, world, excludeId, queryYawRad)) r.z = next.z;
    return r;
}

// ── Slide con step-up ────────────────────────────────────────────────────

glm::vec3 slideMoveWithStepUp(const glm::vec3& prev, const glm::vec3& next,
                               const glm::vec3& half, World& world,
                               float stepHeight, EntityId excludeId, float queryYawRad)
{
    glm::vec3 r = prev;

    // Asse X
    if (!hasCollision({next.x, r.y, r.z}, half, world, excludeId, queryYawRad))
    {
        r.x = next.x;
    }
    else
    {
        const glm::vec3 stepped = {next.x, r.y + stepHeight, r.z};
        if (!hasCollision(stepped, half, world, excludeId, queryYawRad) &&
            !hasCollision({r.x, r.y + stepHeight, r.z}, half, world, excludeId, queryYawRad))
        {
            r.x = next.x;
            r.y += stepHeight;
        }
    }

    // Asse Y (gravità — no step-up)
    if (!hasCollision({r.x, next.y, r.z}, half, world, excludeId, queryYawRad))
        r.y = next.y;

    // Asse Z
    if (!hasCollision({r.x, r.y, next.z}, half, world, excludeId, queryYawRad))
    {
        r.z = next.z;
    }
    else
    {
        const glm::vec3 stepped = {r.x, r.y + stepHeight, next.z};
        if (!hasCollision(stepped, half, world, excludeId, queryYawRad) &&
            !hasCollision({r.x, r.y + stepHeight, r.z}, half, world, excludeId, queryYawRad))
        {
            r.z = next.z;
            r.y += stepHeight;
        }
    }

    return r;
}

glm::vec3 nudgeOutOfColliders(const glm::vec3& pos, const glm::vec3& half,
                              World& world, float step, int rings)
{
    if (!hasCollision(pos, half, world)) return pos;
    static const float dirs[8][2] = {
        { 1, 0}, {-1, 0}, { 0, 1}, { 0,-1},
        { 0.707f, 0.707f}, {-0.707f, 0.707f},
        { 0.707f,-0.707f}, {-0.707f,-0.707f} };

    // Ricerca 3D a livelli di quota crescenti. Il livello 0 è la ricerca
    // orizzontale a terra di prima (muri: ci si sposta di lato). Se TUTTO il
    // livello 0 è bloccato — è il caso della geometria rialzata più larga del
    // raggio, es. la lastra larga di un command post — si sale di un livello e
    // si riprova: così ci si posiziona SOPRA la piattaforma invece di restare
    // incastrati. Passo verticale fine per atterrare vicino alla superficie;
    // tetto finito così da non teletrasportare in alto all'infinito.
    const float vStep  = 0.4f;
    const int   vSteps = 15;   // ~6 m di risalita massima
    for (int v = 0; v <= vSteps; ++v)
    {
        const float y = pos.y + vStep * (float)v;
        // Punto esatto a questa quota (a v=0 è già noto bloccato, si salta).
        if (v > 0 && !hasCollision({pos.x, y, pos.z}, half, world))
            return {pos.x, y, pos.z};
        for (int ring = 1; ring <= rings; ++ring)
            for (const auto& d : dirs)
            {
                const glm::vec3 p = {pos.x + d[0] * step * (float)ring,
                                     y,
                                     pos.z + d[1] * step * (float)ring};
                if (!hasCollision(p, half, world)) return p;
            }
    }
    return pos;
}

// ── Ray-AABB LOS ─────────────────────────────────────────────────────────

static bool rayBlockedByAABB(const glm::vec3& o, const glm::vec3& d,
                              const glm::vec3& bMin, const glm::vec3& bMax)
{
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float di = d[i];
        if (std::abs(di) < 1e-6f)
        {
            if (o[i] < bMin[i] || o[i] > bMax[i]) return false;
        }
        else
        {
            float t1 = (bMin[i] - o[i]) / di;
            float t2 = (bMax[i] - o[i]) / di;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    return true;
}

bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to, World& world,
                    EntityId ignore)
{
    const glm::vec3 dir = to - from;

    // Solo i collider vicini al segmento (inviluppo XZ del raggio) via indice.
    const glm::vec3 segMin{std::min(from.x, to.x), 0.0f, std::min(from.z, to.z)};
    const glm::vec3 segMax{std::max(from.x, to.x), 0.0f, std::max(from.z, to.z)};

    // forRange ritorna true se un candidato BLOCCA → la vista è libera se NON blocca.
    const bool blocked = ensureGrid(world).forRange(segMin, segMax,
        [&](const GridItem& it) -> bool
    {
        if (it.id == ignore) return false;   // il bersaglio non si occlude da sé (doc 35)
        const auto& t   = it.t;
        const auto& col = it.c;

        // Collider ruotato: porta il segmento nello spazio LOCALE del box
        // (rotazione inversa attorno al centro) e testa contro l'AABB
        // locale — test esatto, coerente coi proiettili/hitbox (KI #23).
        const float ryMod = std::fmod(std::abs(t.ry), 180.0f);
        const bool rotated = !(ryMod < 0.01f || std::abs(ryMod - 180.0f) < 0.01f);

        glm::vec3 o = from, d = dir;
        glm::vec3 bMin, bMax;
        if (rotated)
        {
            const float ry = t.ry * PI / 180.0f;
            const float co = std::cos(ry), si = std::sin(ry);
            // Inversa della rotazione glm attorno a Y (w = R * l):
            // l.x = co*w.x - si*w.z ; l.z = si*w.x + co*w.z
            const float wx = from.x - t.x, wz = from.z - t.z;
            o = { co * wx - si * wz, from.y - t.y, si * wx + co * wz };
            d = { co * dir.x - si * dir.z, dir.y, si * dir.x + co * dir.z };
            bMin = {-col.hx, -col.hy, -col.hz};
            bMax = { col.hx,  col.hy,  col.hz};
        }
        else
        {
            bMin = it.aabb.min; bMax = it.aabb.max;   // AABB già precalcolata
        }

        // Ignora se l'origine è dentro il box (AI a contatto col muro)
        if (o.x > bMin.x && o.x < bMax.x &&
            o.y > bMin.y && o.y < bMax.y &&
            o.z > bMin.z && o.z < bMax.z)
            return false;

        return rayBlockedByAABB(o, d, bMin, bMax);
    });
    return !blocked;
}

} // namespace mini::physics
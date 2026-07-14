// NavManager.cpp — pipeline Recast (solo-mesh) + query Detour (ADR-017, Phase A).
#include "mini/game/nav/NavManager.hpp"
#include "mini/game/data/Definitions.hpp"   // MapDef, MapGeometryBox
#include "mini/core/GameConfig.hpp"          // STEP_HEIGHT, PLAYER_HALF

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace mini
{

// ── Parametri di build ───────────────────────────────────────────────────────
// Tarati per la geometria a box di GFEngine (mappe ~50x40m). walkableClimb =
// STEP_HEIGHT (scavalca scalini bassi); muri/coperture alte → non walkable, e
// Detour ci path INTORNO (fix del comportamento "AI stuck").
namespace
{
constexpr float kCellSize    = 0.30f;   // voxel XZ (m)
constexpr float kCellHeight  = 0.10f;   // voxel Y (m): fine → superficie navmesh
                                        // vicina al pavimento reale (piedi a terra)
constexpr float kAgentHeight = 1.80f;   // altezza agente (m)
constexpr float kAgentRadius = config::AI_HALF_X;  // 0.40m
constexpr float kAgentClimb  = config::STEP_HEIGHT; // 0.55m — scalino max
constexpr float kAgentSlope  = 45.0f;   // pendenza max walkable (gradi)

// ── Aree semantiche (Phase C, ADR-017 / Todo #15) ────────────────────────────
// Id area dei poligoni navmesh (0..63). Il dtQueryFilter assegna un COSTO per
// area: DANGER alto → il pathfinding aggira le zone pericolose autorate nel
// MapDef; GROUND/COVER neutri (COVER è pronta per costi per-ruolo futuri).
constexpr unsigned char kAreaGround = 0;
constexpr unsigned char kAreaDanger = 1;
constexpr unsigned char kAreaCover  = 2;
constexpr float         kCostDanger = 10.0f;   // costo di traversata delle danger zone
} // namespace

NavManager::~NavManager() { clear(); }

void NavManager::clear()
{
    if (m_crowd)   { dtFreeCrowd(m_crowd);        m_crowd = nullptr; }
    if (m_query)   { dtFreeNavMeshQuery(m_query); m_query = nullptr; }
    if (m_navMesh) { dtFreeNavMesh(m_navMesh);    m_navMesh = nullptr; }
}

// Aggiunge i 12 triangoli (winding esterno) di un box ruotato attorno a Y a
// (verts,tris). vert = float flat [x,y,z]; tri = int flat [i0,i1,i2].
static void appendBox(const MapGeometryBox& b,
                      std::vector<float>& verts, std::vector<int>& tris)
{
    const float hx = b.sx * 0.5f, hy = b.sy * 0.5f, hz = b.sz * 0.5f;
    const float ry = b.ry * 3.14159265f / 180.0f;
    const float co = std::cos(ry), si = std::sin(ry);

    // 8 corner locali (segni), poi rotazione Y + traslazione al centro.
    static const int sgn[8][3] = {
        {-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1},
        {-1, 1,-1},{ 1, 1,-1},{ 1, 1, 1},{-1, 1, 1} };
    const int base = (int)(verts.size() / 3);
    for (auto& s : sgn)
    {
        const float lx = s[0] * hx, ly = s[1] * hy, lz = s[2] * hz;
        verts.push_back(b.x + co * lx + si * lz);   // rotate-Y (glm convention)
        verts.push_back(b.y + ly);
        verts.push_back(b.z - si * lx + co * lz);
    }
    // 12 triangoli, normali verso l'esterno (top +Y walkable, lati verticali).
    static const int idx[36] = {
        4,7,6, 4,6,5,   // top  (+Y)
        0,1,2, 0,2,3,   // bottom (-Y)
        0,4,5, 0,5,1,   // front (-Z)
        3,2,6, 3,6,7,   // back  (+Z)
        0,3,7, 0,7,4,   // left  (-X)
        1,5,6, 1,6,2 }; // right (+X)
    for (int i : idx) tris.push_back(base + i);
}

NavBuildStats NavManager::build(const MapDef& map)
{
    clear();
    NavBuildStats st;

    // ── 1. Geometria di input: box collider → triangle soup ──────────────
    std::vector<float> verts;
    std::vector<int>   tris;
    for (const auto& b : map.geometry)
        if (b.collider) appendBox(b, verts, tris);
    const int nverts = (int)(verts.size() / 3);
    const int ntris  = (int)(tris.size() / 3);
    st.inputTris = ntris;
    if (ntris == 0) return st;

    float bmin[3], bmax[3];
    rcCalcBounds(verts.data(), nverts, bmin, bmax);
    st.bmin = {bmin[0], bmin[1], bmin[2]};
    st.bmax = {bmax[0], bmax[1], bmax[2]};

    // ── 2. Config ────────────────────────────────────────────────────────
    rcConfig cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.cs = kCellSize;
    cfg.ch = kCellHeight;
    cfg.walkableSlopeAngle    = kAgentSlope;
    cfg.walkableHeight        = (int)std::ceil (kAgentHeight / cfg.ch);
    cfg.walkableClimb         = (int)std::floor(kAgentClimb  / cfg.ch);
    cfg.walkableRadius        = (int)std::ceil (kAgentRadius / cfg.cs);
    cfg.maxEdgeLen            = (int)(12.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea         = (int)rcSqr(8);
    cfg.mergeRegionArea       = (int)rcSqr(20);
    cfg.maxVertsPerPoly       = 6;
    cfg.detailSampleDist      = cfg.cs * 6.0f;
    cfg.detailSampleMaxError  = cfg.ch * 1.0f;
    rcVcopy(cfg.bmin, bmin);
    rcVcopy(cfg.bmax, bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    // ── 3. Pipeline Recast ───────────────────────────────────────────────
    rcContext ctx(false);
    rcHeightfield*        solid = rcAllocHeightfield();
    rcCompactHeightfield* chf   = rcAllocCompactHeightfield();
    rcContourSet*         cset  = rcAllocContourSet();
    rcPolyMesh*           pmesh = rcAllocPolyMesh();
    rcPolyMeshDetail*     dmesh = rcAllocPolyMeshDetail();

    auto fail = [&]() -> NavBuildStats {
        rcFreeHeightField(solid); rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);   rcFreePolyMesh(pmesh); rcFreePolyMeshDetail(dmesh);
        clear();
        return st;   // st.ok resta false
    };

    if (!solid || !chf || !cset || !pmesh || !dmesh) return fail();
    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height,
                             cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) return fail();

    std::vector<unsigned char> areas(ntris, 0);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts.data(), nverts,
                            tris.data(), ntris, areas.data());
    if (!rcRasterizeTriangles(&ctx, verts.data(), nverts, tris.data(),
                              areas.data(), ntris, *solid, cfg.walkableClimb))
        return fail();

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb,
                                   *solid, *chf)) return fail();
    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) return fail();

    // ── Aree semantiche (Phase C): marca le danger zone e i cover point come
    //    aree dedicate (cilindri sull'intera altezza mappa). Va DOPO l'erosione
    //    e PRIMA delle regioni, così il costo si propaga fino ai poligoni. ──
    const float cylH = (bmax[1] - bmin[1]) + 2.0f;
    for (const auto& d : map.dangerZones)
    {
        const float pos[3] = {d.x, bmin[1] - 1.0f, d.z};
        rcMarkCylinderArea(&ctx, pos, d.radius, cylH, kAreaDanger, *chf);
    }
    for (const auto& c : map.coverPoints)
    {
        const float pos[3] = {c.x, bmin[1] - 1.0f, c.z};
        rcMarkCylinderArea(&ctx, pos, 1.2f, cylH, kAreaCover, *chf);  // raggio piccolo
    }

    if (!rcBuildDistanceField(&ctx, *chf)) return fail();
    if (!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea))
        return fail();
    if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError,
                         cfg.maxEdgeLen, *cset)) return fail();
    if (cset->nconts == 0) return fail();
    if (!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh)) return fail();
    if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist,
                               cfg.detailSampleMaxError, *dmesh)) return fail();

    // Flag/area (Phase C): RC_WALKABLE_AREA→GROUND; DANGER/COVER preservate dal
    // marking sopra. Tutti walkable (flag 1); il COSTO per area lo dà il filtro.
    for (int i = 0; i < pmesh->npolys; ++i)
    {
        if (pmesh->areas[i] == RC_WALKABLE_AREA) pmesh->areas[i] = kAreaGround;
        pmesh->flags[i] = 1;   // bit "walk" (default dtQueryFilter lo include)
        if      (pmesh->areas[i] == kAreaDanger) ++st.dangerPolys;
        else if (pmesh->areas[i] == kAreaCover)  ++st.coverPolys;
    }
    st.polyCount = pmesh->npolys;
    st.vertCount = pmesh->nverts;

    // ── 4. Detour: crea il navmesh (single tile) + query ─────────────────
    dtNavMeshCreateParams np;
    std::memset(&np, 0, sizeof(np));
    np.verts            = pmesh->verts;
    np.vertCount        = pmesh->nverts;
    np.polys            = pmesh->polys;
    np.polyAreas        = pmesh->areas;
    np.polyFlags        = pmesh->flags;
    np.polyCount        = pmesh->npolys;
    np.nvp              = pmesh->nvp;
    np.detailMeshes     = dmesh->meshes;
    np.detailVerts      = dmesh->verts;
    np.detailVertsCount = dmesh->nverts;
    np.detailTris       = dmesh->tris;
    np.detailTriCount   = dmesh->ntris;
    np.walkableHeight   = kAgentHeight;
    np.walkableRadius   = kAgentRadius;
    np.walkableClimb    = kAgentClimb;
    rcVcopy(np.bmin, pmesh->bmin);
    rcVcopy(np.bmax, pmesh->bmax);
    np.cs          = cfg.cs;
    np.ch          = cfg.ch;
    np.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&np, &navData, &navDataSize)) return fail();

    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh || dtStatusFailed(
            m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA)))
    { dtFree(navData); return fail(); }

    m_query = dtAllocNavMeshQuery();
    if (!m_query || dtStatusFailed(m_query->init(m_navMesh, 2048))) return fail();

    // Crowd (Phase B): fino a 128 agenti, raggio max = raggio agente.
    m_crowd = dtAllocCrowd();
    if (!m_crowd || !m_crowd->init(128, kAgentRadius, m_navMesh)) return fail();

    // Costi per area del filtro del crowd (Phase C): DANGER caro → il
    // pathfinding aggira le danger zone. Filtro 0 = tutte le AI; filtri
    // aggiuntivi (per-ruolo) sono un'estensione banale (queryFilterType != 0).
    if (dtQueryFilter* f = m_crowd->getEditableFilter(0))
    {
        f->setAreaCost(kAreaGround, 1.0f);
        f->setAreaCost(kAreaDanger, kCostDanger);
        f->setAreaCost(kAreaCover,  1.0f);
    }

    // Intermedi non più necessari (il navMesh possiede navData via DT_TILE_FREE_DATA)
    rcFreeHeightField(solid); rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);   rcFreePolyMesh(pmesh); rcFreePolyMeshDetail(dmesh);

    ++m_generation;   // il CrowdSystem resetta la sua mappa idx→entità
    st.ok = true;
    return st;
}

bool NavManager::findPath(const glm::vec3& start, const glm::vec3& end,
                          std::vector<glm::vec3>& out) const
{
    out.clear();
    if (!m_query) return false;

    dtQueryFilter filter;   // include flag !=0; DANGER caro (Phase C)
    filter.setAreaCost(kAreaDanger, kCostDanger);
    const float ext[3] = {2.0f, 4.0f, 2.0f};   // tolleranza di ricerca poly
    const float s[3] = {start.x, start.y, start.z};
    const float e[3] = {end.x,   end.y,   end.z};

    dtPolyRef sRef = 0, eRef = 0;
    float sPt[3], ePt[3];
    m_query->findNearestPoly(s, ext, &filter, &sRef, sPt);
    m_query->findNearestPoly(e, ext, &filter, &eRef, ePt);
    if (!sRef || !eRef) return false;

    dtPolyRef path[256];
    int npath = 0;
    if (dtStatusFailed(m_query->findPath(sRef, eRef, sPt, ePt, &filter,
                                         path, &npath, 256)) || npath == 0)
        return false;

    float   straight[256 * 3];
    unsigned char sflags[256];
    dtPolyRef     srefs[256];
    int nstraight = 0;
    if (dtStatusFailed(m_query->findStraightPath(sPt, ePt, path, npath,
            straight, sflags, srefs, &nstraight, 256)))
        return false;

    out.reserve(nstraight);
    for (int i = 0; i < nstraight; ++i)
        out.push_back({straight[i*3], straight[i*3+1], straight[i*3+2]});
    return nstraight > 0;
}

// ── Crowd (Phase B) ──────────────────────────────────────────────────────────
int NavManager::addAgent(const glm::vec3& pos, float radius, float height,
                         float maxSpeed)
{
    if (!m_crowd) return -1;
    dtCrowdAgentParams ap;
    std::memset(&ap, 0, sizeof(ap));
    ap.radius                = radius;
    ap.height                = height;
    ap.maxAcceleration       = 12.0f;
    ap.maxSpeed              = maxSpeed;
    ap.collisionQueryRange   = radius * 12.0f;
    ap.pathOptimizationRange = radius * 30.0f;
    ap.separationWeight      = 2.0f;
    ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OBSTACLE_AVOIDANCE
                   | DT_CROWD_SEPARATION | DT_CROWD_OPTIMIZE_VIS
                   | DT_CROWD_OPTIMIZE_TOPO;
    ap.obstacleAvoidanceType = 3;
    const float p[3] = {pos.x, pos.y, pos.z};
    return m_crowd->addAgent(p, &ap);   // -1 se pieno
}

void NavManager::removeAgent(int idx)
{
    if (m_crowd && idx >= 0) m_crowd->removeAgent(idx);
}

void NavManager::requestMoveTarget(int idx, const glm::vec3& target)
{
    if (!m_crowd || idx < 0) return;
    // Se il target è ~invariato NON ripianificare: chiamare requestMoveTarget
    // ogni frame resetta il path e rende il movimento lento/a scatti. Lascia
    // che il crowd segua il corridoio già calcolato.
    const dtCrowdAgent* a = m_crowd->getAgent(idx);
    if (a && a->active && a->targetState == DT_CROWDAGENT_TARGET_VALID)
    {
        const float dx = a->targetPos[0] - target.x;
        const float dz = a->targetPos[2] - target.z;
        if (dx * dx + dz * dz < 0.5f * 0.5f) return;   // stesso target
    }
    const dtQueryFilter* filter = m_crowd->getFilter(0);
    const float* ext = m_crowd->getQueryExtents();
    const float t[3] = {target.x, target.y, target.z};
    dtPolyRef ref = 0;
    float nearest[3];
    m_crowd->getNavMeshQuery()->findNearestPoly(t, ext, filter, &ref, nearest);
    if (ref) m_crowd->requestMoveTarget(idx, ref, nearest);
}

void NavManager::requestMoveVelocity(int idx, const glm::vec3& vel)
{
    if (!m_crowd || idx < 0) return;
    const float v[3] = {vel.x, vel.y, vel.z};
    m_crowd->requestMoveVelocity(idx, v);
}

void NavManager::updateCrowd(float dt)
{
    if (m_crowd) m_crowd->update(dt, nullptr);
}

bool NavManager::agentPos(int idx, glm::vec3& out) const
{
    if (!m_crowd || idx < 0) return false;
    const dtCrowdAgent* a = m_crowd->getAgent(idx);
    if (!a || !a->active) return false;
    // npos.y è la superficie del navmesh, che la voxelizzazione mette ~cellHeight
    // SOPRA la superficie reale. Sottraggo la polarizzazione così la Y tornata è
    // quella del pavimento vero (il chiamante poi ci somma l'offset del centro).
    out = {a->npos[0], a->npos[1] - kCellHeight, a->npos[2]};
    return true;
}

} // namespace mini

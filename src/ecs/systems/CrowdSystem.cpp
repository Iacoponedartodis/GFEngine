#include "mini/ecs/systems/CrowdSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/game/nav/NavManager.hpp"
#include "mini/core/GameConfig.hpp"

#include <glm/glm.hpp>
#include <cmath>
#include <vector>

namespace mini
{

void CrowdSystem::update(World& world, float dt)
{
    NavManager* nav = world.nav;
    if (!nav || !nav->crowdReady()) return;

    // Crowd ricostruito (restart/cambio mappa): reset della mappa idx→entità.
    // Le entità sono comunque fresche (crowdAgentIdx=-1) dopo world.initialize().
    if (nav->generation() != m_lastGen)
    {
        m_lastGen = nav->generation();
        m_agentEntity.clear();
    }

    // ── 1. Reap agenti morti (entità non più valida) ─────────────────────
    for (int idx = 0; idx < (int)m_agentEntity.size(); ++idx)
    {
        const EntityId e = m_agentEntity[idx];
        if (e != 0 && !world.isValidEntity(e))
        { nav->removeAgent(idx); m_agentEntity[idx] = 0; }
    }

    // ── 2. Registra i nuovi agenti (AI ancora senza indice) ──────────────
    for (EntityId e : world.getEntities())
    {
        auto* ai = world.getAi(e);
        auto* tr = world.getTransform(e);
        if (!ai || !tr || ai->crowdAgentIdx >= 0) continue;
        const float maxSpeed = ai->seekSpeed > 0.1f ? ai->seekSpeed : 3.5f;
        const int idx = nav->addAgent({tr->x, tr->y, tr->z},
                                      config::AI_HALF_X, 1.8f, maxSpeed);
        ai->crowdAgentIdx = idx;
        if (idx >= 0)
        {
            if ((int)m_agentEntity.size() <= idx) m_agentEntity.resize(idx + 1, 0);
            m_agentEntity[idx] = e;
        }
    }

    // ── 3. Tick del crowd (una volta per step fisso) ─────────────────────
    nav->updateCrowd(dt);

    // Veicoli come OBB per il push-out (KI #31): il crowd (ADR-017) non conosce
    // le entità dinamiche → il navmesh evita solo la geometria statica e le AI
    // attraversavano gli speeder. Raccolti UNA volta; sono pochi e fermi.
    struct VehBox { float x, y, z, hx, hy, hz, co, si; };
    std::vector<VehBox> vehicles;
    for (EntityId e : world.getEntities())
    {
        const auto* v = world.getVehicle(e);
        const auto* c = world.getCollider(e);
        const auto* t = world.getTransform(e);
        if (!v || !c || !t) continue;
        const float ry = t->ry * 3.14159265f / 180.0f;
        vehicles.push_back({t->x, t->y, t->z, c->hx, c->hy, c->hz,
                            std::cos(ry), std::sin(ry)});
    }

    // ── 4. Write-back npos → transform ───────────────────────────────────
    // npos è sulla SUPERFICIE del navmesh (≈ piedi); il transform.y dell'entità
    // è il CENTRO fisico (spawn = suolo + AI_HALF_Y). Ripristino l'offset,
    // altrimenti i modelli sprofondano di AI_HALF_Y (piedi sottoterra).
    for (EntityId e : world.getEntities())
    {
        auto* ai = world.getAi(e);
        auto* tr = world.getTransform(e);
        if (!ai || !tr || ai->crowdAgentIdx < 0) continue;
        glm::vec3 p;
        if (nav->agentPos(ai->crowdAgentIdx, p))
        { tr->x = p.x; tr->y = p.y + config::AI_HALF_Y; tr->z = p.z; }

        // Push-out dai veicoli: risolve SOLO la penetrazione nell'OBB del
        // veicolo (la geometria statica la gestisce già il navmesh), spingendo
        // l'AI fuori lungo l'asse di MINIMA penetrazione → deterministico, senza
        // jitter. Convenzione assi identica a physics::hasCollision (localX =
        // (co,-si), localZ = (si,co)). Così l'AI scivola lungo lo speeder invece
        // di attraversarlo (KI #31, LOW).
        for (const auto& vb : vehicles)
        {
            if (std::abs(tr->y - vb.y) >= config::AI_HALF_Y + vb.hy) continue;
            const float d0 = tr->x - vb.x, d1 = tr->z - vb.z;
            float lx = d0 * vb.co - d1 * vb.si;   // coord. locale X del veicolo
            float lz = d0 * vb.si + d1 * vb.co;   // coord. locale Z
            const float ex = vb.hx + config::AI_HALF_X;
            const float ez = vb.hz + config::AI_HALF_Z;
            const float penX = ex - std::abs(lx), penZ = ez - std::abs(lz);
            if (penX <= 0.0f || penZ <= 0.0f) continue;   // non penetra
            if (penX < penZ) lx += (lx >= 0.0f ? penX : -penX);
            else             lz += (lz >= 0.0f ? penZ : -penZ);
            tr->x = vb.x + (lx * vb.co + lz * vb.si);      // local → world
            tr->z = vb.z + (-lx * vb.si + lz * vb.co);
        }
    }
}

} // namespace mini

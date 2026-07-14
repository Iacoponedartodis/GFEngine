#include "mini/ecs/systems/CrowdSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/game/nav/NavManager.hpp"
#include "mini/core/GameConfig.hpp"

#include <glm/glm.hpp>

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
    }
}

} // namespace mini

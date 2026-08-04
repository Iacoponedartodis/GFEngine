#pragma once
#include "mini/ecs/ISystem.hpp"
#include "mini/ecs/Entity.hpp"
#include <vector>

namespace mini
{

// CrowdSystem (ADR-017 Phase B): collega le unità AI al dtCrowd del NavManager.
// Registra le AI come agenti, fa il reap di quelle morte, ticka il crowd UNA
// volta per step fisso e riscrive npos→transform. Registrato DOPO AiSystem, che
// nello stesso tick ha già impostato i target/velocità degli agenti.
// No-op se world.nav è assente o il navmesh non è pronto (→ fallback aiMove).
class CrowdSystem : public ISystem
{
public:
    void update(World& world, float dt) override;
    const char* name() const override { return "crowd"; }

private:
    std::vector<EntityId> m_agentEntity;   // idx agente → entità (per il reap)
    unsigned              m_lastGen = 0;    // generazione navmesh vista (reset)
};

} // namespace mini

#pragma once
#include "mini/ecs/ISystem.hpp"

namespace mini
{

class CombatSystem : public ISystem
{
public:
    void update(World& world, float dt) override;

    // Raggio di hit fallback (usato se l'entità non ha HitboxComponent)
    static constexpr float k_hitRadius = 0.9f;
};

} // namespace mini
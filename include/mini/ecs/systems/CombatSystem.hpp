#pragma once
#include "mini/ecs/ISystem.hpp"

namespace mini
{

class CombatSystem : public ISystem
{
public:
    void update(World& world, float dt) override;
    static constexpr float k_hitRadius = 0.9f;
};

} // namespace mini
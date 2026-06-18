#pragma once

#include "mini/ecs/ISystem.hpp"

namespace mini
{

class MovementSystem final : public ISystem
{
public:
    void update(World& world, float deltaTime) override;
};

} // namespace mini
#pragma once

#include "mini/ecs/ISystem.hpp"

namespace mini
{

class MovementSystem final : public ISystem
{
public:
    void update(World& world, float deltaTime) override;
    const char* name() const override { return "movement"; }
};

} // namespace mini
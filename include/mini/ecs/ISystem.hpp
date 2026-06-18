#pragma once

namespace mini
{

class World;

class ISystem
{
public:
    virtual ~ISystem() = default;

    virtual void update(World& world, float deltaTime) = 0;
};

} // namespace mini
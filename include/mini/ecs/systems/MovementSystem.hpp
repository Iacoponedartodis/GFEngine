#pragma once

namespace mini
{
    class World;

    class MovementSystem
    {
    public:
        void update(World& world, float deltaTime);
    };
}
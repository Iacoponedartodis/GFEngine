#pragma once

namespace mini
{
    class World;

    class ConquestMode
    {
    public:
        void start(World& world);
        void update(World& world, float deltaTime);
    };
}
#include "mini/ecs/systems/MovementSystem.hpp"

#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"

namespace mini
{
    void MovementSystem::update(World& world, float deltaTime)
    {
        for (EntityId entity : world.getEntities())
        {
            auto* transform = world.getTransform(entity);
            auto* velocity = world.getVelocity(entity);

            if (transform == nullptr || velocity == nullptr)
            {
                continue;
            }

            transform->x += velocity->vx * deltaTime;
            transform->y += velocity->vy * deltaTime;
        }
    }
}
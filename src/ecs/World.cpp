#include "mini/ecs/World.hpp"

#include "mini/ecs/systems/MovementSystem.hpp"

#include <iostream>

namespace mini
{
    World::World()
    {
        m_movementSystem = new MovementSystem();
    }

    void World::initialize()
    {
        m_tickCount = 0;
        m_nextEntityId = 1;
        m_entities.clear();
        m_transforms.clear();
        m_teams.clear();
        m_velocities.clear();

        std::cout << "[World] Inizializzato." << std::endl;
    }

    void World::tick(float deltaTime)
    {
        ++m_tickCount;

        if (m_movementSystem != nullptr)
        {
            m_movementSystem->update(*this, deltaTime);
        }

        std::cout << "[World] Update tick=" << m_tickCount
                  << " dt=" << deltaTime
                  << " entities=" << m_entities.size()
                  << std::endl;

        for (EntityId entity : m_entities)
        {
            std::cout << "  [Entity " << entity << "]";

            if (hasTeam(entity))
            {
                std::cout << " team=" << m_teams[entity].teamId;
            }

            if (hasTransform(entity))
            {
                const auto& transform = m_transforms[entity];
                std::cout << " pos=(" << transform.x << ", " << transform.y << ")";
            }

            if (hasVelocity(entity))
            {
                const auto& velocity = m_velocities[entity];
                std::cout << " vel=(" << velocity.vx << ", " << velocity.vy << ")";
            }

            std::cout << std::endl;
        }
    }

    EntityId World::createEntity()
    {
        const EntityId entity = m_nextEntityId++;
        m_entities.push_back(entity);
        return entity;
    }

    void World::addTransform(EntityId entity, const TransformComponent& transform)
    {
        m_transforms[entity] = transform;
    }

    void World::addTeam(EntityId entity, const TeamComponent& team)
    {
        m_teams[entity] = team;
    }

    void World::addVelocity(EntityId entity, const VelocityComponent& velocity)
    {
        m_velocities[entity] = velocity;
    }

    bool World::hasTransform(EntityId entity) const
    {
        return m_transforms.find(entity) != m_transforms.end();
    }

    bool World::hasTeam(EntityId entity) const
    {
        return m_teams.find(entity) != m_teams.end();
    }

    bool World::hasVelocity(EntityId entity) const
    {
        return m_velocities.find(entity) != m_velocities.end();
    }

    TransformComponent* World::getTransform(EntityId entity)
    {
        auto it = m_transforms.find(entity);
        if (it == m_transforms.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    TeamComponent* World::getTeam(EntityId entity)
    {
        auto it = m_teams.find(entity);
        if (it == m_teams.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    VelocityComponent* World::getVelocity(EntityId entity)
    {
        auto it = m_velocities.find(entity);
        if (it == m_velocities.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    std::uint64_t World::getTickCount() const
    {
        return m_tickCount;
    }

    const std::vector<EntityId>& World::getEntities() const
    {
        return m_entities;
    }
}
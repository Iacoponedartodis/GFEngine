#pragma once

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mini
{
    class MovementSystem;

    class World
    {
    public:
        World();

        void initialize();
        void tick(float deltaTime);

        EntityId createEntity();

        void addTransform(EntityId entity, const TransformComponent& transform);
        void addTeam(EntityId entity, const TeamComponent& team);
        void addVelocity(EntityId entity, const VelocityComponent& velocity);

        bool hasTransform(EntityId entity) const;
        bool hasTeam(EntityId entity) const;
        bool hasVelocity(EntityId entity) const;

        TransformComponent* getTransform(EntityId entity);
        TeamComponent* getTeam(EntityId entity);
        VelocityComponent* getVelocity(EntityId entity);

        std::uint64_t getTickCount() const;
        const std::vector<EntityId>& getEntities() const;

    private:
        std::uint64_t m_tickCount = 0;
        EntityId m_nextEntityId = 1;

        std::vector<EntityId> m_entities;
        std::unordered_map<EntityId, TransformComponent> m_transforms;
        std::unordered_map<EntityId, TeamComponent> m_teams;
        std::unordered_map<EntityId, VelocityComponent> m_velocities;

        MovementSystem* m_movementSystem = nullptr;
    };
}
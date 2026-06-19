#pragma once

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mini
{

class ISystem;

class World
{
public:
    World()  = default;
    ~World() = default;

    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    void initialize();
    void tick(float deltaTime);
    void registerSystem(std::unique_ptr<ISystem> system);

    // Lifecycle entita'
    EntityId createEntity();
    bool     destroyEntity(EntityId entity);
    [[nodiscard]] bool isValidEntity(EntityId entity) const;

    // Transform
    void addTransform(EntityId e, const TransformComponent& c);
    [[nodiscard]] bool hasTransform(EntityId e) const;
    TransformComponent*       getTransform(EntityId e);
    const TransformComponent* getTransform(EntityId e) const;

    // Team
    void addTeam(EntityId e, const TeamComponent& c);
    [[nodiscard]] bool hasTeam(EntityId e) const;
    TeamComponent*       getTeam(EntityId e);
    const TeamComponent* getTeam(EntityId e) const;

    // Velocity
    void addVelocity(EntityId e, const VelocityComponent& c);
    [[nodiscard]] bool hasVelocity(EntityId e) const;
    VelocityComponent*       getVelocity(EntityId e);
    const VelocityComponent* getVelocity(EntityId e) const;

    // Health
    void addHealth(EntityId e, const HealthComponent& c);
    [[nodiscard]] bool hasHealth(EntityId e) const;
    HealthComponent*       getHealth(EntityId e);
    const HealthComponent* getHealth(EntityId e) const;

    // MeshRenderer
    void addMeshRenderer(EntityId e, const MeshRendererComponent& c);
    [[nodiscard]] bool hasMeshRenderer(EntityId e) const;
    MeshRendererComponent*       getMeshRenderer(EntityId e);
    const MeshRendererComponent* getMeshRenderer(EntityId e) const;

    // Debug
    void setDebugLogging(bool enabled);
    [[nodiscard]] bool isDebugLoggingEnabled() const;

    [[nodiscard]] std::uint64_t                getTickCount() const;
    [[nodiscard]] const std::vector<EntityId>& getEntities()  const;

private:
    std::uint64_t m_tickCount    = 0;
    EntityId      m_nextEntityId = 1;

    std::vector<std::unique_ptr<ISystem>>               m_systems;
    std::vector<EntityId>                               m_entities;
    std::unordered_set<EntityId>                        m_aliveEntities;

    std::unordered_map<EntityId, TransformComponent>    m_transforms;
    std::unordered_map<EntityId, TeamComponent>         m_teams;
    std::unordered_map<EntityId, VelocityComponent>     m_velocities;
    std::unordered_map<EntityId, HealthComponent>       m_healths;
    std::unordered_map<EntityId, MeshRendererComponent> m_meshRenderers;

    bool m_debugLogging = false;
};

} // namespace mini
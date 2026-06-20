#pragma once

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mini { class ISystem; }

namespace mini
{

class World
{
public:
    World()  = default;
    ~World() = default;
    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    void initialize();
    void tick(float dt);
    void registerSystem(std::unique_ptr<ISystem> system);

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

    // Bullet
    void addBullet(EntityId e, const BulletComponent& c);
    [[nodiscard]] bool hasBullet(EntityId e) const;
    BulletComponent*       getBullet(EntityId e);
    const BulletComponent* getBullet(EntityId e) const;

    // Ai
    void addAi(EntityId e, const AiComponent& c);
    [[nodiscard]] bool hasAi(EntityId e) const;
    AiComponent*       getAi(EntityId e);
    const AiComponent* getAi(EntityId e) const;

    // Collider (AABB statico — solo oggetti ambiente)
    void addCollider(EntityId e, const ColliderComponent& c);
    [[nodiscard]] bool hasCollider(EntityId e) const;
    ColliderComponent*       getCollider(EntityId e);
    const ColliderComponent* getCollider(EntityId e) const;

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
    std::unordered_map<EntityId, BulletComponent>       m_bullets;
    std::unordered_map<EntityId, AiComponent>           m_ais;
    std::unordered_map<EntityId, ColliderComponent>     m_colliders;

    bool m_debugLogging = false;
};

} // namespace mini
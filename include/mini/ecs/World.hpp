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
 
    // --- Lifecycle entita ---
    EntityId createEntity();
    bool     destroyEntity(EntityId entity);
    [[nodiscard]] bool isValidEntity(EntityId entity) const;
 
    // --- Transform ---
    void addTransform(EntityId entity, const TransformComponent& transform);
    [[nodiscard]] bool hasTransform(EntityId entity) const;
    TransformComponent*       getTransform(EntityId entity);
    const TransformComponent* getTransform(EntityId entity) const;
 
    // --- Team ---
    void addTeam(EntityId entity, const TeamComponent& team);
    [[nodiscard]] bool hasTeam(EntityId entity) const;
    TeamComponent*       getTeam(EntityId entity);
    const TeamComponent* getTeam(EntityId entity) const;
 
    // --- Velocity ---
    void addVelocity(EntityId entity, const VelocityComponent& velocity);
    [[nodiscard]] bool hasVelocity(EntityId entity) const;
    VelocityComponent*       getVelocity(EntityId entity);
    const VelocityComponent* getVelocity(EntityId entity) const;
 
    // --- Health ---
    void addHealth(EntityId entity, const HealthComponent& health);
    [[nodiscard]] bool hasHealth(EntityId entity) const;
    HealthComponent*       getHealth(EntityId entity);
    const HealthComponent* getHealth(EntityId entity) const;
 
    // --- Debug e utility ---
    void setDebugLogging(bool enabled);
    [[nodiscard]] bool isDebugLoggingEnabled() const;
 
    [[nodiscard]] std::uint64_t                getTickCount() const;
    [[nodiscard]] const std::vector<EntityId>& getEntities()  const;
 
private:
    std::uint64_t m_tickCount    = 0;
    EntityId      m_nextEntityId = 1;
 
    std::vector<std::unique_ptr<ISystem>>            m_systems;
    std::vector<EntityId>                            m_entities;
    std::unordered_set<EntityId>                     m_aliveEntities;
    std::unordered_map<EntityId, TransformComponent> m_transforms;
    std::unordered_map<EntityId, TeamComponent>      m_teams;
    std::unordered_map<EntityId, VelocityComponent>  m_velocities;
    std::unordered_map<EntityId, HealthComponent>    m_healths;
 
    bool m_debugLogging = false;
};
 
} // namespace mini
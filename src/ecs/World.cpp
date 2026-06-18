#include "mini/ecs/World.hpp"
#include "mini/ecs/ISystem.hpp"
 
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
 
namespace mini
{
 
void World::initialize()
{
    m_tickCount    = 0;
    m_nextEntityId = 1;
 
    m_entities.clear();
    m_aliveEntities.clear();
    m_transforms.clear();
    m_teams.clear();
    m_velocities.clear();
    m_healths.clear();
 
    std::cout << "[World] Inizializzato." << std::endl;
}
 
void World::tick(float deltaTime)
{
    ++m_tickCount;
 
    for (auto& system : m_systems)
    {
        system->update(*this, deltaTime);
    }
 
    if (!m_debugLogging)
    {
        return;
    }
 
    std::cout << "[World] tick=" << m_tickCount
              << " dt="          << deltaTime
              << " entities="    << m_entities.size()
              << std::endl;
 
    for (EntityId entity : m_entities)
    {
        std::cout << "  [Entity " << entity << "]";
 
        if (const TeamComponent* tm = getTeam(entity))
            std::cout << " team=" << tm->teamId;
 
        if (const TransformComponent* t = getTransform(entity))
            std::cout << " pos=(" << t->x << ", " << t->y << ", " << t->z << ")";
 
        if (const VelocityComponent* v = getVelocity(entity))
            std::cout << " vel=(" << v->vx << ", " << v->vy << ", " << v->vz << ")";
 
        if (const HealthComponent* h = getHealth(entity))
            std::cout << " hp=" << h->current << "/" << h->maximum;
 
        std::cout << std::endl;
    }
}
 
void World::registerSystem(std::unique_ptr<ISystem> system)
{
    if (!system)
    {
        return;
    }
 
    m_systems.push_back(std::move(system));
}
 
// =============================================================
// Lifecycle entita
// =============================================================
 
EntityId World::createEntity()
{
    if (m_nextEntityId == std::numeric_limits<EntityId>::max())
    {
        std::cerr << "[World] ERRORE: limite massimo EntityId raggiunto." << std::endl;
        return 0;
    }
 
    const EntityId entity = m_nextEntityId++;
    m_entities.push_back(entity);
    m_aliveEntities.insert(entity);
    return entity;
}
 
bool World::destroyEntity(EntityId entity)
{
    if (!isValidEntity(entity))
    {
        return false;
    }
 
    m_transforms.erase(entity);
    m_teams.erase(entity);
    m_velocities.erase(entity);
    m_healths.erase(entity);
 
    m_aliveEntities.erase(entity);
 
    const auto it = std::remove(m_entities.begin(), m_entities.end(), entity);
    m_entities.erase(it, m_entities.end());
 
    return true;
}
 
bool World::isValidEntity(EntityId entity) const
{
    if (entity == 0)
    {
        return false;
    }
 
    return m_aliveEntities.count(entity) > 0;
}
 
// =============================================================
// Transform
// =============================================================
 
void World::addTransform(EntityId entity, const TransformComponent& transform)
{
    if (!isValidEntity(entity)) return;
    m_transforms[entity] = transform;
}
 
bool World::hasTransform(EntityId entity) const
{
    if (!isValidEntity(entity)) return false;
    return m_transforms.find(entity) != m_transforms.end();
}
 
TransformComponent* World::getTransform(EntityId entity)
{
    if (!isValidEntity(entity)) return nullptr;
    auto it = m_transforms.find(entity);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}
 
const TransformComponent* World::getTransform(EntityId entity) const
{
    if (!isValidEntity(entity)) return nullptr;
    const auto it = m_transforms.find(entity);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}
 
// =============================================================
// Team
// =============================================================
 
void World::addTeam(EntityId entity, const TeamComponent& team)
{
    if (!isValidEntity(entity)) return;
    m_teams[entity] = team;
}
 
bool World::hasTeam(EntityId entity) const
{
    if (!isValidEntity(entity)) return false;
    return m_teams.find(entity) != m_teams.end();
}
 
TeamComponent* World::getTeam(EntityId entity)
{
    if (!isValidEntity(entity)) return nullptr;
    auto it = m_teams.find(entity);
    return (it != m_teams.end()) ? &it->second : nullptr;
}
 
const TeamComponent* World::getTeam(EntityId entity) const
{
    if (!isValidEntity(entity)) return nullptr;
    const auto it = m_teams.find(entity);
    return (it != m_teams.end()) ? &it->second : nullptr;
}
 
// =============================================================
// Velocity
// =============================================================
 
void World::addVelocity(EntityId entity, const VelocityComponent& velocity)
{
    if (!isValidEntity(entity)) return;
    m_velocities[entity] = velocity;
}
 
bool World::hasVelocity(EntityId entity) const
{
    if (!isValidEntity(entity)) return false;
    return m_velocities.find(entity) != m_velocities.end();
}
 
VelocityComponent* World::getVelocity(EntityId entity)
{
    if (!isValidEntity(entity)) return nullptr;
    auto it = m_velocities.find(entity);
    return (it != m_velocities.end()) ? &it->second : nullptr;
}
 
const VelocityComponent* World::getVelocity(EntityId entity) const
{
    if (!isValidEntity(entity)) return nullptr;
    const auto it = m_velocities.find(entity);
    return (it != m_velocities.end()) ? &it->second : nullptr;
}
 
// =============================================================
// Health
// =============================================================
 
void World::addHealth(EntityId entity, const HealthComponent& health)
{
    if (!isValidEntity(entity)) return;
    m_healths[entity] = health;
}
 
bool World::hasHealth(EntityId entity) const
{
    if (!isValidEntity(entity)) return false;
    return m_healths.find(entity) != m_healths.end();
}
 
HealthComponent* World::getHealth(EntityId entity)
{
    if (!isValidEntity(entity)) return nullptr;
    auto it = m_healths.find(entity);
    return (it != m_healths.end()) ? &it->second : nullptr;
}
 
const HealthComponent* World::getHealth(EntityId entity) const
{
    if (!isValidEntity(entity)) return nullptr;
    const auto it = m_healths.find(entity);
    return (it != m_healths.end()) ? &it->second : nullptr;
}
 
// =============================================================
// Debug e utility
// =============================================================
 
void World::setDebugLogging(bool enabled)
{
    m_debugLogging = enabled;
}
 
bool World::isDebugLoggingEnabled() const
{
    return m_debugLogging;
}
 
std::uint64_t World::getTickCount() const
{
    return m_tickCount;
}
 
const std::vector<EntityId>& World::getEntities() const
{
    return m_entities;
}
 
} // namespace mini
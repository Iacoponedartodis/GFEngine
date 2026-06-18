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
 
    std::cout << "[World] tick="     << m_tickCount
              << " dt="              << deltaTime
              << " entities="        << m_entities.size()
              << std::endl;
 
    for (EntityId entity : m_entities)
    {
        std::cout << "  [Entity " << entity << "]";
 
        if (const TeamComponent* team = getTeam(entity))
        {
            std::cout << " team=" << team->teamId;
        }
 
        if (const TransformComponent* transform = getTransform(entity))
        {
            std::cout << " pos=(" << transform->x << ", " << transform->y << ")";
        }
 
        if (const VelocityComponent* velocity = getVelocity(entity))
        {
            std::cout << " vel=(" << velocity->vx << ", " << velocity->vy << ")";
        }
 
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
 
void World::addTransform(EntityId entity, const TransformComponent& transform)
{
    if (!isValidEntity(entity))
    {
        return;
    }
 
    m_transforms[entity] = transform;
}
 
void World::addTeam(EntityId entity, const TeamComponent& team)
{
    if (!isValidEntity(entity))
    {
        return;
    }
 
    m_teams[entity] = team;
}
 
void World::addVelocity(EntityId entity, const VelocityComponent& velocity)
{
    if (!isValidEntity(entity))
    {
        return;
    }
 
    m_velocities[entity] = velocity;
}
 
bool World::hasTransform(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return false;
    }
 
    return m_transforms.find(entity) != m_transforms.end();
}
 
bool World::hasTeam(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return false;
    }
 
    return m_teams.find(entity) != m_teams.end();
}
 
bool World::hasVelocity(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return false;
    }
 
    return m_velocities.find(entity) != m_velocities.end();
}
 
// --- Non-const getters ---
 
TransformComponent* World::getTransform(EntityId entity)
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    auto it = m_transforms.find(entity);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}
 
TeamComponent* World::getTeam(EntityId entity)
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    auto it = m_teams.find(entity);
    return (it != m_teams.end()) ? &it->second : nullptr;
}
 
VelocityComponent* World::getVelocity(EntityId entity)
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    auto it = m_velocities.find(entity);
    return (it != m_velocities.end()) ? &it->second : nullptr;
}
 
// --- Const getters (per uso su const World&) ---
 
const TransformComponent* World::getTransform(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    const auto it = m_transforms.find(entity);
    return (it != m_transforms.end()) ? &it->second : nullptr;
}
 
const TeamComponent* World::getTeam(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    const auto it = m_teams.find(entity);
    return (it != m_teams.end()) ? &it->second : nullptr;
}
 
const VelocityComponent* World::getVelocity(EntityId entity) const
{
    if (!isValidEntity(entity))
    {
        return nullptr;
    }
 
    const auto it = m_velocities.find(entity);
    return (it != m_velocities.end()) ? &it->second : nullptr;
}
 
// --- Debug e utility ---
 
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
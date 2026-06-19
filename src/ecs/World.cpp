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
    m_tickCount = 0; m_nextEntityId = 1;
    m_entities.clear(); m_aliveEntities.clear();
    m_transforms.clear(); m_teams.clear();
    m_velocities.clear(); m_healths.clear();
    m_meshRenderers.clear();
    std::cout << "[World] Inizializzato." << std::endl;
}

void World::tick(float dt)
{
    ++m_tickCount;
    for (auto& s : m_systems) s->update(*this, dt);
}

void World::registerSystem(std::unique_ptr<ISystem> s)
{ if (s) m_systems.push_back(std::move(s)); }

EntityId World::createEntity()
{
    if (m_nextEntityId == std::numeric_limits<EntityId>::max())
    { std::cerr << "[World] ERRORE: limite EntityId.\n"; return 0; }
    const EntityId e = m_nextEntityId++;
    m_entities.push_back(e);
    m_aliveEntities.insert(e);
    return e;
}

bool World::destroyEntity(EntityId e)
{
    if (!isValidEntity(e)) return false;
    m_transforms.erase(e); m_teams.erase(e);
    m_velocities.erase(e); m_healths.erase(e);
    m_meshRenderers.erase(e); m_aliveEntities.erase(e);
    auto it = std::remove(m_entities.begin(), m_entities.end(), e);
    m_entities.erase(it, m_entities.end());
    return true;
}

bool World::isValidEntity(EntityId e) const
{ return e != 0 && m_aliveEntities.count(e) > 0; }

// --- Transform ---
void World::addTransform(EntityId e, const TransformComponent& c)    { if (isValidEntity(e)) m_transforms[e] = c; }
bool World::hasTransform(EntityId e) const                            { return isValidEntity(e) && m_transforms.count(e); }
TransformComponent*       World::getTransform(EntityId e)             { if (!isValidEntity(e)) return nullptr; auto it = m_transforms.find(e); return it != m_transforms.end() ? &it->second : nullptr; }
const TransformComponent* World::getTransform(EntityId e) const       { if (!isValidEntity(e)) return nullptr; auto it = m_transforms.find(e); return it != m_transforms.end() ? &it->second : nullptr; }

// --- Team ---
void World::addTeam(EntityId e, const TeamComponent& c)               { if (isValidEntity(e)) m_teams[e] = c; }
bool World::hasTeam(EntityId e) const                                  { return isValidEntity(e) && m_teams.count(e); }
TeamComponent*       World::getTeam(EntityId e)                        { if (!isValidEntity(e)) return nullptr; auto it = m_teams.find(e); return it != m_teams.end() ? &it->second : nullptr; }
const TeamComponent* World::getTeam(EntityId e) const                  { if (!isValidEntity(e)) return nullptr; auto it = m_teams.find(e); return it != m_teams.end() ? &it->second : nullptr; }

// --- Velocity ---
void World::addVelocity(EntityId e, const VelocityComponent& c)       { if (isValidEntity(e)) m_velocities[e] = c; }
bool World::hasVelocity(EntityId e) const                              { return isValidEntity(e) && m_velocities.count(e); }
VelocityComponent*       World::getVelocity(EntityId e)                { if (!isValidEntity(e)) return nullptr; auto it = m_velocities.find(e); return it != m_velocities.end() ? &it->second : nullptr; }
const VelocityComponent* World::getVelocity(EntityId e) const          { if (!isValidEntity(e)) return nullptr; auto it = m_velocities.find(e); return it != m_velocities.end() ? &it->second : nullptr; }

// --- Health ---
void World::addHealth(EntityId e, const HealthComponent& c)            { if (isValidEntity(e)) m_healths[e] = c; }
bool World::hasHealth(EntityId e) const                                 { return isValidEntity(e) && m_healths.count(e); }
HealthComponent*       World::getHealth(EntityId e)                     { if (!isValidEntity(e)) return nullptr; auto it = m_healths.find(e); return it != m_healths.end() ? &it->second : nullptr; }
const HealthComponent* World::getHealth(EntityId e) const               { if (!isValidEntity(e)) return nullptr; auto it = m_healths.find(e); return it != m_healths.end() ? &it->second : nullptr; }

// --- MeshRenderer ---
void World::addMeshRenderer(EntityId e, const MeshRendererComponent& c){ if (isValidEntity(e)) m_meshRenderers[e] = c; }
bool World::hasMeshRenderer(EntityId e) const                           { return isValidEntity(e) && m_meshRenderers.count(e); }
MeshRendererComponent*       World::getMeshRenderer(EntityId e)         { if (!isValidEntity(e)) return nullptr; auto it = m_meshRenderers.find(e); return it != m_meshRenderers.end() ? &it->second : nullptr; }
const MeshRendererComponent* World::getMeshRenderer(EntityId e) const   { if (!isValidEntity(e)) return nullptr; auto it = m_meshRenderers.find(e); return it != m_meshRenderers.end() ? &it->second : nullptr; }

// --- Utility ---
void World::setDebugLogging(bool v)               { m_debugLogging = v; }
bool World::isDebugLoggingEnabled() const         { return m_debugLogging; }
std::uint64_t World::getTickCount() const         { return m_tickCount; }
const std::vector<EntityId>& World::getEntities() const { return m_entities; }

} // namespace mini
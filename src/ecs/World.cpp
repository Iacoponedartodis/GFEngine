#include "mini/ecs/World.hpp"
#include "mini/ecs/ISystem.hpp"

#include <algorithm>
#include <iostream>
#include <limits>

namespace mini
{

void World::initialize()
{
    m_tickCount = 0; m_nextEntityId = 1;
    m_entities.clear(); m_aliveEntities.clear();
    m_transforms.clear(); m_teams.clear(); m_velocities.clear();
    m_healths.clear(); m_meshRenderers.clear(); m_bullets.clear(); m_ais.clear();
    std::cout << "[World] Inizializzato." << std::endl;
}

void World::tick(float dt)
{ ++m_tickCount; for (auto& s : m_systems) s->update(*this, dt); }

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
    m_transforms.erase(e); m_teams.erase(e); m_velocities.erase(e);
    m_healths.erase(e); m_meshRenderers.erase(e); m_bullets.erase(e); m_ais.erase(e);
    m_aliveEntities.erase(e);
    auto it = std::remove(m_entities.begin(), m_entities.end(), e);
    m_entities.erase(it, m_entities.end());
    return true;
}

bool World::isValidEntity(EntityId e) const
{ return e != 0 && m_aliveEntities.count(e) > 0; }

#define IMPL(Type, Map) \
void World::add##Type(EntityId e, const Type##Component& c) { if (isValidEntity(e)) Map[e]=c; } \
bool World::has##Type(EntityId e) const { return isValidEntity(e) && Map.count(e); } \
Type##Component*       World::get##Type(EntityId e)       { if(!isValidEntity(e))return nullptr; auto it=Map.find(e); return it!=Map.end()?&it->second:nullptr; } \
const Type##Component* World::get##Type(EntityId e) const { if(!isValidEntity(e))return nullptr; auto it=Map.find(e); return it!=Map.end()?&it->second:nullptr; }

IMPL(Transform,    m_transforms)
IMPL(Team,         m_teams)
IMPL(Velocity,     m_velocities)
IMPL(Health,       m_healths)
IMPL(MeshRenderer, m_meshRenderers)
IMPL(Bullet,       m_bullets)
IMPL(Ai,           m_ais)
#undef IMPL

void World::setDebugLogging(bool v)               { m_debugLogging = v; }
bool World::isDebugLoggingEnabled() const         { return m_debugLogging; }
std::uint64_t World::getTickCount() const         { return m_tickCount; }
const std::vector<EntityId>& World::getEntities() const { return m_entities; }

} // namespace mini
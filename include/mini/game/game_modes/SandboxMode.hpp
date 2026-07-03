#pragma once
#include "mini/ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <utility>
#include <string>

namespace mini
{
class World;
class Mesh;
class Texture;
class DefinitionRegistry;
using MeshCache = std::unordered_map<std::string, Mesh*>;

// Modalità sandbox: arena aperta piatta, niente obiettivi, niente AI attiva.
// Usata per testare movimenti, armi e posizionamento modelli.
class SandboxMode
{
public:
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry = nullptr,
               const MeshCache* meshCache = nullptr);
    void update(World& world, float dt);

    [[nodiscard]] EntityId  getPlayerEntity() const { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()     const { return m_spawnPos; }
    [[nodiscard]] int       getTeam1Tickets() const { return 999; }
    [[nodiscard]] int       getTeam2Tickets() const { return 0;   }

    int consumeTeam1Ticket() { return 999; }
    void overridePlayerEntity(EntityId e) { m_playerEntity = e; }

    float playerHp = 100.0f;

private:
    EntityId  m_playerEntity = 0;
    glm::vec3 m_spawnPos     = {0, 0.86f, 0.0f};

    Mesh*    m_mesh      = nullptr;
    Texture* m_tex       = nullptr;
    const DefinitionRegistry* m_registry  = nullptr;
    const MeshCache*          m_meshCache = nullptr;

    // Manichino: posizione + tipo, per il respawn automatico.
    struct DummyInfo { float x = 0, z = 0; std::string id; };
    std::vector<std::pair<EntityId, DummyInfo>> m_dummies;     // attivi
    std::vector<std::pair<float, DummyInfo>>    m_respawnQueue; // in attesa
    float m_respawnDelay = 2.0f;

    EntityId spawnDummy(World& world, const DummyInfo& info);
    void buildMapGeometry(World& world);  // geometria firebase
    void buildArena(World& world);        // fallback arena piatta
};

} // namespace mini

#pragma once
#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/CommandPosts.hpp"
#include "mini/game/MatchSettings.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <utility>
#include <string>

namespace mini
{

// Modalità sandbox: mappa firebase, manichini che respawnano, niente
// obiettivi. Usata per testare mappa, spawn point, armi e modelli.
class SandboxMode : public IGameMode
{
public:
    void applySettings(const MatchSettings& s) override { playerHp = s.playerHp; }
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry,
               const MeshCache* meshCache) override;
    void update(World& world, float dt) override;

    [[nodiscard]] EntityId  getPlayerEntity() const override { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()     const override { return m_spawnPos; }
    [[nodiscard]] int       getTeam1Tickets() const override { return 999; }
    [[nodiscard]] int       getTeam2Tickets() const override { return 0;   }
    [[nodiscard]] bool hasVictoryCondition()  const override { return false; }

    int consumeTeam1Ticket() override { return 999; }
    void overridePlayerEntity(EntityId e) override { m_playerEntity = e; }

    float playerHp = 100.0f;

private:
    EntityId  m_playerEntity = 0;
    glm::vec3 m_spawnPos     = {0, 0.86f, 0.0f};

    Mesh*    m_mesh      = nullptr;
    Texture* m_tex       = nullptr;
    const DefinitionRegistry* m_registry  = nullptr;
    const MeshCache*          m_meshCache = nullptr;

    // Command post visibili/catturabili anche in sandbox (per testarli).
    CommandPosts m_commandPosts;

    // Manichino: posizione + tipo + team, per il respawn automatico.
    // team 2 = nemico (da data/enemies), team 1 = alleato (da data/allies).
    struct DummyInfo { float x = 0, z = 0; std::string id; int team = 2; };
    std::vector<std::pair<EntityId, DummyInfo>> m_dummies;     // attivi
    std::vector<std::pair<float, DummyInfo>>    m_respawnQueue; // in attesa
    float m_respawnDelay = 2.0f;

    EntityId spawnDummy(World& world, const DummyInfo& info);
    void buildMapGeometry(World& world);  // geometria firebase
    void buildArena(World& world);        // fallback arena piatta
};

} // namespace mini

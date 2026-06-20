#pragma once
#include "mini/ecs/Entity.hpp"
#include "mini/game/MatchSettings.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace mini { class World; class Mesh; class Texture; }

namespace mini
{

struct RespawnEntry
{
    float timer;
    float x, z;
    int   teamId;
    float mr, mg, mb;
    float br, bg, bb;
    float hp;
    float pax, paz, pbx, pbz;
    float patSpd, interval, range;
    bool  stationary;
};

class ConquestMode
{
public:
    void applySettings(const MatchSettings& s);   // chiama PRIMA di start()
    void start(World& world, Mesh* mesh, Texture* texture);
    void update(World& world, float deltaTime);

    [[nodiscard]] EntityId  getPlayerEntity() const { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()     const { return m_spawnPos; }

    [[nodiscard]] int getTeam1Tickets() const { return m_team1Tickets; }
    [[nodiscard]] int getTeam2Tickets() const { return m_team2Tickets; }

    // Campi pubblici (retrocompatibilità e accesso diretto)
    int   initialTeam1Tickets = 5;
    int   initialTeam2Tickets = 10;
    float respawnDelay        = 4.0f;
    float aiSpeed             = 3.5f;
    float aiFireInterval      = 1.8f;
    float aiRange             = 12.0f;
    float playerHp            = 100.0f;
    float playerSpeed         = 5.0f;

private:
    EntityId  m_playerEntity = 0;
    glm::vec3 m_spawnPos     = {0, 0.86f, 8.0f};

    Mesh*    m_mesh = nullptr;
    Texture* m_tex  = nullptr;

    int m_team1Tickets = 5;
    int m_team2Tickets = 10;

    std::vector<RespawnEntry> m_respawnQueue;

    void spawnUnit(World& world, const RespawnEntry& info);
    void checkDeaths(World& world);
};

} // namespace mini
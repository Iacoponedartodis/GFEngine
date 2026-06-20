#pragma once
#include "mini/ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace mini { class World; class Mesh; class Texture; }

namespace mini
{

struct RespawnEntry
{
    float timer;          // secondi rimanenti prima del respawn
    float x, z;           // posizione originale di spawn
    int   teamId;
    float mr, mg, mb;     // colore mesh
    float br, bg, bb;     // colore proiettile
    float hp;
    float pax, paz, pbx, pbz;
    float patSpd, interval, range;
    bool  stationary;
};

class ConquestMode
{
public:
    void start(World& world, Mesh* mesh, Texture* texture);
    void update(World& world, float deltaTime);

    [[nodiscard]] EntityId  getPlayerEntity() const { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()     const { return m_spawnPos; }

    // Ticket system
    [[nodiscard]] int getTeam1Tickets() const { return m_team1Tickets; }
    [[nodiscard]] int getTeam2Tickets() const { return m_team2Tickets; }

    // Configura prima di start()
    int initialTeam1Tickets = 5;   // alleati (giocatore incluso)
    int initialTeam2Tickets = 10;  // nemici
    float respawnDelay      = 4.0f; // secondi

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
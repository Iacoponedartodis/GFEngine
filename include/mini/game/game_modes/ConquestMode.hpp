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
    void applySettings(const MatchSettings& s);
    void start(World& world, Mesh* mesh, Texture* texture);
    void update(World& world, float deltaTime);

    [[nodiscard]] EntityId  getPlayerEntity() const { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()     const { return m_spawnPos; }

    [[nodiscard]] int getTeam1Tickets() const { return m_team1Tickets; }
    [[nodiscard]] int getTeam2Tickets() const { return m_team2Tickets; }

    // Scala un ticket del team 1 (chiamato da Application quando il giocatore muore)
    // Restituisce il numero di ticket rimasti DOPO la scalata
    int consumeTeam1Ticket()
    {
        if (m_team1Tickets > 0) --m_team1Tickets;
        return m_team1Tickets;
    }

    // Permette ad Application di aggiornare l'entità giocatore dopo un respawn
    void overridePlayerEntity(EntityId e) { m_playerEntity = e; }

    int   initialTeam1Tickets = 5;
    int   initialTeam2Tickets = 10;
    int   team1AiCount        = 1;
    int   team2AiCount        = 6;
    float respawnDelay        = 4.0f;
    float playerHp            = 100.0f;

private:
    EntityId  m_playerEntity = 0;
    glm::vec3 m_spawnPos     = {0, 0.86f, 8.0f};

    Mesh*    m_mesh = nullptr;
    Texture* m_tex  = nullptr;

    int m_team1Tickets = 5;
    int m_team2Tickets = 10;

    std::vector<RespawnEntry> m_respawnQueue;

    // Traccia le unità vive per rilevare le morti (sostituisce il static global)
    struct UnitTemplate
    {
        float x, z, yPos;
        int   teamId;
        float mr, mg, mb;
        float br, bg, bb;
        float hp;
        float pax, paz, pbx, pbz;
        float patSpd, interval, range;
        bool  stationary;
    };
    std::vector<std::pair<EntityId, UnitTemplate>> m_trackedUnits;

    void spawnUnit(World& world, const RespawnEntry& info);
    void checkDeaths(World& world);
};

} // namespace mini
#pragma once
#include "mini/game/data/Definitions.hpp"
#include "mini/ecs/Entity.hpp"
#include "mini/game/MatchSettings.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace mini { class World; class Mesh; class Texture; class DefinitionRegistry; }

namespace mini
{

// Mappa path OBJ → puntatore Mesh (non-owning, vive in Application)
using MeshCache = std::unordered_map<std::string, Mesh*>;

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
    std::string hitboxProfileId;

    // Stats proiettile dall'arma primaria (WeaponDef)
    float bulletSpeed    = 8.0f;
    float bulletDamage   = 20.0f;
    float bulletLifetime = 5.0f;

    // Mesh specifica dell'entità (nullptr = usa default)
    Mesh* entityMesh = nullptr;
};

class ConquestMode
{
public:
    void applySettings(const MatchSettings& s);
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry = nullptr,
               const MeshCache* meshCache = nullptr);
    void update(World& world, float deltaTime);

    [[nodiscard]] EntityId  getPlayerEntity()  const { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()      const { return m_spawnPos; }
    [[nodiscard]] int getTeam1Tickets()        const { return m_team1Tickets; }
    [[nodiscard]] int getTeam2Tickets()        const { return m_team2Tickets; }
    [[nodiscard]] Mesh*    getDefaultMesh()    const { return m_mesh; }
    [[nodiscard]] Texture* getDefaultTexture() const { return m_tex; }

    int consumeTeam1Ticket()
    {
        if (m_team1Tickets > 0) --m_team1Tickets;
        return m_team1Tickets;
    }
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

    Mesh*    m_mesh      = nullptr;
    Texture* m_tex       = nullptr;
    const DefinitionRegistry* m_registry  = nullptr;
    const MeshCache*          m_meshCache = nullptr;

    int m_team1Tickets = 5;
    int m_team2Tickets = 10;

    std::vector<RespawnEntry> m_respawnQueue;

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
        std::string hitboxProfileId;
        float bulletSpeed    = 8.0f;
        float bulletDamage   = 20.0f;
        float bulletLifetime = 5.0f;
        Mesh* entityMesh     = nullptr;
    };
    std::vector<std::pair<EntityId, UnitTemplate>> m_trackedUnits;

    void spawnUnit(World& world, const RespawnEntry& info);
    void checkDeaths(World& world);
};

} // namespace mini

#pragma once
#include "mini/game/data/Definitions.hpp"
#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/CommandPosts.hpp"
#include "mini/game/MatchSettings.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

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
    std::string hitboxProfileId;

    // Stats proiettile dall'arma primaria (WeaponDef)
    float bulletSpeed    = 8.0f;
    float bulletDamage   = 20.0f;
    float bulletLifetime = 5.0f;

    // Mesh specifica dell'entità (nullptr = usa default)
    Mesh* entityMesh = nullptr;

    // Trasformazione modello dall'EnemyDef (applicata solo con mesh custom)
    float meshRotX  = 0.0f;
    float meshRotY  = 0.0f;
    float meshScale = 1.0f;

    // Arma primaria: risolta in spawnUnit (cadenza, calore, proiettile)
    std::string weaponId;
    // Profilo AI: risolto in spawnUnit (seekSpeed, accuracy, jump, reaction)
    std::string aiProfileId;

    // Arma visibile in mano (mesh + posa nel model space del personaggio)
    Mesh*     weaponMesh  = nullptr;
    glm::mat4 weaponLocal = glm::mat4(1.0f);
};

class ConquestMode : public IGameMode
{
public:
    void applySettings(const MatchSettings& s) override;
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry,
               const MeshCache* meshCache) override;
    void update(World& world, float deltaTime) override;

    [[nodiscard]] EntityId  getPlayerEntity()  const override { return m_playerEntity; }
    [[nodiscard]] glm::vec3 getSpawnPos()      const override { return m_spawnPos; }
    [[nodiscard]] int getTeam1Tickets()        const override { return m_team1Tickets; }
    [[nodiscard]] int getTeam2Tickets()        const override { return m_team2Tickets; }
    [[nodiscard]] bool hasVictoryCondition()   const override { return true; }
    [[nodiscard]] MatchOutcome outcome(const World& world) const override;
    [[nodiscard]] const CommandPosts* commandPosts() const override
    { return &m_commandPosts; }
    [[nodiscard]] Mesh*    getDefaultMesh()    const { return m_mesh; }
    [[nodiscard]] Texture* getDefaultTexture() const { return m_tex; }

    int consumeTeam1Ticket() override
    {
        if (m_team1Tickets > 0) --m_team1Tickets;
        return m_team1Tickets;
    }
    void overridePlayerEntity(EntityId e) override { m_playerEntity = e; }

    int   initialTeam1Tickets = 5;
    int   initialTeam2Tickets = 10;
    int   team1AiCount        = 1;
    int   team2AiCount        = 6;
    float respawnDelay        = 4.0f;
    float playerHp            = 100.0f;

protected:
    // Hook per le modalità derivate (Assalto/Difesa, ADR-014): regole
    // obiettivo applicate ogni update. Default Conquista: la maggioranza
    // dei post drena i ticket avversari.
    virtual void updateObjectiveRules(World& world, float dt);

    EntityId  m_playerEntity = 0;
    glm::vec3 m_spawnPos     = {0, 0.86f, 8.0f};

    Mesh*    m_mesh      = nullptr;
    Texture* m_tex       = nullptr;
    const DefinitionRegistry* m_registry  = nullptr;
    const MeshCache*          m_meshCache = nullptr;
    const MapDef*             m_map       = nullptr; // mappa attiva (per suolo/ostacoli)

    int m_team1Tickets = 5;
    int m_team2Tickets = 10;

    // Command post (ADR-009)
    CommandPosts m_commandPosts;
    float        m_bleedTimer    = 0.0f;
    float        m_bleedInterval = 6.0f;

private:
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
        float meshRotX  = 0.0f;
        float meshRotY  = 0.0f;
        float meshScale = 1.0f;
        std::string weaponId;
        std::string aiProfileId;
        Mesh*     weaponMesh  = nullptr;
        glm::mat4 weaponLocal = glm::mat4(1.0f);
    };
    std::vector<std::pair<EntityId, UnitTemplate>> m_trackedUnits;

    void spawnUnit(World& world, const RespawnEntry& info);
    void checkDeaths(World& world);
};

} // namespace mini

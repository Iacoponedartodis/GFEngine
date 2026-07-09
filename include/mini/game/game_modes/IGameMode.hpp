#pragma once
// IGameMode — contratto comune dei game mode (ADR-008).
// Ogni modalità (Conquista, Sandbox, futura Assalto/Difesa) è una
// configurazione dei sistemi core dietro questa interfaccia: Application
// non conosce le classi concrete, le istanzia solo via createGameMode().

#include "mini/ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>
#include <memory>

namespace mini
{
class World;
class Mesh;
class Texture;
class DefinitionRegistry;
struct MatchSettings;

// Mappa path mesh → puntatore Mesh (non-owning, vive in Application)
using MeshCache = std::unordered_map<std::string, Mesh*>;

class IGameMode
{
public:
    virtual ~IGameMode() = default;

    // Configurazione dalla schermata pre-match (tickets, conteggi AI, hp...)
    virtual void applySettings(const MatchSettings& s) = 0;

    // Costruisce il mondo della modalità (giocatore, unità, geometria mappa)
    virtual void start(World& world, Mesh* defaultMesh, Texture* texture,
                       const DefinitionRegistry* registry,
                       const MeshCache* meshCache) = 0;

    virtual void update(World& world, float dt) = 0;

    [[nodiscard]] virtual EntityId  getPlayerEntity() const = 0;
    [[nodiscard]] virtual glm::vec3 getSpawnPos()     const = 0;
    [[nodiscard]] virtual int getTeam1Tickets() const = 0;
    [[nodiscard]] virtual int getTeam2Tickets() const = 0;
    virtual int  consumeTeam1Ticket() = 0;
    virtual void overridePlayerEntity(EntityId e) = 0;

    // false = modalità senza vittoria/sconfitta (es. sandbox di prova)
    [[nodiscard]] virtual bool hasVictoryCondition() const = 0;
};

// Factory: punto unico di registrazione delle modalità.
// Id noti: "conquest", "sandbox". Id sconosciuto → fallback conquest + log.
std::unique_ptr<IGameMode> createGameMode(const std::string& modeId);

} // namespace mini

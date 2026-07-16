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
class CommandPosts;
struct MatchSettings;

// Mappa path mesh → puntatore Mesh (non-owning, vive in Application)
using MeshCache = std::unordered_map<std::string, Mesh*>;

// Esito della partita, deciso dal MODE (ADR-014): Application non hardcoda
// più le regole di vittoria — le chiede alla modalità attiva.
enum class MatchOutcome { Ongoing, Team1Win, Team2Win };

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
    // I ticket sono la RISERVA DI RINFORZI della squadra: li consuma la morte di
    // un'UNITÀ (ConquestMode::checkDeaths → un rimpiazzo entra dalla riserva),
    // non quella del giocatore. Qui si leggono soltanto: quanti rinforzi restano
    // è ciò che decide se la caduta del giocatore è recuperabile (Application).
    // `consumeTeam1Ticket()` è stato RIMOSSO il 2026-07-16: serviva solo a far
    // pagare al giocatore le proprie morti — comportamento contrario al design.
    // Non reintrodurlo: senza il metodo, la regola è strutturale invece che una
    // convenzione da ricordare.
    [[nodiscard]] virtual int getTeam1Tickets() const = 0;
    [[nodiscard]] virtual int getTeam2Tickets() const = 0;
    virtual void overridePlayerEntity(EntityId e) = 0;

    // false = modalità senza vittoria/sconfitta (es. sandbox di prova)
    [[nodiscard]] virtual bool hasVictoryCondition() const = 0;

    // Esito della partita secondo le regole della modalità (ADR-014).
    // Default: mai conclusa (sandbox). Il player-death/ticket-zero del
    // giocatore resta gestito da Application (riguarda il giocatore, non
    // le regole obiettivo della modalità).
    [[nodiscard]] virtual MatchOutcome outcome(const World&) const
    { return MatchOutcome::Ongoing; }

    // Command post della modalità (per l'HUD). nullptr = nessuno.
    [[nodiscard]] virtual const CommandPosts* commandPosts() const
    { return nullptr; }
};

// Factory: punto unico di registrazione delle modalità.
// Id noti: "conquest", "sandbox". Id sconosciuto → fallback conquest + log.
std::unique_ptr<IGameMode> createGameMode(const std::string& modeId);

} // namespace mini

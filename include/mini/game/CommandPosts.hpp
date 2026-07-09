#pragma once
// CommandPosts — sistema riusabile di punti di comando catturabili (ADR-009).
// Non è un game mode: è un blocco condiviso che ogni modalità configura dal
// MapDef e interroga (countOwnedBy) per le proprie regole (ticket bleed,
// obiettivi di assalto/difesa, ecc.).

#include "mini/game/data/Definitions.hpp"
#include "mini/ecs/Entity.hpp"
#include <vector>

namespace mini
{
class World;
class Mesh;
class Texture;

class CommandPosts
{
public:
    // Crea le entità visuali (bandiera + base) e inizializza gli stati.
    void init(World& world, const std::vector<CommandPostDef>& defs,
              Mesh* mesh, Texture* tex);

    // Avanza la logica di cattura: presenza esclusiva di un team nel raggio
    // fa progredire la cattura; zona contesa o vuota → il progresso decade.
    void update(World& world, float dt);

    [[nodiscard]] int count() const { return (int)m_posts.size(); }
    [[nodiscard]] int countOwnedBy(int team) const;

    // Stato leggibile per l'HUD (proprietario + progresso cattura 0..1).
    struct Status
    {
        std::string label;
        int   owner         = 0;
        int   capturingTeam = 0;
        float progress01    = 0.0f;
    };
    [[nodiscard]] std::vector<Status> status() const;

    // Forza il proprietario iniziale di TUTTI i post (usato da Assalto/
    // Difesa: la mappa li autora neutrali, la modalità decide chi li tiene).
    void forceAllOwners(World& world, int team);

private:
    struct Post
    {
        CommandPostDef def;
        int      owner        = 0;     // 0 neutrale, 1, 2
        int      capturingTeam= 0;     // team che sta catturando (0 = nessuno)
        float    progress     = 0.0f;  // 0..captureTime
        EntityId flagEntity   = 0;     // palo colorato per team
        EntityId baseEntity   = 0;     // piastra a terra
    };
    std::vector<Post> m_posts;

    void applyOwnerColor(World& world, Post& p);
};

} // namespace mini

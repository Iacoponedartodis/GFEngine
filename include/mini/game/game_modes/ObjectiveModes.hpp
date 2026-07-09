#pragma once
// ObjectiveModes — Assalto e Difesa (ADR-014).
// Configurazioni di ConquestMode (spawn/geometria/respawn identici) con
// regole obiettivo e condizioni di vittoria diverse sugli STESSI
// CommandPosts (ADR-009). La mappa autora i post neutrali: è la modalità
// a decidere chi li possiede all'inizio.

#include "mini/game/game_modes/ConquestMode.hpp"

namespace mini
{

// ── Assalto: il team 1 (giocatore) ATTACCA ────────────────────────────────
// I post partono in mano ai nemici. La pressione dell'attacco costa: i
// ticket alleati calano nel tempo. Vittoria: catturare TUTTI i post.
// Sconfitta: ticket alleati esauriti prima.
class AssaultMode : public ConquestMode
{
public:
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry,
               const MeshCache* meshCache) override;

    [[nodiscard]] MatchOutcome outcome(const World& world) const override;

protected:
    void updateObjectiveRules(World& world, float dt) override;
};

// ── Difesa: il team 1 (giocatore) DIFENDE ─────────────────────────────────
// I post partono in mano agli alleati. Gli attaccanti (nemici) consumano
// ticket nel tempo. Vittoria: resistere finché i ticket nemici finiscono.
// Sconfitta: i nemici catturano TUTTI i post.
class DefenseMode : public ConquestMode
{
public:
    void start(World& world, Mesh* defaultMesh, Texture* texture,
               const DefinitionRegistry* registry,
               const MeshCache* meshCache) override;

    [[nodiscard]] MatchOutcome outcome(const World& world) const override;

protected:
    void updateObjectiveRules(World& world, float dt) override;
};

} // namespace mini

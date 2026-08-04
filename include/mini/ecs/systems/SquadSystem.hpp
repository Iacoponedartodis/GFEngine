#pragma once
#include "mini/ecs/ISystem.hpp"
#include "mini/ecs/Entity.hpp"

namespace mini
{

// SquadSystem (26_SquadAndCommand, ADR-020) — Phase A.
// Gira FRA CombatSystem e AiSystem: l'ordine è un VINCOLO sulla decisione dell'AI,
// non un override del movimento (se girasse dopo, gli alleati sarebbero telecomandati).
// Ordine risultante del tick: Movement → Combat → Squad → Ai → Crowd.
//
// Responsabilità Phase A:
//  1. forma/aggiorna la squadra alleata (i respawn creano entità nuove ogni volta);
//  2. assegna l'ordine di default (Follow del leader) a chi non ha ordini;
//  3. aggiorna il ciclo di vita degli ordini — ognuno finisce completato o
//     FALLITO CON CAUSA ESPLICITA, mai in silenzio (doc 26), con evento telemetria.
// L'esecuzione vera del movimento resta dell'AI (doc 16) + crowd (doc 22).
class SquadSystem : public ISystem
{
public:
    void update(World& world, float dt) override;
    const char* name() const override { return "squad"; }

private:
    void formAlliedSquad(World& world);
    EntityId m_leader = 0;   // leader corrente della squadra alleata (0 = nessuno)
    // Esito annunciato una volta per ORDINE, non una per membro (5 alleati che
    // completano darebbero 5 righe identiche sull'HUD). Si riarma a ogni ordine nuovo.
    bool m_outcomeAnnounced = false;
};

} // namespace mini

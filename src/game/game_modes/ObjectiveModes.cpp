// ObjectiveModes.cpp — Assalto e Difesa (ADR-014).
#include "mini/game/game_modes/ObjectiveModes.hpp"
#include "mini/ecs/World.hpp"
#include "mini/core/Telemetry.hpp"

#include <iostream>

namespace mini
{

// ═════════════════════════════ ASSALTO ═════════════════════════════════════

void AssaultMode::start(World& world, Mesh* defaultMesh, Texture* texture,
                        const DefinitionRegistry* registry,
                        const MeshCache* meshCache)
{
    ConquestMode::start(world, defaultMesh, texture, registry, meshCache);
    // La mappa autora i post neutrali: in Assalto partono in mano ai NEMICI.
    m_commandPosts.forceAllOwners(world, 2);
    telemetry::logInfo("Assalto: " + std::to_string(m_commandPosts.count())
                       + " post da catturare, ticket alleati "
                       + std::to_string(m_team1Tickets));
    std::cout << "[Assalto] Cattura tutti i post prima di esaurire i ticket!\n";
}

void AssaultMode::updateObjectiveRules(World& /*world*/, float dt)
{
    if (m_commandPosts.count() == 0) return;

    // Pressione dell'attacco: i ticket alleati calano nel tempo.
    // Ogni post GIA' catturato allunga l'intervallo (progresso = respiro).
    m_bleedTimer += dt;
    const float interval = m_bleedInterval
                         * (1.0f + (float)m_commandPosts.countOwnedBy(1));
    if (m_bleedTimer < interval) return;
    m_bleedTimer -= interval;

    if (m_team1Tickets > 0)
    {
        --m_team1Tickets;
        std::cout << "[Assalto] La pressione costa: ticket alleati -> "
                  << m_team1Tickets << "\n";
    }
}

MatchOutcome AssaultMode::outcome(const World& world) const
{
    if (world.getTickCount() <= 10) return MatchOutcome::Ongoing;
    const int n = m_commandPosts.count();
    if (n > 0 && m_commandPosts.countOwnedBy(1) == n)
        return MatchOutcome::Team1Win;                 // tutti i post catturati
    if (m_team1Tickets <= 0)
        return MatchOutcome::Team2Win;                 // attacco esaurito
    return MatchOutcome::Ongoing;
}

// ═════════════════════════════ DIFESA ══════════════════════════════════════

void DefenseMode::start(World& world, Mesh* defaultMesh, Texture* texture,
                        const DefinitionRegistry* registry,
                        const MeshCache* meshCache)
{
    ConquestMode::start(world, defaultMesh, texture, registry, meshCache);
    // In Difesa i post partono in mano agli ALLEATI.
    m_commandPosts.forceAllOwners(world, 1);
    telemetry::logInfo("Difesa: " + std::to_string(m_commandPosts.count())
                       + " post da difendere, ticket nemici "
                       + std::to_string(m_team2Tickets));
    std::cout << "[Difesa] Resisti finche' i ticket nemici si esauriscono!\n";
}

void DefenseMode::updateObjectiveRules(World& /*world*/, float dt)
{
    if (m_commandPosts.count() == 0) return;

    // L'attacco nemico costa: i ticket nemici calano nel tempo.
    // Ogni post PERSO dagli alleati accorcia l'intervallo per i nemici
    // (piu' territorio → attacco piu' sostenibile).
    m_bleedTimer += dt;
    const float interval = m_bleedInterval
                         * (1.0f + (float)m_commandPosts.countOwnedBy(2));
    if (m_bleedTimer < interval) return;
    m_bleedTimer -= interval;

    if (m_team2Tickets > 0)
    {
        --m_team2Tickets;
        std::cout << "[Difesa] L'attacco nemico costa: ticket nemici -> "
                  << m_team2Tickets << "\n";
    }
}

MatchOutcome DefenseMode::outcome(const World& world) const
{
    if (world.getTickCount() <= 10) return MatchOutcome::Ongoing;
    const int n = m_commandPosts.count();
    if (n > 0 && m_commandPosts.countOwnedBy(2) == n)
        return MatchOutcome::Team2Win;                 // linea difensiva caduta
    if (m_team2Tickets <= 0)
    {
        bool enemyAlive = false;
        for (EntityId id : world.getEntities())
        {
            const auto* tm = world.getTeam(id);
            const auto* hp = world.getHealth(id);
            if (tm && tm->teamId == 2 && hp && hp->current > 0.0f
                && !world.getBullet(id))
            { enemyAlive = true; break; }
        }
        if (!enemyAlive) return MatchOutcome::Team1Win; // assalto respinto
    }
    return MatchOutcome::Ongoing;
}

} // namespace mini

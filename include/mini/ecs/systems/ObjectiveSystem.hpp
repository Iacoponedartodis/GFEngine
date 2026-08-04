#pragma once
#include "mini/ecs/ISystem.hpp"
#include "mini/ecs/Entity.hpp"

#include <string>
#include <vector>

namespace mini
{
struct ObjectiveDef;

// ObjectiveSystem (25_ObjectivesAndMissions, ADR-019) — Phase A.
// Gira DOPO Ai/Crowd: gli obiettivi valutano lo stato del mondo quando le unità
// si sono già mosse in questo tick. Legge la missione attiva dalla mailbox
// `World::activeMission`; senza missione non fa nulla — i mode esistenti
// (ADR-008/009/014) continuano a funzionare identici.
//
// Il framework è GENERICO per costruzione: nessun `if (missionId == ...)` e
// nessun `if (objectiveId == ...)`. Un obiettivo nuovo è un JSON, non codice.
class ObjectiveSystem : public ISystem
{
public:
    void update(World& world, float dt) override;
    const char* name() const override { return "objective"; }

    // Esito della missione secondo le regole DICHIARATE nel MissionDef.
    enum class Outcome { Ongoing, Success, Failure };
    [[nodiscard]] Outcome outcome() const { return m_outcome; }

    // Stato runtime di un obiettivo (per HUD/telemetria/debug).
    enum class State { Inactive, Active, Completed, Failed };
    struct Runtime
    {
        const ObjectiveDef* def   = nullptr;
        State               state = State::Inactive;
        float   elapsed  = 0.0f;   // tempo da quando è ATTIVO (per timeLimit)
        float   holdTime = 0.0f;   // presenza accumulata (HoldAreaForDuration)
        int     progress = 0;      // eliminazioni conteggiate (EliminateTarget)
        const char* failureReason = nullptr;   // mai nullptr se Failed
    };
    [[nodiscard]] const std::vector<Runtime>& objectives() const { return m_objs; }

private:
    void bind(World& world);          // risolve gli id → def (una volta per missione)
    bool evaluate(World& world, Runtime& r, float dt);   // true = completato
    void activateReady(World& world);
    [[nodiscard]] bool isComplete(const std::string& id) const;

    std::vector<Runtime> m_objs;
    const void* m_boundMission = nullptr;   // missione a cui m_objs è legato
    float   m_missionTime = 0.0f;
    Outcome m_outcome     = Outcome::Ongoing;
    bool    m_rejected    = false;   // gate: missione invalida → non parte
    // I sistemi SOPRAVVIVONO a World::initialize() (che azzera solo entità e
    // mailbox): al riavvio della partita la missione è lo stesso puntatore, quindi
    // il solo confronto con m_boundMission NON rileverebbe il restart e gli
    // obiettivi resterebbero "già completati". Il tick che torna indietro è il
    // segnale di riavvio — World::initialize() rimette m_tickCount a 0.
    std::uint64_t m_lastTick = 0;
};

} // namespace mini

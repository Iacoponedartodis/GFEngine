#pragma once
// MissionEditor (25_ObjectivesAndMissions, ADR-019) — authoring di missioni e
// obiettivi, incluse le CONSEGUENZE.
//
// Perché esiste: schema e runtime degli obiettivi sono in force dal 2026-07-15/16,
// ma si autoravano a mano nei JSON. La direttiva dell'utente (10_ProjectMemory,
// 2026-07-16) è netta: *"più cose posso modificare dall'editor meglio è; quello
// rimane lo strumento principale che IO posso usare"*. Il doc 25 stesso prevedeva
// "prima lo schema e il runtime, poi l'authoring".
//
// Vincoli non negoziabili rispettati qui:
//  - **Dropdown dal registry, mai id a testo libero** (CLAUDE.md): obiettivi dal
//    registry, mappa dal registry, post dalla mappa DELLA MISSIONE, tipi dagli enum.
//  - **saveJsonRMW** per ogni scrittura (ADR-010): si toccano solo i campi propri.
//  - **Rinomina via comando** (ADR-010), mai creando un file nuovo: `rename::Category
//    ::Objective` aggiorna anche le missioni che lo referenziano.
//  - id = filename stem (ADR-001): l'id non è un campo editabile del JSON.

#include "mini/game/data/DefinitionRegistry.hpp"

#include <string>
#include <vector>

namespace editor
{

class MissionEditor
{
public:
    MissionEditor();

    void draw();

private:
    // Due liste indipendenti: una missione COMPONE obiettivi che esistono di per sé
    // (un obiettivo può essere usato da più missioni). Rispecchia i due tipi di
    // definizione del registry.
    enum class Tab { Missions, Objectives };
    Tab m_tab = Tab::Missions;

    struct MissionEntry  { std::string id, jsonPath; mini::MissionDef  def; };
    struct ObjectiveEntry{ std::string id, jsonPath; mini::ObjectiveDef def; };

    std::vector<MissionEntry>   m_missions;
    std::vector<ObjectiveEntry> m_objectives;
    // Selezione del dropdown "aggiungi obiettivo": UNA PER LISTA. Erano uno static
    // condiviso dentro la lambda → i due dropdown si muovevano insieme (bug 07-16).
    int  m_addSelPrimary  = 0;
    int  m_addSelOptional = 0;
    int  m_selMission   = -1;
    int  m_selObjective = -1;

    std::string m_pendingSelectId;   // riselezione dopo rinomina/creazione
    std::string m_status;            // esito ultima operazione (mostrato in UI)

    mini::DefinitionRegistry m_registry;   // per i dropdown: armi, mappe, post...
    float m_listW = 210.0f;

    void reload();
    void drawMissionList();
    void drawMissionProps();
    void drawObjectiveList();
    void drawObjectiveProps();
    // Conseguenze: la parte che rende un obiettivo una MOSSA e non una casella.
    void drawConsequences(std::vector<mini::ConsequenceDef>& list,
                          const char* label, const std::string& mapIdForPosts);
    void saveMission(const MissionEntry& e);
    void saveObjective(const ObjectiveEntry& e);
    // Post della mappa indicata: il riferimento di CaptureZone/unlock_spawn è una
    // label, e deve venire da un dropdown della mappa giusta (mai testo libero).
    [[nodiscard]] std::vector<std::string> postsOfMap(const std::string& mapId) const;
};

} // namespace editor

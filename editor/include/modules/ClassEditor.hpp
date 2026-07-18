#pragma once
// ClassEditor (14_ClassSystem) — authoring delle classi.
//
// Chiude l'ultimo pezzo di contenuto che si autorava a mano nei JSON (direttiva
// utente 2026-07-16: l'editor è lo strumento principale).
//
// ⚠️ ADR-022 (Proposed): il GDD cap. 12 dice che una classe è una **professione
// militare, non una categoria di armi** — deve cambiare anche il comportamento
// (aiProfileId, role consumato). Il `ClassDef` odierno copre 1 dei 6 parametri del
// GDD. Questo modulo autora ciò che ESISTE oggi; quando ADR-022 sarà approvato,
// crescerà con lo schema invece di essere riscritto.

#include "mini/game/data/DefinitionRegistry.hpp"
#include <string>
#include <vector>

namespace editor
{

class ClassEditor
{
public:
    ClassEditor();
    void draw();

private:
    struct Entry { std::string id, jsonPath; mini::ClassDef def; };
    std::vector<Entry> m_entries;
    int m_sel = -1;
    int m_abilitySel = 0;   // selezione del dropdown "aggiungi abilita'"
    std::string m_pendingSelectId;
    std::string m_status;

    mini::DefinitionRegistry m_registry;   // dropdown: armi e abilità
    float m_listW = 210.0f;

    void reload();
    void save(const Entry& e);
    void drawList();
    void drawProps();
};

} // namespace editor

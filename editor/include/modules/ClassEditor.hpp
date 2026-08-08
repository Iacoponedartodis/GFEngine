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

    // ── Lavoro non salvato (doc 52 F3) ───────────────────────────────────
    // Questo modulo non aveva NESSUN rilevamento: le modifiche restano in memoria
    // fino al pulsante "Salva", e chiudere l'editor le buttava via in silenzio.
    // Il rilevamento è volutamente PRUDENTE — basta che un campo diventi attivo per
    // marcare il modulo come modificato, anche se poi il valore non cambia. Un falso
    // "vuoi salvare?" costa un clic; un falso "non c'è niente da salvare" costa il
    // lavoro. Fra i due errori possibili si sceglie quello che non fa danni.
    [[nodiscard]] bool hasUnsavedChanges() const { return m_dirty; }
    [[nodiscard]] std::string unsavedWhat() const
    { return (m_sel >= 0 && m_sel < (int)m_entries.size())
             ? ("la classe \"" + m_entries[m_sel].id + "\"") : std::string("una classe"); }
    void savePending()
    { if (m_dirty && m_sel >= 0 && m_sel < (int)m_entries.size()) save(m_entries[m_sel]); }

private:
    bool m_dirty = false;
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

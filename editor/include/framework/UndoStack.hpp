#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace editor
{

// ── PILA DI ANNULLAMENTO CONDIVISA (doc 52 F2, ADR-049) ─────────────────────
//
// Perché esiste: prima l'annullamento viveva in UN modulo su sette. Ctrl+Z non
// funzionava nemmeno nei tab strutture — dentro lo stesso Map Editor — perché la
// fotografia riguardava la mappa e non i tab. Scriverne un settimo a mano sarebbe
// stato ripetere l'errore invece di ripararlo.
//
// NON è un componente inventato da zero: è la semantica già matura del Map Editor,
// estratta e resa generica. Comprese le due regole che sembrano dettagli e non lo
// sono:
//   · **coalescenza per etichetta**: un trascinamento genera una modifica per frame,
//     e senza raggruppamento "annulla" tornerebbe indietro di un pixel per volta;
//   · **una nuova azione taglia il ramo di ripristino**, come in qualunque editor.
//
// NESSUNA callback memorizzata, di proposito. Un primo abbozzo teneva `capture` e
// `apply` come `std::function` che catturavano l'elemento del modulo: bastava
// aggiungere un tab perché il `vector` si riallocasse e quelle callback puntassero
// a memoria morta. Qui lo stato ENTRA e ESCE dai metodi, quindi il componente non
// possiede riferimenti a nulla e non può sopravvivere male a chi lo usa.
//
// Composizione, non ereditarietà (ADR-049): un modulo la possiede, non la eredita.
template <typename State>
class UndoStack
{
public:
    explicit UndoStack(std::size_t depth = 64) : m_depth(depth ? depth : 1) {}

    [[nodiscard]] bool canUndo() const { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.empty(); }
    [[nodiscard]] std::size_t undoCount() const { return m_undo.size(); }
    [[nodiscard]] std::size_t redoCount() const { return m_redo.size(); }

    // Da chiamare PRIMA di modificare, passando lo stato com'è adesso.
    // `tag` identifica il gesto: due push con lo stesso tag entro `window` secondi
    // contano come uno solo. `now` è l'orologio del modulo, in secondi — passarlo
    // invece di leggerlo qui tiene il componente fuori dal ciclo di frame e lo rende
    // collaudabile senza finestre.
    // `window` negativa = NIENTE coalescenza: il gesto è già concluso e va registrato
    // comunque. Serve a chi consegna una fotografia presa prima (il gancio sui widget
    // del Map Editor): fonderla col gesto precedente perderebbe uno stato intermedio
    // che l'utente ha effettivamente attraversato.
    void push(const State& current, const char* tag, float now, float window = 0.6f)
    {
        if (window >= 0.0f && m_lastTag == (tag ? tag : "")
            && (now - m_lastTime) < window)
        { m_lastTime = now; return; }
        m_lastTag  = tag ? tag : "";
        m_lastTime = now;

        m_undo.push_back(current);
        if (m_undo.size() > m_depth) m_undo.erase(m_undo.begin());
        m_redo.clear();   // una nuova azione taglia il ramo di ripristino
    }

    // `state` entra con lo stato attuale ed esce con quello precedente.
    bool undo(State& state)
    {
        if (m_undo.empty()) return false;
        m_redo.push_back(state);
        state = m_undo.back();
        m_undo.pop_back();
        // Il prossimo push non deve fondersi col gesto appena annullato.
        m_lastTag.clear();
        return true;
    }

    bool redo(State& state)
    {
        if (m_redo.empty()) return false;
        m_undo.push_back(state);
        state = m_redo.back();
        m_redo.pop_back();
        m_lastTag.clear();
        return true;
    }

    void clear() { m_undo.clear(); m_redo.clear(); m_lastTag.clear(); m_lastTime = -1e9f; }

private:
    std::vector<State> m_undo, m_redo;
    std::size_t m_depth = 64;
    std::string m_lastTag;
    float       m_lastTime = -1e9f;
};

} // namespace editor

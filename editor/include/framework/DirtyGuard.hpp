#pragma once
#include <functional>
#include <string>
#include <vector>

namespace editor
{

// ── GUARDIA DEL LAVORO NON SALVATO (doc 52 F3) ──────────────────────────────
//
// Perché esiste: **cinque moduli su sette** tengono uno stato "modificato", e
// **uno solo** lo dichiarava. Uscendo da GFEditor con modifiche in Entity, Weapon,
// Vehicle o Balance Editor, quelle modifiche sparivano **in silenzio** — lo stesso
// difetto che era stato riparato per il solo Map Editor due giorni prima, cioè in
// un quinto dei casi.
//
// *Un avviso che vale in un posto solo è peggio di nessun avviso*, perché insegna a
// fidarsi: se l'editor avvisa quando esci dalla mappa, dai per scontato che avvisi
// sempre.
//
// I registranti si dichiarano AL MOMENTO DELLA DOMANDA, non una volta per sempre:
// niente callback che sopravvivono a chi le ha date. Stessa disciplina di
// `UndoStack` e `ViewportEditing`, e per lo stesso motivo (un `std::function` che
// cattura un modulo distrutto è un puntatore penzolante che nessuno vede finché non
// esplode).
class DirtyGuard
{
public:
    struct Source
    {
        std::string          what;   // "la mappa \"Training Ground\"", "3 armi"
        bool                 dirty = false;
        std::function<void()> save;  // vuota = non salvabile a comando
    };

    void clear() { m_sources.clear(); }
    void add(Source s) { if (s.dirty) m_sources.push_back(std::move(s)); }

    [[nodiscard]] bool any() const { return !m_sources.empty(); }
    // Tutti sanno salvarsi? Se no, l'uscita non deve offrire "Salva ed esci": una
    // promessa mantenuta a metà è peggio di un'offerta assente.
    [[nodiscard]] bool allSaveable() const
    {
        for (const auto& s : m_sources) if (!s.save) return false;
        return true;
    }

    // "la mappa X, 2 tipi di struttura e 1 entità" — l'elenco in chiaro, perché
    // "ci sono modifiche non salvate" non dice DOVE andare a guardare.
    [[nodiscard]] std::string summary() const
    {
        std::string out;
        for (std::size_t i = 0; i < m_sources.size(); ++i)
        {
            if (i > 0) out += (i + 1 == m_sources.size()) ? " e " : ", ";
            out += m_sources[i].what;
        }
        return out;
    }

    void saveAll() const { for (const auto& s : m_sources) if (s.save) s.save(); }

private:
    std::vector<Source> m_sources;
};

} // namespace editor

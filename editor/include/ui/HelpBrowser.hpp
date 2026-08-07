#pragma once
#include <string>
#include <vector>

namespace editor
{

// ── GUIDA IN-EDITOR (richiesta utente 2026-08-06) ────────────────────────────
// *"una sezione nel menu in alto per accedere ad una pagina con documentazione
// dettagliata delle funzionalità dell'editor, tipo tutorial, diviso per moduli e
// capitoli per navigarci facilmente"*.
//
// Perché serve davvero, non è un vezzo: gli assemblaggi erano implementati,
// collaudati e documentati in ProjectDocs — e l'utente non li ha trovati. Una
// funzione che esiste ma che nessuno sa invocare non esiste. ProjectDocs spiega
// PERCHÉ le cose sono come sono (è per me); questa spiega COME si usano (è per chi
// costruisce). Sono due documenti diversi e non si sostituiscono.
//
// I contenuti vivono in `data/help/*.md`, uno per capitolo: si correggono senza
// ricompilare, e chi tocca una funzione tocca il file accanto. L'ordine dei capitoli
// è dato dal prefisso numerico del nome file.
class HelpBrowser
{
public:
    void draw(bool* open);
    void reload();          // rilegge i file dal disco
    // Per il collaudo: quanti capitoli e quante sezioni sono stati letti. Serve a
    // verificare che il PERCORSO sia giusto — una guida che non trova i suoi file
    // mostra "nessun contenuto" e sembra un problema di contenuti, non di path.
    [[nodiscard]] int chapterCount() const { return (int)m_chapters.size(); }
    [[nodiscard]] int sectionCount() const;

private:
    struct Section { std::string title; std::string body; };
    struct Chapter
    {
        std::string file;              // nome file, per l'ordinamento
        std::string title;             // prima riga "# ..."
        std::vector<Section> sections; // ogni "## ..."
    };
    std::vector<Chapter> m_chapters;
    int  m_sel = 0;
    bool m_loaded = false;
    char m_filter[64] = "";

    void ensureLoaded();
    static void drawMarkdownLite(const std::string& text);
};

} // namespace editor

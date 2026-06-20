#pragma once
#include "mini/render/Ui2D.hpp"
#include "mini/core/InputManager.hpp"

namespace mini
{

// Menu opzioni generale del gioco.
// Aperto con O da FreeRoam o dal menu di pausa.
//
// Struttura a pagine:
//   Root      → elenco categorie (Controlli, [Audio], [Video] in futuro)
//   Controls  → editor keybinding: rimappa i tasti su InputManager
//
// A differenza del vecchio OptionsMenu, NON gestisce piu' le regole di gioco
// o i preset: quelli appartengono a PreMatchMenu. Qui stanno solo le opzioni
// generali (input, e in futuro audio/video/grafica).
class OptionsMenu
{
public:
    OptionsMenu(int screenW, int screenH);

    enum class Result { None, Back };

    // input: serve per leggere/scrivere i keybinding correnti
    Result handleKey(int sdlScancode, InputManager& input);
    void   render(const InputManager& input) const;

private:
    Ui2D m_ui;

    enum class Page { Root, Controls };
    Page m_page = Page::Root;

    int  m_rootRow     = 0;   // categoria selezionata in Root
    int  m_controlRow  = 0;   // azione selezionata in Controls
    bool m_awaitingKey = false; // true = in attesa del nuovo tasto da assegnare

    // ── Handler per pagina ───────────────────────────────────────────
    Result handleRoot(int sc);
    Result handleControls(int sc, InputManager& input);

    // ── Render per pagina ────────────────────────────────────────────
    void renderRoot()                          const;
    void renderControls(const InputManager& in) const;
};

} // namespace mini
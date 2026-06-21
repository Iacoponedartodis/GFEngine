#include "mini/render/MainMenuScreen.hpp"
#include <SDL2/SDL.h>

namespace mini
{

MainMenuScreen::MainMenuScreen(int w, int h) : m_ui(w, h) {}

MainMenuScreen::Result MainMenuScreen::handleKey(int sc)
{
    constexpr int ITEMS = 3;

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    { m_selected = (m_selected - 1 + ITEMS) % ITEMS; return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_selected = (m_selected + 1) % ITEMS; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        switch (m_selected)
        {
        case 0: return Result::NewGame;
        case 1: return Result::Options;
        case 2: return Result::Quit;
        }
    }

    if (sc == SDL_SCANCODE_ESCAPE)
        return Result::Quit;

    return Result::None;
}

void MainMenuScreen::render() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f;

    m_ui.begin();

    m_ui.rect(0, 0, W, H, 0.04f, 0.05f, 0.10f);

    // Titolo
    m_ui.textCentered(cx, H * 0.12f, 5.5f, "GFENGINE", 0.85f, 0.75f, 0.3f);
    m_ui.textCentered(cx, H * 0.12f + 50, 1.8f, "Main Menu", 0.5f, 0.5f, 0.55f);

    // Voci menu
    struct Item { const char* label; };
    const Item items[] = {
        {"Nuova Partita"},
        {"Opzioni"},
        {"Esci"},
    };
    constexpr int N = 3;

    const float startY = H * 0.38f;
    const float rowH   = 68.0f;

    for (int i = 0; i < N; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_selected);
        const float bx = cx - 180, bw = 360, bh = 52;

        m_ui.rect(bx, y, bw, bh,
                  sel ? 0.14f : 0.07f, sel ? 0.25f : 0.07f, sel ? 0.45f : 0.09f,
                  sel ? 0.8f : 0.5f);
        m_ui.border(bx, y, bw, bh,
                    sel ? 0.5f : 0.2f, sel ? 0.6f : 0.2f, sel ? 0.8f : 0.3f);

        m_ui.textCentered(cx, y + 14, sel ? 2.8f : 2.2f,
                          items[i].label,
                          sel ? 1.0f : 0.6f, sel ? 0.95f : 0.6f, sel ? 0.5f : 0.6f);
    }

    // Istruzioni
    m_ui.rect(0, H - 42, W, 42, 0, 0, 0, 0.5f);
    m_ui.textCentered(cx, H - 32, 1.7f,
                      "INVIO = seleziona   |   SU/GIU = naviga   |   ESC = esci",
                      0.5f, 0.5f, 0.55f);

    m_ui.end();
}

} // namespace mini
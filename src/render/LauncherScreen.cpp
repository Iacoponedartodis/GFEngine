#include "mini/render/LauncherScreen.hpp"
#include <SDL2/SDL.h>

namespace mini
{

LauncherScreen::LauncherScreen(int w, int h) : m_ui(w, h) { loadVersions(); }

void LauncherScreen::loadVersions()
{
    // Per ora hardcoded — in futuro caricherà da data/versions.json
    m_versions = {
        {"Stable",  "Build 0.1 — Release candidate", "config/stable"},
        {"Dev",     "Build 0.1-dev — Latest changes", "config/dev"},
    };
}

LauncherScreen::Result LauncherScreen::handleKey(int sc)
{
    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    { m_selected = (m_selected - 1 + (int)m_versions.size()) % (int)m_versions.size(); return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_selected = (m_selected + 1) % (int)m_versions.size(); return Result::None; }
    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
        return Result::Launch;
    if (sc == SDL_SCANCODE_ESCAPE)
        return Result::Quit;
    return Result::None;
}

void LauncherScreen::render() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f;

    m_ui.begin();

    // Sfondo
    m_ui.rect(0, 0, W, H, 0.03f, 0.04f, 0.08f);

    // Titolo
    m_ui.textCentered(cx, H * 0.15f, 6.0f, "GFENGINE", 0.85f, 0.75f, 0.3f);
    m_ui.textCentered(cx, H * 0.15f + 55, 2.0f, "Clone Wars Tactical Shooter", 0.6f, 0.6f, 0.7f);

    // Linea separatrice
    m_ui.rect(cx - 200, H * 0.35f, 400, 1, 0.3f, 0.3f, 0.4f);

    // Selezione versione
    m_ui.textCentered(cx, H * 0.40f, 2.2f, "Seleziona versione:", 0.7f, 0.7f, 0.7f);

    const float startY = H * 0.48f;
    const float rowH   = 64.0f;

    for (int i = 0; i < (int)m_versions.size(); ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_selected);
        const float bx = cx - 220, bw = 440, bh = 52;

        m_ui.rect(bx, y, bw, bh,
                  sel ? 0.12f : 0.06f, sel ? 0.22f : 0.06f, sel ? 0.42f : 0.08f,
                  sel ? 0.8f : 0.5f);
        m_ui.border(bx, y, bw, bh,
                    sel ? 0.5f : 0.2f, sel ? 0.6f : 0.2f, sel ? 0.8f : 0.3f);

        m_ui.textCentered(cx, y + 10, sel ? 2.5f : 2.0f,
                          m_versions[i].label.c_str(),
                          sel ? 1.0f : 0.6f, sel ? 0.95f : 0.6f, sel ? 0.5f : 0.6f);
        m_ui.textCentered(cx, y + 32, 1.4f,
                          m_versions[i].description.c_str(),
                          0.5f, 0.5f, 0.55f);
    }

    // Istruzioni
    const float bottomY = H - 50;
    m_ui.rect(0, bottomY - 8, W, 50, 0, 0, 0, 0.5f);
    m_ui.textCentered(cx, bottomY, 1.8f,
                      "INVIO = Avvia   |   SU/GIU = seleziona   |   ESC = esci",
                      0.55f, 0.55f, 0.6f);

    m_ui.end();
}

} // namespace mini
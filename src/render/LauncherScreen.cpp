#include "mini/render/LauncherScreen.hpp"
#include <SDL2/SDL.h>

namespace mini
{

LauncherScreen::LauncherScreen(int w, int h)
    : m_ui(w, h)
{}

LauncherScreen::Result LauncherScreen::handleKey(int sc)
{
    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE)
        return Result::Launch;

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_Q)
        return Result::Quit;

    return Result::None;
}

void LauncherScreen::render() const
{
    const float W  = (float)m_ui.width();
    const float H  = (float)m_ui.height();
    const float cx = W * 0.5f;

    m_ui.begin();

    // Sfondo
    m_ui.rect(0, 0, W, H, 0.03f, 0.04f, 0.08f, 1.0f);

    // Titolo
    m_ui.textCentered(cx, H * 0.28f, 6.0f, "GFENGINE", 0.85f, 0.75f, 0.3f);
    m_ui.textCentered(cx, H * 0.28f + 58, 2.0f, "v0.1 — alpha", 0.45f, 0.45f, 0.5f);

    // Separatore
    m_ui.rect(cx - 180, H * 0.50f - 2, 360, 1, 0.25f, 0.25f, 0.35f);

    // Pulsante AVVIA
    const float bw = 280, bh = 50;
    const float bx = cx - bw * 0.5f;
    const float by = H * 0.54f;

    m_ui.rect(bx, by, bw, bh, 0.10f, 0.25f, 0.48f, 0.85f);
    m_ui.border(bx, by, bw, bh, 0.3f, 0.5f, 0.8f);
    m_ui.textCentered(cx, by + 14, 2.4f, "AVVIA", 1.0f, 1.0f, 1.0f);

    // Hint tasti
    const float bottomY = H - 44;
    m_ui.rect(0, bottomY - 8, W, 52, 0.0f, 0.0f, 0.0f, 0.55f);
    m_ui.textCentered(cx, bottomY, 1.8f,
                      "INVIO / SPAZIO = avvia   |   ESC = esci",
                      0.4f, 0.4f, 0.45f);

    m_ui.end();
}

} // namespace mini
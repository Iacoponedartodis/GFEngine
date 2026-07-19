#include "mini/render/OptionsMenu.hpp"
#include "mini/platform/OpenGL.hpp"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>

namespace mini
{

OptionsMenu::OptionsMenu(int screenW, int screenH)
    : m_ui(screenW, screenH) {}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch
// ─────────────────────────────────────────────────────────────────────────────

OptionsMenu::Result OptionsMenu::handleKey(int sc, InputManager& input)
{
    switch (m_page)
    {
    case Page::Root:     return handleRoot(sc);
    case Page::Controls: return handleControls(sc, input);
    }
    return Result::None;
}

// ── Root: elenco categorie ───────────────────────────────────────────────
OptionsMenu::Result OptionsMenu::handleRoot(int sc)
{
    constexpr int ITEMS = 1; // 0 = Controlli (in futuro: Audio, Video...)

    if (sc == SDL_SCANCODE_UP   || sc == SDL_SCANCODE_W)
    { m_rootRow = (m_rootRow - 1 + ITEMS) % ITEMS; return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_rootRow = (m_rootRow + 1) % ITEMS; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        if (m_rootRow == 0) { m_page = Page::Controls; m_controlRow = 0; }
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
        return Result::Back;

    return Result::None;
}

// ── Controls: editor keybinding ──────────────────────────────────────────
void OptionsMenu::assignAwaited(InputBinding b, InputManager& input)
{
    if (!m_awaitingKey || m_page != Page::Controls) return;
    input.rebind(InputManager::rebindableAt(m_controlRow), b);
    m_awaitingKey = false;
}

OptionsMenu::Result OptionsMenu::handleControls(int sc, InputManager& input)
{
    const int count = InputManager::rebindableCount();

    // Se siamo in attesa di un nuovo tasto, il prossimo scancode lo assegna
    if (m_awaitingKey)
    {
        if (sc == SDL_SCANCODE_ESCAPE)
        {
            m_awaitingKey = false; // annulla
            return Result::None;
        }
        // Assegna il nuovo tasto all'azione selezionata
        Action a = InputManager::rebindableAt(m_controlRow);
        input.rebind(a, InputBinding::key((SDL_Scancode)sc));
        m_awaitingKey = false;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_UP   || sc == SDL_SCANCODE_W)
    { m_controlRow = (m_controlRow - 1 + count) % count; return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_controlRow = (m_controlRow + 1) % count; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        m_awaitingKey = true; // entra in modalità "premi un tasto"
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    { m_page = Page::Root; return Result::None; }

    return Result::None;
}

// ── Mouse ─────────────────────────────────────────────────────────────────
// Geometrie identiche a renderRoot/renderControls.
OptionsMenu::Result OptionsMenu::handleMouse(float mx, float my, bool clicked)
{
    const float W = (float)m_ui.width(), H = (float)m_ui.height();
    const float cx = W * 0.5f, cy = H * 0.5f;

    // Pulsante "Indietro" (controparte di ESC): Controls → Root, Root → Back.
    if (mx >= 20 && mx <= 160 && my >= 18 && my <= 52)
    {
        if (clicked)
        {
            if (m_page == Page::Controls) { m_page = Page::Root; return Result::None; }
            return Result::Back;
        }
        return Result::None;
    }

    if (m_page == Page::Root)   // renderRoot: startY=cy-30, box cx-240 w480 (y-4,h44)
    {
        const float y = cy - 30.0f, bx = cx - 240.0f;
        if (mx >= bx && mx <= bx + 480 && my >= y - 4 && my <= y + 40)
        {
            m_rootRow = 0;
            if (clicked) { m_page = Page::Controls; m_controlRow = 0; }
        }
    }
    else                        // Controls: startY=78, rowH=42, riga 48..648 (y-5,h38)
    {
        if (m_awaitingKey) return Result::None;   // in attesa: il click lo cattura assignAwaited
        const int count = InputManager::rebindableCount();
        const float startY = 78.0f, rowH = 42.0f, labelX = 60.0f;
        for (int i = 0; i < count; ++i)
        {
            const float y = startY + i * rowH;
            if (mx < labelX - 12 || mx > labelX - 12 + 600
                || my < y - 5 || my > y - 5 + (rowH - 4)) continue;
            m_controlRow = i;
            if (clicked) m_awaitingKey = true;   // click = rimappa, poi premi il nuovo input
            break;
        }
    }
    return Result::None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render
// ─────────────────────────────────────────────────────────────────────────────

void OptionsMenu::render(const InputManager& input) const
{
    m_ui.begin();
    switch (m_page)
    {
    case Page::Root:     renderRoot();           break;
    case Page::Controls: renderControls(input);  break;
    }

    // Pulsante "Indietro" (controparte mouse di ESC), in alto a sinistra.
    m_ui.rect(20, 18, 140, 34, 0.10f, 0.12f, 0.18f, 0.90f);
    m_ui.border(20, 18, 140, 34, 0.40f, 0.50f, 0.70f);
    m_ui.text(34, 27, 1.7f, "< Indietro", 0.80f, 0.85f, 0.95f);

    m_ui.end();
}

void OptionsMenu::renderRoot() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f, cy = H * 0.5f;

    m_ui.rect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.82f);
    m_ui.textCentered(cx, cy - 130, 3.5f, "OPZIONI", 0.95f, 0.85f, 0.3f);

    struct Item { const char* label; };
    const Item items[] = { { "Controlli  (tastiera e mouse)" } };
    constexpr int N = 1;

    const float startY = cy - 30.0f;
    const float rowH   = 58.0f;

    for (int i = 0; i < N; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_rootRow);
        const float bx = cx - 240, bw = 480, bh = 44;

        m_ui.rect(bx, y - 4, bw, bh,
                  sel ? 0.12f : 0.08f, sel ? 0.28f : 0.08f, sel ? 0.50f : 0.10f,
                  sel ? 0.65f : 0.45f);
        m_ui.border(bx, y - 4, bw, bh, 0.25f, 0.35f, 0.55f);

        float scale = sel ? 2.4f : 2.0f;
        float c = sel ? 1.0f : 0.6f;
        m_ui.textCentered(cx, y + 10, scale, items[i].label, c * 0.5f, c, c);
    }

    m_ui.textCentered(cx, startY + N * rowH + 24, 1.7f,
                      "SU/GIU = naviga   INVIO = apri   ESC = indietro",
                      0.5f, 0.5f, 0.5f);
}

void OptionsMenu::renderControls(const InputManager& input) const
{
    const float W = (float)m_ui.width();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, (float)m_ui.height(), 0.0f, 0.0f, 0.0f, 0.85f);
    m_ui.textCentered(cx, 28, 3.0f, "CONTROLLI", 0.95f, 0.85f, 0.3f);

    const int count = InputManager::rebindableCount();
    const float startY = 78.0f;
    const float rowH   = 42.0f;
    // Colonna sinistra: rimappabili. Colonna destra: tasti fissi (audit).
    const float labelX = 60.0f;
    const float keyX   = 400.0f;

    for (int i = 0; i < count; ++i)
    {
        Action a = InputManager::rebindableAt(i);
        const float y = startY + i * rowH;
        const bool sel = (i == m_controlRow);

        if (sel)
            m_ui.rect(labelX - 12, y - 5, 600.0f, rowH - 4,
                      0.15f, 0.32f, 0.55f, 0.55f);

        // Nome azione
        m_ui.text(labelX, y + 6, 1.9f, InputManager::actionName(a),
                  sel ? 1.0f : 0.8f, sel ? 0.95f : 0.8f, sel ? 0.5f : 0.8f);

        // Input attualmente assegnato: getKeyName descrive OGNI tipo (tasto,
        // pulsante mouse, rotella). Prima usava getScancode+SDL_GetScancodeName,
        // che su un binding mouse/rotella dava UNKNOWN → nome vuoto ("—").
        const char* keyName = input.getKeyName(a);
        char keyBuf[48];

        if (sel && m_awaitingKey)
        {
            std::snprintf(keyBuf, sizeof(keyBuf), "[ tasto / mouse / rotella... ]");
            m_ui.rect(keyX - 8, y - 2, 230, 30, 0.4f, 0.2f, 0.05f, 0.7f);
            m_ui.text(keyX, y + 6, 1.7f, keyBuf, 1.0f, 0.7f, 0.2f);
        }
        else
        {
            std::snprintf(keyBuf, sizeof(keyBuf), "%s",
                          (keyName && keyName[0]) ? keyName : "—");
            m_ui.rect(keyX - 8, y - 2, 230, 30, 0.12f, 0.12f, 0.15f, 0.8f);
            m_ui.border(keyX - 8, y - 2, 230, 30, 0.3f, 0.35f, 0.45f);
            m_ui.text(keyX, y + 6, 1.8f, keyBuf,
                      sel ? 1.0f : 0.85f, sel ? 1.0f : 0.85f, sel ? 0.6f : 0.85f);
        }
    }

    // Voci fisse: mouse + tasti di sistema non rimappabili. Tenere questa
    // lista allineata alle azioni reali di Application (audit 2026-07-10).
    struct FixedRow { const char* label; const char* key; };
    const FixedRow fixed[] = {
        { "Sparo",                    "Mouse Sinistro" },
        { "Mira (ADS)",               "Mouse Destro"   },
        { "Prima/terza persona",      "V"   },
        { "Sali/scendi dal veicolo",  "E"   },
        { "Log eventi (scorri)",      "L (PAGSU/PAGGIU)" },
        { "Sandbox: menu prova",      "TAB" },
        { "Sandbox: partita (PreMatch)", "P" },
        { "Sandbox: armi rapide",     "1-9" },
        { "Dump stato (telemetria)",  "F12" },
        { "Schermo intero",           "F11" },
    };
    // Colonna DESTRA (11 rimappabili + 10 fisse non stanno in una colonna)
    constexpr int kFixedCount = (int)(sizeof(fixed) / sizeof(fixed[0]));
    const float fLabelX = W * 0.62f;
    const float fKeyX   = W * 0.86f;
    m_ui.text(fLabelX, startY - 26, 1.6f, "Tasti fissi:", 0.55f, 0.55f, 0.6f);
    for (int i = 0; i < kFixedCount; ++i)
    {
        const float fy = startY + i * (rowH - 6.0f);
        m_ui.text(fLabelX, fy + 6, 1.6f, fixed[i].label, 0.5f, 0.5f, 0.5f);
        m_ui.text(fKeyX,   fy + 6, 1.6f, fixed[i].key,   0.6f, 0.6f, 0.65f);
    }

    const float ly = startY + count * rowH + 12;
    m_ui.rect(0, ly - 8, W, 64, 0, 0, 0, 0.55f);
    if (m_awaitingKey)
    {
        m_ui.textCentered(cx, ly, 1.7f,
                          "Premi tasto, pulsante mouse o rotella   (ESC = annulla)",
                          1.0f, 0.7f, 0.2f);
    }
    else
    {
        m_ui.textCentered(cx, ly, 1.6f,
                          "SU/GIU = naviga   INVIO = rimappa tasto", 0.6f, 0.6f, 0.6f);
        m_ui.textCentered(cx, ly + 20, 1.6f,
                          "ESC = torna alle opzioni", 0.6f, 0.6f, 0.6f);
    }
}

} // namespace mini
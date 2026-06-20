#pragma once

namespace mini
{

// Primitive di rendering 2D condivise per HUD e menu.
// Estrae il codice fixed-function GL duplicato in HUD, OptionsMenu, PreMatchMenu.
//
// Uso tipico:
//   Ui2D ui(screenW, screenH);
//   ui.begin();
//   ui.rect(x, y, w, h, r, g, b, a);
//   ui.text(x, y, scale, "Ciao", r, g, b);
//   ui.end();
class Ui2D
{
public:
    Ui2D(int screenW, int screenH) : m_w(screenW), m_h(screenH) {}

    void setSize(int w, int h) { m_w = w; m_h = h; }
    [[nodiscard]] int width()  const { return m_w; }
    [[nodiscard]] int height() const { return m_h; }

    // Entra/esce dalla modalità 2D ortografica
    void begin() const;
    void end()   const;

    // Disegna un rettangolo riempito
    void rect(float x, float y, float w, float h,
              float r, float g, float b, float a = 1.0f) const;

    // Disegna testo (stb_easy_font)
    void text(float x, float y, float scale, const char* str,
              float r, float g, float b) const;

    // Helper: cornice (4 bordi) attorno a un rettangolo
    void border(float x, float y, float w, float h,
                float r, float g, float b, float thickness = 1.0f) const;

    // Helper: testo centrato orizzontalmente attorno a cx
    void textCentered(float cx, float y, float scale, const char* str,
                      float r, float g, float b) const;

    // Stima la larghezza in pixel di un testo (per centrare)
    [[nodiscard]] float textWidth(const char* str, float scale) const;

private:
    int m_w, m_h;
};

} // namespace mini
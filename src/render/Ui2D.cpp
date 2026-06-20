#include "mini/render/Ui2D.hpp"
#include "mini/platform/OpenGL.hpp"

#include <stb_easy_font.h>
#include <cstring>

namespace mini
{

void Ui2D::begin() const
{
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0, m_w, m_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
}

void Ui2D::end() const
{
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Ui2D::rect(float x, float y, float w, float h,
                float r, float g, float b, float a) const
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);     glVertex2f(x+w, y);
    glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

void Ui2D::text(float x, float y, float scale, const char* str,
                float r, float g, float b) const
{
    static char buf[131072];
    int quads = stb_easy_font_print(0, 0, const_cast<char*>(str), nullptr, buf, sizeof(buf));
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glColor3f(r, g, b);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 16, buf);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

void Ui2D::border(float x, float y, float w, float h,
                  float r, float g, float b, float t) const
{
    rect(x,       y,       w, t, r, g, b);  // top
    rect(x,       y+h-t,   w, t, r, g, b);  // bottom
    rect(x,       y,       t, h, r, g, b);  // left
    rect(x+w-t,   y,       t, h, r, g, b);  // right
}

float Ui2D::textWidth(const char* str, float scale) const
{
    return stb_easy_font_width(const_cast<char*>(str)) * scale;
}

void Ui2D::textCentered(float cx, float y, float scale, const char* str,
                        float r, float g, float b) const
{
    text(cx - textWidth(str, scale) * 0.5f, y, scale, str, r, g, b);
}

} // namespace mini
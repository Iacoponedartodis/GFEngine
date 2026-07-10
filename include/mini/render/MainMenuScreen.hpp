#pragma once
#include "mini/render/Ui2D.hpp"

namespace mini
{

class MainMenuScreen
{
public:
    MainMenuScreen(int screenW, int screenH);

    enum class Result { None, NewGame, Options, Quit };

    Result handleKey(int sdlScancode);
    // Mouse in coordinate finestra: hover evidenzia la voce, click la attiva.
    Result handleMouse(float mx, float my, bool clicked);
    void   render() const;

private:
    Ui2D m_ui;
    int  m_selected = 0;
};

} // namespace mini
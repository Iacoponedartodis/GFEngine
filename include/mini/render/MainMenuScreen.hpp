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
    void   render() const;

private:
    Ui2D m_ui;
    int  m_selected = 0;
};

} // namespace mini
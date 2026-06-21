#pragma once
#include "mini/render/Ui2D.hpp"

namespace mini
{

class LauncherScreen
{
public:
    LauncherScreen(int screenW, int screenH);

    enum class Result { None, Launch, Quit };

    Result handleKey(int sdlScancode);
    void   render() const;

private:
    Ui2D m_ui;
};

} // namespace mini
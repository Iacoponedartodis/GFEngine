#pragma once

namespace mini
{

enum class GameState : int
{
    Launcher  = -5,
    MainMenu  = -4,
    PreMatch  = -3,
    Options   = -2,
    Paused    = -1,
    Playing   =  0,
    Win       =  1,
    Lose      =  2,
};

inline bool isMenuState(GameState s)
{
    return s == GameState::Launcher || s == GameState::MainMenu
        || s == GameState::PreMatch || s == GameState::Options;
}

inline bool isOverlayState(GameState s)
{
    return s == GameState::Paused || s == GameState::Win || s == GameState::Lose;
}

} // namespace mini
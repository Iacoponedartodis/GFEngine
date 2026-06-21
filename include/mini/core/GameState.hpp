#pragma once

namespace mini
{

enum class GameState : int
{
    Launcher  = -6,   // schermata iniziale con titolo e selezione versione
    MainMenu  = -5,   // menu principale (Nuova Partita / Opzioni / Esci)
    PreMatch  = -4,   // menu pre-partita (regole/preset)
    Options   = -3,   // menu opzioni (keybinding)
    Paused    = -2,   // pausa in-game
    FreeRoam  = -1,   // volo libero pre-partita
    Playing   =  0,   // partita in corso
    Win       =  1,   // vittoria
    Lose      =  2,   // sconfitta
};

inline bool isMenuState(GameState s)
{
    return s == GameState::Launcher || s == GameState::MainMenu
        || s == GameState::PreMatch || s == GameState::Options;
}

inline bool isGameplayState(GameState s)
{
    return s == GameState::Playing || s == GameState::FreeRoam;
}

inline bool isOverlayState(GameState s)
{
    return s == GameState::Paused || s == GameState::Win || s == GameState::Lose;
}

} // namespace mini
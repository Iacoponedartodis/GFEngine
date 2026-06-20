#pragma once

#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>

namespace mini
{

// Azioni logiche — il resto del codice usa queste, MAI scancode diretti.
enum class Action : int
{
    MoveForward, MoveBack, MoveLeft, MoveRight,
    Jump, Shoot, Reload,
    Pause,           // toggle pausa
    Restart,         // riavvia partita
    FreeRoam,        // torna a volo libero
    StartGame,       // ENTER per iniziare
    ToggleMouse,     // TAB
    Quit,            // ESC in free roam
    COUNT
};

class InputManager
{
public:
    InputManager();

    // Chiama una volta per frame, PRIMA di usare isPressed/isDown
    void update();

    // Processa un singolo SDL_Event (per eventi discreti come click)
    void processEvent(const SDL_Event& event);

    // isDown: il tasto è premuto in questo frame (continuous)
    [[nodiscard]] bool isDown(Action a) const;

    // isPressed: il tasto è stato premuto QUESTO frame (single-shot, no repeat)
    [[nodiscard]] bool isPressed(Action a) const;

    // Mouse
    [[nodiscard]] bool isShootClicked() const { return m_shootClicked; }
    [[nodiscard]] int  mouseDX() const { return m_mdx; }
    [[nodiscard]] int  mouseDY() const { return m_mdy; }

    // Rimappa un'azione a un nuovo scancode
    void rebind(Action action, SDL_Scancode scancode);

    // Restituisce il nome del tasto per un'azione (per UI)
    [[nodiscard]] const char* getKeyName(Action a) const;

private:
    // Mappa Action → SDL_Scancode
    std::unordered_map<int, SDL_Scancode> m_bindings;

    // Stato tasti: frame corrente e precedente
    const Uint8* m_keyState = nullptr;
    Uint8 m_prev[SDL_NUM_SCANCODES] = {};

    bool m_shootClicked = false;
    int  m_mdx = 0, m_mdy = 0;
};

} // namespace mini
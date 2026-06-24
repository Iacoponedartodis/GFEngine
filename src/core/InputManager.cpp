#include "mini/core/InputManager.hpp"
#include <cstring>

namespace mini
{

InputManager::InputManager()
{
    // ── Binding di default ────────────────────────────────────────────
    m_bindings[(int)Action::MoveForward]  = SDL_SCANCODE_W;
    m_bindings[(int)Action::MoveBack]     = SDL_SCANCODE_S;
    m_bindings[(int)Action::MoveLeft]     = SDL_SCANCODE_A;
    m_bindings[(int)Action::MoveRight]    = SDL_SCANCODE_D;
    m_bindings[(int)Action::Jump]         = SDL_SCANCODE_SPACE;
    m_bindings[(int)Action::Shoot]        = SDL_SCANCODE_UNKNOWN; // mouse sinistro
    m_bindings[(int)Action::Reload]       = SDL_SCANCODE_R;
    m_bindings[(int)Action::Aim]          = SDL_SCANCODE_UNKNOWN; // mouse destro
    m_bindings[(int)Action::Sprint]       = SDL_SCANCODE_LSHIFT;
    m_bindings[(int)Action::Crouch]       = SDL_SCANCODE_LCTRL;
    m_bindings[(int)Action::Roll]         = SDL_SCANCODE_Q;
    m_bindings[(int)Action::SwitchWeapon] = SDL_SCANCODE_F;
    m_bindings[(int)Action::Pause]        = SDL_SCANCODE_ESCAPE;
    m_bindings[(int)Action::Restart]      = SDL_SCANCODE_R;
    m_bindings[(int)Action::StartGame]    = SDL_SCANCODE_RETURN;
    m_bindings[(int)Action::ToggleMouse]  = SDL_SCANCODE_TAB;
    m_bindings[(int)Action::Quit]         = SDL_SCANCODE_ESCAPE;

    m_keyState = SDL_GetKeyboardState(nullptr);
}

void InputManager::update()
{
    std::memcpy(m_prev, m_keyState, SDL_NUM_SCANCODES);

    SDL_GetRelativeMouseState(&m_mdx, &m_mdy);

    const Uint32 mb = SDL_GetMouseState(nullptr, nullptr);
    m_mouseHeld    = (mb & SDL_BUTTON_LMASK) != 0;
    m_aimHeld      = (mb & SDL_BUTTON_RMASK) != 0;

    m_shootClicked = false;
}

void InputManager::processEvent(const SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        m_shootClicked = true;
}

bool InputManager::isDown(Action a) const
{
    if (a == Action::Shoot) return m_mouseHeld;
    if (a == Action::Aim)   return m_aimHeld;

    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end() || it->second == SDL_SCANCODE_UNKNOWN) return false;
    return m_keyState[it->second] != 0;
}

bool InputManager::isPressed(Action a) const
{
    if (a == Action::Aim || a == Action::Shoot) return false; // mouse: usa isDown
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end() || it->second == SDL_SCANCODE_UNKNOWN) return false;
    return m_keyState[it->second] != 0 && m_prev[it->second] == 0;
}

void InputManager::rebind(Action action, SDL_Scancode scancode)
{
    m_bindings[(int)action] = scancode;
}

const char* InputManager::getKeyName(Action a) const
{
    if (a == Action::Shoot) return "Mouse Sx";
    if (a == Action::Aim)   return "Mouse Dx";
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return "???";
    return SDL_GetScancodeName(it->second);
}

SDL_Scancode InputManager::getScancode(Action a) const
{
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return SDL_SCANCODE_UNKNOWN;
    return it->second;
}

const char* InputManager::actionName(Action a)
{
    switch (a)
    {
        case Action::MoveForward:  return "Avanti";
        case Action::MoveBack:     return "Indietro";
        case Action::MoveLeft:     return "Sinistra";
        case Action::MoveRight:    return "Destra";
        case Action::Jump:         return "Salto";
        case Action::Shoot:        return "Sparo";
        case Action::Reload:       return "Ricarica";
        case Action::Aim:          return "Mira (ADS)";
        case Action::Sprint:       return "Corsa";
        case Action::Crouch:       return "Accovacciato";
        case Action::Roll:         return "Schivata";
        case Action::SwitchWeapon: return "Cambia arma";
        case Action::Pause:        return "Pausa";
        case Action::Restart:      return "Riavvia";
        case Action::StartGame:    return "Avvia/Conferma";
        case Action::ToggleMouse:  return "Cattura mouse";
        case Action::Quit:         return "Esci";
        default:                   return "???";
    }
}

// Rimappabili: esclude mouse (Shoot/Aim), tasti di sistema (Pause/Quit/StartGame/Restart)
static const Action s_rebindable[] = {
    Action::MoveForward, Action::MoveBack, Action::MoveLeft, Action::MoveRight,
    Action::Jump,
    Action::Sprint, Action::Crouch, Action::Roll, Action::SwitchWeapon,
    Action::Reload,
    Action::ToggleMouse,
};

int InputManager::rebindableCount()
{
    return (int)(sizeof(s_rebindable) / sizeof(s_rebindable[0]));
}

Action InputManager::rebindableAt(int index)
{
    if (index < 0 || index >= rebindableCount()) return Action::MoveForward;
    return s_rebindable[index];
}

} // namespace mini

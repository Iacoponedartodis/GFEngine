#include "mini/core/InputManager.hpp"
#include <cstring>

namespace mini
{

InputManager::InputManager()
{
    // Binding di default
    m_bindings[(int)Action::MoveForward]  = SDL_SCANCODE_W;
    m_bindings[(int)Action::MoveBack]     = SDL_SCANCODE_S;
    m_bindings[(int)Action::MoveLeft]     = SDL_SCANCODE_A;
    m_bindings[(int)Action::MoveRight]    = SDL_SCANCODE_D;
    m_bindings[(int)Action::Jump]         = SDL_SCANCODE_SPACE;
    m_bindings[(int)Action::Shoot]        = SDL_SCANCODE_UNKNOWN; // mouse click
    m_bindings[(int)Action::Reload]       = SDL_SCANCODE_R;
    m_bindings[(int)Action::Pause]        = SDL_SCANCODE_ESCAPE;
    m_bindings[(int)Action::Restart]      = SDL_SCANCODE_R;
    m_bindings[(int)Action::FreeRoam]     = SDL_SCANCODE_F;
    m_bindings[(int)Action::StartGame]    = SDL_SCANCODE_RETURN;
    m_bindings[(int)Action::ToggleMouse]  = SDL_SCANCODE_TAB;
    m_bindings[(int)Action::Quit]         = SDL_SCANCODE_ESCAPE;

    m_keyState = SDL_GetKeyboardState(nullptr);
}

void InputManager::update()
{
    // Salva lo stato precedente PRIMA di aggiornare
    std::memcpy(m_prev, m_keyState, SDL_NUM_SCANCODES);

    // Mouse delta
    SDL_GetRelativeMouseState(&m_mdx, &m_mdy);

    // Reset eventi discreti
    m_shootClicked = false;
}

void InputManager::processEvent(const SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        m_shootClicked = true;
}

bool InputManager::isDown(Action a) const
{
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end() || it->second == SDL_SCANCODE_UNKNOWN) return false;
    return m_keyState[it->second] != 0;
}

bool InputManager::isPressed(Action a) const
{
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
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return "???";
    return SDL_GetScancodeName(it->second);
}

} // namespace mini
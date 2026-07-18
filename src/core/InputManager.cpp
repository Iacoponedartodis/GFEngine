#include "mini/core/InputManager.hpp"
#include "mini/core/Telemetry.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <cstdio>
#include <string>
#include <fstream>
#include <filesystem>

namespace mini
{

InputManager::InputManager()
{
    // ── Binding di default (tastiera) ─────────────────────────────────
    auto K = [](SDL_Scancode s){ return InputBinding::key(s); };
    m_bindings[(int)Action::MoveForward]  = K(SDL_SCANCODE_W);
    m_bindings[(int)Action::MoveBack]     = K(SDL_SCANCODE_S);
    m_bindings[(int)Action::MoveLeft]     = K(SDL_SCANCODE_A);
    m_bindings[(int)Action::MoveRight]    = K(SDL_SCANCODE_D);
    m_bindings[(int)Action::Jump]         = K(SDL_SCANCODE_SPACE);
    m_bindings[(int)Action::Shoot]        = K(SDL_SCANCODE_UNKNOWN); // mouse sinistro
    m_bindings[(int)Action::Reload]       = K(SDL_SCANCODE_R);
    m_bindings[(int)Action::Aim]          = K(SDL_SCANCODE_UNKNOWN); // mouse destro
    m_bindings[(int)Action::Sprint]       = K(SDL_SCANCODE_LSHIFT);
    m_bindings[(int)Action::Crouch]       = K(SDL_SCANCODE_LCTRL);
    m_bindings[(int)Action::Roll]         = K(SDL_SCANCODE_Q);
    m_bindings[(int)Action::SwitchWeapon] = K(SDL_SCANCODE_F);
    m_bindings[(int)Action::SquadOrder]   = K(SDL_SCANCODE_G);   // ADR-020 Phase B
    m_bindings[(int)Action::CommandWheel] = K(SDL_SCANCODE_B);   // ruota comandi (doc 26)
    m_bindings[(int)Action::Pause]        = K(SDL_SCANCODE_ESCAPE);
    m_bindings[(int)Action::Restart]      = K(SDL_SCANCODE_R);
    m_bindings[(int)Action::StartGame]    = K(SDL_SCANCODE_RETURN);
    m_bindings[(int)Action::ToggleMouse]  = K(SDL_SCANCODE_TAB);
    m_bindings[(int)Action::Quit]         = K(SDL_SCANCODE_ESCAPE);

    // Le rimappature salvate VINCONO sui default (se il file esiste).
    loadBindings();

    m_keyState = SDL_GetKeyboardState(nullptr);
}

std::string InputManager::bindingsPath()
{
    char* base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    SDL_free(base);
    return dir + "user_presets/keybindings.json";
}

void InputManager::loadBindings()
{
    std::ifstream f(bindingsPath());
    if (!f.is_open()) return;   // primo avvio / nessuna rimappatura: default
    nlohmann::json j;
    try { f >> j; } catch (...) { return; }   // file corrotto: si resta ai default
    if (!j.is_object()) return;

    // Per NOME azione: un enum riordinato non deve rimappare i tasti sbagliati.
    // Ogni binding è {type, code}. Retrocompat: un intero nudo = tasto (formato
    // vecchio, solo tastiera).
    for (int i = 0; i < (int)Action::COUNT; ++i)
    {
        const char* name = actionName((Action)i);
        if (!name || !j.contains(name)) continue;
        const auto& v = j[name];
        if (v.is_number_integer())   // formato legacy: solo scancode
            m_bindings[i] = InputBinding::key((SDL_Scancode)v.get<int>());
        else if (v.is_object() && v.contains("type"))
            m_bindings[i] = InputBinding{ (InputType)v.value("type", 0),
                                          v.value("code", 0) };
    }
}

void InputManager::saveBindings() const
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(bindingsPath()).parent_path(), ec);

    nlohmann::json j;
    for (int i = 0; i < (int)Action::COUNT; ++i)
    {
        const char* name = actionName((Action)i);
        auto it = m_bindings.find(i);
        if (name && it != m_bindings.end())
            j[name] = { {"type", (int)it->second.type}, {"code", it->second.code} };
    }
    std::ofstream f(bindingsPath());
    if (f.is_open()) f << j.dump(2);
}

void InputManager::update()
{
    std::memcpy(m_prev, m_keyState, SDL_NUM_SCANCODES);

    SDL_GetRelativeMouseState(&m_mdx, &m_mdy);

    m_mouseMask    = SDL_GetMouseState(nullptr, nullptr);
    m_mouseHeld    = (m_mouseMask & SDL_BUTTON_LMASK) != 0;
    m_aimHeld      = (m_mouseMask & SDL_BUTTON_RMASK) != 0;

    m_shootClicked = false;
    // Azzerati QUI, prima del loop eventi del frame: processEvent() li riempie,
    // le query li leggono dopo (update() è chiamato prima di SDL_PollEvent).
    m_wheelY = 0.0f;
    for (bool& b : m_mbPressed) b = false;
}

void InputManager::processEvent(const SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        m_shootClicked = true;

    // Tracking per i binding non-tastiera (rotella/pulsanti mouse).
    if (event.type == SDL_MOUSEWHEEL)
        m_wheelY += (float)event.wheel.y;   // >0 su, <0 giù
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button < 8)
        m_mbPressed[event.button.button] = true;   // edge di questo frame

    // ── Input recorder (ADR-013): tasti e click con frame corrente ────────
    // Serve per replicare i crash: _telemetry_data/input_history.log.
    switch (event.type)
    {
    case SDL_KEYDOWN:
        if (event.key.repeat == 0)
            telemetry::recordInput(std::string("KEY_DOWN   ")
                + SDL_GetScancodeName(event.key.keysym.scancode));
        break;
    case SDL_KEYUP:
        telemetry::recordInput(std::string("KEY_UP     ")
            + SDL_GetScancodeName(event.key.keysym.scancode));
        break;
    case SDL_MOUSEBUTTONDOWN:
        telemetry::recordInput("MOUSE_DOWN btn=" + std::to_string(event.button.button)
            + " x=" + std::to_string(event.button.x)
            + " y=" + std::to_string(event.button.y));
        break;
    case SDL_MOUSEBUTTONUP:
        telemetry::recordInput("MOUSE_UP   btn=" + std::to_string(event.button.button)
            + " x=" + std::to_string(event.button.x)
            + " y=" + std::to_string(event.button.y));
        break;
    default: break;
    }
}

bool InputManager::isDown(Action a) const
{
    if (a == Action::Shoot) return m_mouseHeld;
    if (a == Action::Aim)   return m_aimHeld;

    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return false;
    const InputBinding& b = it->second;
    switch (b.type)
    {
    case InputType::Key:
        return b.code != SDL_SCANCODE_UNKNOWN && m_keyState[b.code] != 0;
    case InputType::MouseButton:
        return (m_mouseMask & SDL_BUTTON(b.code)) != 0;
    case InputType::WheelUp:   return m_wheelY > 0.0f;   // momentaneo: solo nel frame
    case InputType::WheelDown: return m_wheelY < 0.0f;
    default: return false;
    }
}

bool InputManager::isPressed(Action a) const
{
    if (a == Action::Aim || a == Action::Shoot) return false; // mouse: usa isDown
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return false;
    const InputBinding& b = it->second;
    switch (b.type)
    {
    case InputType::Key:
        return b.code != SDL_SCANCODE_UNKNOWN
            && m_keyState[b.code] != 0 && m_prev[b.code] == 0;   // edge
    case InputType::MouseButton:
        return (b.code >= 0 && b.code < 8) && m_mbPressed[b.code];
    // La rotella è per natura un evento discreto = una "pressione".
    case InputType::WheelUp:   return m_wheelY > 0.0f;
    case InputType::WheelDown: return m_wheelY < 0.0f;
    default: return false;
    }
}

void InputManager::rebind(Action action, SDL_Scancode scancode)
{
    rebind(action, InputBinding::key(scancode));
}

void InputManager::rebind(Action action, InputBinding binding)
{
    m_bindings[(int)action] = binding;
    saveBindings();   // persistito subito: come un preset, sopravvive al riavvio
}

InputBinding InputManager::getBinding(Action a) const
{
    auto it = m_bindings.find((int)a);
    return (it == m_bindings.end()) ? InputBinding{} : it->second;
}

const char* InputManager::getKeyName(Action a) const
{
    if (a == Action::Shoot) return "Mouse Sx";
    if (a == Action::Aim)   return "Mouse Dx";
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end()) return "???";
    const InputBinding& b = it->second;
    // Buffer statico: la UI lo usa e lo disegna subito (nessuna persistenza).
    static char buf[32];
    switch (b.type)
    {
    case InputType::Key:
        return SDL_GetScancodeName((SDL_Scancode)b.code);
    case InputType::MouseButton:
        switch (b.code) {
            case SDL_BUTTON_LEFT:   return "Mouse Sx";
            case SDL_BUTTON_RIGHT:  return "Mouse Dx";
            case SDL_BUTTON_MIDDLE: return "Mouse Centrale";
            case SDL_BUTTON_X1:     return "Mouse 4";
            case SDL_BUTTON_X2:     return "Mouse 5";
            default: std::snprintf(buf, sizeof(buf), "Mouse %d", b.code); return buf;
        }
    case InputType::WheelUp:   return "Rotella su";
    case InputType::WheelDown: return "Rotella giu'";
    default: return "(nessuno)";
    }
}

SDL_Scancode InputManager::getScancode(Action a) const
{
    auto it = m_bindings.find((int)a);
    if (it == m_bindings.end() || it->second.type != InputType::Key)
        return SDL_SCANCODE_UNKNOWN;
    return (SDL_Scancode)it->second.code;
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
        case Action::SquadOrder:   return "Ordine squadra";
        case Action::CommandWheel: return "Ruota comandi";
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
    Action::SquadOrder,
    Action::CommandWheel,
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

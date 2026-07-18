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
    Aim,             // mira (ADS) — tasto sinistro mouse destro
    Sprint,          // corsa
    Crouch,          // accovacciato
    Roll,            // schivata rapida
    SwitchWeapon,    // cambia arma primaria/secondaria
    SquadOrder,      // ordine contestuale alla squadra (ADR-020 Phase B, doc 26)
    CommandWheel,    // ruota di comando livello 2 (Regroup/Hold/Advance, doc 26)
    Pause,           // toggle pausa
    Restart,         // riavvia partita
    StartGame,       // ENTER per iniziare
    ToggleMouse,     // TAB
    Quit,            // ESC in free roam
    COUNT
};

// Un'azione può essere legata a INPUT DIVERSI, non solo alla tastiera: tasto,
// pulsante del mouse, o rotella su/giù. È ciò che permette all'utente di mettere
// "cambia arma sulla rotella" o "ordini sul tasto centrale" dai keybinding.
enum class InputType : unsigned char { None, Key, MouseButton, WheelUp, WheelDown };

struct InputBinding
{
    InputType type = InputType::None;
    int       code = 0;   // Key: SDL_Scancode · MouseButton: SDL_BUTTON_* (1..5)

    static InputBinding key(SDL_Scancode s) { return {InputType::Key, (int)s}; }
    static InputBinding mouseButton(int b)  { return {InputType::MouseButton, b}; }
    static InputBinding wheelUp()           { return {InputType::WheelUp, 0}; }
    static InputBinding wheelDown()         { return {InputType::WheelDown, 0}; }
    [[nodiscard]] bool valid() const { return type != InputType::None; }
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
    [[nodiscard]] bool isAimHeld()      const { return m_aimHeld; }   // tasto destro
    [[nodiscard]] int  mouseDX() const { return m_mdx; }
    [[nodiscard]] int  mouseDY() const { return m_mdy; }

    // Rimappa un'azione a un tasto (compat) o a un binding qualsiasi (mouse/rotella)
    void rebind(Action action, SDL_Scancode scancode);
    void rebind(Action action, InputBinding binding);

    // Nome leggibile del binding corrente (tastiera/mouse/rotella), per la UI
    [[nodiscard]] const char* getKeyName(Action a) const;

    // Binding attualmente associato a un'azione
    [[nodiscard]] InputBinding getBinding(Action a) const;

    // Restituisce lo scancode se il binding è un tasto (SDL_SCANCODE_UNKNOWN altrimenti)
    [[nodiscard]] SDL_Scancode getScancode(Action a) const;

    // Nome leggibile di un'azione (per UI keybinding)
    [[nodiscard]] static const char* actionName(Action a);

    // Numero di azioni rimappabili (esclude Shoot che è il mouse)
    [[nodiscard]] static int rebindableCount();
    [[nodiscard]] static Action rebindableAt(int index);

private:
    // Persistenza dei binding (come un preset): senza, ogni riavvio ripristina i
    // default e le rimappature dell'utente si perdevano (bug playtest). File in
    // <exe>/user_presets/ (fuori da data/, sopravvive alle rebuild — come i preset
    // partita, KI #19). Salvato per NOME azione: robusto al riordino dell'enum.
    static std::string bindingsPath();
    void loadBindings();          // sovrascrive i default con i valori salvati
    void saveBindings() const;    // scrive tutti i binding correnti

    // Mappa Action → binding (tasto / pulsante mouse / rotella)
    std::unordered_map<int, InputBinding> m_bindings;

    // Stato tasti: frame corrente e precedente
    const Uint8* m_keyState = nullptr;
    Uint8 m_prev[SDL_NUM_SCANCODES] = {};

    // Stato mouse per i binding non-tastiera. m_wheelY e m_mbPressed sono azzerati
    // in update() e riempiti dagli eventi del frame (processEvent): le query li
    // leggono dopo il loop eventi. m_mouseMask è la maschera pulsanti (per isDown).
    Uint32 m_mouseMask = 0;
    float  m_wheelY    = 0.0f;   // >0 su, <0 giù (accumulato nel frame)
    bool   m_mbPressed[8] = {};  // pulsante premuto QUESTO frame (edge)

    bool m_shootClicked = false;
    bool m_mouseHeld    = false;
    bool m_aimHeld      = false;   // tasto destro tenuto
    int  m_mdx = 0, m_mdy = 0;
};

} // namespace mini
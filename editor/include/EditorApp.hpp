#pragma once
#include "mini/game/data/ContentValidation.hpp"   // ADR-018
#include <memory>
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace editor
{

class HomeScreen;
class FreeCameraViewport;
class BalanceEditor;
class EntityEditor;
class MapEditor;
class WeaponEditor;
class VehicleEditor;
class MissionEditor;
class ClassEditor;

// Modulo attivo
enum class ActiveModule
{
    Home,
    FreeCameraViewport,
    BalanceEditor,
    EntityEditor,
    MapEditor,
    WeaponEditor,
    VehicleEditor,
    AssetManager,
    AiEditor,
    ContentValidation,   // ADR-018: pannello gate contenuti
    MissionEditor,       // ADR-019/doc 25: authoring missioni+obiettivi
    ClassEditor,         // doc 14: authoring classi
};

// Applicazione principale di GFEditor.
// Gestisce: SDL2 + OpenGL + ImGui (docking), main loop, moduli.
class EditorApp
{
public:
    explicit EditorApp();
    ~EditorApp();

    void run();

private:
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_glCtx   = nullptr;
    bool          m_running = false;

    ActiveModule m_active = ActiveModule::Home;
    // Modulo attivo al frame precedente: serve a rilasciare la cattura del mouse
    // quando si lascia un modulo con viewport (vedi tick()). Inizializzato uguale
    // a m_active così il primo frame non è mai un "cambio".
    ActiveModule m_prevActive = ActiveModule::Home;

    std::unique_ptr<HomeScreen>          m_homeScreen;
    std::unique_ptr<FreeCameraViewport>  m_viewport;
    std::unique_ptr<BalanceEditor>       m_balanceEditor;
    std::unique_ptr<EntityEditor>        m_entityEditor;
    std::unique_ptr<MapEditor>           m_mapEditor;
    std::unique_ptr<WeaponEditor>        m_weaponEditor;
    std::unique_ptr<VehicleEditor>       m_vehicleEditor;
    std::unique_ptr<MissionEditor>       m_missionEditor;
    std::unique_ptr<ClassEditor>         m_classEditor;

    // Pannello validazione (ADR-018): diagnostiche calcolate su richiesta —
    // mai per-frame (loadAll + validate fanno I/O).
    mini::Diagnostics m_diags;
    bool        m_diagsDirty = true;

    void init();
    void shutdown();
    void processEvents();
    void tick(float dt);
    void render();

    // Rilascia la cattura del mouse su OGNI modulo con viewport. Idempotente.
    // Chiamata al cambio modulo: garantisce l'invariante "solo il modulo attivo
    // può tenere il mouse", che il tick per-modulo da solo non può mantenere
    // (il tick del modulo che si lascia smette di girare).
    void releaseAllMouseCapture();

    void renderMenuBar();
    void renderValidationPanel();   // ADR-018 (usa validateContent, mai una copia)
    void renderDockSpace();

    void launchGame();    // Lancia GFEngine.exe con --direct-prematch
    void launchSandbox(); // Lancia GFEngine.exe con --sandbox
};

} // namespace editor
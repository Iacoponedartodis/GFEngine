#pragma once
#include <memory>
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace editor
{

class HomeScreen;
class FreeCameraViewport;
class BalanceEditor;
class HitboxEditor;

// Modulo attivo
enum class ActiveModule
{
    Home,
    FreeCameraViewport,
    HitboxEditor,
    BalanceEditor,
    AssetManager,
    AiEditor,
    MapMetadataEditor,
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

    std::unique_ptr<HomeScreen>          m_homeScreen;
    std::unique_ptr<FreeCameraViewport>  m_viewport;
    std::unique_ptr<BalanceEditor>       m_balanceEditor;
    std::unique_ptr<HitboxEditor>        m_hitboxEditor;

    void init();
    void shutdown();
    void processEvents();
    void tick(float dt);
    void render();

    void renderMenuBar();
    void renderDockSpace();

    // Lancia GFEngine.exe con --direct-prematch
    void launchGame();
};

} // namespace editor
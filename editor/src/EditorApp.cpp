#include "EditorApp.hpp"
#include "ui/HomeScreen.hpp"
#include "viewport/FreeCameraViewport.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
#include <mini/platform/OpenGL.hpp>

#include <iostream>
#include <cstdlib>
#include <stdexcept>

namespace editor
{

EditorApp::EditorApp() { init(); }
EditorApp::~EditorApp() { shutdown(); }

void EditorApp::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(
        "GFEditor v0.1",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1440, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (!m_window)
        throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

    m_glCtx = SDL_GL_CreateContext(m_window);
    if (!m_glCtx)
        throw std::runtime_error(std::string("GL context: ") + SDL_GetError());

    SDL_GL_SetSwapInterval(1);
    mini::loadGLFunctions();

    // ── ImGui ─────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Stile scuro
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.ItemSpacing       = {8.0f, 6.0f};
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_TitleBg]   = ImVec4(0.06f, 0.07f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.18f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_Button]    = ImVec4(0.12f, 0.22f, 0.40f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.35f, 0.60f, 1.0f);

    ImGui_ImplSDL2_InitForOpenGL(m_window, m_glCtx);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ── Moduli ────────────────────────────────────────────────────────
    m_homeScreen = std::make_unique<HomeScreen>();
    m_viewport   = std::make_unique<FreeCameraViewport>();

    m_running = true;
    std::cout << "[GFEditor] Avviato." << std::endl;
}

void EditorApp::shutdown()
{
    m_viewport.reset();
    m_homeScreen.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (m_glCtx)  SDL_GL_DeleteContext(m_glCtx);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void EditorApp::launchGame()
{
#ifdef _WIN32
    // Cerca l'exe nella stessa cartella o nella build debug
    std::system("start \"\" \"GFEngine.exe\" --direct-prematch");
    // Fallback build path
    std::system("start \"\" \"build\\windows-debug\\Debug\\GFEngine.exe\" --direct-prematch");
#else
    std::system("./GFEngine --direct-prematch &");
#endif
    std::cout << "[GFEditor] Avvio GFEngine con --direct-prematch" << std::endl;
}

void EditorApp::processEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) m_running = false;
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE
            && m_active != ActiveModule::Home)
        {
            m_active = ActiveModule::Home;
        }
    }
}

void EditorApp::tick(float dt)
{
    if (m_active == ActiveModule::FreeCameraViewport)
        m_viewport->tick(dt);
}

void EditorApp::renderMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Home", "Esc")) m_active = ActiveModule::Home;
        ImGui::Separator();
        if (ImGui::MenuItem("Esci")) m_running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Moduli"))
    {
        if (ImGui::MenuItem("Free Camera Viewport"))   m_active = ActiveModule::FreeCameraViewport;
        if (ImGui::MenuItem("Hitbox Editor (presto)")) {}
        if (ImGui::MenuItem("Balance Editor (presto)")){}
        if (ImGui::MenuItem("Asset Manager (presto)")) {}
        if (ImGui::MenuItem("AI Editor (presto)"))     {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Gioco"))
    {
        if (ImGui::MenuItem("Avvia GFEngine")) launchGame();
        ImGui::EndMenu();
    }

    // Indicatore modulo attivo
    const char* modName = "Home";
    if (m_active == ActiveModule::FreeCameraViewport) modName = "Free Camera Viewport";
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::TextDisabled("| %s", modName);

    ImGui::EndMainMenuBar();
}

void EditorApp::renderDockSpace()
{
    // DockSpace che copre l'intera finestra
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags dsFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##DockSpace", nullptr, dsFlags);
    ImGui::PopStyleVar(3);

    ImGui::DockSpace(ImGui::GetID("RootDock"), ImVec2(0,0),
                     ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void EditorApp::render()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderMenuBar();
    renderDockSpace();

    // ── Modulo attivo ─────────────────────────────────────────────────
    if (m_active == ActiveModule::Home)
    {
        bool wantsLaunch = false;
        ActiveModule next = m_homeScreen->draw(wantsLaunch);
        if (wantsLaunch) launchGame();
        if (next != ActiveModule::Home) m_active = next;
    }
    else if (m_active == ActiveModule::FreeCameraViewport)
    {
        ImGui::Begin("Free Camera Viewport", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        m_viewport->draw();
        ImGui::End();
    }
    else
    {
        // Stub per moduli futuri
        ImGui::Begin("Modulo");
        ImGui::TextDisabled("Questo modulo sarà disponibile in una prossima milestone.");
        if (ImGui::Button("Torna alla Home")) m_active = ActiveModule::Home;
        ImGui::End();
    }

    // Render ImGui
    ImGui::Render();
    int dispW, dispH;
    SDL_GL_GetDrawableSize(m_window, &dispW, &dispH);
    glViewport(0, 0, dispW, dispH);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Multi-viewport support (richiesto da ImGuiConfigFlags_ViewportsEnable)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window*   backupWin = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupCtx = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupWin, backupCtx);
    }

    SDL_GL_SwapWindow(m_window);
}

void EditorApp::run()
{
    Uint32 lastTime = SDL_GetTicks();

    while (m_running)
    {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        if (dt > 0.25f) dt = 0.25f;
        lastTime = now;

        processEvents();
        tick(dt);
        render();
    }
}

} // namespace editor
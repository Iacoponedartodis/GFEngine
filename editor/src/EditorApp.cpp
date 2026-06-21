#include "EditorApp.hpp"
#include "ui/HomeScreen.hpp"
#include "viewport/FreeCameraViewport.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
#include <mini/platform/OpenGL.hpp>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
#ifdef _WIN32
  #include <windows.h>
  #include <shellapi.h>
#endif

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

    // Adatta dimensione finestra allo schermo disponibile
    SDL_DisplayMode dm; SDL_GetCurrentDisplayMode(0, &dm);
    int winW = (int)(dm.w * 0.85f);
    int winH = (int)(dm.h * 0.85f);
    if (winW < 800) winW = 800;
    if (winH < 600) winH = 600;

    m_window = SDL_CreateWindow(
        "GFEditor v0.1",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (!m_window)
        throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

    m_glCtx = SDL_GL_CreateContext(m_window);
    if (!m_glCtx)
        throw std::runtime_error(std::string("GL context: ") + SDL_GetError());

    SDL_GL_SetSwapInterval(1);
    miniGLLoad(); // carica le funzioni OpenGL 3.3

    // ── ImGui ─────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.ItemSpacing       = {8.0f, 6.0f};
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.06f, 0.07f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.18f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.12f, 0.22f, 0.40f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.35f, 0.60f, 1.0f);

    ImGui_ImplSDL2_InitForOpenGL(m_window, m_glCtx);
    ImGui_ImplOpenGL3_Init("#version 330");

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
    // Trova GFEngine.exe nella stessa cartella di GFEditor.exe
    char* base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    SDL_free(base);

    std::string exePath = dir + "GFEngine.exe";

    // Verifica esistenza
    FILE* f = fopen(exePath.c_str(), "rb");
    if (!f) {
        std::cerr << "[GFEditor] GFEngine.exe non trovato in: " << exePath << "\n";
        return;
    }
    fclose(f);

#ifdef _WIN32
    // ShellExecuteA è il modo più semplice e affidabile su Windows
    HINSTANCE result = ShellExecuteA(
        nullptr,          // hwnd
        "open",           // operazione
        exePath.c_str(),  // file da aprire
        "--direct-prematch", // parametri
        dir.c_str(),      // working directory
        SW_SHOWNORMAL     // modalità finestra
    );
    if ((intptr_t)result <= 32)
    {
        std::cerr << "[GFEditor] ShellExecute fallito: " << (intptr_t)result << "\n";
        return;
    }
#else
    std::string cmd = "\"" + exePath + "\" --direct-prematch &";
    std::system(cmd.c_str());
#endif

    std::cout << "[GFEditor] GFEngine avviato: " << exePath << "\n";
}

void EditorApp::processEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) m_running = false;
        if (ev.type == SDL_KEYDOWN
            && ev.key.keysym.sym == SDLK_ESCAPE
            && m_active != ActiveModule::Home)
            m_active = ActiveModule::Home;
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
        if (ImGui::MenuItem("Chiudi GFEditor")) m_running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Moduli"))
    {
        if (ImGui::MenuItem("Free Camera Viewport"))    m_active = ActiveModule::FreeCameraViewport;
        if (ImGui::MenuItem("Hitbox Editor (presto)"))  {}
        if (ImGui::MenuItem("Balance Editor (presto)")) {}
        if (ImGui::MenuItem("Asset Manager (presto)"))  {}
        if (ImGui::MenuItem("AI Editor (presto)"))      {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Gioco"))
    {
        if (ImGui::MenuItem("Avvia GFEngine")) launchGame();
        ImGui::EndMenu();
    }

    // Modulo attivo al centro
    const char* modName = "Home";
    if (m_active == ActiveModule::FreeCameraViewport) modName = "Free Camera Viewport";
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::TextDisabled("| %s", modName);

    // Pulsante chiudi a destra
    float rightEdge = ImGui::GetWindowWidth();
    ImGui::SetCursorPosX(rightEdge - 90.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.08f, 0.08f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.15f, 0.15f, 1.00f));
    if (ImGui::Button("  X  Esci  ")) m_running = false;
    ImGui::PopStyleColor(2);

    ImGui::EndMainMenuBar();
}

void EditorApp::renderDockSpace()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags dsFlags =
        ImGuiWindowFlags_NoDocking       | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse      | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove          | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus      | ImGuiWindowFlags_NoBackground;

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

    if (m_active == ActiveModule::Home)
    {
        bool wantsLaunch = false;
        ActiveModule next = m_homeScreen->draw(wantsLaunch);
        if (wantsLaunch) launchGame();
        if (next != ActiveModule::Home) m_active = next;
    }
    else if (m_active == ActiveModule::FreeCameraViewport)
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 25),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(vp->WorkSize.x - 20, vp->WorkSize.y - 35),
            ImGuiCond_FirstUseEver);
        ImGui::Begin("Free Camera Viewport", nullptr,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        m_viewport->draw();
        ImGui::End();
    }
    else
    {
        ImGui::Begin("Modulo");
        ImGui::TextDisabled("Questo modulo sara' disponibile in una prossima milestone.");
        if (ImGui::Button("Torna alla Home")) m_active = ActiveModule::Home;
        ImGui::End();
    }

    ImGui::Render();
    int dispW, dispH;
    SDL_GL_GetDrawableSize(m_window, &dispW, &dispH);
    glViewport(0, 0, dispW, dispH);
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
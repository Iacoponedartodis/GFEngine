#include "util/DataPath.hpp"
#include "EditorApp.hpp"
#include "ui/HomeScreen.hpp"
#include "viewport/FreeCameraViewport.hpp"
#include "modules/BalanceEditor.hpp"
#include "modules/EntityEditor.hpp"
#include "modules/MapEditor.hpp"
#include "modules/WeaponEditor.hpp"
#include "modules/VehicleEditor.hpp"
#include "modules/MissionEditor.hpp"
#include "modules/ClassEditor.hpp"
#include <mini/game/data/DefinitionRegistry.hpp>   // ADR-018: pannello validazione

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
#include <filesystem>
#include <mini/platform/OpenGL.hpp>
#include <mini/core/Telemetry.hpp>

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
    // Telemetria (ADR-013): logger + crash net anche per l'editor.
    mini::telemetry::init("GFEditor");

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

    m_homeScreen    = std::make_unique<HomeScreen>();
    m_viewport      = std::make_unique<FreeCameraViewport>();
    m_balanceEditor = std::make_unique<BalanceEditor>();
    m_entityEditor  = std::make_unique<EntityEditor>();
    m_mapEditor     = std::make_unique<MapEditor>();
    m_weaponEditor  = std::make_unique<WeaponEditor>();
    m_vehicleEditor = std::make_unique<VehicleEditor>();
    m_missionEditor = std::make_unique<MissionEditor>();
    m_classEditor   = std::make_unique<ClassEditor>();

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

void EditorApp::launchSandbox()
{
    char* base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    SDL_free(base);
    std::string exePath = dir + "GFEngine.exe";
    FILE* f = fopen(exePath.c_str(), "rb");
    if (!f) { std::cerr << "[GFEditor] GFEngine.exe non trovato: " << exePath << "\n"; return; }
    fclose(f);
#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(nullptr, "open", exePath.c_str(),
                                     "--sandbox", dir.c_str(), SW_SHOWNORMAL);
    if ((intptr_t)result <= 32)
        std::cerr << "[GFEditor] ShellExecute sandbox fallito: " << (intptr_t)result << "\n";
    else
        std::cout << "[GFEditor] Sandbox avviata: " << exePath << "\n";
#else
    std::string cmd = "\"" + exePath + "\" --sandbox &";
    std::system(cmd.c_str());
#endif
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

void EditorApp::releaseAllMouseCapture()
{
    if (m_viewport)      m_viewport->releaseMouseCapture();
    if (m_entityEditor)  m_entityEditor->releaseMouseCapture();
    if (m_mapEditor)     m_mapEditor->releaseMouseCapture();
    if (m_weaponEditor)  m_weaponEditor->releaseMouseCapture();
    if (m_vehicleEditor) m_vehicleEditor->releaseMouseCapture();
}

void EditorApp::tick(float dt)
{
    // Cambio modulo → libera il mouse. Se si lascia un viewport mentre la
    // cattura Tab è attiva, il suo tick smette di girare e non può più spegnere
    // SDL_SetRelativeMouseMode (stato GLOBALE): il cursore resterebbe invisibile
    // e non liberabile finché non si riapre un viewport. È il punto che SA quando
    // il modulo cambia, quindi è qui che l'invariante va imposto. Vale anche per
    // Esc→Home (processEvents), che diventa così un'uscita d'emergenza dal mouse.
    if (m_active != m_prevActive)
    {
        releaseAllMouseCapture();
        m_prevActive = m_active;
    }

    if (m_active == ActiveModule::FreeCameraViewport)
        m_viewport->tick(dt);
    else if (m_active == ActiveModule::EntityEditor)
        m_entityEditor->tick(dt);
    else if (m_active == ActiveModule::MapEditor)
        m_mapEditor->tick(dt);
    else if (m_active == ActiveModule::WeaponEditor)
        m_weaponEditor->tick(dt);
    else if (m_active == ActiveModule::VehicleEditor)
        m_vehicleEditor->tick(dt);
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
        if (ImGui::MenuItem("Balance Editor"))  m_active = ActiveModule::BalanceEditor;
        if (ImGui::MenuItem("Entity Editor"))   m_active = ActiveModule::EntityEditor;
        if (ImGui::MenuItem("Map Editor"))      m_active = ActiveModule::MapEditor;
        if (ImGui::MenuItem("Weapon Editor"))   m_active = ActiveModule::WeaponEditor;
        if (ImGui::MenuItem("Vehicle Editor"))  m_active = ActiveModule::VehicleEditor;
        if (ImGui::MenuItem("Missioni e obiettivi")) m_active = ActiveModule::MissionEditor;
        if (ImGui::MenuItem("Classi")) m_active = ActiveModule::ClassEditor;
        ImGui::Separator();
        if (ImGui::MenuItem("Validazione contenuti")) m_active = ActiveModule::ContentValidation;
        if (ImGui::MenuItem("Asset Manager (presto)"))  {}
        if (ImGui::MenuItem("AI Editor (presto)"))      {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Gioco"))
    {
        if (ImGui::MenuItem("Avvia GFEngine"))  launchGame();
        if (ImGui::MenuItem("Avvia Sandbox"))   launchSandbox();
        ImGui::EndMenu();
    }

    // Modulo attivo al centro
    const char* modName = "Home";
    if (m_active == ActiveModule::FreeCameraViewport) modName = "Free Camera Viewport";
    if (m_active == ActiveModule::BalanceEditor)     modName = "Balance Editor";
    if (m_active == ActiveModule::EntityEditor)      modName = "Entity Editor";
    if (m_active == ActiveModule::MapEditor)         modName = "Map Editor";
    if (m_active == ActiveModule::WeaponEditor)      modName = "Weapon Editor";
    if (m_active == ActiveModule::VehicleEditor)     modName = "Vehicle Editor";
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

// R8 CHIUSO (2026-07-17): la radice si risolve in `util/DataPath`, una volta sola.
// Il debito non era teorico — le otto copie erano già divergenti (quattro
// verificavano `data/weapons`, quattro la sola esistenza della cartella). Qui
// pesava più che altrove: il pannello avrebbe validato una data/ diversa da
// quella che il gioco carica, cioè avrebbe mentito.
static std::string editorDataPath() { return editor::datapath::root(); }

// ── Pannello Validazione contenuti (24_ContentValidation, ADR-018) ────────
// Terzo consumatore dello STESSO `validateContent()` che usano il runtime e
// `--validate`. È il punto centrale dell'ADR: se l'editor avesse una copia più
// debole delle regole, esisterebbe contenuto che l'editor accetta e il gioco
// rifiuta — cioè esattamente il bug che il gate serve a togliere.
void EditorApp::renderValidationPanel()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 25),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 20, vp->WorkSize.y - 40),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("Validazione contenuti");

    if (m_diagsDirty)
    {
        mini::DefinitionRegistry reg;
        reg.loadAll(editorDataPath());
        m_diags = mini::validateContent(reg, editorDataPath());
        m_diagsDirty = false;
    }

    const int errors   = mini::countBy(m_diags, mini::telemetry::Level::Error);
    const int warnings = mini::countBy(m_diags, mini::telemetry::Level::Warn);

    if (ImGui::Button("Rivalida"))  m_diagsDirty = true;
    ImGui::SameLine();
    if (errors > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
                           "%d errori (bloccano l'avvio del gioco), %d warning",
                           errors, warnings);
    else if (warnings > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                           "Nessun errore, %d warning", warnings);
    else
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Contenuto valido");

    ImGui::Separator();
    ImGui::TextDisabled("Stesse regole del runtime e di GFEngine.exe --validate.");
    ImGui::Spacing();

    if (ImGui::BeginTable("##diags", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
          | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Gravita'", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("File",     ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("Problema e correzione");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (const auto& d : m_diags)
        {
            const bool err = (d.severity == mini::telemetry::Level::Error);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(err ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f)
                                   : ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                               err ? "ERRORE" : "warning");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(d.file.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", d.message.c_str());
            // Il suggerimento è la parte che rende la diagnostica azionabile:
            // senza "cosa fare", è solo un altro messaggio da ignorare.
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.8f, 1.0f, 1.0f));
            ImGui::TextWrapped("-> %s", d.suggestion.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
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
        bool wantsLaunch = false, wantsSandbox = false;
        ActiveModule next = m_homeScreen->draw(wantsLaunch, wantsSandbox);
        if (wantsLaunch)   launchGame();
        if (wantsSandbox)  launchSandbox();
        if (next != ActiveModule::Home) m_active = next;
    }
    else if (m_active == ActiveModule::ContentValidation)
    {
        renderValidationPanel();
    }
    else if (m_active == ActiveModule::MissionEditor)
    {
        m_missionEditor->draw();
    }
    else if (m_active == ActiveModule::ClassEditor)
    {
        m_classEditor->draw();
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
        m_viewport->draw(true); // mostra barra di caricamento modello/mappa
        ImGui::End();
    }
    else if (m_active == ActiveModule::BalanceEditor)
    {
        const ImGuiViewport* vp2 = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp2->WorkPos.x+10,vp2->WorkPos.y+25), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(vp2->WorkSize.x-20,vp2->WorkSize.y-35), ImGuiCond_Appearing);
        ImGui::Begin("Balance Editor", nullptr);
        m_balanceEditor->draw();
        ImGui::End();
    }
    else if (m_active == ActiveModule::EntityEditor)
    {
        const ImGuiViewport* vp2 = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp2->WorkPos.x+10,vp2->WorkPos.y+25), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(vp2->WorkSize.x-20,vp2->WorkSize.y-35), ImGuiCond_Appearing);
        ImGui::Begin("Entity Editor", nullptr);
        m_entityEditor->draw();
        ImGui::End();
    }
    else if (m_active == ActiveModule::MapEditor)
    {
        const ImGuiViewport* vp2 = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp2->WorkPos.x+10,vp2->WorkPos.y+25), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(vp2->WorkSize.x-20,vp2->WorkSize.y-35), ImGuiCond_Appearing);
        ImGui::Begin("Map Editor", nullptr);
        m_mapEditor->draw();
        ImGui::End();
    }
    else if (m_active == ActiveModule::WeaponEditor)
    {
        const ImGuiViewport* vp2 = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp2->WorkPos.x+10,vp2->WorkPos.y+25), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(vp2->WorkSize.x-20,vp2->WorkSize.y-35), ImGuiCond_Appearing);
        ImGui::Begin("Weapon Editor", nullptr);
        m_weaponEditor->draw();
        ImGui::End();
    }
    else if (m_active == ActiveModule::VehicleEditor)
    {
        const ImGuiViewport* vp2 = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp2->WorkPos.x+10,vp2->WorkPos.y+25), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(vp2->WorkSize.x-20,vp2->WorkSize.y-35), ImGuiCond_Appearing);
        ImGui::Begin("Vehicle Editor", nullptr);
        m_vehicleEditor->draw();
        ImGui::End();
    }
    else
    {
        ImGui::Begin("Modulo");
        ImGui::TextDisabled("Questo modulo sarà disponibile in una prossima milestone.");
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
        mini::telemetry::beginFrame();
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        if (dt > 0.25f) dt = 0.25f;
        lastTime = now;
        processEvents();
        tick(dt);
        render();
    }
    mini::telemetry::shutdown();
}

} // namespace editor
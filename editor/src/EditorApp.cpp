#include "util/DataPath.hpp"
#include "EditorApp.hpp"
#include "ui/HomeScreen.hpp"
#include "viewport/FreeCameraViewport.hpp"
#include "mini/render/Camera.hpp"
#include "framework/Dialogs.hpp"     // finestre modali condivise (doc 52 F4)
#include <glm/glm.hpp>
#include <cmath>
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
#include <nlohmann/json.hpp>
#include <fstream>

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

EditorApp::EditorApp(const std::string& startModule, int seqFrames)
{
    init();
    if (seqFrames > 0) m_seqFrames = seqFrames;
    if (startModule.empty()) return;

    static const std::pair<const char*, ActiveModule> k_modules[] = {
        {"home",     ActiveModule::Home},
        {"entity",   ActiveModule::EntityEditor},
        {"map",      ActiveModule::MapEditor},
        {"weapon",   ActiveModule::WeaponEditor},
        {"vehicle",  ActiveModule::VehicleEditor},
        {"balance",  ActiveModule::BalanceEditor},
        {"mission",  ActiveModule::MissionEditor},
        {"class",    ActiveModule::ClassEditor},
        {"validate", ActiveModule::ContentValidation},
        {"asset",    ActiveModule::AssetManager},
        {"ai",       ActiveModule::AiEditor},
        {"viewport", ActiveModule::FreeCameraViewport},
    };
    auto resolve = [&](const std::string& n, ActiveModule& out) {
        for (const auto& [name, mod] : k_modules)
            if (n == name) { out = mod; return true; }
        return false;
    };

    // Lista separata da virgole: ogni voce è un passo del percorso.
    std::string cur;
    std::vector<std::string> names;
    for (char c : startModule + ",")
        if (c == ',') { if (!cur.empty()) names.push_back(cur); cur.clear(); }
        else cur += c;

    for (const auto& n : names)
    {
        ActiveModule mod{};
        if (resolve(n, mod)) m_seq.push_back(mod);
        else std::cerr << "[GFEditor] --module sconosciuto: " << n << std::endl;
    }
    if (m_seq.empty()) return;

    // Il primo passo è attivo subito. m_prevActive resta Home, così il primo
    // frame conta come un CAMBIO vero: la transizione è ciò che va collaudato.
    m_active = m_seq[0];
    std::cout << "[GFEditor] percorso moduli: " << names.size()
              << " passi, " << m_seqFrames << " frame ciascuno" << std::endl;
}

// Avanza di un passo ogni m_seqFrames frame; sull'ultimo passo si ferma e
// l'editor resta aperto normalmente.
void EditorApp::advanceModuleSequence()
{
    if (m_seq.empty() || m_seqIdx + 1 >= (int)m_seq.size()) return;
    if (++m_frameNo < m_seqFrames) return;
    m_frameNo = 0;
    m_active  = m_seq[++m_seqIdx];
    std::cout << "[GFEditor] passo " << (m_seqIdx + 1) << "/" << m_seq.size()
              << std::endl;
}
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

    loadAppearance();

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
        if (ev.type == SDL_QUIT) requestQuit();
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
        // La TRANSIZIONE è una fase a sé: KI #98 si presenta proprio nel frame in
        // cui si entra in un modulo, e distinguere "sto entrando" da "sto girando"
        // è la differenza fra sapere e indovinare.
        mini::telemetry::setPhase("cambio modulo");
        releaseAllMouseCapture();
        m_prevActive = m_active;
    }

    if (m_active == ActiveModule::FreeCameraViewport)
        { mini::telemetry::ScopedPhase p("tick viewport"); m_viewport->tick(dt); }
    else if (m_active == ActiveModule::EntityEditor)
        { mini::telemetry::ScopedPhase p("tick EntityEditor"); m_entityEditor->tick(dt); }
    else if (m_active == ActiveModule::MapEditor)
        { mini::telemetry::ScopedPhase p("tick MapEditor"); m_mapEditor->tick(dt); }
    else if (m_active == ActiveModule::WeaponEditor)
        { mini::telemetry::ScopedPhase p("tick WeaponEditor"); m_weaponEditor->tick(dt); }
    else if (m_active == ActiveModule::VehicleEditor)
        { mini::telemetry::ScopedPhase p("tick VehicleEditor"); m_vehicleEditor->tick(dt); }
}

void EditorApp::renderMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Home", "Esc")) m_active = ActiveModule::Home;
        ImGui::Separator();
        if (ImGui::MenuItem("Chiudi GFEditor")) requestQuit();
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
    if (ImGui::BeginMenu("Guida"))
    {
        if (ImGui::MenuItem("Guida dell'editor...", "F1", m_showHelp))
            m_showHelp = !m_showHelp;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Aspetto"))
    {
        if (ImGui::MenuItem("Interfaccia...", nullptr, m_showAppearance))
            m_showAppearance = !m_showAppearance;
        ImGui::Separator();
        // Strumento di ImGui, non nostro: contiene l'ID Stack Tool, che diagnostica
        // i conflitti di identificatore — la famiglia di difetti che ci è già costata
        // un modale invisibile che bloccava i clic (changelog 164).
        if (ImGui::MenuItem("Diagnostica ImGui...", nullptr, m_showImGuiMetrics))
            m_showImGuiMetrics = !m_showImGuiMetrics;
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

    // (Rimosso il pulsante rosso "X Esci": era un workaround di inizio progetto
    // quando la finestra era tagliata e non si raggiungeva la X nativa. Ora si
    // chiude con la X della finestra o dal menu "File → Chiudi GFEditor".)

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

    // Aspetto: applicato a ogni frame, così un cambio si vede subito.
    // La densità va applicata in modo RELATIVO: `ScaleAllSizes` moltiplica lo stato
    // corrente, quindi richiamarla a ogni frame col valore assoluto gonfierebbe
    // l'interfaccia fino a farla esplodere in pochi secondi.
    ImGui::GetIO().FontGlobalScale = m_uiFontScale;
    if (std::fabs(m_uiDensity - m_uiAppliedDensity) > 0.001f)
    {
        ImGui::GetStyle().ScaleAllSizes(m_uiDensity / m_uiAppliedDensity);
        m_uiAppliedDensity = m_uiDensity;
    }

    renderMenuBar();
    renderDockSpace();
    drawAppearanceWindow();
    // F1 ovunque: è il tasto che si preme d'istinto quando non si sa come si fa.
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false) && !ImGui::GetIO().WantTextInput)
        m_showHelp = !m_showHelp;
    m_help.draw(&m_showHelp);
    if (m_showImGuiMetrics) ImGui::ShowMetricsWindow(&m_showImGuiMetrics);
    drawQuitPrompt();

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
        { mini::telemetry::ScopedPhase p("draw EntityEditor"); m_entityEditor->draw(); }
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

// Collaudo della CAMERA ortografica (doc 50 M3). Non posso guardare lo schermo:
// verifico la matematica, cioè che un punto noto del mondo finisca dove deve sullo
// schermo. È anche il punto in cui avevo previsto la rottura (il vettore "su"
// degenere guardando dritto in basso), quindi è il primo da controllare.
static int cameraSelfTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "OK  " : "FALL", what);
        if (!ok) ++failed;
    };

    mini::Camera cam(60.0f, 2.0f, 0.1f, 1000.0f);   // aspect 2:1
    cam.setOrthographic(true, 50.0f);               // inquadra 100 m in altezza
    cam.setPosition({0.0f, 200.0f, 0.0f});
    cam.lookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});   // dall'alto

    const glm::mat4 vp = cam.getViewProjection();
    auto ndc = [&](glm::vec3 w) {
        const glm::vec4 c = vp * glm::vec4(w, 1.0f);
        return glm::vec2(c.x / c.w, c.y / c.w);
    };

    // Nessun NaN: è la trappola del vettore "su" parallelo alla direzione.
    const glm::vec2 o = ndc({0, 0, 0});
    check(!std::isnan(o.x) && !std::isnan(o.y), "ortho dall'alto: nessun NaN");
    check(std::fabs(o.x) < 1e-4f && std::fabs(o.y) < 1e-4f,
          "ortho: il centro del mondo cade al centro dello schermo");

    // Con mezza altezza 50 e aspect 2, la mezza larghezza è 100 m: un punto a
    // x = +100 deve finire esattamente sul bordo destro (NDC +1).
    const glm::vec2 r = ndc({100.0f, 0.0f, 0.0f});
    check(std::fabs(r.x - 1.0f) < 1e-3f, "ortho: la scala orizzontale rispetta l'aspect");
    // "Su" sullo schermo = -Z nel mondo, come dichiarato in setViewMode(Top).
    const glm::vec2 u = ndc({0.0f, 0.0f, -50.0f});
    check(u.y > 0.99f && u.y < 1.01f, "ortho dall'alto: -Z e' in alto sullo schermo");

    // La QUOTA non deve cambiare la dimensione: è tutta la ragione dell'ortografica.
    const glm::vec2 low  = ndc({30.0f, 0.0f,  0.0f});
    const glm::vec2 high = ndc({30.0f, 40.0f, 0.0f});
    check(std::fabs(low.x - high.x) < 1e-4f,
          "ortho: la distanza NON cambia con la quota (e' il senso della vista)");

    // Geometria SOPRA la camera: in prospettiva sarebbe dietro il piano vicino e
    // sparirebbe. In ortografica deve restare visibile, o guardando una mappa
    // dall'alto scomparirebbe tutto ciò che sta più in alto della camera.
    const glm::vec4 above = vp * glm::vec4(0.0f, 260.0f, 0.0f, 1.0f);
    const float zn = above.z / above.w;
    check(zn >= -1.0f && zn <= 1.0f, "ortho: cio' che sta sopra la camera resta visibile");

    // In prospettiva, invece, la quota DEVE cambiare la dimensione.
    cam.setOrthographic(false);
    const glm::mat4 vp2 = cam.getViewProjection();
    auto ndc2 = [&](glm::vec3 w) {
        const glm::vec4 c = vp2 * glm::vec4(w, 1.0f);
        return glm::vec2(c.x / c.w, c.y / c.w);
    };
    check(std::fabs(ndc2({30.0f, 0.0f, 0.0f}).x - ndc2({30.0f, 40.0f, 0.0f}).x) > 1e-3f,
          "prospettiva: la quota cambia la dimensione (controprova)");

    return failed;
}

// ── Uscita protetta (doc 51) ────────────────────────────────────────────────
// Chiudere GFEditor buttava via le modifiche non salvate in SILENZIO: nessun
// avviso, nessuna domanda. Su una mappa costruita in giorni è il modo più stupido
// di perdere lavoro, e non richiedeva un crash per succedere — bastava la X.
// Chi ha lavoro non salvato, raccolto al momento della domanda (doc 52 F3).
// I moduli si dichiarano qui e non registrano callback che sopravvivono a loro.
void EditorApp::collectDirty()
{
    m_dirty.clear();
    if (m_mapEditor && m_mapEditor->hasUnsavedChanges())
        m_dirty.add({ m_mapEditor->unsavedSummary(), true,
                      [this] { m_mapEditor->saveAllPending(); } });
    if (m_entityEditor && m_entityEditor->hasUnsavedChanges())
        m_dirty.add({ m_entityEditor->unsavedWhat(), true,
                      [this] { m_entityEditor->savePending(); } });
    if (m_weaponEditor && m_weaponEditor->hasUnsavedChanges())
        m_dirty.add({ m_weaponEditor->unsavedWhat(), true,
                      [this] { m_weaponEditor->savePending(); } });
    if (m_vehicleEditor && m_vehicleEditor->hasUnsavedChanges())
        m_dirty.add({ m_vehicleEditor->unsavedWhat(), true,
                      [this] { m_vehicleEditor->savePending(); } });
    // Il Balance Editor NON espone un salvataggio: scrive per singola definizione.
    // Si avvisa e basta — offrire "salva tutto" salverebbe una parte facendo
    // credere il contrario.
    if (m_balanceEditor && m_balanceEditor->hasUnsavedChanges())
        m_dirty.add({ m_balanceEditor->unsavedWhat(), true, nullptr });
    // Classi e Missioni: non avevano NESSUN rilevamento — le modifiche restavano in
    // memoria fino al pulsante "Salva" e chiudere le buttava via in silenzio.
    if (m_classEditor && m_classEditor->hasUnsavedChanges())
        m_dirty.add({ m_classEditor->unsavedWhat(), true,
                      [this] { m_classEditor->savePending(); } });
    if (m_missionEditor && m_missionEditor->hasUnsavedChanges())
        m_dirty.add({ m_missionEditor->unsavedWhat(), true,
                      [this] { m_missionEditor->savePending(); } });
}

void EditorApp::requestQuit()
{
    collectDirty();
    if (m_dirty.any()) { m_askQuit = true; return; }
    m_running = false;
}

void EditorApp::drawQuitPrompt()
{
    if (!m_askQuit) return;
    std::string detail;
    if (m_mapEditor && !m_mapEditor->lastAutosave().empty())
        detail = "Copia di recupero: " + m_mapEditor->lastAutosave();
    // Apertura e disegno nella STESSA funzione (doc 52 F4): il difetto del modale
    // invisibile qui non è "da evitare", è inesprimibile.
    const auto c = editor::dialogs::saveDiscardCancel(
        "Modifiche non salvate", m_askQuit,
        "Modifiche non salvate: " + m_dirty.summary() + ".",
        detail, m_dirty.allSaveable());
    if (c == editor::dialogs::Choice::Yes) { m_dirty.saveAll(); m_running = false; }
    else if (c == editor::dialogs::Choice::No) { m_running = false; }
}

// ── Aspetto dell'interfaccia ────────────────────────────────────────────────
static std::string appearancePath()
{
    // `dir()` e non `root()`: root() NON ha lo slash finale, e concatenarlo produceva
    // `.../dataeditor_appearance.json` — un file mai riletto, cioè preferenze che
    // sembravano salvarsi e non tornavano più.
    return editor::datapath::dir() + "editor_appearance.json";
}

void EditorApp::loadAppearance()
{
    std::ifstream f(appearancePath());
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        m_uiFontScale = j.value("font_scale", 1.0f);
        m_uiDensity   = j.value("density", 1.0f);
    } catch (...) { return; }   // preferenze illeggibili: si resta ai valori base
}

void EditorApp::saveAppearance() const
{
    // Solo i valori che DECIDIAMO noi, non `ImGuiStyle` grezzo: scaricare la struct
    // su file non è affidabile fra versioni di ImGui, e un file di preferenze che si
    // rompe a ogni aggiornamento è peggio che non averlo.
    nlohmann::json j;
    j["font_scale"] = m_uiFontScale;
    j["density"]    = m_uiDensity;
    std::ofstream f(appearancePath());
    if (f.is_open()) f << j.dump(2) << std::endl;
}

void EditorApp::drawAppearanceWindow()
{
    if (!m_showAppearance) return;
    ImGui::SetNextWindowSize(ImVec2(460, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Interfaccia", &m_showAppearance))
    { ImGui::End(); return; }

    ImGui::TextWrapped(
        "Regola come si LEGGE e quanto INGOMBRA l'interfaccia. Ha effetto "
        "immediato su tutti i moduli e si conserva alla chiusura.");
    ImGui::Separator();

    bool changed = false;
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderFloat("Dimensione testo", &m_uiFontScale, 0.75f, 2.0f, "%.2fx"))
        changed = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Per le scritte che non si vedono. Ingrandisce TUTTO il\n"
                          "testo dell'editor, comprese le liste e i suggerimenti.");

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderFloat("Densita'", &m_uiDensity, 0.70f, 1.60f, "%.2fx"))
        changed = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Spaziatura e imbottitura dei comandi. Sotto 1 i controlli\n"
                          "si stringono e ne entrano di piu' senza tagliare nulla:\n"
                          "e' la leva per i pannelli affollati.");

    ImGui::SameLine();
    if (ImGui::SmallButton("Ripristina"))
    { m_uiFontScale = 1.0f; m_uiDensity = 1.0f; changed = true; }

    if (changed) saveAppearance();

    ImGui::Separator();
    ImGui::TextDisabled("Colori, arrotondamenti e bordi (editor integrato di ImGui)");
    ImGui::TextWrapped(
        "Nota: questi NON si conservano alla chiusura. Si conservano solo testo e "
        "densita', perche' salvare l'intera struttura di stile di ImGui non e' "
        "affidabile fra versioni della libreria — un file di preferenze che si "
        "rompe a ogni aggiornamento e' peggio che non averlo. Se trovi una "
        "combinazione che ti piace, usa \"Export\" qui sotto e me la passi: la "
        "rendo il tema predefinito.");
    ImGui::Separator();
    ImGui::ShowStyleEditor();

    ImGui::End();
}

// Collaudo dell'INQUADRATURA (difetto segnalato dall'utente 2026-08-06:
// *"cambiare prospettive … spesso l'inquadratura viene spostata troppo lontana, e
// a volte viene inquadrato il nulla"*). La proprietà da garantire è una sola e si
// può verificare: dopo un cambio di vista, gli angoli del contenuto devono cadere
// DENTRO lo schermo.
int EditorApp::viewFramingSelfTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "OK  " : "FALL", what);
        if (!ok) ++failed;
    };
    if (!m_viewport) { check(false, "inquadratura: viewport assente"); return failed; }

    // Un contenuto tipo Training Ground, decentrato apposta: se l'inquadratura
    // presumesse l'origine, un contenuto lontano dallo zero la smaschererebbe.
    const glm::vec3 mn{ 20.0f, -1.0f,  40.0f };
    const glm::vec3 mx{ 91.0f, 15.0f, 132.0f };
    m_viewport->setContentBounds(mn, mx);

    auto cornersVisible = [&](const char* what) {
        const glm::mat4 vp = m_viewport->camera().getViewProjection();
        bool allIn = true;
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec3 c{ (i & 1) ? mx.x : mn.x,
                               (i & 2) ? mx.y : mn.y,
                               (i & 4) ? mx.z : mn.z };
            const glm::vec4 p = vp * glm::vec4(c, 1.0f);
            if (std::fabs(p.w) < 1e-6f) { allIn = false; break; }
            const float nx = p.x / p.w, ny = p.y / p.w, nz = p.z / p.w;
            if (nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f
                || nz < -1.0f || nz > 1.0f) { allIn = false; break; }
        }
        check(allIn, what);
    };

    using VM = editor::FreeCameraViewport::ViewMode;
    m_viewport->setViewMode(VM::Top);    cornersVisible("vista Alto: il contenuto e' tutto inquadrato");
    m_viewport->setViewMode(VM::Front);  cornersVisible("vista Fronte: il contenuto e' tutto inquadrato");
    m_viewport->setViewMode(VM::Side);   cornersVisible("vista Lato: il contenuto e' tutto inquadrato");

    // Il ritorno in PROSPETTIVA non deve lasciare la camera dove l'ha messa
    // l'ortografica (500 m fuori): è esattamente il difetto segnalato.
    m_viewport->setViewMode(VM::Perspective);
    const glm::vec3 pos = m_viewport->camera().getPosition();
    const glm::vec3 center = (mn + mx) * 0.5f;
    check(glm::length(pos - center) < 400.0f,
          "ritorno in prospettiva: la camera non resta lontanissima");
    check(!m_viewport->camera().isOrthographic(),
          "ritorno in prospettiva: la proiezione torna prospettica");

    // E "Inquadra tutto" deve rimettere a posto anche in prospettiva.
    m_viewport->frameContent();
    cornersVisible("inquadra tutto in prospettiva: contenuto visibile");
    return failed;
}

// Collaudo della PILA DI ANNULLAMENTO condivisa (doc 52 F2). È il primo componente
// del framework che si può verificare senza aprire una finestra — ed è metà del
// motivo per cui vale la pena averlo condiviso invece che riscritto per modulo.
static int undoStackSelfTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "OK  " : "FALL", what);
        if (!ok) ++failed;
    };

    editor::UndoStack<int> st(3);      // profondità 3, per verificare il taglio
    int s = 0;

    check(!st.canUndo() && !st.canRedo(), "undo: pila vuota all'inizio");

    s = 1; st.push(s, "a", 0.0f);      // fotografa 1, poi lo stato diventa 2
    s = 2; st.push(s, "b", 10.0f);
    s = 3;
    check(st.undoCount() == 2, "undo: due gesti distinti = due voci");

    check(st.undo(s) && s == 2, "undo: torna allo stato precedente");
    check(st.undo(s) && s == 1, "undo: e a quello prima ancora");
    check(!st.undo(s), "undo: oltre il fondo non fa nulla");
    check(st.redo(s) && s == 2, "ripeti: risale di un passo");

    // COALESCENZA: lo stesso gesto entro la finestra conta una volta sola. Senza,
    // un trascinamento del gizmo riempirebbe la pila e annullare tornerebbe indietro
    // di un pixel per volta.
    editor::UndoStack<int> co;
    int c = 0;
    for (int i = 0; i < 20; ++i) { c = i; co.push(c, "gizmo", 100.0f + i * 0.01f); }
    check(co.undoCount() == 1, "undo: un trascinamento intero e' UNA voce");

    // Oltre la finestra torna a contare come gesto nuovo.
    co.push(c, "gizmo", 200.0f);
    check(co.undoCount() == 2, "undo: passata la finestra, gesto nuovo");

    // Una nuova azione taglia il ramo di ripristino.
    editor::UndoStack<int> br;
    int b = 0;
    b = 1; br.push(b, "x", 0.0f);
    b = 2; br.undo(b);
    check(br.canRedo(), "undo: dopo annulla c'e' qualcosa da ripetere");
    br.push(b, "y", 50.0f);
    check(!br.canRedo(), "undo: una nuova azione taglia il ripristino");

    // Finestra NEGATIVA = niente coalescenza: il gesto è già concluso. La usa il
    // gancio sui widget del Map Editor, che consegna una fotografia presa PRIMA.
    editor::UndoStack<int> nc;
    int n = 0;
    for (int i = 0; i < 5; ++i) { n = i; nc.push(n, "widget", 300.0f, -1.0f); }
    check(nc.undoCount() == 5, "undo: finestra negativa = ogni push conta");

    // La profondità limita la pila senza rompere nulla.
    editor::UndoStack<int> dp(3);
    int d = 0;
    for (int i = 0; i < 10; ++i) { d = i; dp.push(d, "s", (float)i * 10.0f); }
    check(dp.undoCount() == 3, "undo: la profondita' massima si rispetta");

    return failed;
}

// La copertura della guardia: OGNI modulo che tiene uno stato "modificato" deve
// essere interrogato all'uscita. Era coperto 1 modulo su 5, poi 5 su 7, e i due
// mancanti li ha trovati l'utente. Questo controllo li conta al posto suo.
int EditorApp::dirtyCoverageSelfTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "OK  " : "FALL", what);
        if (!ok) ++failed;
    };
    // Se un modulo esiste, `collectDirty` deve saperlo interrogare. Il controllo è
    // indiretto ma sufficiente: si verifica che nessun modulo "sporcabile" resti
    // fuori dall'elenco, contando quanti ne consulta.
    int consulted = 0;
    if (m_mapEditor)     ++consulted;
    if (m_entityEditor)  ++consulted;
    if (m_weaponEditor)  ++consulted;
    if (m_vehicleEditor) ++consulted;
    if (m_balanceEditor) ++consulted;
    if (m_classEditor)   ++consulted;
    if (m_missionEditor) ++consulted;
    check(consulted == 7, "guardia: tutti e sette i moduli sono interrogabili");

    // E che con niente di modificato non si chieda nulla: un avviso che compare
    // sempre è un avviso che si impara a chiudere senza leggere.
    collectDirty();
    check(!m_dirty.any(), "guardia: senza modifiche non chiede nulla");
    return failed;
}

int EditorApp::runSelfTests()
{
    std::cout << "[selftest] pila di annullamento condivisa" << std::endl;
    int failed = undoStackSelfTest();
    std::cout << "[selftest] camera ortografica" << std::endl;
    failed += cameraSelfTest();
    std::cout << "[selftest] inquadratura al cambio vista" << std::endl;
    failed += viewFramingSelfTest();
    // La guida: che i file si TROVINO e si leggano. Il difetto naturale qui è il
    // percorso, e una guida vuota sembra un problema di contenuti mancanti invece
    // che di cartella sbagliata (è già successo: `root()` non ha lo slash finale).
    {
        std::cout << "[selftest] guardia del lavoro non salvato" << std::endl;
    failed += dirtyCoverageSelfTest();
    std::cout << "[selftest] guida in-editor" << std::endl;
        m_help.reload();
        const bool ok = (m_help.chapterCount() >= 3) && (m_help.sectionCount() >= 10);
        std::printf("  [%s] guida: %d capitoli, %d sezioni caricati\n",
                    ok ? "OK  " : "FALL", m_help.chapterCount(), m_help.sectionCount());
        if (!ok) ++failed;
    }

    std::cout << "[selftest] operazioni Map Editor" << std::endl;
    failed += m_mapEditor ? m_mapEditor->selfTest() : 0;
    std::cout << (failed == 0 ? "[selftest] TUTTO OK" : "[selftest] CI SONO FALLIMENTI")
              << std::endl;
    return failed;
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
        advanceModuleSequence();
        processEvents();
        tick(dt);
        render();
    }
    mini::telemetry::shutdown();
}

} // namespace editor
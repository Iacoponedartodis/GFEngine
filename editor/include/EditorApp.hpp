#pragma once
#include "mini/game/data/ContentValidation.hpp"   // ADR-018
#include "ui/HelpBrowser.hpp"
#include "framework/DirtyGuard.hpp"   // lavoro non salvato, da tutti i moduli (doc 52 F3)
#include <memory>
#include <string>
#include <vector>

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
    // startModule: apre i moduli indicati senza passare dal mouse, così un crash
    // di modulo si riproduce da riga di comando e produce una traccia coi simboli
    // invece di un racconto (ADR-050). Un solo nome (`--module entity`) apre quel
    // modulo; una lista (`--module weapon,entity`) li ATTRAVERSA, ed è la forma
    // che conta davvero: quasi nessun difetto di modulo sta nell'apertura, sta nel
    // passaggio da un modulo all'altro con lo stato che il precedente ha lasciato.
    // seqFrames: quanti frame dura ogni passo (0 = predefinito). Abbassarlo
    // trasforma il percorso in un martellamento di transizioni.
    explicit EditorApp(const std::string& startModule = "", int seqFrames = 0);
    ~EditorApp();

    void run();

    // ── Aspetto dell'interfaccia (richiesta utente 2026-08-06) ───────────
    // *"un tool che mi permette di modificare l'interfaccia grafica, per risolvere
    // da solo problemi tipo tasti troppo in mezzo, scritte che non servono o che
    // non si vedono"*. Copre densità, dimensione del testo e colori — non lo
    // SPOSTAMENTO dei comandi, che richiede una barra configurabile (doc 51).
    // Si salva SOLO una manciata di valori scelti, non `ImGuiStyle` grezzo:
    // scaricare la struct su file non è affidabile fra versioni di ImGui
    // (ocornut/imgui #8659, #101), e un file di preferenze che si rompe a ogni
    // aggiornamento è peggio che non averlo.
    // Uscita protetta: chiedere prima di buttare via modifiche non salvate.
    void requestQuit();
    void drawQuitPrompt();
    void collectDirty();
    DirtyGuard m_dirty;

    void drawAppearanceWindow();
    void loadAppearance();
    void saveAppearance() const;
    // Esegue i collaudi headless dei moduli e ritorna il numero di controlli
    // falliti. Nessuna finestra, nessun frame: si può mettere in una build.
    int  runSelfTests();
    int  viewFramingSelfTest();

private:
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_glCtx   = nullptr;
    bool          m_running = false;

    ActiveModule m_active = ActiveModule::Home;
    // Modulo attivo al frame precedente: serve a rilasciare la cattura del mouse
    // quando si lascia un modulo con viewport (vedi tick()). Inizializzato uguale
    // a m_active così il primo frame non è mai un "cambio".
    ActiveModule m_prevActive = ActiveModule::Home;

    // Aspetto: i pochi valori che si salvano davvero (vedi drawAppearanceWindow).
    bool  m_showAppearance   = false;
    bool  m_showImGuiMetrics = false;
    bool  m_askQuit          = false;
    bool  m_showHelp         = false;
    HelpBrowser m_help;
    float m_uiFontScale      = 1.0f;   // dimensione del testo
    float m_uiDensity        = 1.0f;   // spaziatura/imbottitura: quanto "respira"
    float m_uiAppliedDensity = 1.0f;   // ultima applicata: ScaleAllSizes è cumulativo

    // Percorso automatico fra moduli (`--module a,b,c`): avanza di un passo ogni
    // m_seqFrames frame, poi si ferma sull'ultimo. Vuoto = nessun automatismo.
    std::vector<ActiveModule> m_seq;
    int m_seqIdx    = 0;
    int m_seqFrames = 240;
    int m_frameNo   = 0;
    void advanceModuleSequence();

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
#include "EditorApp.hpp"
#include "util/StartupOptions.hpp"
#include <mini/core/Telemetry.hpp>
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char** argv)
{
    // --module <nome>: apre subito quel modulo invece della Home. Serve a
    // riprodurre un crash di modulo senza mouse, quindi con la traccia
    // simbolica del build Debug invece che con un racconto.
    std::string startModule;
    int  moduleFrames = 0;   // 0 = lascia il valore predefinito
    bool crashTest    = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--module" && i + 1 < argc) startModule = argv[++i];
        else if (a.rfind("--module=", 0) == 0) startModule = a.substr(9);
        else if (a == "--module-frames" && i + 1 < argc) moduleFrames = std::atoi(argv[++i]);
        else if (a == "--editor-selftest") editor::startup::g_selfTest = true;
        else if (a == "--crash-test") crashTest = true;
        else if (a == "--entity" && i + 1 < argc)
        {
            editor::startup::g_entitySelectSet = true;
            editor::startup::g_entitySelect = argv[++i];
        }
        else if (a == "--struct-tab")
        {
            editor::startup::g_structTabSet = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') editor::startup::g_structTab = argv[++i];
        }
    }

    // --crash-test: provoca un access violation dentro una fase nota, per
    // verificare che la RETE funzioni davvero — che il report nomini la fase e
    // risolva i simboli. Un miglioramento al crash reporting che non si è mai
    // visto funzionare è una speranza, non uno strumento (KI #98).
    if (crashTest)
    {
        mini::telemetry::init("GFEditor");
        mini::telemetry::setPhase("crash-test volontario");
        std::cerr << "[GFEditor] --crash-test: provoco un access violation." << std::endl;
        volatile int* p = nullptr;
        *p = 1;
        return 0;   // irraggiungibile
    }

    try
    {
        editor::EditorApp app(startModule, moduleFrames);
        if (editor::startup::g_selfTest) return app.runSelfTests();
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GFEditor] Errore fatale: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
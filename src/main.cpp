#include "mini/core/Application.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/data/ContentValidation.hpp"

#include <cstring>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    bool autoSim        = false;
    bool validateOnly   = false;   // --validate: gate contenuti headless (ADR-018)
    std::string mapOverride;
    int  stressAiCount  = 0;   // --stress N: N AI per team nel sim (profiling)
    std::string missionId;     // --mission <id>: missione attiva (ADR-019)
    std::string classId;       // --class <id>: classe del giocatore (doc 14)
    int  simTicks       = 0;   // --sim-ticks N: sim di durata FISSA (misure confrontabili)
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--direct-prematch") == 0)
            directPreMatch = true;
        else if (std::strcmp(argv[i], "--sandbox") == 0)
            sandbox = true;
        else if (std::strcmp(argv[i], "--sim") == 0) // sandbox + simulazione AI
        { sandbox = true; autoSim = true; }
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            mapOverride = argv[++i];   // mappa iniziale (test/debug, R3)
        else if (std::strcmp(argv[i], "--stress") == 0 && i + 1 < argc)
        {   // stress test AI: forza sim + N AI per team (per profilare con Tracy)
            sandbox = true; autoSim = true;
            stressAiCount = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--mission") == 0 && i + 1 < argc)
            missionId = argv[++i];   // ADR-019: attiva una missione (test/debug)
        else if (std::strcmp(argv[i], "--class") == 0 && i + 1 < argc)
            classId = argv[++i];   // 14_ClassSystem: loadout confezionato
        else if (std::strcmp(argv[i], "--sim-ticks") == 0 && i + 1 < argc)
        {   // sim a durata FISSA in tick: misure confrontabili fra due run/build
            sandbox = true; autoSim = true;
            simTicks = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--validate") == 0)
            validateOnly = true;
    }

    // ── --validate: terzo consumatore del gate (24_ContentValidation, ADR-018) ──
    // Nessuna finestra, nessun mondo: carica il registry, applica LE STESSE regole
    // di runtime ed editor, stampa diagnostiche azionabili + JSONL, exit code != 0
    // se c'è contenuto critico invalido. È ciò che rende il gate usabile da CI e
    // da un LLM senza aprire l'editor.
    if (validateOnly)
    {
        mini::telemetry::init("GFEngine-validate");
        const std::string dataPath = mini::getDataPath();
        mini::DefinitionRegistry registry;
        registry.loadAll(dataPath);

        // Salute tattica delle mappe (doc 41 B4): stesse regole del pannello editor.
        // Headless → utilizzabile in CI e senza aprire l'editor.
        for (const auto& [mapId, map] : registry.maps())
        {
            const auto defects = mini::analyzeTacticalHealth(map);
            if (defects.empty()) continue;
            int problems = 0;
            for (const auto& d : defects) if (d.severity == 1) ++problems;
            std::cout << "[Tattica] " << mapId << ": " << problems << " problemi, "
                      << (defects.size() - problems) << " avvisi\n";
            for (const auto& d : defects)
                std::cout << "          " << (d.severity == 1 ? "! " : "- ") << d.text << "\n";
        }

        const mini::Diagnostics diags = mini::validateContent(registry, dataPath);
        const bool failed = mini::reportDiagnostics(diags, /*printToStdout=*/true);
        const int errors   = mini::countBy(diags, mini::telemetry::Level::Error);
        const int warnings = mini::countBy(diags, mini::telemetry::Level::Warn);
        std::cout << "\n[Validate] " << errors << " errori, "
                  << warnings << " warning.\n"
                  << (failed ? "[Validate] CONTENUTO CRITICO INVALIDO\n"
                             : "[Validate] contenuto valido\n");
        mini::telemetry::flushEvents();
        return failed ? 1 : 0;
    }

    mini::Application app;
    app.run(directPreMatch, sandbox, autoSim, mapOverride, stressAiCount, missionId,
            classId, simTicks);
    return 0;
}

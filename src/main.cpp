#include "mini/core/Application.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/data/ContentValidation.hpp"
#include "mini/ecs/systems/AiSystem.hpp"   // --trace-ai: scatola nera per-agente
#include "mini/game/nav/NavCheck.hpp"      // --navcheck: navmesh vero, headless (doc 53 L5)

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    bool autoSim        = false;
    bool validateOnly   = false;   // --validate: gate contenuti headless (ADR-018)
    bool navCheckOnly   = false;   // --navcheck: navmesh VERO, senza finestra (doc 53 L5)
    std::string mapOverride;
    int  stressAiCount  = 0;   // --stress N: N AI per team nel sim (profiling)
    std::string missionId;     // --mission <id>: missione attiva (ADR-019)
    std::string classId;       // --class <id>: classe del giocatore (doc 14)
    int  simTicks       = 0;   // --sim-ticks N: sim di durata FISSA (misure confrontabili)
    int  traceAi        = 0;   // --trace-ai <id>: scatola nera per-agente (-1 = tutte)
    bool verboseTelemetry = false;   // --telemetry-verbose: abbassa la soglia a Debug
    mini::Application::DevLaunch devLaunch;   // --at x,z e --walk (doc 53 L4)
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
        else if (std::strcmp(argv[i], "--trace-ai") == 0 && i + 1 < argc)
            traceAi = std::atoi(argv[++i]);   // KI #86: osserva UNA unità, tick per tick
        else if (std::strcmp(argv[i], "--telemetry-verbose") == 0)
            verboseTelemetry = true;   // eventi per-evento/per-entità (Debug level)
        else if (std::strcmp(argv[i], "--validate") == 0)
            validateOnly = true;
        else if (std::strcmp(argv[i], "--navcheck") == 0)
            navCheckOnly = true;
        else if (std::strcmp(argv[i], "--walk") == 0)
        {
            // `--walk` (doc 53 L4): si entra DIRETTAMENTE nella mappa, da soli, per
            // provare com'è percorrerla. È la sandbox con zero manichini — non un
            // game mode nuovo: la sandbox porta già tutto il resto (geometria,
            // strutture, veicoli, controller, nessun menu), e duplicarla per
            // toglierle i bersagli avrebbe prodotto un secondo mode destinato a
            // restare indietro.
            sandbox = true;
            devLaunch.walkOnly = true;
        }
        else if (std::strcmp(argv[i], "--at") == 0 && i + 1 < argc)
        {
            // `--at x,z` (doc 53 L4): il giocatore nasce lì invece che allo spawn
            // della mappa. È ciò che rende "Prova da qui" dell'editor un ciclo
            // chiuso: si costruisce, si preme, si cammina in QUEL punto.
            // Due forme: `x,z` (a terra, come prima) e `x,y,z` (con la quota, per
            // nascere sul piano giusto in una mappa a più livelli). Due valori
            // restano validi: nessuno script esistente si rompe.
            const std::string v = argv[++i];
            std::vector<float> n;
            for (std::size_t p = 0; p <= v.size(); )
            {
                const std::size_t c = v.find(',', p);
                const std::string tok = v.substr(p, c == std::string::npos ? c : c - p);
                if (!tok.empty()) n.push_back((float)std::atof(tok.c_str()));
                if (c == std::string::npos) break;
                p = c + 1;
            }
            if (n.size() == 2)
            { devLaunch.x = n[0]; devLaunch.z = n[1]; devLaunch.hasSpawn = true; }
            else if (n.size() == 3)
            {
                devLaunch.x = n[0]; devLaunch.y = n[1]; devLaunch.z = n[2];
                devLaunch.hasSpawn = true; devLaunch.hasY = true;
            }
            else
                std::cerr << "[--at] serve 'x,z' oppure 'x,y,z' (es. --at 12.5,4,-8): "
                             "argomento ignorato\n";
        }
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

    // ── --navcheck: il navmesh VERO, senza finestra (doc 53 L5) ──────────────
    // Perché esiste. `validateNavmesh` vive nell'editor e richiede un contesto
    // grafico: chi non può aprire una finestra — la CI, e soprattutto **io** — non
    // può esaminare il navmesh di una mappa. Il risultato è che sulle isole ho
    // ragionato per due giri sulla descrizione dell'utente, e ho sbagliato: ho
    // chiamato "schegge" zone da decine di metri quadri (changelog 197).
    //
    // Testuale dell'utente: *"il fatto che tu non riesca ad esaminare bene cose come
    // il navmesh è un problema … meno vedi più fai cose su basi sbagliate"*. Ha
    // ragione, e questo è il rimedio: STESSO NavManager del gioco e dell'editor,
    // nessuna terza costruzione.
    if (navCheckOnly)
    {
        mini::telemetry::init("GFEngine-navcheck");
        mini::DefinitionRegistry registry;
        registry.loadAll(mini::getDataPath());
        int worst = 0;
        for (const auto& [mapId, map] : registry.maps())
        {
            if (!mapOverride.empty() && mapId != mapOverride) continue;
            worst = std::max(worst, mini::navcheck::report(mapId, map, std::cout));
        }
        mini::telemetry::flushEvents();
        return worst;
    }

    // Scatola nera per-agente (KI #86): si accende PRIMA di costruire il mondo, così
    // la prima unità che si ferma è già osservata.
    // La traccia per-agente emette a livello Debug: chiederla implica alzarne la
    // verbosità, altrimenti `--trace-ai` sarebbe silenzioso — un flag che non fa
    // nulla è il difetto che abbiamo appena scoperto in `--stress`.
    if (verboseTelemetry || traceAi != 0)
        mini::telemetry::setMinLevel(mini::telemetry::Level::Debug);
    mini::AiSystem::setTraceEntity(traceAi);

    mini::Application app;
    app.run(directPreMatch, sandbox, autoSim, mapOverride, stressAiCount, missionId,
            classId, simTicks, devLaunch);
    return 0;
}

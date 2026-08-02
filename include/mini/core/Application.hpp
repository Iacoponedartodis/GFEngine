#pragma once
#include <string>

namespace mini
{
class Window;

// Radice dei dati risolta dall'exe (non dal CWD). Esposta per `--validate`
// (ADR-018): il gate deve validare ESATTAMENTE la data/ che il gioco carica.
std::string getDataPath();

class Application
{
public:
    // `simTicks > 0` (--sim-ticks N): esce dopo N TICK DI SIMULAZIONE invece che a
    // tempo. Serve a rendere le misure CONFRONTABILI: `--sim` + timeout esterno gira
    // finché la macchina lo consente, quindi i totali accumulati (eventi di
    // combattimento, contatori di telemetria) variano col carico — misurato ±10% sulla
    // STESSA build, abbastanza da nascondere una piccola regressione. A tick fissi la
    // quantità di simulazione è identica fra due run, e i totali tornano confrontabili.
    void run(bool directPreMatch = false, bool sandbox = false, bool autoSim = false,
             const std::string& mapOverride = "", int stressAiCount = 0,
             const std::string& missionId = "",    // ADR-019: missione attiva
             const std::string& classId = "",      // 14_ClassSystem
             int simTicks = 0);

    void initialize();
    void shutdown();
    void requestShutdown();

private:
    bool m_running = false;
    bool m_shootRequested = false;
    void processEvents(Window& window);
};

} // namespace mini
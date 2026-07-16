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
    void run(bool directPreMatch = false, bool sandbox = false, bool autoSim = false,
             const std::string& mapOverride = "", int stressAiCount = 0,
             const std::string& missionId = "",    // ADR-019: missione attiva
             const std::string& classId = "");     // 14_ClassSystem

    void initialize();
    void shutdown();
    void requestShutdown();

private:
    bool m_running = false;
    bool m_shootRequested = false;
    void processEvents(Window& window);
};

} // namespace mini
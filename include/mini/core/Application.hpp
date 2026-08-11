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
    // ── COME CI HA AVVIATI L'EDITOR (doc 53 L4) ──────────────────────────
    // Serve a chiudere il ciclo di costruzione: dall'editor si preme "Prova da qui"
    // e ci si ritrova a camminare NEL punto che si stava guardando. La letteratura
    // di level design è unanime — l'errore più frequente è la scala, e si vede solo
    // attraversando lo spazio alla velocità del giocatore. Volare nell'editor non lo
    // sostituisce, e nemmeno passare da un menu.
    struct DevLaunch
    {
        // `--at x,z` oppure `--at x,y,z`: dove nasce il giocatore, invece dello
        // spawn della mappa. Con la `y` (la quota della telecamera dell'editor) si
        // nasce sulla superficie più alta **sotto** quella quota: posando la
        // telecamera sopra una passerella si nasce SULLA passerella, non sul
        // pavimento sotto. Senza, "la superficie più alta a quelle coordinate" e
        // "quella su cui sono" coincidono solo su una mappa piatta.
        bool  hasSpawn = false;
        bool  hasY     = false;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        // `--walk`: si entra DIRETTAMENTE nella mappa, da soli. Tecnicamente è la
        // sandbox con zero manichini — non un terzo game mode. La sandbox ha già
        // tutto ciò che serve (geometria, strutture, veicoli, controller, nessun
        // menu, nessuna condizione di vittoria); quello che NON serve a una prova
        // di percorribilità sono i bersagli. Un mode nuovo avrebbe duplicato il
        // resto e sarebbe rimasto indietro al primo cambio (ADR-014).
        bool  walkOnly = false;
    };

    void run(bool directPreMatch = false, bool sandbox = false, bool autoSim = false,
             const std::string& mapOverride = "", int stressAiCount = 0,
             const std::string& missionId = "",    // ADR-019: missione attiva
             const std::string& classId = "",      // 14_ClassSystem
             int simTicks = 0,
             DevLaunch devLaunch = {});

    void initialize();
    void shutdown();
    void requestShutdown();

private:
    bool m_running = false;
    bool m_shootRequested = false;
    void processEvents(Window& window);
};

} // namespace mini
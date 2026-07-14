#pragma once
// Telemetry — telemetria e debugging estremo (ADR-013).
// Quattro sistemi in sinergia, tutti su file dentro _telemetry_data/
// (creata automaticamente all'avvio, mai nella root del progetto):
//   1. Logging (spdlog)          → _telemetry_data/engine_run.log
//   2. Dump stato su richiesta   → _telemetry_data/game_state.json (tasto F12)
//   3. Input recorder            → _telemetry_data/input_history.log
//   4. Crash net (cpptrace)      → _telemetry_data/crash_report.txt + terminale
//
// L'header è volutamente leggero: nessun include spdlog/cpptrace qui, così i
// sistemi del motore possono loggare senza trascinarsi dipendenze pesanti.

#include <cstdint>
#include <functional>
#include <string>

// fwd (nlohmann): il dump accetta un json già costruito dal chiamante
#include <nlohmann/json_fwd.hpp>

namespace mini::telemetry
{

// Inizializza logger, crash handler e cartella. appName finisce nei log.
// Idempotente: la seconda chiamata è un no-op.
void init(const char* appName);
void shutdown();

// ── Logging (livelli spdlog) ──────────────────────────────────────────────
void logTrace(const std::string& msg);
void logInfo (const std::string& msg);
void logWarn (const std::string& msg);
void logError(const std::string& msg);

// ── Event log strutturato JSONL (LLM-observable, ADR-016) ─────────────────
// Sink ADDITIVO accanto a engine_run.log (NON lo sostituisce, ADR-013 resta):
// engine_run.log per gli umani, session_latest.jsonl per il parsing perfetto.
// Ogni evento è UNA riga JSON valida in _telemetry_data/session_latest.jsonl:
//   {"frame":N,"time":T,"system":"AI","level":"WARN","msg":"...","data":{...}}
// Riusa il frame counter (frame()) e nlohmann/json già in casa. Il buffering e
// il flush li gestisce spdlog; ERROR/FATAL forzano il flush immediato su disco.
enum class Level { Trace, Debug, Info, Warn, Error, Fatal };

void event(Level level, const char* system, const std::string& msg,
           const nlohmann::json& data);
void event(Level level, const char* system, const std::string& msg);
void flushEvents();   // forza la scrittura del buffer JSONL (es. a fine frame)

// ── Frame counter (per correlare log/input/dump) ──────────────────────────
void     beginFrame();   // chiamare una volta per frame nel main loop
uint64_t frame();

// ── Input recorder ────────────────────────────────────────────────────────
// Registra un evento input (già formattato) con il frame corrente.
void recordInput(const std::string& event);

// ── Dump stato ────────────────────────────────────────────────────────────
// Scrive lo stato (json costruito dal chiamante, arricchito con frame,
// timestamp e memoria) in game_state.json. Ritorna false su errore I/O.
bool dumpGameState(const nlohmann::json& state);

// Callback per il dump automatico su crash (ADR-016 Phase 4): il chiamante
// (Application) registra una lambda che costruisce e scrive lo stato completo;
// il crash net la invoca best-effort DOPO aver salvato crash_report.txt
// (protetta da try/catch + guardia di ri-entranza). Passa {} per disattivarla.
void setStateDumpCallback(std::function<void()> cb);

// ── Diagnostica ───────────────────────────────────────────────────────────
float       memoryUsageMB();   // working set del processo (0 se non disponibile)
std::string dataDir();         // percorso assoluto di _telemetry_data/

} // namespace mini::telemetry

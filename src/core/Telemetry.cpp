// Telemetry.cpp — implementazione telemetria (ADR-013).
#include "mini/core/Telemetry.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cpptrace/cpptrace.hpp>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <psapi.h>
#endif

namespace fs = std::filesystem;

namespace mini::telemetry
{

namespace
{
bool                            g_initialized = false;
std::string                     g_dir;                 // _telemetry_data/ assoluta
std::shared_ptr<spdlog::logger> g_log;                 // engine_run.log + console
std::shared_ptr<spdlog::logger> g_input;               // input_history.log
std::shared_ptr<spdlog::logger> g_events;              // session_latest.jsonl (ADR-016)
std::atomic<uint64_t>           g_frame{0};
std::chrono::steady_clock::time_point g_start;         // per il campo "time" del JSONL
std::function<void()>           g_stateDumpCb;         // dump stato su crash (Phase 4)
std::atomic<bool>               g_dumping{false};       // guardia ri-entranza

// ── Mappe Level (ADR-016) ─────────────────────────────────────────────────
const char* levelStr(Level l)
{
    switch (l) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "INFO";
}
spdlog::level::level_enum toSpd(Level l)
{
    switch (l) {
        case Level::Trace: return spdlog::level::trace;
        case Level::Debug: return spdlog::level::debug;
        case Level::Info:  return spdlog::level::info;
        case Level::Warn:  return spdlog::level::warn;
        case Level::Error: return spdlog::level::err;
        case Level::Fatal: return spdlog::level::critical;
    }
    return spdlog::level::info;
}

// ── Risoluzione cartella: root progetto (3 livelli sopra l'exe, come data/),
//    fallback accanto all'exe. Creata se assente. ─────────────────────────
std::string resolveDir()
{
    char* base = SDL_GetBasePath();
    fs::path exeDir = base ? base : ".";
    if (base) SDL_free(base);

    std::error_code ec;
    fs::path root = fs::canonical(exeDir / "../../..", ec);
    fs::path dir  = (!ec && fs::exists(root / "data", ec))
                    ? root / "_telemetry_data"
                    : exeDir / "_telemetry_data";

    fs::create_directories(dir, ec); // idempotente
    return dir.string();
}

// ── Crash net: stack trace su terminale + crash_report.txt ───────────────
void writeCrashReport(const std::string& reason)
{
    const std::string trace = cpptrace::generate_trace().to_string();

    // Terminale (sempre)
    fprintf(stderr, "\n========== CRASH: %s ==========\n%s\n",
            reason.c_str(), trace.c_str());
    fflush(stderr);

    // File (best-effort: siamo in un contesto di crash)
    if (!g_dir.empty())
    {
        std::ofstream f(g_dir + "/crash_report.txt");
        if (f.is_open())
        {
            const auto now = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            f << "GFEngine crash report\n"
              << "reason: " << reason << "\n"
              << "frame:  " << g_frame.load() << "\n"
              << "time:   " << now << " (unix)\n\n"
              << trace << "\n";
        }
    }
    if (g_log)
    {
        g_log->critical("CRASH: {} (frame {}) — stack trace in crash_report.txt",
                        reason, g_frame.load());
        g_log->flush();
    }

    // Dump stato completo su crash (Phase 4), best-effort: DOPO aver salvato la
    // stack trace (già al sicuro), con guardia di ri-entranza + try/catch così
    // un eventuale ri-crash del dump non manda in loop l'handler.
    if (g_stateDumpCb && !g_dumping.exchange(true))
    {
        try { g_stateDumpCb(); }
        catch (...) { fprintf(stderr, "[telemetry] dump stato su crash fallito\n"); }
    }
}

#ifdef _WIN32
LONG WINAPI sehHandler(EXCEPTION_POINTERS* info)
{
    const DWORD code = info && info->ExceptionRecord
                       ? info->ExceptionRecord->ExceptionCode : 0;
    char reason[64];
    snprintf(reason, sizeof(reason), "eccezione SEH 0x%08lX%s",
             (unsigned long)code,
             code == EXCEPTION_ACCESS_VIOLATION ? " (access violation)" : "");
    writeCrashReport(reason);
    return EXCEPTION_CONTINUE_SEARCH; // lascia comunque morire il processo
}
#endif

void onTerminate()
{
    std::string reason = "std::terminate";
    if (auto ex = std::current_exception())
    {
        try { std::rethrow_exception(ex); }
        catch (const std::exception& e) { reason += std::string(": ") + e.what(); }
        catch (...)                     { reason += ": eccezione non-std"; }
    }
    writeCrashReport(reason);
    std::abort();
}
} // namespace

void init(const char* appName)
{
    if (g_initialized) return;
    g_initialized = true;

    g_dir = resolveDir();

    // File di log PER-APP: editor ed engine girano spesso in contemporanea
    // (l'editor lancia il gioco) — un file condiviso veniva troncato e
    // intrecciato dal secondo processo (visto nei log del 2026-07-09).
    const bool isEditor = std::string(appName).find("Editor") != std::string::npos;
    const std::string logName  = isEditor ? "editor_run.log" : "engine_run.log";

    // ── Logger principale: file (TRACE) + console (WARN+) ────────────────
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        g_dir + "/" + logName, /*truncate=*/true);
    fileSink->set_level(spdlog::level::trace);

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::warn);

    g_log = std::make_shared<spdlog::logger>(
        "engine", spdlog::sinks_init_list{fileSink, consoleSink});
    g_log->set_level(spdlog::level::trace);
    g_log->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    g_log->flush_on(spdlog::level::warn);
    spdlog::register_logger(g_log);
    spdlog::flush_every(std::chrono::seconds(3));

    // ── Input recorder: file separato (per-app), pattern minimale ─────────
    auto inputSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        g_dir + "/" + (isEditor ? "editor_input_history.log"
                                : "input_history.log"), /*truncate=*/true);
    g_input = std::make_shared<spdlog::logger>("input", inputSink);
    g_input->set_level(spdlog::level::trace);
    g_input->set_pattern("%v");
    spdlog::register_logger(g_input);

    // ── Event log JSONL (ADR-016): una riga JSON per evento, pattern "%v"
    //    (nessun prefisso spdlog), flush immediato su ERROR/FATAL ───────────
    g_start = std::chrono::steady_clock::now();
    auto eventsSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        g_dir + "/" + (isEditor ? "editor_session.jsonl"
                                : "session_latest.jsonl"), /*truncate=*/true);
    g_events = std::make_shared<spdlog::logger>("events", eventsSink);
    g_events->set_level(spdlog::level::trace);
    g_events->set_pattern("%v");
    g_events->flush_on(spdlog::level::err);   // ERROR/FATAL su disco subito
    spdlog::register_logger(g_events);

    // ── Crash net ─────────────────────────────────────────────────────────
#ifdef _WIN32
    SetUnhandledExceptionFilter(sehHandler);
#endif
    std::set_terminate(onTerminate);

    g_log->info("=== {} avviato — telemetria attiva in '{}' ===", appName, g_dir);
    g_log->info("memoria iniziale: {:.1f} MB", memoryUsageMB());
}

void shutdown()
{
    if (!g_initialized) return;
    if (g_log)
    {
        g_log->info("=== shutdown pulito al frame {} — memoria {:.1f} MB ===",
                    g_frame.load(), memoryUsageMB());
        g_log->flush();
    }
    if (g_input)  g_input->flush();
    if (g_events) g_events->flush();
    spdlog::shutdown();
    g_initialized = false;
}

void logTrace(const std::string& m) { if (g_log) g_log->trace(m); }
void logInfo (const std::string& m) { if (g_log) g_log->info(m); }
void logWarn (const std::string& m) { if (g_log) g_log->warn(m); }
void logError(const std::string& m) { if (g_log) g_log->error(m); }

// ── Event log JSONL (ADR-016) ─────────────────────────────────────────────
void event(Level level, const char* system, const std::string& msg,
           const nlohmann::json& data)
{
    if (!g_events) return;
    const double t = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - g_start).count();
    nlohmann::json j;
    j["frame"]  = g_frame.load();
    j["time"]   = t;
    j["system"] = system ? system : "";
    j["level"]  = levelStr(level);
    j["msg"]    = msg;
    j["data"]   = data;
    // "{}" come format-string letterale: le graffe del JSON sono nell'ARGOMENTO,
    // così fmt/spdlog non prova a interpretarle come segnaposto (una riga = un
    // oggetto JSON). j.dump() senza indent = compatto, una sola riga.
    g_events->log(toSpd(level), "{}", j.dump());
    if (level >= Level::Error) g_events->flush();   // ERROR/FATAL: subito su disco
}

void event(Level level, const char* system, const std::string& msg)
{
    event(level, system, msg, nlohmann::json::object());
}

void flushEvents() { if (g_events) g_events->flush(); }

void setStateDumpCallback(std::function<void()> cb) { g_stateDumpCb = std::move(cb); }

void beginFrame()
{
    const uint64_t f = ++g_frame;
    // Battito di memoria ogni ~10s a 60fps: abbastanza per vedere i leak
    // nel log senza inondarlo.
    if (g_log && f % 600 == 0)
        g_log->trace("frame {} — memoria {:.1f} MB", f, memoryUsageMB());
}

uint64_t frame() { return g_frame.load(); }

void recordInput(const std::string& event)
{
    if (g_input)
        g_input->trace("frame {:>8} | {}", g_frame.load(), event);
}

bool dumpGameState(const nlohmann::json& state)
{
    if (g_dir.empty()) return false;

    nlohmann::json j = state;
    j["frame"]      = g_frame.load();
    j["memory_mb"]  = memoryUsageMB();
    j["timestamp"]  = std::chrono::system_clock::to_time_t(
                          std::chrono::system_clock::now());

    std::ofstream f(g_dir + "/game_state.json");
    if (!f.is_open())
    {
        logError("dumpGameState: impossibile scrivere game_state.json");
        return false;
    }
    f << j.dump(4) << "\n";
    logInfo("game_state.json scritto (frame " + std::to_string(g_frame.load()) + ")");
    // Feedback VISIBILE sul terminale del gioco (la console spdlog mostra
    // solo WARN+): senza questo l'utente non sa che il dump è avvenuto.
    printf("[F12] game_state.json scritto (frame %llu) -> %s\n",
           (unsigned long long)g_frame.load(), g_dir.c_str());
    fflush(stdout);
    return true;
}

float memoryUsageMB()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (float)pmc.WorkingSetSize / (1024.0f * 1024.0f);
#endif
    return 0.0f;
}

std::string dataDir() { return g_dir; }

} // namespace mini::telemetry

# 21 — Telemetry & Debugging

Sistema di telemetria e osservabilità (ADR-013 = base, ADR-016 = sink JSONL LLM-observable).
Modulo `mini::telemetry` (`include/mini/core/Telemetry.hpp`, `src/core/Telemetry.cpp`), header
leggero (nessun include spdlog/cpptrace esposto), linkato a ENTRAMBI i binari. Tutti gli
artefatti in `_telemetry_data/` (root progetto, auto-creata, gitignored). Stato: **in force**.

## A cosa serve
Rendere il motore trasparente via file passabili tra sessioni di debug — incluse quelle di un
agente LLM. Diagnosi post-mortem senza riprodurre a mano: log + input history + crash report +
event log strutturato + dump stato. Il **frame counter** (`frame()`) correla tutti i file.

## I cinque sottosistemi
1. **Logging (spdlog)** → `engine_run.log` (o `editor_run.log`). Livelli TRACE/INFO/WARN/ERROR;
   file sink a TRACE, console a WARN+; flush su warn + ogni 3 s. Leggibile dagli UMANI.
2. **Event log JSONL (ADR-016)** → `session_latest.jsonl` (`editor_session.jsonl`). Una riga =
   un oggetto JSON valido, per il parsing PERFETTO (LLM/tool). Vedi sotto.
3. **Input recorder** → `input_history.log`: ogni KEY_DOWN/UP e MOUSE_DOWN/UP col numero frame
   (per replicare i crash).
4. **Crash net (cpptrace)** → `crash_report.txt` + terminale: `SetUnhandledExceptionFilter` (SEH,
   incl. access violation) + `std::set_terminate` → stack trace con frame e motivo. Su crash
   invoca anche il dump stato (best-effort, vedi callback).
5. **Dump stato** → `game_state.json`: snapshot completo (vedi schema sotto), triggerato da
   F12 / fine-partita / crash.

## API (Telemetry.hpp)
```
void init(const char* appName);   void shutdown();     // idempotenti
// logging umano
void logTrace/logInfo/logWarn/logError(const std::string&);
// frame counter (correla tutti i file)
void beginFrame();  uint64_t frame();
// input recorder
void recordInput(const std::string& event);
// event log JSONL (ADR-016)
enum class Level { Trace, Debug, Info, Warn, Error, Fatal };
void event(Level, const char* system, const std::string& msg, const nlohmann::json& data);
void event(Level, const char* system, const std::string& msg);   // senza data
void flushEvents();               // svuota il buffer JSONL (a fine frame)
// dump stato
bool dumpGameState(const nlohmann::json& state);            // arricchito con frame/mem/timestamp
void setStateDumpCallback(std::function<void()> cb);        // invocata dal crash net
// diagnostica
float memoryUsageMB();   std::string dataDir();
```

## Schema di una riga JSONL
```json
{"frame":N,"time":T,"system":"AI","level":"INFO","msg":"...","data":{...}}
```
- `frame` = frame counter; `time` = secondi dallo start (steady_clock).
- `system`/`level`/`msg` = categoria / livello / sommario leggibile.
- `data` = oggetto JSON libero con lo stato esatto delle variabili.
- Buffering: gestito da spdlog (sink dedicato, pattern `%v`); `flush_on(err)` + flush esplicito
  su ERROR/FATAL; `flushEvents()` chiamato a fine frame nel main loop. `j.dump()` compatto,
  passato come ARGOMENTO a spdlog (`"{}"`) — mai come format-string (le graffe del JSON!).

## Hook attuali (dove nascono gli eventi)
| system | msg | data | dove |
|---|---|---|---|
| Engine | session start | flag avvio | Application (avvio) |
| GameMode | mode created | mode_id | GameModeFactory |
| GameMode | Ticket bleed | tickets_ally/enemy, posts, drained | ConquestMode::updateObjectiveRules |
| GameMode | match end | outcome | Application (Win/Lose) |
| CommandPost | Capture started / Capture update | cp_id(label), progress, owner | CommandPosts::update |
| AI | state change | bot_id, state, pos, target_pos | AiSystem (solo sul CAMBIO stato) |
| AI | stuck | bot_id, state, pos, stuck_time | AiSystem (WARN, una per episodio; NON in Alert) |
| Nav | navmesh built / sample path | polys, danger/cover_polys, bounds, waypoints | Application::initWorld |

## Schema game_state.json (dump completo, Phase 4)
Costruito da `Application::buildStateDump(reason)`: `app`, `dump_reason` (f12/match_win/lose/
crash), `game_state`, `camera{pos,forward}`, `player{hp,dead,weapon,heat}`, `tickets{team1,2}`,
e **`entities[]`** con OGNI entità attiva: `{id, pos:[x,y,z], team, hp/hp_max, ai_state
(Patrol/Alert/Hunt/Search), goal:[x,z] (lastKnown), kind (bullet/vehicle)}`. Arricchito da
`dumpGameState` con `frame`, `memory_mb`, `timestamp`.

## Come aggiungere un evento
```cpp
#include "mini/core/Telemetry.hpp"
#include <nlohmann/json.hpp>   // serve la libreria completa per costruire i data
telemetry::event(telemetry::Level::Info, "MioSistema", "cosa è successo",
                 {{"chiave", valore}, {"pos", {x, y, z}}});
```
Regole: **eventi DISCRETI** (transizioni, non ogni frame — altrimenti si inonda il log);
`Error`/`Fatal` forzano il flush su disco. Il TU che costruisce i `data` deve includere
`<nlohmann/json.hpp>` (l'header telemetria ha solo il forward-declare).

## Convenzione (04_CodingStandards)
Ogni sistema nuovo DEVE loggare i suoi stati chiave (init, errori, transizioni) — testo per gli
umani (`logInfo`), evento JSONL per il parsing. Il sink JSONL NON sostituisce `engine_run.log`:
lo affianca (ADR-016 è additivo su ADR-013).

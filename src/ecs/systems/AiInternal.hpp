#pragma once
// ── AiInternal — seam INTERNO fra AiSystem.cpp e AiCommandLayer.cpp ──────────
// NON è un'API pubblica: header privato di `src/ecs/systems/`, non esposto in
// `include/`. Esiste solo perché il layer di COMANDO (settori, torre di controllo,
// quadro tattico, direttive del Droide Tattico, selezione delle posizioni per gli
// ordini) è stato separato dal monolite `AiSystem.cpp` (2578 righe → audit #7,
// changelog 94): stessa logica, due unità di traduzione.
//
// Divisione delle responsabilità:
//   · AiCommandLayer.cpp → COSA il livello di comando decide (analisi dei settori,
//     segnali della torre, direttive, scelta della posizione per una postura).
//   · AiSystem.cpp       → COME la singola unità esegue (sensing, combattimento,
//     manovra, movimento, ciclo per-entità).
// Le funzioni qui erano `static` in AiSystem.cpp: spostate VERBATIM, quindi il
// comportamento è invariato per costruzione. Nessuna usa la telemetria `g_tac` né
// le utility di movimento, che restano in AiSystem.cpp (taglio verificato).

#include "mini/ecs/Entity.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/game/data/Definitions.hpp"

#include <vector>

namespace mini
{

// Nome leggibile dello stato AI. Definito in AiSystem.cpp, usato anche da AiTrace.cpp.
const char* aiStateName(AiState s);

// ── aitrace — la SCATOLA NERA per-agente (KI #86) ───────────────────────────
// Perché esiste: due diagnosi di fila su questo bug sono state fuorviate da metriche
// AGGREGATE (eventi di combattimento fra run divergenti, e una classificazione con
// soglia fissa). L'utente ha chiesto la cosa giusta: poter *vedere* il comportamento
// della singola AI. Qui non si aggrega — si osserva un agente per volta e si registra
// PERCHÉ ha fatto quello che ha fatto.
namespace aitrace
{

// Ferma da quanti secondi conta come STALLO, e quanto può muoversi restando "ferma".
// 3 s è oltre qualunque fase autorata (hide_duration_max = 1.8 s): sotto è vita
// normale, sopra è un'AI che ha smesso di combattere.
constexpr float STALL_SECONDS = 3.0f;
constexpr float STALL_RADIUS  = 0.6f;

// `--trace-ai <id>`: registra ogni decisione di QUELL'unità, tick per tick.
// 0 = spento (default). -1 = tutte (rumoroso: solo per sim brevi).
void setTraceEntity(int entityId);

// Da chiamare una volta per AI viva, a fine del suo aggiornamento.
void observe(World& world, EntityId e, float dt, unsigned long long tick);

// Riepilogo della finestra + azzeramento. Sullo stesso battito della telemetria AI.
void flush();

} // namespace aitrace

namespace aicmd
{

// Command post più vicino a (x,z) NON posseduto da `team` (ADR-024 v2 / ADR-034).
// `areaRadius > 0` limita la scelta ai post dentro il settore-obiettivo.
bool nearestCapturablePost(const World& world, float x, float z, int team,
                           float& outX, float& outZ,
                           float areaX = 0.0f, float areaZ = 0.0f,
                           float areaRadius = 0.0f);

// Stato dei settori (ADR-034): presenze contrapposte, controllo, pressione.
void updateSectorStates(World& world, const std::vector<EntityId>& snap);

// Peso tattico di un settore per `myTeam` — analisi CONDIVISA da torre di controllo
// e Droide Tattico (changelog 88): una sola formula, non possono divergere.
float sectorTacticalWeight(const SectorDef& sec, const World::SectorState& st, int myTeam);

// Torre di controllo dei cloni (doc 36, ADR-040): pubblica SEGNALI, non ordini.
void updateAllyIntel(World& world);

// Unità nemica viva più vicina a (x,z) entro maxDist — restituisce anche la quota
// (serve alla LOS verticale, changelog 82).
bool nearestEnemyNear(const World& world, int myTeam, float x, float z,
                      float maxDist, float& ox, float& oy, float& oz);

// Quale segnale della torre segue QUESTO clone: scelta pesata ma decorrelata dal
// `bias`, con saturazione (KI #73) → i cloni si distribuiscono.
bool pickAllySignal(const World& world, float bias,
                    float fromX, float fromZ,
                    float& outX, float& outZ, float& outRadius);

// Quale FRONTE segue QUESTO droide (doc 32 v2): peso × prossimità (changelog 86).
const World::EnemyCommand::Directive*
pickEnemyDirective(const World& world, float bias, float x, float z);

// Punto di RIPIEGO per `team`: il settore che controlla più vicino, altrimenti il
// proprio spawn (changelog 86/91).
void retreatPointForTeam(const World& world, int team, float x, float z,
                         float& outX, float& outZ);

// QUADRO TATTICO della torre (Fase torre-hub, changelog 93): per ogni posizione, se
// BATTE un nemico ORA (LOS) + il punteggio. Lavoro pesante fatto UNA volta per tutti.
void updateAllyTactical(World& world);

// Miglior posizione LIBERA per un ordine, tower-aware (occupancy `allyTac.claimed`,
// direzione ±1 verso/lontano dalla minaccia). La reachability la verifica il chiamante.
const TacticalPositionDef* bestOrderPosition(
    World& world, float px, float pz,
    float areaX, float areaZ, float areaRadius,
    int dirToThreat, float threatX, float threatZ, int* outIdx);

// Selettore UNIFICATO del waypoint per gli ORDINI di postura del player
// (Hold/Advance/Retreat/Follow/Regroup) — [[orders-design-vision]].
void selectOrderWaypoint(World& world, AiComponent& ai, float dt,
                         const SquadComponent& sq, float px, float py, float pz);

// La struttura nemica viva più vicina (doc 35): ultimo bersaglio da finire.
bool nearestEnemyStructure(const World& world, int myTeam,
                           float x, float z, float& outX, float& outZ);

} // namespace aicmd
} // namespace mini

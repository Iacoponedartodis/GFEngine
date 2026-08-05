#pragma once
// ContentValidation — gate di correttezza del contenuto (24_ContentValidation, ADR-018).
//
// UN SOLO posto per le regole, tre consumatori:
//   - runtime  : dopo loadAll() → un Error blocca l'avvio (niente fallback silenzioso)
//   - editor   : pannello di validazione (linka QUESTA funzione, non una copia)
//   - headless : GFEngine.exe --validate → stampa + JSONL + exit code != 0
// L'editor non deve avere una copia più debole delle regole: contenuto accettato
// dall'editor e rifiutato dal runtime è esattamente il bug che stiamo togliendo.
//
// Vive in game/data (accanto a DefinitionRegistry, il nodo centrale dei dati) e non
// nell'editor: entrambi i binari la linkano senza violare ADR-002 (l'editor può
// dipendere dagli header engine, mai il contrario).
//
// Legge SOLO il registry già caricato: nessun re-parse dei JSON. L'unica eccezione è
// il gate sugli asset, che verifica l'esistenza dei path su disco — è ciò che
// distingue "modello invisibile" da "tutto ok" e non si può fare senza guardare.

#include "mini/core/Result.hpp"

#include <string>
#include <vector>

namespace mini
{
class DefinitionRegistry;
struct MissionDef;
struct MapDef;

// Valida l'intero contenuto caricato. `dataRoot` serve solo ai controlli sugli
// asset (path relativi ai file dati); vuoto = salta quei gate.
[[nodiscard]] Diagnostics validateContent(const DefinitionRegistry& reg,
                                          const std::string& dataRoot = "");

// Regole di una singola missione (ADR-019). Estratta perché la usano DUE
// consumatori: validateContent (tutto il contenuto, per editor/--validate) e
// ObjectiveSystem::bind (la missione attiva, a runtime). Se le regole vivessero
// in due posti divergerebbero — ed è la stessa ragione per cui il gate è condiviso
// fra editor e runtime.
[[nodiscard]] Diagnostics validateMission(const MissionDef& m,
                                          const DefinitionRegistry& reg);

// Emette le diagnostiche su telemetria JSONL (doc 21) e le stampa. Ritorna true
// se c'è almeno un Error (= contenuto critico invalido).
bool reportDiagnostics(const Diagnostics& diags, bool printToStdout);

// ── SALUTE TATTICA di una mappa (doc 41 B4) ────────────────────────────────
// Difetti che rendono una mappa tatticamente povera senza essere errori di dato:
// posizioni che non coprono nulla, cieche verso le altre quote, troppo esposte,
// ridondanti; settori privi di posizioni. Sta QUI e non nell'editor per la stessa
// ragione di `validateMission`: i consumatori sono DUE (editor e `--validate`) e
// regole duplicate divergono. L'editor le mostra cliccabili, il gate le stampa.
//
// PRECONDIZIONE: `map.positionCovers`/`positionExposure` già calcolati
// (`worldintel::buildTacticalLinks`) — è vero sia per le mappe del registry sia per
// il MapDef temporaneo dell'editor.
struct TacticalDefect
{
    enum class Target { Position, Sector, Geometry };
    // Categoria del difetto: serve a RAGGRUPPARE l'elenco per tipo, così chi autora
    // può chiudere in blocco le famiglie che nel suo caso sono intenzionali (es. i
    // settori di solo transito) senza perdere le altre. Richiesta utente 2026-08-02:
    // un elenco lungo e indifferenziato si smette di leggere.
    // `UnmarkedCover` (KI #86 causa 3, 2026-08-02): un ostacolo che taglia le linee di
    // tiro ma che nessuno ha marcato come elemento tattico ha il peggio dei due mondi —
    // toglie il tiro come una copertura, ma nessuna AI sa usarlo: non ci si ripara
    // dietro, non lo si aggira, non lo si sfrutta.
    //
    // NOTA DI CALIBRAZIONE, imparata sbagliando. Una misura a runtime aveva stimato il
    // "57% di geometria muta" usando un raggio FISSO di 3 m dal CENTRO del bloccante:
    // per un muro largo 7 m o un impalcato largo 31 m quella soglia è priva di senso, e
    // marcava come mute proprio le coperture autorate ai bordi (misurate poi a 3,3-5,4 m).
    // Il controllo qui è invece PROPORZIONATO alla taglia dell'oggetto — su Training
    // Ground trova 4 ostacoli, non il 57%. Chi tocca questa soglia la scali con l'oggetto.
    enum class Kind { NoCoverage, BlindVertical, HighExposure, Redundant, EmptySector,
                      UnmarkedCover,
                      // `UnreachablePoint` nasce da un bug reale costato mezza
                      // giornata (KI #90): il post "Alpha" di firebase sta su una
                      // piattaforma alta **1,0 m**, mentre il navmesh scala al
                      // massimo `STEP_HEIGHT` = 0,55 m. Detour ci passa INTORNO,
                      // quindi nessuna unità a terra può salirci: la missione che
                      // chiedeva di catturarlo era **incompletabile**, e non c'era
                      // modo di accorgersene se non guardando le AI orbitare.
                      // Un punto che il gioco CHIEDE di raggiungere e che la
                      // navigazione non può raggiungere è un difetto di mappa.
                      UnreachablePoint,
                      Count };
    Target      target   = Target::Position;
    Kind        kind     = Kind::NoCoverage;
    int         index    = 0;      // indice in tacticalPositions / sectors
    int         severity = 0;      // 0 = avviso (valutare), 1 = problema
    std::string text;
};

// Etichetta leggibile di una categoria (usata dal pannello editor e dal gate).
[[nodiscard]] const char* tacticalDefectKindName(TacticalDefect::Kind k);
[[nodiscard]] std::vector<TacticalDefect> analyzeTacticalHealth(const MapDef& map);

} // namespace mini

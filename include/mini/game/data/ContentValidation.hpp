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

namespace mini
{
class DefinitionRegistry;
struct MissionDef;

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

} // namespace mini

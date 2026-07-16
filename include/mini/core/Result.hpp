#pragma once
// Result.hpp — modello degli errori ATTESI (24_ContentValidation, ADR-018).
//
// Distinzione deliberata:
//  - `Diagnostic` = fallimento ATTESO del CONTENUTO (file assente, riferimento
//    invalido, campo incoerente). È un dato: si raccoglie, si mostra, si corregge.
//  - assert / crash  = violazione di un invariante INTERNO, cioè un bug di codice.
// Confonderli è ciò che ha prodotto la classe di bug che questo gate chiude:
// dati sbagliati che non falliscono, ma degradano in silenzio (KI #7/#25/#26).
//
// Ogni Diagnostic è pensata per essere emessa ANCHE come evento JSONL (doc 21):
// la validazione deve essere leggibile da un tool e da un LLM senza aprire l'editor.

#include "mini/core/Telemetry.hpp"   // telemetry::Level — niente enum parallelo

#include <string>
#include <vector>

namespace mini
{

struct Diagnostic
{
    // Error = contenuto CRITICO invalido → blocca. Warn = degrado non critico,
    // loggato ma mai silenzioso. Non si usano altri livelli qui.
    telemetry::Level severity = telemetry::Level::Warn;
    std::string category;     // "Content" | "Asset" | "Map" | "Mission" ... (doc 21)
    std::string file;         // path/id del dato incriminato — DOVE
    std::string message;      // cosa non va — COSA
    std::string suggestion;   // cosa fare — AZIONABILE (senza questo è solo rumore)
};

using Diagnostics = std::vector<Diagnostic>;

[[nodiscard]] inline bool hasErrors(const Diagnostics& d)
{
    for (const auto& x : d)
        if (x.severity == telemetry::Level::Error) return true;
    return false;
}

[[nodiscard]] inline int countBy(const Diagnostics& d, telemetry::Level lvl)
{
    int n = 0;
    for (const auto& x : d) if (x.severity == lvl) ++n;
    return n;
}

} // namespace mini

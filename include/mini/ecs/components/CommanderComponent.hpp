#pragma once

namespace mini
{

// Comandante nemico (ability type "command"): marca il Droide Tattico serie T (HVT,
// GDD App. B) come lo STRATEGA dei droidi — la controparte del comando del giocatore
// (ADR-020/doc 26). Non è un buff e non combatte: finché è vivo, AiSystem pubblica una
// direttiva strategica (World::enemyCommand) che fa convergere i droidi sull'obiettivo
// scelto; ucciderlo la rimuove (i droidi perdono coordinamento). Marker per la v0: i
// parametri della direttiva (che post, che ordine) li decide AiSystem dallo stato partita.
// Espansioni (gradi, ordini ricchi, entità a sé) → doc 32 + [[command-rank-system]].
struct CommanderComponent
{
};

} // namespace mini

#pragma once
// DefinitionRename — comando di rinomina sicuro per le definizioni (ADR-010).
// id = nome file (ADR-001): rinominare = rinominare il file FISICO e
// aggiornare OGNI cross-reference in data/ che puntava al vecchio id.
// La mappa dei campi cross-reference è ESPLICITA (niente string-replace
// generico: falsi positivi peggiori di riferimenti mancati — vedi ADR-010).

#include <string>

namespace editor::rename
{

enum class Category
{
    Weapon,         // data/weapons     ← enemies/allies: weapons[], weapon, weapon_display.id
    Enemy,          // data/enemies     ← maps: enemy_types[]
    Ally,           // data/allies      ← maps: ally_types[]
    HitboxProfile,  // data/hitboxes    ← enemies/allies: hitbox_profile
    AiProfile,      // data/ai          ← enemies/allies: ai_profile
    Ability,        // data/abilities   ← enemies/allies: abilities[]
    Map,            // data/maps        ← nessun cross-ref dati (ma vedi nota ADR-008:
                    //                    i game mode caricano ancora "firebase" hardcoded)
    Character,      // data/characters  ← nessun cross-ref dati al momento
    Vehicle,        // data/vehicles    ← maps: vehicle_spawns[].vehicle_id
    Objective,      // data/objectives  ← missions: primary_objectives[]/optional_objectives[],
                    //                    objectives: activation.objective, linked_objectives[]
    Class,          // data/classes   ← nessun cross-ref in data/: la classe si sceglie
                    //                  nel PreMatch e si persiste per id nei preset
                    //                  utente (fuori da data/, KI #19)
    Mission         // data/missions    ← nessun cross-ref dati (la missione si sceglie nel
                    //                    PreMatch e si persiste per id nei preset utente,
                    //                    fuori da data/: un preset con id stantio ricade su
                    //                    "(nessuna)" — degradazione onesta, non silenziosa)
};

// Esegue la rinomina. dataDir termina con '/'. Ritorna "" se ok, altrimenti
// il messaggio d'errore. outUpdatedRefs (opzionale): n. file aggiornati.
std::string renameDefinition(const std::string& dataDir, Category cat,
                             const std::string& oldId, const std::string& newId,
                             int* outUpdatedRefs = nullptr);

} // namespace editor::rename

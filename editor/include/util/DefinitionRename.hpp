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

// ── ELIMINA (doc 39 regola R1) ───────────────────────────────────────────────
// L'audit di coerenza dell'editor ha misurato che *Elimina* mancava in 5 moduli
// su 7: si poteva creare e rinominare una definizione ma non toglierla, e l'unico
// modo era cancellare il file a mano fuori dall'editor. Sta qui, accanto a
// `renameDefinition`, perché è lo **stesso dominio**: entrambi manipolano il file
// che PORTA l'id (ADR-001), e tenerli separati significherebbe due idee diverse
// di dove vivano le definizioni.
//
// COSA NON FA, dichiarato: **non** ripulisce i cross-reference. È deliberato — un
// riferimento rotto lo segnala `--validate` con il file e il campo, mentre una
// pulizia automatica cancellerebbe in silenzio scelte dell'autore (un roster che
// perde una riga senza dirlo). La rinomina li aggiorna perché lì l'intento è
// "questa cosa ora si chiama così"; qui l'intento è "questa cosa non c'è più", e
// cosa farne altrove è una decisione di chi autora.
//
// Ritorna "" se ok, altrimenti il messaggio d'errore. `outPath` (opzionale):
// il percorso del file rimosso, per il messaggio di conferma.
std::string deleteDefinition(const std::string& dataDir, Category cat,
                             const std::string& id, std::string* outPath = nullptr);

} // namespace editor::rename

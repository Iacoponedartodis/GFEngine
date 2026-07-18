#pragma once
// ClassResolve — la regola "cosa vince fra la CLASSE e i campi dell'unità"
// (ADR-022), in UN SOLO posto.
//
// Perché esiste. La regola viveva solo dentro `ConquestMode::resolveUnitArchetype`,
// mentre gli altri consumatori leggevano ancora i campi grezzi dell'`EnemyDef`.
// Risultato, trovato il 2026-07-17: con una classe assegnata un'unità
//   - SPARAVA l'arma della classe      (out.weaponId, class-aware)
//   - IMPUGNAVA quella dell'entità     (WeaponAttach → def->primaryWeaponId())
//   - coi DANNI di quest'ultima        (bullet stats → enemy->primaryWeaponId())
// cioè tre risposte diverse alla stessa domanda. È lo stesso guasto del
// 2026-07-11 (`weapon_display` che divergeva dal loadout), un livello più in alto.
//
// La cura non è "ricordarsi di applicare la classe ovunque": è che la domanda
// abbia una sola implementazione (ADR-018 — una sola fonte per le regole).
// Chi aggiunge un consumatore dell'arma di un'unità DEVE passare da qui.

#include "mini/game/data/Definitions.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include <string>

namespace mini::classres
{

// Ritorno per VALORE, non per riferimento: le forme "a campi sciolti" qui sotto
// ricevono stringhe che il chiamante può costruire al volo, e restituire un
// riferimento a un parametro temporaneo sarebbe un dangling silenzioso. Il costo
// è una copia per unità al momento dello spawn: irrilevante.

// ── Forme a campi sciolti ────────────────────────────────────────────────
// Servono a chi NON ha un `EnemyDef` sottomano — in particolare l'editor, che
// lavora sui propri Entry. È il modo per far usare a tutti la STESSA regola
// invece di riscriverla: l'editor che la riscrive è come il runtime che la
// riscrive, e diverge allo stesso modo.
inline std::string primaryWeaponId(const DefinitionRegistry& reg,
                                   const std::string& classId,
                                   const std::string& entityPrimary)
{
    if (!classId.empty())
        if (const ClassDef* c = reg.getClass(classId))
            if (!c->primaryWeaponId.empty())
                return c->primaryWeaponId;
    return entityPrimary;
}

inline std::string aiProfileId(const DefinitionRegistry& reg,
                               const std::string& classId,
                               const std::string& entityProfile)
{
    if (!classId.empty())
        if (const ClassDef* c = reg.getClass(classId))
            if (!c->aiProfileId.empty())
                return c->aiProfileId;
    return entityProfile;
}

// ── Forme su EnemyDef (runtime) ──────────────────────────────────────────
// Delegano: una sola implementazione della regola, due comodità di chiamata.

// Arma primaria EFFETTIVA di un'unità. La classe vince **solo se valorizzata**:
// un'unità può referenziare una classe e tenersi comunque la propria arma
// (additività, ADR-022 §2).
inline std::string primaryWeaponId(const DefinitionRegistry& reg, const EnemyDef& def)
{ return primaryWeaponId(reg, def.classId, def.primaryWeaponId()); }

// Profilo AI EFFETTIVO. Stessa regola: è ciò che rende la classe una
// PROFESSIONE e non un pacchetto di armi (ADR-022).
inline std::string aiProfileId(const DefinitionRegistry& reg, const EnemyDef& def)
{ return aiProfileId(reg, def.classId, def.aiProfileId); }

} // namespace mini::classres

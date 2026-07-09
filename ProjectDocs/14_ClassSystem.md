# 14 — Class System (Planned Feature)

**Status: Planned Feature — not yet implemented in code.**
This document describes a system that does NOT exist yet in the codebase. It exists to give
Claude Code (or a human developer) a precise target to implement against, ahead of Phase 3.
Do not treat any field/struct name below as already present until 05_CurrentState confirms
it and this header is updated to "Current Implementation".

## Overview
A **Class** is a named, authorable composition of one primary weapon, optional secondary
equipment, and a role tag, assignable to a `PlayerDef` (and later to progression unlocks).
It is a new definition type in the `DefinitionRegistry`, following the same id=filename-stem
convention as every other definition (ADR-001).

## Goal
Let content authors define "Trooper", "Heavy Gunner", "Marksman", etc. as reusable loadout
packages, instead of wiring a single `weaponIds[]` entry directly onto `PlayerDef`.

## Problem Solved
`EnemyDef`/`PlayerDef` currently reference `weaponIds[]` directly (08_KnownIssues #10). This
conflates "which weapon can this unit use" with "what identity/loadout does this unit have."
Phase 3 (military career progression, grade unlocks) needs the latter as a first-class,
independently unlockable/swappable concept. Without this, progression work would require
retrofitting `PlayerDef`/`EnemyDef` under time pressure later.

## Scope
- New `ClassDef` schema in `include/mini/game/data/Definitions.hpp`:
  - `id` (filename stem, canonical per ADR-001)
  - `name` (display name)
  - `primaryWeaponId` (single weapon reference, dropdown from `registry.weapons()`)
  - `secondaryWeaponId` (optional, same dropdown pattern)
  - `abilityIds[]` (reuse existing ability reference pattern already used by `EnemyDef`)
  - `role` (free descriptive tag for now — e.g. "assault", "support", "sniper"; not an enum
    tied to AI behavior yet, to avoid coupling this system to AI role logic prematurely)
- `DefinitionRegistry::loadClasses()` loader (mirrors `loadWeapons`/`loadAbilities` pattern
  exactly — same JSON helpers, same id convention, same map storage `m_classes`).
- `getClass(id)` accessor + `classes()` map view, following the existing accessor pattern in
  03_SystemReference.
- New editor module OR new tab in an existing module (decision deferred to implementation
  time — see Technical Decisions below) to author `ClassDef` with dropdown-only weapon/
  ability assignment (04_CodingStandards: no free-text id fields, no exceptions).
- `PlayerDef` gains an optional `classId` reference (dropdown from `registry.classes()`).

## Out of Scope
- Grade/rank progression logic (Phase 3 feature, this document only introduces the
  composition unit progression will attach to).
- Unlock conditions, XP, or persistence of unlocked classes — that is a save-system concern,
  not part of this schema.
- Changing `EnemyDef`. Enemies/allies keep `weaponIds[]` as-is; `ClassDef` is a player-facing
  concept only for now. Do not couple enemy AI archetypes to `ClassDef` without a separate
  ADR — this would re-introduce the exact coupling problem this system is meant to avoid.
- Any AI behavior driven by `role` — the field is descriptive metadata only until a tactical
  AI system (Phase 2/06_Todo #3) explicitly consumes it via its own documented schema.

## Architecture
`ClassDef` sits at the same layer as `WeaponDef`/`AbilityDef` — a pure data definition
resolved through `DefinitionRegistry`. It does not introduce new runtime systems; it is
consumed by whatever loadout/progression code reads `PlayerDef.classId` and resolves it to
`primaryWeaponId`/`secondaryWeaponId`/`abilityIds[]` at spawn/equip time, using the exact
same resolution pattern `ConquestMode` already uses for `weaponIds[]` (see 03_SystemReference,
"AI usa l'arma assegnata").

## Dependencies
- `DefinitionRegistry` (extends it — additive change, no impact on existing loaders).
- `WeaponDef`, `AbilityDef` (referenced by id, no schema change required to either).
- `PlayerDef` (gains one new optional field).
- Editor: whichever module authors `ClassDef` depends on the dropdown-population pattern
  already implemented in EntityEditor/WeaponEditor (03_SystemReference) — reuse, do not
  reinvent.

## Integration
- Editor: new "Classi" tab/module, following the exact UI pattern already established
  (list on the left, properties on the right, dropdowns for every reference field, save via
  the RMW helper once ADR-010 is implemented — see 13_ADR).
- Runtime: pre-match/loadout code resolves `PlayerDef.classId` → `ClassDef` → concrete weapon/
  ability ids, at the same point where weapon selection already happens today. No new game
  loop hook is required.

## Data Flow
1. Author creates/edits `data/classes/<id>.json` in the editor.
2. `DefinitionRegistry::loadClasses()` loads it at startup (both GFEditor and GFEngine, per
   the two-binary file-only contract, ADR-002).
3. `PlayerDef.classId` (if set) is resolved at pre-match/spawn time to concrete weapon/ability
   ids via `registry.getClass(id)`.
4. If `classId` is empty/unset, fall back to legacy direct `weaponIds[]` resolution on
   `PlayerDef` — this system is additive, not a breaking schema change.

## Performance Considerations
None beyond existing `DefinitionRegistry` load cost (one more JSON folder scan at startup,
same order of magnitude as `weapons/`/`abilities/`). No per-frame cost: resolution happens
once at spawn/equip time, not per tick.

## Future Expansion
- Phase 3: grade-gated unlock list of `ClassDef` ids per player save.
- Possible later addition of `armorId`/`equipmentIds[]` fields once an equipment system
  exists — additive, does not require revisiting this document's core schema.
- `role` may eventually be formalized into an enum consumed by tactical AI (Phase 2), but
  only via a separate ADR that explicitly defines that coupling — do not assume it here.
# 15 — Map Metadata (Implementato)

**Status: Implementato (schema + loader + authoring) — 2026-07-10.** Consumo AI implementato
(doc 18, 2026-07-10): cover in hide, repulsione danger, patrol route come waypoint; il navmesh
marca DANGER/COVER come aree semantiche (doc 22). **La riga "nessuna AI li consuma" era vera solo
al 2026-07-10 — ora superata.**
`MapDef.coverPoints/patrolRoutes/dangerZones` esistono, sono parse-ati dal
`DefinitionRegistry` (chiavi JSON `cover_points`/`patrol_routes`/`danger_zones`) e sono
autorabili nel MapEditor (sezione "Metadata AI": gizmo + slider + RMW, marker dedicati
nel viewport).

> **EVOLUZIONE PIANIFICATA → doc 33 (World Tactical Intelligence).** Questo schema minimo
> (posizione+fronte+altezza / route / danger) è la **v0** dei metadata. Il piano 33 lo evolve
> verso una rappresentazione tattica ricca (cover intelligence, tactical points, rete di
> navigazione tattica, settori) a fasi. Non implementare estensioni qui senza seguire doc 33.

## Overview
Map Metadata is a set of optional, author-placed spatial hints on top of the existing
`MapDef.geometry` — cover points, patrol routes, and danger zones — queryable at runtime by
AI systems that need more than raw collision geometry to behave tactically.

## Goal
Give Phase 2 tactical AI (06_Todo #3, currently blocked) queryable spatial data without
requiring the AI to infer tactical structure from raw geometry at runtime.

## Problem Solved
Raw `MapGeometryBox` data (position + size + collider flag) tells AI what is solid, not what
is tactically meaningful. Without explicit authoring, "take cover," "patrol this route," or
"avoid this exposed area" behaviors would each need bespoke, fragile geometric inference code
per map. Authoring this once, per map, in the Map Editor is cheaper and more controllable
than inferring it at runtime.

## Scope
- Extend `MapDef` (`include/mini/game/data/Definitions.hpp`) with three new optional arrays,
  all empty by default (zero impact on existing maps until authored):
  - `coverPoints[]`: `{ x, y, z, facingDeg, height }` — a position + facing direction an AI
    can path to and crouch/peek from; `height` distinguishes low cover (peek-over) from tall
    cover (peek-around), mirroring `AiProfileDef`'s existing `coverPreference`/`peek`/`hide`
    fields (03_SystemReference) so the two schemas compose directly.
  - `patrolRoutes[]`: `{ id, points[] }` where `points[]` is an ordered list of `{x,y,z}` —
    named so multiple squads/roles can reference different named routes later.
  - `dangerZones[]`: `{ x, y, z, radius, dangerLevel }` — a soft spatial hint (not a
    collider) that tactical AI can weight into path/positioning decisions; `dangerLevel` is a
    plain float (0..1), semantics owned by whatever AI logic consumes it, not by this schema.
- `DefinitionRegistry` JSON parse additions in the existing `loadMaps()` function (additive
  fields on the existing `MapDef` parse block, using the same `gets/geti/getf` helpers).
- New "Map Metadata" section in MapEditor (new tab or new sub-panel next to the existing
  geometry/spawn/command-post sections), authoring these three arrays with the same gizmo +
  sliderRow UI pattern already used for geometry boxes and command posts
  (03_SystemReference).

## Out of Scope
- Any AI logic that consumes this data. This document defines the **data schema and
  authoring tool only**. The AI system that reads `coverPoints`/`patrolRoutes`/`dangerZones`
  is 06_Todo #3 (tactical AI) and must be documented separately when it is designed —
  do not pre-design AI consumption logic here.
- Automatic/procedural generation of cover points or danger zones from geometry. Phase 1/2
  scope is manual authoring only; procedural generation is a possible Future Expansion, not
  a current requirement.
- Sectors/zones for strategic-level AI (Phase 2 "fronti multipli" per 00_Vision) — that is a
  larger, separate concept (likely spanning multiple maps or map regions) and should get its
  own document when Phase 2 planning starts, not be folded into this schema prematurely.

## Architecture
Pure additive data on `MapDef`, following the exact precedent of ADR-009 (`commandPosts`):
data lives in `MapDef`, authored in MapEditor, with zero new runtime systems required to
store or load it. Any runtime consumption is the responsibility of whichever AI system reads
it later (out of scope here, see above) — this keeps the data layer decoupled from AI logic,
matching the Engine Design Principle "Modularità" (00_Vision).

## Dependencies
- `MapDef` schema (additive change).
- `DefinitionRegistry::loadMaps()` (additive parse block).
- MapEditor (new authoring section; reuses existing gizmo/sliderRow/RMW-save patterns —
  no new UI infrastructure required).
- Indirectly informs `AiProfileDef` design (03_SystemReference already has
  `coverPreference`/`peek`/`hide` fields defined but currently unconsumed per 06_Todo #3) —
  this document's `coverPoints[].height` field is deliberately shaped to compose with those
  existing fields when the AI system is eventually built.

## Integration
- Editor: MapEditor authors and saves these arrays into the map's JSON (`data/maps/<id>.json`)
  via the same RMW discipline as geometry/command posts (04_CodingStandards; ADR-010 once
  implemented).
- Runtime: `DefinitionRegistry::getMap(id)->coverPoints/patrolRoutes/dangerZones` become
  queryable the moment they're parsed — no runtime wiring is required by this document beyond
  making the fields available; consumption is deferred to the tactical AI system.

## Data Flow
1. Author places cover points/patrol routes/danger zones in MapEditor's new section.
2. Saved into `data/maps/<id>.json` under new top-level JSON keys (e.g. `cover_points`,
   `patrol_routes`, `danger_zones`), read-modify-write per ADR-010.
3. `DefinitionRegistry::loadMaps()` parses the new keys into `MapDef`.
4. Available via `registry.getMap(id)` to any future consumer — no consumer exists yet.

## Performance Considerations
Negligible at authoring/load scale (arrays of small structs, parsed once at startup like the
rest of `MapDef`). Runtime query cost is not yet a concern because no consumer exists; when
the tactical AI system is designed, that document must specify its own query-frequency and
spatial-indexing needs (e.g. whether a naive linear scan over `coverPoints[]` is sufficient
or whether a spatial index is needed) — do not assume either way here.

## Future Expansion
- **Cover più ricche (richiesta utente 2026-07-10):** ~~il cover point è solo posizione+fronte+
  altezza~~ → **parzialmente fatto (ADR-026, doc 33 Fase 1):** aggiunti `protection` + `canShoot`
  e la scelta AI per protezione (`bestCoverToward`) + auto-gen editor. Restano le **pose FPS** alle
  coperture — crouch, mira DA copertura, peek-over/around guidati da `height` — **bloccate** finché
  non ci sono le animazioni. Idoneità-per-ruolo e link fra coperture → doc 33 Fase 2.
- **Geometrie oltre i box (richiesta utente 2026-07-10):** mappa, hitbox e collisioni
  sono oggi limitate a parallelepipedi. Servirà un sistema di shape/collision più
  ricco (vedi 06_Todo, voce dedicata) sia per l'ambiente che per le entità.
- Procedural generation/validation tools (e.g. "highlight cover points with no line-of-sight
  to any danger zone") once the AI system exists to define what "useful" means.
- Named patrol route reuse across multiple unit spawns (already supported by giving routes an
  `id` in this schema).
- Sector/front abstraction for Phase 2 strategic AI, as a separate, later document — this
  schema intentionally does not attempt to solve that problem now.
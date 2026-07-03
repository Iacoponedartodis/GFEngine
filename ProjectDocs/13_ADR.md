# 13 — Architecture Decision Records

One entry per structural decision. Newest last.

## ADR-001 — Canonical id = filename stem
- **Decision:** For every definition type the id is the JSON filename stem; the in-file
  `id`/`profile_id` field is non-authoritative.
- **Consequence:** Renaming = renaming the file; cross-references (`weaponIds`, `aiProfileId`,
  `hitboxProfileId`, `MapDef.enemyTypes/allyTypes`) are by that stem. Registry maps are keyed
  by stem. Status: **in force.**

## ADR-002 — Two-binary, file-only contract
- **Decision:** GFEngine (runtime) and GFEditor (tool) are separate binaries communicating only
  via `data/*.json` + assets by path. Editor launches runtime with `--direct-prematch` /
  `--sandbox`. Runtime never links editor code.
- **Consequence:** No circular src/editor deps; editor may read engine schemas. Status: **in force.**

## ADR-003 — Client-side vertex arrays (no VAO/VBO)
- **Decision:** Keep OpenGL 3.3 Compatibility Profile with client-side arrays as an Intel-driver
  workaround. Status: **in force** (do not migrate without a concrete driver justification).

## ADR-004 — Map is data-driven via MapDef.geometry (2026-07-03)
- **Decision:** Map collision/visual geometry lives in `MapDef.geometry` (authored in MapEditor),
  read by ConquestMode and SandboxMode. Hardcoded box layout retained only as an empty-geometry
  fallback.
- **Consequence:** New/edited maps are data changes. Status: **in force.**

## ADR-005 — SandboxMode shares the firebase map (2026-07-03)
- **Decision:** `--sandbox` loads the firebase `MapDef` (geometry + spawn points) and spawns
  respawning dummies, so map/spawn authoring is testable from the arena. Status: **in force.**

## OPEN — Hitbox single source of truth (blocks KnownIssues #1)
- **Problem:** Runtime uses hitbox PROFILE; EntityEditor authors inline entity zones that never
  reach the game. Two divergent stores.
- **Options:** (a) EntityEditor writes to the referenced profile; (b) runtime also reads inline
  zones; (c) drop inline zones from EntityEditor. **Not yet decided** — must ADR before coding.

## OPEN — ConquestMode fallback ids (blocks KnownIssues #2)
- **Problem:** `grunt/heavy/sniper` fallback ids don't exist. **Decide** on a registry-derived
  safe fallback vs requiring `MapDef.enemyTypes`. Not yet decided.

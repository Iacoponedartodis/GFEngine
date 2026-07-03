# 08 — Known Issues

## 1. Two divergent hitbox systems (HIGH)
- Entity-inline `hitbox_zones` (authored in EntityEditor, saved in `data/enemies|allies/*.json`)
  vs shared hitbox PROFILE (`data/hitboxes/*.json`, authored in HitboxEditor).
- **The runtime uses only the PROFILE** (via `EnemyDef.hitboxProfileId` ->
  `registry.getHitboxProfile`). Inline zones never reach the game.
- Symptom seen: a head hitbox created inline appeared correct in EntityEditor but the game/
  HitboxEditor showed the old profile zone. Needs a single source of truth (Todo #1 / ADR).

## 2. ConquestMode dead fallback archetype ids (HIGH)
- `buildEnemySpawnList` falls back to `{"grunt","heavy","sniper"}` when `MapDef.enemyTypes`
  is empty, but those ids have **no enemy JSON files** (real ids: `B1 Battle Droid`,
  `B1 Heavy Droid`). Renaming/removing maps' `enemyTypes` silently breaks spawning with no
  compile error. Only works today because `firebase.json` supplies `enemyTypes`.
- Note: the hitbox fallback `"grunt"` for team2 DOES exist in `data/hitboxes/`.

## 3. FreeCameraViewport GL/FBO state (WATCH)
- Historically caused black-screen / state-restoration issues across render passes. Current
  code renders to an FBO and restores depth func. Re-check whenever the viewport render path
  or ImGui multi-viewport handling changes.

## 4. EntityEditor gizmo/marker only correct at identity transform (MEDIUM)
- Attach-point gizmo target uses raw model-space coords while markers render at
  M = rotate(rotX)*scale(scale). Correct only when scale=1, rotX=0 (true for the B1 droid).
  MapEditor/HitboxEditor gizmos ARE correct (world-space).

## 5. Clone Trooper oversized (MEDIUM)
- `clone_trooper` GLB is authored in FBX centimeters (~285 units tall). With `mesh_scale`=1 it
  is enormous in-game. Needs per-entity scale or asset normalization.

## 6. Repo hygiene (LOW)
- `build/`, `imgui.ini`, `presets.cfg` are runtime/generated; verify they are gitignored, not
  tracked.

## 7. Near-duplicate data files (LOW)
- Duplicate-looking menu/loadout lists are usually caused by near-identical weapon JSONs, not
  code. Manage via editor create/rename + data hygiene.

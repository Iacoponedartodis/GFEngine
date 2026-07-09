# 08 — Known Issues

## 1. ~~Two divergent hitbox systems~~ — RESOLVED 2026-07-04 (ADR-006)
- Profile is now the single source of truth; EntityEditor reads/writes the profile, inline
  zones are legacy-fallback-only and get erased on save. Residual watch item: HitboxEditor and
  EntityEditor edit the same profile file — concurrent unsaved edits in both modules follow
  "last save wins".

## 2. ~~ConquestMode dead fallback archetype ids~~ — RESOLVED 2026-07-04 (ADR-007)
- Fallback is now registry-derived (sorted enemy ids); empty registry → no spawns + error log.
- Note: the hitbox fallback `"grunt"` for team2 in `spawnUnit` still exists in `data/hitboxes/`
  and remains valid.

## 3. FreeCameraViewport GL/FBO state (WATCH)
- Historically caused black-screen / state-restoration issues across render passes. Current
  code renders to an FBO and restores depth func. Re-check whenever the viewport render path
  or ImGui multi-viewport handling changes.

## 4. ~~EntityEditor gizmo/marker only correct at identity transform~~ — RESOLVED 2026-07-04
- Gizmo targets are now set in world space via `toWorld()` (M = rotX*scale) and drag deltas
  are converted back with `deltaToLocal()` (inverse of M) at every call site + in `tick()`.
  Needs one manual verification pass with a scaled/rotated model (e.g. clone trooper).

## 5. Clone Trooper oversized (MEDIUM)
- `clone_trooper` GLB is authored in FBX centimeters (~285 units tall). With `mesh_scale`=1 it
  is enormous in-game. Needs per-entity scale or asset normalization.

## 6. ~~Repo hygiene~~ — RESOLVED 2026-07-04
- `.gitignore` was corrupted (contained an old CMakeLists.txt copy, no ignore patterns) —
  rewritten with real patterns. `build/` (1113 files), `imgui.ini`, `presets.cfg` untracked
  from the index (`git rm --cached`); files remain on disk. Ready to commit.

## 15. Salvataggi distruttivi (classe di bug) — mitigato 2026-07-08 (WATCH)
- BalanceEditor riscriveva i JSON da zero perdendo i campi degli altri moduli (incidente
  firebase.json). Ora saveMap/saveWeapon/saveEnemy/saveAlly fanno RMW. **Ogni nuovo save
  path va scritto RMW** (regola in 04_CodingStandards). Verificare i futuri moduli.
- Difesa ulteriore possibile (non implementata): backup .bak automatico prima di ogni save.

## 13. Hitbox zone rotation ignorata nel combat test (LOW)
- CombatSystem trasforma le zone per scala/yaw/meshOffsetY (fix 2026-07-04) ma il test resta
  AABB: `eulerDeg` per-zona (es. testa B1 a -58°) non inclina il volume di collisione.
  Serve un test OBB se la precisione diventa importante.

## 14. Asset default mancanti (LOW)
- `assets/textures/default.png` e `assets/models/default.obj` non esistono: fallback a
  checkerboard + cubo funziona, ma il log sporca l'avvio. Aggiungere gli asset o rimuovere
  i tentativi di caricamento.

## 7. Near-duplicate data files (LOW)
- Duplicate-looking menu/loadout lists are usually caused by near-identical weapon JSONs, not
  code. Manage via editor create/rename + data hygiene.

## 8. ~~No game-mode abstraction~~ — RESOLVED 2026-07-04 (ADR-008)
- `IGameMode` + `createGameMode()` factory; Application usa solo l'interfaccia. Aggiungere
  una modalità = una classe + una riga nella factory. Smoke test runtime `--sandbox` passato.
- Residuo (LOW): l'id modalità viene ancora dal flag CLI (`--sandbox`); in futuro dovrebbe
  arrivare da MapDef/PreMatch.

## 9. ~~No Objective / Command Post abstraction~~ — RESOLVED 2026-07-04 (ADR-009)
- `CommandPosts` riusabile + `MapDef.commandPosts` + authoring nel Map Editor + ticket bleed
  in Conquista + catturabili in sandbox. Smoke test passato (3 post caricati e inizializzati).
- Residuo (LOW): il progresso di cattura non è ancora visibile nell'HUD (solo colore
  bandiera al completamento).

## 10. No Class concept distinct from Weapon (MEDIUM)
- `EnemyDef`/`PlayerDef` reference `weaponIds[]` directly; there is no "Class" definition
  (weapon + equipment + role composition) as its own entity. This will need to be introduced
  before Phase 3 progression (grades, unlocks) without coupling class identity to a single
  weapon id. Blocks Todo #10.

## 11. MapDef lacks AI-relevant tactical metadata (MEDIUM)
- `MapDef` currently carries geometry + spawn points only (ADR-004). It does not yet carry
  cover points, patrol routes, danger zones, or sectors that a tactical AI (Phase 2) or a
  Map Metadata editor module would need to query at runtime. Blocks Todo #11.

## 12. Split-screen/multi-viewport support unverified (MEDIUM)
- Local split-screen (2 players) is a stated functional requirement (Vision). No evidence has
  been found that the current camera/input/rendering pipeline supports more than one active
  local viewport/input source simultaneously. Needs explicit verification before further
  single-player-assuming systems are built on top. Blocks Todo #12.
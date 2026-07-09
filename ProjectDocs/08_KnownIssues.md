# 08 — Known Issues

## 1. ~~Two divergent hitbox systems~~ — RESOLVED 2026-07-04 (ADR-006) + 2026-07-09 (ADR-012)
- Profile = single source of truth (ADR-006). Il residuo "last save wins" tra HitboxEditor
  ed EntityEditor è chiuso: HitboxEditor RIMOSSO, authoring solo in EntityEditor (ADR-012).

## 16. Rename profilo hitbox senza UI dedicata (LOW — accettato da ADR-012)
- Con l'HitboxEditor rimosso non c'è UI per rinominare un profilo hitbox standalone, e il
  rename di un'entità NON rinomina il profilo che referenzia (il riferimento resta valido).
  `DefinitionRename` supporta già Category::HitboxProfile: se servirà, basta esporre la UI
  nell'Entity Editor.

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

## 7. Near-duplicate data files (ESCALATED to P0 — 2026-07-09)
- **Confirmed in production data (user-reported 2026-07-09):** renaming a weapon's name/id
  without a dedicated tool produced duplicate-looking entries in the in-game loadout menu
  (e.g. "DC-15A Blaster" / "DC-15A Blaster Rifle" appearing as near-identical separate rows).
- **Root cause:** `id = filename stem` (ADR-001) is correct and must NOT change, but there is
  currently no in-editor rename command. The only way to "rename" today is to create a new
  file with a new filename/id and abandon the old one — the old file remains on disk as a
  live, loaded, near-duplicate definition. This is a process gap, not a registry bug.
- **Secondary contributing cause:** where free-text id input still exists instead of registry
  dropdowns, it is possible to reference or create ids that don't correspond cleanly to a
  single canonical file, worsening the duplicate risk.
- **Resolution path:** 06_Todo #1 (rename tooling, P0) + 06_Todo #2 (dropdown-only audit, P0)
  + ADR-010 (Proposed, see 13_ADR). Until implemented: manage via careful manual file
  deletion + full cross-reference grep (see 04_CodingStandards, rename discipline).
- Status: **IMPLEMENTED 2026-07-09 (ADR-010 Accepted) — awaiting manual GUI smoke.**
  Il comando Rinomina esiste in WeaponEditor/EntityEditor/HitboxEditor/MapEditor con sweep
  cross-reference automatico. Il duplicato originale era già stato ripulito a mano, quindi
  lo smoke previsto ("fix di un duplicato reale") va sostituito con: rinominare una
  definizione qualsiasi dall'editor e verificare che (a) il file sia rinominato, (b) i
  riferimenti nei JSON siano aggiornati, (c) il gioco carichi senza id orfani. Chiudere
  dopo questo test manuale.

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
  weapon id. Blocks Todo #14.

## 11. MapDef lacks AI-relevant tactical metadata (MEDIUM)
- `MapDef` currently carries geometry + spawn points only (ADR-004). It does not yet carry
  cover points, patrol routes, danger zones, or sectors that a tactical AI (Phase 2) or a
  Map Metadata editor module would need to query at runtime. Blocks Todo #15.

## 12. Split-screen/multi-viewport support unverified — RISOLTO 2026-07-09 (caso a)
- Spike ADR-011 eseguito: due viewport + seconda Camera sulla stessa scena live funzionano
  con modifiche minori al Renderer (`drawMeshFrom`/`setViewportRect`). Toggle debug F9 in
  partita. Il lavoro restante per la feature vera (secondo input locale, secondo HUD) è
  additivo, non un redesign. Conferma visiva manuale (F9) in carico allo sviluppatore.

## 17. Memoria GFEditor crescente (WATCH — rilevata via telemetria 2026-07-09)
- editor_run: 73 → 259 MB in ~1 minuto di uso normale (heartbeat frame 600/1800/2400).
  Sospetti principali: ricaricamenti modello nel viewport (loadModel rigenera i buffer a
  ogni cambio slider), texture/FBO. Da profilare prima che diventi un problema di sessioni
  lunghe. L'engine resta stabile (~63 MB).

## 18. Sandbox: verifica post-fix "nemici non muoiono" (PENDING SMOKE)
- Causa identificata e corretta (profilo hitbox svuotato → headshot nel vuoto, vedi
  Changelog 2026-07-09 (7)). Profilo B1 ripristinato dal .bak. Da confermare al prossimo
  playtest sandbox: colpi a testa/corpo → log `hit:`/`kill:` in engine_run.log.

## 13. Hitbox zone rotation ignorata nel combat test (LOW)
- CombatSystem trasforma le zone per scala/yaw/meshOffsetY (fix 2026-07-04) ma il test resta
  AABB: `eulerDeg` per-zona (es. testa B1 a -58°) non inclina il volume di collisione.
  Serve un test OBB se la precisione diventa importante.

## 14. Asset default mancanti (LOW)
- `assets/textures/default.png` e `assets/models/default.obj` non esistono: fallback a
  checkerboard + cubo funziona, ma il log sporca l'avvio. Aggiungere gli asset o rimuovere
  i tentativi di caricamento.

## 15. ~~Salvataggi distruttivi (classe di bug)~~ — RESOLVED 2026-07-09 (ADR-010)
- `saveJsonRMW` centralizzato (`util/JsonSave.hpp`) con backup `.bak` automatico; **tutti**
  i save path di tutti i moduli migrati (nessun `ofstream` JSON fuori dall'helper). La
  regola RMW è ora un vincolo strutturale. I nuovi moduli DEVONO usare l'helper
  (04_CodingStandards).
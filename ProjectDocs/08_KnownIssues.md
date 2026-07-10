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

## 3. FreeCameraViewport GL/FBO state — CHIUSO 2026-07-10 (assorbito da #17)
- Il sospetto storico si è concretizzato come churn FBO da oscillazione dimensioni
  (KI #17, fixato con texture only-grow). Il render path FBO resta da tenere d'occhio
  solo se cambia la gestione multi-viewport ImGui.

## 4. ~~EntityEditor gizmo/marker only correct at identity transform~~ — RESOLVED 2026-07-04
- Gizmo targets are now set in world space via `toWorld()` (M = rotX*scale) and drag deltas
  are converted back with `deltaToLocal()` (inverse of M) at every call site + in `tick()`.
  Needs one manual verification pass with a scaled/rotated model (e.g. clone trooper).

## 5. ~~Clone Trooper oversized~~ — RISOLTO 2026-07-04 dall'utente
- Nuovo GLB + `mesh_scale` 0.011 autorato via editor (vedi 05_CurrentState "Resolved
  2026-07-04 (later batches)"). Voce rimasta aperta per svista, chiusa 2026-07-10.

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

## 11. MapDef lacks AI-relevant tactical metadata — RISOLTO 2026-07-10 (lato dati)
- 15_MapMetadata implementato: `MapDef.coverPoints/patrolRoutes/dangerZones`, parse nel
  registry, authoring nel MapEditor (sezione "Metadata AI"). Il CONSUMER runtime (AI
  tattica fase 2) resta da progettare/documentare — per scelta di scope, non per gap.

## 12. Split-screen/multi-viewport support unverified — RISOLTO 2026-07-09 (caso a)
- Spike ADR-011 eseguito: due viewport + seconda Camera sulla stessa scena live funzionano
  con modifiche minori al Renderer (`drawMeshFrom`/`setViewportRect`). Toggle debug F9 in
  partita. Il lavoro restante per la feature vera (secondo input locale, secondo HUD) è
  additivo, non un redesign. Conferma visiva manuale (F9) in carico allo sviluppatore.

## 17. Memoria GFEditor crescente — FIX PROBABILE 2026-07-10 (verifica al prossimo uso)
- Misura: sulla Home la memoria è PIATTA (67MB per 75s) → il leak era nei moduli col
  viewport 3D. Root cause identificata: `FreeCameraViewport::resizeFBO` ricreava
  FBO+texture+renderbuffer a OGNI oscillazione di pixel dell'area disponibile
  (scrollbar/separatori) → churn GL continuo (era anche il vecchio sospetto #3).
  Fix: texture allocata a multipli di 64 e SOLO ingrandita, pannello mostrato come
  sub-regione via UV; ogni realloc reale è loggato su stdout. **Da confermare:** una
  sessione d'uso normale nei moduli con l'heartbeat memoria stabile.

## 18. Sandbox: verifica post-fix "nemici non muoiono" (PENDING SMOKE)
- Causa identificata e corretta (profilo hitbox svuotato → headshot nel vuoto, vedi
  Changelog 2026-07-09 (7)). Profilo B1 ripristinato dal .bak. Da confermare al prossimo
  playtest sandbox: colpi a testa/corpo → log `hit:`/`kill:` in engine_run.log.

## 13. Hitbox zone rotation ignorata — RISOLTO 2026-07-10 (test OBB condiviso)
- Nuovo `physics/HitTest.hpp`: segmento-vs-OBB (yaw entità * eulerDeg zona, ordine
  Y*X*Z identico al wireframe editor), usato SIA dal CombatSystem SIA dal mirino
  (che ora è un segmento di 80m con lo stesso helper): mirino e proiettili
  concordano per costruzione anche su zone inclinate (testa B1 a -58°).

## 14. Asset default mancanti (LOW)
- `assets/textures/default.png` e `assets/models/default.obj` non esistono: fallback a
  checkerboard + cubo funziona, ma il log sporca l'avvio. Aggiungere gli asset o rimuovere
  i tentativi di caricamento.

## 15. ~~Salvataggi distruttivi (classe di bug)~~ — RESOLVED 2026-07-09 (ADR-010)
- `saveJsonRMW` centralizzato (`util/JsonSave.hpp`) con backup `.bak` automatico; **tutti**
  i save path di tutti i moduli migrati (nessun `ofstream` JSON fuori dall'helper). La
  regola RMW è ora un vincolo strutturale. I nuovi moduli DEVONO usare l'helper
  (04_CodingStandards).
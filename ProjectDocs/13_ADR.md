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

## ADR-006 — Hitbox single source of truth = PROFILE (2026-07-04)
- **Decision:** The hitbox PROFILE (`data/hitboxes/<id>.json`) is the only authoritative store,
  because it is what the runtime consumes. EntityEditor's Hitbox tab now reads/writes the
  profile referenced by `hitbox_profile` (auto-created with id = entity id if the reference is
  empty). Entity-inline `hitbox_zones` are **deprecated**: ignored when a profile exists, read
  only as a legacy fallback, erased from the entity JSON on the next save.
- **Consequence:** Zones authored in EntityEditor now reach the game. HitboxEditor and
  EntityEditor edit the same file — last save wins; both write the runtime schema
  (`damage_multiplier`, `bone`, `rotation`). Rationale for (a) over (b): zero runtime changes,
  one store instead of two.

## ADR-008 — IGameMode interface + factory (2026-07-04)
- **Decision:** Tutte le modalità implementano `IGameMode`
  (`include/mini/game/game_modes/IGameMode.hpp`): applySettings/start/update, accessor
  player/spawn/tickets, `hasVictoryCondition()`. Le istanze si creano SOLO via
  `createGameMode(id)` (`src/game/game_modes/GameModeFactory.cpp`, id: "conquest",
  "sandbox"; sconosciuto → fallback conquest + log). Application detiene un
  `unique_ptr<IGameMode>` e non conosce le classi concrete (rimosse le lambda `useSandbox`).
- **Consequence:** Assalto/Difesa (Fase 1) = nuova classe + una riga nella factory; la
  logica win/lose usa `hasVictoryCondition()` invece di flag di modalità. `MeshCache` è
  definito una sola volta in IGameMode.hpp. Prossima evoluzione naturale: id modalità
  scelto da MapDef/PreMatch invece che dal flag CLI.

## ADR-009 — Command post come sistema riusabile, dati nel MapDef (2026-07-04)
- **Decision:** I punti di comando sono dati di mappa (`MapDef.commandPosts`, JSON
  `command_posts`: label/x/y/z/radius/team/capture_time), autorati nel Map Editor come i
  box/spawn. La logica (presenza esclusiva nel raggio → cattura in captureTime secondi,
  conteso/vuoto → decay; visual palo+piastra colorati per team) vive nella classe riusabile
  `CommandPosts` (`include/mini/game/CommandPosts.hpp`), NON nei game mode.
- **Consequence:** Ogni modalità configura e interroga il sistema per le proprie regole:
  Conquista applica ticket bleed (maggioranza post → -1 ticket avversario ogni 6s);
  la Sandbox li rende catturabili senza conseguenze (test dal vivo). Le future
  Assalto/Difesa riusano lo stesso blocco con regole diverse. KnownIssues #9 chiuso.

## ADR-007 — Game-mode fallback ids come from the registry (2026-07-04)
- **Decision:** `ConquestMode::buildEnemySpawnList` no longer hardcodes archetype ids. When
  `MapDef.enemyTypes` is empty it falls back to the sorted list of ids actually registered in
  `data/enemies/`; if that is also empty, it spawns nothing and logs an error (no blind ids).
- **Consequence:** Renaming enemy files can no longer silently break spawning via dead
  hardcoded strings (KnownIssues #2 closed). Maps should still declare `enemyTypes` for
  intentional composition.

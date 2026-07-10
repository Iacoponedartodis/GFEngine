# 10 — Project Memory (durable, code-verified facts)

## Canonical conventions
- **id = filename stem** for every definition type (weapon, enemy, ally, ai profile, hitbox
  profile, map, ability, character). In-file `id`/`profile_id` is redundant/deprecated.
- **Data path resolution:** canonicalize `<exe>/../../../data` (project root `data/`),
  fallback `<exe>/data`. Same pattern for `assets/`. CMake POST_BUILD also copies data+assets
  next to each exe. Editors read/write the SOURCE `data/` so edits persist without rebuild.
- **Two-binary contract:** GFEngine (runtime) and GFEditor (tool) communicate only via files.
  Editor launches runtime with `--direct-prematch` (pre-match) or `--sandbox` (arena).
  Runtime must never link editor code.

## Rendering constraints (do not "modernize")
- OpenGL 3.3 Compatibility Profile, **client-side vertex arrays** (no VAO/VBO): Intel-driver
  workaround. `mini::Mesh` = 11 floats/vertex (pos3+nrm3+col3+uv2).
- `TINYGLTF_IMPLEMENTATION` only in `src/vendor/tinygltf_impl.cpp`.
- GFEssentials is NOT part of this project; all work targets GFEngine.

## GLB / skeleton facts
- Non-skinned GLBs (e.g. B1 droid) place geometry via node hierarchy -> loader BAKES node
  world transforms into vertices; their "skeleton" is named structural nodes.
- Skinned GLBs (e.g. clone trooper) -> vertices already in bind space -> loader uses IDENTITY;
  joints come from `skins[0].joints`. RigReader returns real world joint positions for both.
- Clone trooper GLB is FBX-cm scale (~285 units).

## Model placement
- `MeshRendererComponent.meshOffsetY` is applied at render time to drop feet to ground.
  GLB units set it to `-AI_HALF_Y` (ConquestMode) / `-footY - AI_HALF_Y` (SandboxMode).
  Cube placeholders use 0.

## Runtime hitbox source
- The game reads hitboxes from the **profile** (`EnemyDef.hitboxProfileId` ->
  `registry.getHitboxProfile`), NOT from entity-inline `hitbox_zones`. (ADR-006, resolved.)

## Save/rename tooling (ADR-010 — IMPLEMENTED 2026-07-09)
- **`saveJsonRMW` ESISTE** (`editor/include/util/JsonSave.hpp`): OGNI salvataggio JSON
  dell'editor DEVE usarlo (regola CLAUDE.md/04_CodingStandards). Fa RMW + backup `.bak`.
  Tutti i moduli esistenti sono già migrati; nessun `ofstream` JSON diretto è ammesso.
- **Comando Rinomina ESISTE** (`util/DefinitionRename.hpp`, UI in Weapon/Entity/Hitbox/Map
  editor): rinomina file + sweep cross-ref con mappa esplicita + pulizia id deprecati.
  Mai rinominare creando un nuovo file a mano.
- I save rimuovono progressivamente i campi `id`/`profile_id` deprecati dai JSON (ADR-001).

## Data integrity incidents (confirmed, do not repeat)
- **2026-07-09 (#2):** cambiare `hitbox_profile` nel combo EntityEditor senza ricaricare le
  zone dal profilo selezionato → il salvataggio scriveva le zone del profilo PRECEDENTE su
  quello nuovo (B1 svuotato via Heavy). Recuperato dal `.bak` automatico (ADR-010). Regola:
  ogni combo che cambia un RIFERIMENTO a un file condiviso deve ricaricare lo stato in
  editing da quel file (fix: `loadZonesFromProfile`).
- **2026-07-08:** an editor save path (`BalanceEditor::saveMap`) wrote a fresh JSON object
  with only the old schema's fields, destroying `geometry`/`command_posts`/`ally_*` from
  `firebase.json`. Root cause: missing read-modify-write. **Now structurally enforced**
  via ADR-010 (Accepted): centralized `saveJsonRMW` used by every save path.
- **2026-07-09 (user-reported):** renaming a weapon by creating a new file/id instead of
  using a rename tool left the old file in place, causing duplicate-looking entries in the
  in-game loadout menu. Root cause: no in-editor rename command exists yet, despite
  id=filename (ADR-001) being otherwise sound. Tracked as 08_KnownIssues #7 (escalated to
  P0) and ADR-010 (Proposed). **Do not manually create a new file to "rename" a definition**
  until the rename tool exists — delete the old file and grep every cross-reference instead
  (04_CodingStandards, Migration Discipline).

## Game vision (Star Wars: Galactic Front) — treat as active, not speculative
- The project targets a **modular war ecosystem**, not a single linear game: FPS/TPS tactical
  combat, strategic command layer, RPG-style progression, and a battle sandbox all share one
  **core battlefield system** (combat, units/classes, hierarchical AI, objectives, vehicles,
  modular maps, spawn/command posts). New modes must be configurations of this core.
- Development is explicitly phased (see 00_Vision.md): Phase 1 core playable shooter must be
  fun on its own before any tactical/progression/strategic layer is added.
- **Local split-screen co-op is a real functional requirement; online competitive multiplayer
  is explicitly out of scope.** Any future "scale" discussion (e.g. large battles) refers to
  local simulation scale (AI/entity counts), not network session scale. Feasibility has not
  yet been empirically verified — see ADR-011 (Proposed, 13_ADR): a time-boxed spike is
  planned but not yet run.
- The editor's role extends beyond balancing: it is the **metadata authoring system** for the
  whole project — map metadata useful to AI (cover/patrol/danger zones — see 15_MapMetadata,
  Planned Feature, in addition to the already-implemented geometry/spawn/command-post data),
  and model metadata for weapons/characters (attach points, bones, animations, hitboxes) that
  allows fluid integration of new weapons/armor onto character models without engine code
  changes. EntityEditor's bone-bindable attach points and WeaponEditor's attach_points are
  the first concrete implementation of this principle.

## User-stated long-term direction
- **Dropdown-based data assignment everywhere (no free-text ids).** No longer just a stated
  preference — binding rule in 04_CodingStandards, audit tracked as P0 in 06_Todo #2.
- **In-editor rename for all definition types with cross-reference awareness.** Design
  registered in ADR-010 (Proposed, 13_ADR); implementation tracked as P0 in 06_Todo #1.
- Future UI/Interface Editor to centralize menu text/layout/palette/fonts.
- EntityEditor is the primary enemy/ally tool (BalanceEditor is now a redirect).
- **Future Map Metadata layer** (cover/danger/patrol/sectors) consumed by tactical AI —
  schema and authoring plan now documented in 15_MapMetadata (Planned Feature); AI
  consumption logic still undesigned (deliberately, per that document's Out of Scope).
- ~~Future GameMode interface/registry~~ — IMPLEMENTED (ADR-008): Assault/Defense/strategic
  modes are configurations via `createGameMode()`, not new hardcoded classes.
- **Future Class concept** (weapon + equipment + role composition) distinct from a single
  weapon — schema now documented in 14_ClassSystem (Planned Feature), ahead of Phase 3.

## Planned Feature index (aggiornato 2026-07-10)
- 14_ClassSystem.md — `ClassDef` + `PlayerDef.classId` — **ancora Planned, zero codice**.
- 15_MapMetadata.md — IMPLEMENTATO (dati + authoring MapEditor); consumer AI in 18.
- 16_AiBehavior.md — IMPLEMENTATO (scope core); abilità attive ancora out.
- 17_SandboxTools.md — IMPLEMENTATO; gadget player-side ancora out.
- 18_AiMapConsumption.md — IMPLEMENTATO; pathfinding e pose alle cover ancora out.
- 19_Vehicles.md — Fase A IMPLEMENTATA; Fase B pianificata (armi di bordo, multi-posto,
  AI alla guida, hitbox/attach veicoli, respawn mezzi, collider reciproco).
- ADR-010 — FATTO (rename + saveJsonRMW). ADR-011 — spike FATTO, esito (a), Accepted.

## Vincoli confermati sul codice reale (sessione 2026-07-10)
- **Gli input sintetici (SendKeys / keybd_event) NON raggiungono la finestra SDL** nei
  test automatizzati da questa postazione (finestra senza focus reale; verificato con
  l'input recorder vuoto). I bug di gameplay interattivo si diagnosticano con
  telemetria dedicata letta dal log del run dell'utente (`drive:`, `veicolo: E...`,
  heartbeat `ai:`), non provando a pilotare il gioco dall'esterno.
- Il titolo della finestra engine è "GFEngine v0.1" (non "GFEngine").
- **Pattern mailbox su World** per la comunicazione sistemi↔Application senza
  accoppiare l'ECS al codice di gioco: `combatFeedback`, `eventFeed`, `activeMap`
  (puntatore opaco + forward declaration). Preferirlo a nuovi include di gioco in ecs/.
- L'area disponibile di un pannello ImGui può OSCILLARE di pochi px tra frame: ogni
  risorsa GL dimensionata su di essa deve essere only-grow/con isteresi (incidente
  KI #17: churn FBO nel viewport → centinaia di MB/min).
- Liste `enemy_types`/`ally_types` di MapDef VUOTE = auto (tutte le definizioni
  registrate, ordinate): comportamento voluto, scritto nella UI del BalanceEditor.
  Non "riempire per sicurezza" le liste nei dati.
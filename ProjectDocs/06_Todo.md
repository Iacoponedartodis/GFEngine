# 06 — Todo (reality-based, prioritized)

## Done 2026-07-04
- ~~Unify hitbox authoring~~ → ADR-006: profile = single source of truth; EntityEditor
  reads/writes the profile; inline zones deprecated + B1 data migrated.
- ~~ConquestMode dead fallback ids~~ → ADR-007: registry-derived fallback, empty-safe.
- ~~EntityEditor gizmo under scale/rotation~~ → toWorld()/deltaToLocal() at all call sites.
- ~~.gitignore hygiene~~ → .gitignore rewritten (was corrupted), build/ + imgui.ini +
  presets.cfg untracked.

## Done 2026-07-04 (later batches)
- ~~GameMode interface + factory~~ → ADR-008 (`IGameMode` + `createGameMode`); smoke test
  runtime `--sandbox` passato. KnownIssues #8 chiuso.
- ~~Editor professionalization~~ → gizmo 3 modalità, slider ovunque, camera Unreal-style,
  WeaponEditor attach point nel viewport.

## Done 2026-07-04 (5)
- ~~Objective / Command Post riusabile~~ → ADR-009: `CommandPosts` + MapDef + Map Editor
  authoring + ticket bleed in Conquista + test in sandbox. KnownIssues #9 chiuso.

## Done 2026-07-09 — ADR-010 implementato (messa in regola)
- ~~[P0] Rename tooling~~ → `DefinitionRename` (mappa cross-ref esplicita) + UI "Rinomina"
  in Weapon/Entity/Hitbox/Map editor. **Smoke GUI manuale pendente** (KnownIssues #7).
- ~~[P0] Dropdown-only audit~~ → PASSATO: gli unici InputText residui sono creazione nuovi
  id, nomi/etichette/descrizioni e path mesh — nessuna assegnazione di id via testo libero.
- ~~#8 Save-safety helper~~ → `saveJsonRMW` (util/JsonSave.hpp, con .bak) + TUTTI i save
  path migrati. KnownIssues #15 chiuso. In più: `id`/`profile_id` deprecati rimossi dai
  JSON a ogni save (ADR-001).

## P0 — Blocking (superato, riferimento storico)

1. **[P0] Rename tooling for all definition types (was #16, promoted from Low/future).**
   User-confirmed symptom (2026-07-09): renaming weapon names/ids without the tool produced
   duplicate-looking weapon entries in the loadout menu. Root cause: id=filename (ADR-001) +
   no rename command → manual "rename by creating a new file" leaves the old file as an
   orphaned near-duplicate. Scope:
   - Add a "Rinomina" action in EntityEditor, WeaponEditor, HitboxEditor, MapEditor (AI
     profile editor when it exists).
   - Implementation must: rename the physical file (`std::filesystem::rename`), update the
     in-memory id, sweep and rewrite every cross-reference in `data/` that pointed to the old
     id (`weaponIds[]`, `aiProfileId`, `hitboxProfileId`, `enemyTypes[]`, `allyTypes[]`,
     `MapDef` references), then reload the registry.
   - Must use the RMW save discipline (04_CodingStandards) for every file it touches.
   - See 13_ADR, ADR-010 (Proposed) for the full decision record.
   - Blocks: clean data authoring for the rest of Phase 1 content (more weapons/enemies).

2. **[P0] Dropdown-only enforcement audit.** Full audit of every editor module for any
   remaining free-text id input (`ImGui::InputText` on an id/reference field). Convert to
   registry-backed combo per 04_CodingStandards. This is a prerequisite for Todo #1 above:
   the rename tool is only fully effective if there is no remaining path to type a stale id
   by hand.

## High priority

0. ~~Sandbox: selettore armi~~ → FATTO 2026-07-09 (10): tasti 1-9 equipaggiano qualunque
   arma del registry (ordinate per nome, incluse separatiste), toast col nome + hint
   all'avvio, log in telemetria.

3. ~~AI: abilità + ruoli tattici~~ → FATTO 2026-07-10 (core, doc 16_AiBehavior):
   aggression→distanza d'ingaggio, retreat_hp_threshold→disimpegno, peek/hide da
   cover_preference, flank_chance in Hunt; ability "shield" runtime (ShieldComponent,
   assorbimento + regen in CombatSystem). Resta (16_AiBehavior Out of Scope): abilità
   attive (roll/jetpack/...), vera copertura geometrica (dipende da 15_MapMetadata).
4. ~~Assault/Defense mode~~ → FATTO 2026-07-09 (11), ADR-014: ObjectiveModes + selezione
   nel PreMatch + esito deciso dal mode. Resta (minor): salvare modeIndex nei preset.
5. ~~Runtime weapon-in-hand~~ → FATTO 2026-07-04 (8): WeaponAttach.hpp + attachMesh nel
   renderer. Resta: assegnare mesh alle armi Republic (DC-15A ecc.) nel Weapon Editor.
6. ~~HUD: stato command post~~ → FATTO 2026-07-09 (11): barra in alto con proprietario,
   lettera e progresso di cattura per ogni post.
7. ~~Dato mancante: profilo AI "grunt"~~ → FATTO 2026-07-09 (2): creato `data/ai/grunt.json`
   (infantry), log pulito, 3 profili caricati.
8. **Save-safety helper centralizzato.** Introdurre un helper condiviso
   (`saveJsonRMW(path, patchFn)`) usato da tutti i moduli editor per ogni salvataggio JSON,
   così la regola RMW (04_CodingStandards) diventa un vincolo strutturale e non solo una
   disciplina documentata. Motivato dall'incidente reale del 2026-07-08 (clobber di
   firebase.json). Vedi ADR-010 candidate scope in 13_ADR.

## Medium

9. **Clone Trooper scale.** Normalize oversized FBX-cm GLB (author `mesh_scale` per entity or
   pre-scale asset). (KnownIssues #5)
10. **Weapon attach-point authoring in WeaponEditor** must define a `right_hand`/`grip` point
    so the EntityEditor weapon-in-hand aligns precisely (currently defaults to weapon origin).
11. **Runtime weapon rendering:** consume `weapon_display` (or equivalent) to actually show
    the weapon in the character's hand in-game.
12. **Commit the hygiene change** (untracked build/ + new .gitignore) — staged, needs a
    commit.
13. **Introduce explicit Objective and Command Post concepts.** Required by Vision Phase 1/2
    to express Conquest/Assault/Defense as data configurations instead of bespoke mode logic.
    (KnownIssues #9)
14. **Introduce explicit Class concept** (weapon + equipment + role composition) separate
    from a single weapon definition, ahead of Phase 3 progression work. (KnownIssues #10)
15. **Extend MapDef with AI-relevant metadata** (cover points, patrol routes, danger zones)
    and a runtime MapMetadata consumer, beyond the current geometry-only data.
    (KnownIssues #11)
16. ~~Verify split-screen/multi-viewport feasibility~~ → FATTO 2026-07-09 (12): spike
    ADR-011 eseguito, esito (a) — fattibile con modifiche minori. Toggle F9 in partita.
    KnownIssues #12 chiuso; il soft-gate ADR-011 decade.

## Low / future

17. AI Editor module (dropdown-driven, rename-capable — inherits Todo #1 tooling once built).
18. Asset Manager module.
19. UI/Interface Editor (centralize menu text/layout/palette/fonts — currently scattered).
20. Data hygiene pass to remove existing near-duplicate weapon JSONs (manual cleanup once
    rename tooling — Todo #1 — exists, to avoid re-creating the same problem while cleaning).
21. Define AI hierarchy extension points (squad/strategic tiers) even before implementing
    them, so individual-agent AI doesn't need a rewrite when Phase 2/5 AI is added.
22. Vehicle system as ECS-composable entities (multi-seat), not bespoke controllers.
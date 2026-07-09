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

## High priority
1. **AI: salto + abilità + ruoli tattici.** L'AI ha gravità/step-up ma non salta mai
   (ostacoli > STEP_HEIGHT la bloccano); non usa le abilità (shield/roll/...) né i campi del
   profilo AI (accuracy, coverPreference, peek/hide). Progettare in AiSystem: salto quando
   stuck contro ostacolo basso, uso abilità data-driven, comportamento per ruolo.
2. **Assault/Defense mode** come nuove registrazioni della factory: regole diverse sugli
   stessi CommandPosts. Config da MapDef (mode id / post assegnati).
3. ~~Runtime weapon-in-hand~~ → FATTO 2026-07-04 (8): WeaponAttach.hpp + attachMesh nel
   renderer. Resta: assegnare mesh alle armi Republic (DC-15A ecc.) nel Weapon Editor.
4. **HUD: stato command post** (proprietario + progresso cattura).
5. **Dato mancante: profilo AI "grunt"** referenziato da Clone Trooper non esiste in
   `data/ai/` (log "AiProfileDef non trovato") — crearlo nell'editor o riassegnare.

## Medium
5. **Clone Trooper scale.** Normalize oversized FBX-cm GLB (author `mesh_scale` per entity or
   pre-scale asset). (KnownIssues #5)
6. **Weapon attach-point authoring in WeaponEditor** must define a `right_hand`/`grip` point so
   the EntityEditor weapon-in-hand aligns precisely (currently defaults to weapon origin).
7. **Runtime weapon rendering:** consume `weapon_display` (or equivalent) to actually show the
   weapon in the character's hand in-game.
8. **Commit the hygiene change** (untracked build/ + new .gitignore) — staged, needs a commit.
9. **Introduce explicit Objective and Command Post concepts.** Required by Vision Phase 1/2 to
   express Conquest/Assault/Defense as data configurations instead of bespoke mode logic.
   (KnownIssues #9)
10. **Introduce explicit Class concept** (weapon + equipment + role composition) separate from
    a single weapon definition, ahead of Phase 3 progression work. (KnownIssues #10)
11. **Extend MapDef with AI-relevant metadata** (cover points, patrol routes, danger zones) and
    a runtime MapMetadata consumer, beyond the current geometry-only data. (KnownIssues #11)
12. **Verify split-screen/multi-viewport feasibility** in the current camera/input pipeline
    before further systems assume a single active local player. (KnownIssues #12)

## Low / future
13. AI Editor module (dropdown-driven, rename-capable).
14. Asset Manager module.
15. UI/Interface Editor (centralize menu text/layout/palette/fonts — currently scattered).
16. Consistent rename tooling for all definition types with cross-reference updates.
17. Data hygiene pass to remove near-duplicate weapon JSONs (duplicate-looking lists).
18. Define AI hierarchy extension points (squad/strategic tiers) even before implementing them,
    so individual-agent AI doesn't need a rewrite when Phase 2/5 AI is added.
19. Vehicle system as ECS-composable entities (multi-seat), not bespoke controllers.
# 06 — Todo (reality-based, prioritized)

## High priority
1. **Unify hitbox authoring.** Decide single source of truth between entity-inline
   `hitbox_zones` and shared hitbox PROFILE (`data/hitboxes/`). The runtime uses the profile;
   EntityEditor writes inline zones that never reach the game. Options: (a) EntityEditor writes
   to the profile referenced by `hitboxProfileId`; (b) runtime also reads inline zones. Pick one,
   ADR it, migrate. (KnownIssues #1)
2. **Remove/repair ConquestMode dead fallback ids** `grunt/heavy/sniper` in
   `buildEnemySpawnList` — they resolve to nothing. Replace with a safe fallback (first
   available enemy id from registry) or require `MapDef.enemyTypes`. (KnownIssues #2)
3. **EntityEditor gizmo correctness under scale/rotation.** Gizmo target + delta assume
   character transform = identity. Transform target by M and delta by M^-1 for correctness.
   (KnownIssues #4)

## Medium
4. **Clone Trooper scale.** Normalize oversized FBX-cm GLB (author `mesh_scale` per entity or
   pre-scale asset). (KnownIssues #5)
5. **Weapon attach-point authoring in WeaponEditor** must define a `right_hand`/`grip` point so
   the EntityEditor weapon-in-hand aligns precisely (currently defaults to weapon origin).
6. **Runtime weapon rendering:** consume `weapon_display` (or equivalent) to actually show the
   weapon in the character's hand in-game.
7. **.gitignore hygiene:** ensure `build/`, `imgui.ini`, `presets.cfg` are ignored, not tracked.
   (KnownIssues #6)

## Low / future
8. AI Editor module (dropdown-driven, rename-capable).
9. Asset Manager module.
10. UI/Interface Editor (centralize menu text/layout/palette/fonts — currently scattered).
11. Consistent rename tooling for all definition types with cross-reference updates.
12. Data hygiene pass to remove near-duplicate weapon JSONs (duplicate-looking lists).

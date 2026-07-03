# 05 — Current State

_Last verified: 2026-07-03 (against live code)._

## Working
- DefinitionRegistry loads weapons/enemies/allies/ai/hitboxes/maps/abilities/characters
  from `data/`, id = filename stem.
- Two binaries build clean (GFEngine + GFEditor), Debug preset `windows-debug`.
- **Data-driven map:** `MapDef.geometry` authored in MapEditor, read by ConquestMode and
  SandboxMode. `firebase.json` now holds a ~50x40 arena + spawn points.
- **ConquestMode** reads `MapDef.spawnTeam1/2` (player + procedural unit spread) and
  `enemyTypes`/`allyTypes`. Units spawn at ground level.
- **SandboxMode** (`--sandbox`): firebase geometry, player at team1 spawn, respawning
  dummies at team2 spawn (stationary, damageable).
- **GLB pipeline:** node-hierarchy baking (non-skinned) / identity (skinned), multi-primitive
  merge, byteStride-correct accessor reads. `meshOffsetY` applied in render (no floating models).
- **EntityEditor:** mesh browse (+ saved), transform, rig bones visible/clickable, attach
  points (bone-bindable, rendered as boxes + text labels), inline hitbox zones (bone-bindable),
  weapon-in-hand pose persisted as `weapon_display`.
- **HitboxEditor:** 3-column layout, 3D viewport (model+bones+wireframe zones), bone binding,
  auto-snap of bone-bound zones, gizmo.
- **MapEditor & HitboxEditor & EntityEditor:** 3-axis translation gizmo + selection visible
  through models (editing overlays drawn depth-always).
- Weapon GLBs assigned: E5/E-5C -> e-5_blaster_rifle.glb, DC-17 -> dc-17.glb.
- Enemy/ally meshes assigned (B1 droids, Clone Trooper).

## Partial / fragile
- **Hitbox duplication:** entity inline `hitbox_zones` (EntityEditor) vs shared hitbox
  PROFILE (`data/hitboxes/`, HitboxEditor). **The game uses the PROFILE only.** Inline zones
  are editor-only. See KnownIssues #1 — needs unification.
- **ConquestMode enemy fallback ids** `grunt/heavy/sniper` don't exist as enemy files;
  only works because maps supply `enemyTypes`. KnownIssues #2.
- **EntityEditor gizmo/marker** correct only at character scale=1, rotX=0 (raw model-space).
  KnownIssues #4.
- **Clone Trooper GLB** ~285 units (FBX cm) with meshScale=1 -> oversized in-game. KnownIssues #5.

## Not implemented
- AI Editor, Asset Manager, UI/Interface Editor modules.
- Rename tooling for all definition types (partial in BalanceEditor history).
- Weapon rendering in the actual runtime (weapon_display is editor-only preview so far).

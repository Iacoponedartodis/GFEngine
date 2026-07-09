# 05 — Current State

_Last verified: 2026-07-04 (against live code)._

## Position vs Vision roadmap (00_Vision)
**We are mid/late-Phase 1** ("core playable"). Present: 1 map (firebase, data-driven),
infantry both factions, working weapons, spawns, base AI, **IGameMode + factory (ADR-008)**,
**command post catturabili con ticket bleed (ADR-009, autorabili nel Map Editor)**, Conquest
+ Sandbox, editor suite pro (gizmo 3 modalità, slider, camera Unreal-style). Missing for
Phase 1 exit: Assault/Defense come registrazioni della factory, vehicles, runtime
weapon-in-hand, HUD stato post, split-screen feasibility check, "is it fun" iteration.
Phases 2-5 not started (by design).

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
- **MapEditor & HitboxEditor & EntityEditor:** gizmo a 3 modalità (Sposta/Ruota/Scala,
  scorciatoie 1/2/3, barra [Sposta][Ruota][Scala] per modulo) + selezione visibile attraverso
  i modelli; pannelli proprietà a slider+campo numerico (`UiWidgets::sliderRow`); wireframe
  hitbox rotation-aware anche nell'EntityEditor.
- Weapon GLBs assigned: E5/E-5C -> e-5_blaster_rifle.glb, DC-17 -> dc-17.glb.
- Enemy/ally meshes assigned (B1 droids, Clone Trooper).

## Resolved 2026-07-04
- Hitbox authoring unified on the PROFILE (ADR-006); EntityEditor + HitboxEditor edit the same
  store the runtime reads. B1 inline zones migrated out.
- ConquestMode fallback ids now registry-derived (ADR-007).
- EntityEditor gizmo correct under scale/rotation (toWorld/deltaToLocal).
- Repo hygiene: .gitignore rewritten, build/+imgui.ini+presets.cfg untracked (to commit).

## Resolved 2026-07-04 (later batches)
- GameMode abstraction (ADR-008): `IGameMode` + factory; Application interface-only.
- Editor pro: gizmo 3 modalità, slider ovunque, camera Unreal-style (RMB look/fly, wheel,
  MMB pan, niente volo mentre si digita), WeaponEditor attach point nel viewport.
- Clone Trooper scale: risolto dall'utente via editor (nuovo GLB + mesh_scale 0.011).

## Partial / fragile
- **HitboxEditor/EntityEditor concurrency:** same profile file, last save wins (ADR-006 note).
- **Mode id dal flag CLI:** la scelta modalità dovrebbe in futuro venire da MapDef/PreMatch
  (nota in ADR-008).

## Not implemented
- AI Editor, Asset Manager, UI/Interface Editor modules.
- Rename tooling for all definition types (partial in BalanceEditor history).
- Weapon rendering in the actual runtime (weapon_display is editor-only preview so far).

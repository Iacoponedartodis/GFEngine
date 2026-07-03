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
  `registry.getHitboxProfile`), NOT from entity-inline `hitbox_zones`. (See KnownIssues #1.)

## User-stated long-term direction
- Dropdown-based data assignment everywhere (no free-text ids).
- In-editor rename for all definition types with cross-reference awareness.
- Future UI/Interface Editor to centralize menu text/layout/palette/fonts.
- EntityEditor is the primary enemy/ally tool (BalanceEditor is now a redirect).

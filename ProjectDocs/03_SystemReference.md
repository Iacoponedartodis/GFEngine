# 03 — System Reference

## DefinitionRegistry (`src/game/data/DefinitionRegistry.cpp`)
Loads all definitions from a data root. Loaders: `loadAbilities`, `loadWeapons`,
`loadAiProfiles`, `loadHitboxProfiles`, `loadEnemies`, `loadAllies`, `loadMaps`,
`loadPlayerDefs` (called by `loadAll`). JSON helpers: `gets/geti/getf/getb/getColor/getStrArray`.
- **id convention:** `gets(*j, "id", entry.path().stem())` — filename stem is canonical;
  any in-file `id`/`profile_id` is redundant.
- Accessors: `getWeapon/getEnemy/getAlly/getMap/getAiProfile/getHitboxProfile/getAbility/getPlayerDef`
  (return `nullptr` + cerr on miss). Map views: `weapons()/enemies()/allies()/maps()/...`.
- Maps stored in `m_maps`, allies in `m_allies` (same `EnemyDef` struct, team forced to 1).
- **Planned additions (not yet implemented):** `loadClasses()` + `getClass()`/`classes()`
  (see 14_ClassSystem) — mirrors the existing loader pattern exactly, additive only.

## Schemas (`include/mini/game/data/Definitions.hpp`)
- `WeaponDef`: damage, fireRate, bullet*, spread*, heat*, effectiveRange, meshPath,
  projectileMeshPath, faction.
- `EnemyDef` (enemies AND allies): faction, team, meshPath, texturePath, color, `meshRotX/Y`,
  `meshScale`, `footAttach[3]` (+`footY()`), aiProfileId, hitboxProfileId, weaponIds[],
  abilityIds[], hp/moveSpeed/damageScale, bulletColor.
- `AiProfileDef`: role, sight/fov/hearing, reaction, aggression, accuracy, cover, patrol/seek
  speed, shootInterval, etc.
- `HitboxProfile` + `HitZone` (`include/mini/ecs/components/HitboxComponent.hpp`): name,
  offset, halfExtents, damageMultiplier, boneName, eulerDeg.
- `MapDef`: id, name, meshPath, spawnTeam1/2[3], maxTickets, enemyCount/allyCount,
  enemyTypes[], allyTypes[], **`geometry` (vector<MapGeometryBox>)**, `commandPosts`.
  **Planned additions (not yet implemented, see 15_MapMetadata):** `coverPoints[]`,
  `patrolRoutes[]`, `dangerZones[]` — all optional, empty by default, zero impact on
  existing maps until authored.
- `MapGeometryBox`: x/y/z (center), ry, sx/sy/sz (full size), r/g/b, collider.
- `AbilityDef`, `PlayerDef`.
- **Planned addition (not yet implemented, see 14_ClassSystem):** `ClassDef`
  (id, name, primaryWeaponId, secondaryWeaponId, abilityIds[], role) + optional
  `PlayerDef.classId` reference.

## Game modes (`src/game/game_modes/`)
- **ConquestMode** (main): reads `MapDef` for spawn points + `enemyTypes`/`allyTypes`;
  builds unit positions procedurally around spawns; builds geometry from `MapDef.geometry`
  (fallback to a hardcoded box set if empty). `buildEnemySpawnList` fallback ids
  `grunt/heavy/sniper` are DEAD (no such enemy files) — ~~see KnownIssues~~ RESOLVED
  (ADR-007): fallback is now registry-derived.
- **SandboxMode**: player at spawn_team1, loads firebase `MapDef.geometry`, spawns a line of
  respawning dummies at spawn_team2 (stationary, damageable, 100 hp, 2s respawn).

## Rendering of unit meshes
`Application` render applies `MeshRendererComponent.meshOffsetY` (translate Y) so GLB models
(feet at model Y=0) sit on the ground. GLB loader (`Model::loadFromGltf`) bakes node-hierarchy
transforms for non-skinned meshes, uses identity for skinned meshes (vertices already in bind
space); `Model::merged()` combines multi-primitive models into one `Mesh`.

## Editor modules
- **EntityEditor**: primary enemy/ally authoring. Mesh browse, transform, rig bones (from GLB),
  attach points (bone-bindable + visualised as boxes+labels), inline hitbox zones (bone-bindable),
  weapon-in-hand pose (`weapon_display`). Saves `data/enemies|allies/*.json`.
  Reference implementation for the dropdown-only assignment pattern (04_CodingStandards) —
  any new module assigning a definition-to-definition reference must replicate this pattern.
- **WeaponEditor**: weapon defs, mesh, mesh_scale, attach_points. Saves `data/weapons/*.json`.
- ~~HitboxEditor~~ — RIMOSSO (ADR-012): l'authoring hitbox vive solo nell'EntityEditor
  (tab Hitbox: zone, danno, rotazioni, bone binding, debug_visible, wireframe, gizmo);
  il formato profilo runtime (`data/hitboxes/*.json`) resta invariato (ADR-006).
- **MapEditor**: map geometry boxes + spawn points + command posts, gizmo, saves
  `MapDef.geometry` + spawn_team1/2 + `command_posts` into `data/maps/*.json`.
  **Planned extension (not yet implemented, see 15_MapMetadata):** a Map Metadata section
  authoring `coverPoints[]`/`patrolRoutes[]`/`dangerZones[]` with the same gizmo/sliderRow
  pattern.
- **BalanceEditor**: reduced to read-only redirect for enemies/allies -> EntityEditor.
- **FreeCameraViewport**: shared FBO viewport (grid/model/attachment/hitbox/bones/markers,
  click-picking of bones+markers, `popClickedItem`). Gizmo a 3 modalità
  (Sposta/Ruota/Scala — scorciatoie 1/2/3): `setGizmoMode`, `setGizmoRotAxes`,
  `setGizmoCanRotateScale`, `popGizmoDelta`/`popGizmoRotDelta`/`popGizmoScaleDelta`.
  Wireframe hitbox rotation-aware (eulerDeg, ordine Y*X*Z).
  **Navigazione:** RMB tenuto = mouselook + WASD/QE (Shift veloce, rotella = velocità);
  rotella = dolly; MMB = pan; TAB capture come alternativa. Il volo è gated su
  navigazione attiva + `!WantTextInput` (mai mentre si digita).
- **UiWidgets** (`editor/include/util/UiWidgets.hpp`, header-only): `sliderRow`/`sliderRow3`
  (slider + campo numerico), `gizmoModeBar`. Da usare in ogni nuovo pannello proprietà.
- **Telemetry** (`include/mini/core/Telemetry.hpp`, ADR-013): `init/shutdown`,
  `logTrace/Info/Warn/Error`, `beginFrame`/`frame`, `recordInput`, `dumpGameState(json)`,
  `memoryUsageMB`. Ogni nuovo sistema DEVE loggare qui i propri stati chiave (init,
  errori, transizioni). Artefatti in `_telemetry_data/`; F12 = dump stato in partita.
- **Planned tooling (not yet implemented, see ADR-010 in 13_ADR):** an in-editor "Rinomina"
  command in every module above (rename physical file + sweep cross-references + reload
  registry), and a centralized `saveJsonRMW` helper replacing hand-rolled read-modify-write
  in every `save*()` function.
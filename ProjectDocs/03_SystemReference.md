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
- `MapDef`: id, name, meshPath, metadataPath, navmeshPath, spawnTeam1/2[3], maxTickets,
  enemyCount/allyCount, enemyTypes[], allyTypes[], **`geometry` (vector<MapGeometryBox>)**,
  `commandPosts`, e i **Map Metadata IMPLEMENTATI (15_MapMetadata, doc):** `coverPoints[]`
  (`CoverPointDef`: x/y/z, facingDeg, height), `patrolRoutes[]` (`PatrolRouteDef`: id + points),
  `dangerZones[]` (`DangerZoneDef`: x/y/z, radius, dangerLevel). Opzionali, vuoti di default.
  Consumati a runtime: AiSystem (cover/danger/patrol, doc 18) e NavManager (aree navmesh
  DANGER/COVER, ADR-017/doc 22). NB: `MapGeometryBox` runtime NON ha `type`/`label` (editor-only).
- `MapGeometryBox`: x/y/z (center), ry, sx/sy/sz (full size), r/g/b, collider.
- `AbilityDef`, `PlayerDef`.
- **Planned addition (not yet implemented, see 14_ClassSystem):** `ClassDef`
  (id, name, primaryWeaponId, secondaryWeaponId, abilityIds[], role) + optional
  `PlayerDef.classId` reference.

## Game modes (`src/game/game_modes/`, dietro `IGameMode` + factory ADR-008)
Interfaccia `IGameMode`: `start()`, `update(World,dt)`, `outcome(World)` (Win/Lose/Ongoing —
ADR-014), `getPlayerEntity/getSpawnPos`, `getTeam1/2Tickets`, `applySettings`, `commandPosts()`.
- **ConquestMode** (base): legge `MapDef` per spawn + `enemyTypes`/`allyTypes`; posiziona le
  unità a GRIGLIA attorno agli spawn (`genPositions`, `findFreeSpot`); costruisce la geometria
  da `MapDef.geometry` (fallback arena hardcoded se vuota); spawn player posato a terra
  (`groundedSpawn`). `updateObjectiveRules`: la maggioranza dei command post drena i ticket
  avversari (ticket bleed). Fallback id registry-derived (ADR-007).
- **AssaultMode / DefenseMode** (`ObjectiveModes.{hpp,cpp}`, ADR-014): derivano da ConquestMode
  overridando `updateObjectiveRules` + `outcome` + ownership iniziale dei post. Assalto: post
  ai nemici, ticket alleati calano, vittoria = tutti i post. Difesa: speculare.
- **SandboxMode** (`--sandbox`): player allo spawn_team1 (posato a terra), geometria della mappa,
  manichini a GRIGLIA su entrambi gli spawn (uno per definizione, respawn 15s), veicoli da
  `vehicleSpawns`, command post. Menu prova TAB (17_SandboxTools) + simulazione osservatore.

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
- **Telemetry** (`include/mini/core/Telemetry.hpp`, ADR-013+016 — dettagli doc 21):
  `init/shutdown`, `logTrace/Info/Warn/Error` (→ `engine_run.log`), `beginFrame`/`frame`,
  `recordInput`, `dumpGameState(json)` (→ `game_state.json`), `memoryUsageMB`; e il sink JSONL
  LLM-observable: `event(Level, system, msg, json data)` + `flushEvents` (→ `session_latest.jsonl`,
  una riga JSON per evento). `setStateDumpCallback` per il dump su crash. Ogni nuovo sistema DEVE
  loggare qui i propri stati chiave (init, errori, transizioni). Artefatti in `_telemetry_data/`.
- **Tooling ADR-010 — IMPLEMENTATO (2026-07-09):** comando "Rinomina" nei moduli
  (rename file + sweep cross-ref + reload; categorie incluse Vehicle) e `saveJsonRMW`
  centralizzato usato da ogni save path.
## Sistemi aggiunti 2026-07-10 (riferimento rapido; dettagli nei doc 16-19)
- **Shield runtime** (`ShieldComponent`, storage in World): assorbe danno prima degli
  HP in CombatSystem, regen dopo `regenDelay`; assegnato allo spawn da `abilities[]`
  con AbilityDef `type=="shield"` (param1/2/3 = hp/regen/delay). Doc 16.
- **Comportamento tattico AI dal profilo**: aggression→distanza d'ingaggio,
  retreat_hp_threshold→disimpegno, peek/hide (cover_preference), flank in Hunt,
  Search con timeout→Patrol, ricerca attorno alla lastKnown. Doc 16 + fix in 07 (4).
- **Log chat in-game**: `World::eventFeed` (mailbox) → HUD (`pushFeed`); L pannello,
  PAGSU/PAGGIU scroll, righe recenti con fade. Doc 17.
- **Sandbox tools**: `SandboxMenu` (TAB: Armi con slot primaria/secondaria;
  Simulazione: modalità/truppe/ticket/respawn + avvia/ferma), P→PreMatch classico,
  volo osservatore (player team 0, outcome sospeso). Doc 17.
- **Map Metadata + consumo AI**: `MapDef.coverPoints/patrolRoutes/dangerZones`
  (authoring nel MapEditor, sezione "Metadata AI"); `World::activeMap` (mailbox
  opaca) → AiSystem usa cover in hide, repulsione danger fuori ingaggio; ConquestMode
  assegna segmenti route come pattuglie. Doc 15+18.
- **Veicoli Fase A**: `VehicleDef` (data/vehicles), `MapDef.vehicleSpawns`,
  `VehicleComponent`; guida in Application (E sali/scendi con lato libero, W/S/A/D,
  slide/step-up condiviso, camera FPS/TPS, colore blu al mount). Modulo editor
  dedicato "Vehicle Editor". Doc 19.
- **HUD top**: pannelli fazione ai lati (ALLEATI/NEMICI: vivi+ticket), post al
  centro, hitmarker, crosshair-on-target, toast.
- **Diagnostica**: heartbeat `ai:` (stati AI ogni ~10s), `[Conquest] spawn:`,
  `drive:`/`veicolo:` per la guida, `[Viewport] Realloc FBO` nell'editor.

## Sistemi aggiunti 2026-07-11 → 07-14 (riferimento rapido; dettagli nei doc 20-22)
- **Ottimizzazione loop/AI (ADR-015 → doc 20):** frame pacing a doppia precisione
  (`SDL_GetPerformanceCounter`) + frame-cap di sicurezza; profiler **Tracy** opt-in
  (`USE_TRACY_PROFILER`, `ZoneScoped`/`FrameMark`, solo GFEngine). AiSystem: ricerca target in
  array **SoA** contigui; **time-slicing** della sensing (`AI_SENSE_INTERVAL`, bersaglio cachato
  in `AiComponent`); **cap LOS ai K vicini** (`AI_MAX_LOS_CHECKS`). Stress test: `--stress N`
  (cap `MAX_AI_PER_TEAM`), spawn a griglia.
- **Telemetria JSONL (ADR-016 → doc 21):** vedi voce Telemetry sopra. Hook: GameMode (mode
  created, ticket bleed), CommandPost (cattura), AI (state change, stuck WARN con coordinate).
  Dump stato per-entità su F12/fine-partita/crash.
- **Navigazione Recast/Detour (ADR-017 → doc 22):** `NavManager` (`src/game/nav/`) costruisce un
  `dtNavMesh` dai box di `MapDef.geometry` al load; DetourCrowd muove le AI (traversata =
  pathfinding che aggira gli ostacoli, combattimento = velocità tattica + avoidance);
  `CrowdSystem` (dopo AiSystem) registra/reap/ticka/write-back; `World::nav` = puntatore opaco;
  `AiComponent::crowdAgentIdx`. Aree semantiche DANGER/COVER dai metadata con costi
  `dtQueryFilter`. Fallback su `aiMove` se il navmesh manca. Solo GFEngine (ADR-002).

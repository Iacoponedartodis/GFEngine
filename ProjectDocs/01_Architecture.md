# 01 — Architecture

## Two-binary model
- **GFEngine** (`src/`, `include/mini/`): the runtime game.
- **GFEditor** (`editor/`): the ImGui-based authoring tool.
- Both are targets in the root `CMakeLists.txt`. **Dependency rule:** editor may depend on
  engine headers (schemas, Model/Mesh, DefinitionRegistry) and file I/O conventions; the
  **runtime must never depend on editor code**. No circular deps between `src/` and `editor/`.

## Communication contract (file-based only)
- Editor writes `data/<category>/*.json`; runtime reads them via `DefinitionRegistry`.
- Editor launches the runtime with `ShellExecuteA` passing `--direct-prematch` (skip to
  pre-match) or `--sandbox` (training arena). See `editor/src/EditorApp.cpp`.

## Layers (runtime)
1. **Core** (`src/core/`): `Application` (main loop + state machine — ~1250 righe, il cuore),
   `Window`, `Renderer`, `InputManager`, `Audio`, `GameConfig` (costanti globali), `GameState`,
   `Telemetry` (ADR-013/016). Nota: `Clock` esiste ancora ma il main loop ora usa
   `SDL_GetPerformanceCounter` (doppia precisione, ADR-015) — Clock è di fatto inutilizzato.
2. **ECS** (`src/ecs/`): `World` (entità = interi, componenti in `unordered_map` per tipo) +
   componenti (`include/mini/ecs/components/`) + sistemi eseguiti in ordine da `World::tick`:
   `MovementSystem` → `CombatSystem` → `AiSystem` → `CrowdSystem` (ADR-017). I sistemi
   implementano `ISystem::update(World&, dt)`.
3. **Game** (`src/game/`): `DefinitionRegistry` (data access layer), `data/Definitions.hpp`
   (schemi), game mode dietro `IGameMode` + `createGameMode()` factory (ADR-008; concreti:
   `ConquestMode`, `AssaultMode`, `DefenseMode` [ADR-014], `SandboxMode`), `PlayerController`,
   `Weapon`, `VehicleDrive` (guida veicoli, 19_Vehicles), `WeaponAttach` (arma in mano),
   `MapQuery` (query geometriche: `groundHeightAt`/`findFreeSpot`/`groundedSpawn`),
   `VehicleSpawn`, `CommandPosts` (ADR-009), e **`nav/NavManager`** (Recast/Detour, ADR-017).
4. **Physics** (`src/physics/`): `Collision` (AABB + SAT su collider ruotati, slide+step-up,
   LOS), `HitTest` (OBB condiviso mirino/proiettili). Usato da player/proiettili/veicoli;
   l'AI dal Phase B usa il navmesh (fallback su `Collision` se il navmesh manca).
5. **Render** (`src/render/`): `Mesh` (client-side arrays, 11 float/vertice), `Model`
   (OBJ via tinyobj, GLB/glTF via tinygltf), `Shader`, `Camera`, `Texture`, `HUD`, `Ui2D`,
   e le schermate/menu (`Launcher/MainMenu/PreMatch/Options/Sandbox` menu).
6. **Platform** (`src/platform/`): `OpenGL` (loader funzioni GL 3.3 Compatibility).
7. **Vendor** (`src/vendor/`): single-TU impls. `TINYGLTF_IMPLEMENTATION` vive **solo** in
   `tinygltf_impl.cpp`; stb/tinyobj impls isolati allo stesso modo.

## Layers (editor)
- `editor/src/EditorApp.cpp`: host SDL2+GL+ImGui docking, switcher moduli, launcher del gioco.
- `editor/src/modules/`: `EntityEditor` (nemici/alleati, incl. hitbox — ADR-012),
  `WeaponEditor`, `MapEditor`, `VehicleEditor`, `BalanceEditor` (ora redirect/read-only).
  **NB:** l'`HitboxEditor` è stato RIMOSSO (ADR-012): l'authoring hitbox vive nell'EntityEditor.
- `editor/src/viewport/FreeCameraViewport.cpp`: viewport 3D FBO condiviso (griglia, modello,
  attach, hitbox, bone, marker, gizmo 3 modalità, click-picking).
- `editor/src/util/`: `FileDialog`, `RigReader` (estrazione scheletro GLB), `DefinitionRename`
  (rename + sweep cross-ref, ADR-010).

## Central dependency node
`DefinitionRegistry` is the hub. Consumers: `Application`/`ConquestMode`/`SandboxMode`
(runtime) and every editor module (via its own registry instance or direct JSON reads).
Schema changes in `Definitions.hpp` ripple into: registry loaders, editor UIs, and any
game-mode code reading those fields.

## Main loop e timestep (ADR-015)
`Application::run()` contiene TUTTO il loop (non esiste `Application::tick()` separato). Per
frame: `input.update()` → eventi SDL → transizioni di stato → **fixed update** con accumulatore
a doppia precisione (`dt` da `SDL_GetPerformanceCounter`; `mode->update(world,fixedDt)` +
`world.tick(fixedDt)` a passi di 1/60 s) → camera/fisica del player (a `dt` variabile — nota:
timestep MISTO, world a fixedDt vs player a dt) → game logic → render (`beginFrame`→disegno→
`endFrame`=swap) → `FrameMark` Tracy + frame-cap di sicurezza (solo se VSync off) +
`flushEvents` telemetria. VSync ON di default.

## Sistemi trasversali (dettagli nei doc dedicati)
- **Telemetria (`Telemetry`, ADR-013+016 → doc 21):** log spdlog (`engine_run.log`), crash net
  (cpptrace), input recorder, dump stato (F12/fine-partita/crash), e sink JSONL LLM-observable
  (`session_latest.jsonl`) con `event(Level,system,msg,json)`. Ogni sistema DEVE loggare i suoi
  stati chiave. Solo GFEngine per la parte pesante.
- **Navigazione (`NavManager`+`CrowdSystem`, ADR-017 → doc 22):** navmesh Recast/Detour da
  `MapDef.geometry` al load; DetourCrowd muove le AI (pathfinding + avoidance); `World::nav` è
  il puntatore opaco che i sistemi ECS usano. Solo GFEngine.
- **Ottimizzazione AI/loop (ADR-015 → doc 20):** frame pacing, ricerca target SoA, time-slicing
  della sensing, cap LOS ai K vicini, profiler Tracy opt-in.

## Rendering constraint (do not "fix")
OpenGL 3.3 **Compatibility** Profile con **client-side vertex arrays** (no VAO/VBO):
workaround intenzionale per il driver Intel (ADR-003). `mini::Mesh::draw()` imposta
`glVertexAttribPointer` a puntatori CPU grezzi a ogni draw. Il viewport editor renderizza su
un FBO mostrato via `ImGui::Image`.

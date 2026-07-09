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
1. **Core** (`src/core/`): `Application` (main loop + state machine), `Window`, `Renderer`,
   `InputManager`, `Clock`, `Audio`, `GameConfig` (constants), `GameState`.
2. **ECS** (`src/ecs/`): `World` + components (`include/mini/ecs/components/`) + systems
   (`MovementSystem`, `CombatSystem`, `AiSystem`).
3. **Game** (`src/game/`): `DefinitionRegistry` (data access layer), `data/Definitions.hpp`
   (schemas), game modes dietro l'interfaccia `IGameMode` + `createGameMode()` factory
   (ADR-008; concrete: `ConquestMode`, `SandboxMode`), `PlayerController`, `Weapon`.
4. **Render** (`src/render/`): `Mesh` (client-side arrays, 11 floats/vertex), `Model`
   (OBJ via tinyobj, GLB/glTF via tinygltf), `Shader`, `Camera`, `Texture`, HUD/menus.
5. **Vendor** (`src/vendor/`): single-TU impls. `TINYGLTF_IMPLEMENTATION` lives **only** in
   `tinygltf_impl.cpp`; stb/tinyobj impls likewise isolated.

## Layers (editor)
- `editor/src/EditorApp.cpp`: SDL2+GL+ImGui docking host, module switcher, game launcher.
- `editor/src/modules/`: `EntityEditor`, `WeaponEditor`, `HitboxEditor`, `MapEditor`,
  `BalanceEditor` (now redirect/read-only).
- `editor/src/viewport/FreeCameraViewport.cpp`: shared FBO-based 3D viewport (grid, model,
  attachment model, hitboxes, bones, markers, translation gizmo, click-picking).
- `editor/src/util/`: `FileDialog`, `RigReader` (GLB skeleton/joint extraction).

## Central dependency node
`DefinitionRegistry` is the hub. Consumers: `Application`/`ConquestMode`/`SandboxMode`
(runtime) and every editor module (via its own registry instance or direct JSON reads).
Schema changes in `Definitions.hpp` ripple into: registry loaders, editor UIs, and any
game-mode code reading those fields.

## Rendering constraint (do not "fix")
OpenGL 3.3 **Compatibility** Profile with **client-side vertex arrays** (no VAO/VBO):
deliberate Intel-driver workaround. `mini::Mesh::draw()` sets `glVertexAttribPointer` to
raw CPU pointers each draw. Editor viewport renders to an FBO shown via `ImGui::Image`.

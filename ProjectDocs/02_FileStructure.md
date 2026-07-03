# 02 — File Structure

Repository root (verified):
```
CMakeLists.txt        root build (both GFEngine + GFEditor targets)
CMakePresets.json     windows-debug preset -> build/windows-debug
README.md
cmake/                cmake helpers
data/                 JSON definitions (the data-driven core)
  abilities/  ai/  allies/  characters/  enemies/  hitboxes/  maps/  weapons/
assets/               binary assets
  models/{enemies,allies,weapons}/*.glb   textures/  audio/  fonts/  shaders/
docs/                 pre-existing loose docs (NOT the ProjectDocs memory set)
editor/               GFEditor tool (separate target)
  include/{modules,ui,util,viewport}/   src/{modules,ui,util,viewport}/
external/             vendored deps (SDL2, imgui, glm, tinygltf, tinyobj, nlohmann fetched)
include/mini/         public engine headers mirroring src/
  core/ ecs/{components,systems}/ game/{data,game_modes}/ physics/ platform/ render/
src/                  engine/game implementation (mirrors include/mini/)
  core/ ecs/{systems}/ game/{data,game_modes}/ physics/ platform/ render/ vendor/
tests/                test scaffolding
ProjectDocs/          <-- this operational memory set
build/                generated (out of scope for architecture; should be gitignored)
imgui.ini             runtime-generated ImGui layout (should be gitignored)
presets.cfg           runtime-generated (should be gitignored)
```

## Data-path resolution (important)
Both runtime and editor resolve the **source** `data/` by canonicalizing
`<exe>/../../../data` (i.e. `build/windows-debug/Debug/` -> project root). Fallback:
`<exe>/data`. CMake POST_BUILD also copies `data/` and `assets/` next to each exe.
Net effect: edits to source `data/*.json` are picked up without a rebuild; assets are
resolved from source too (`assets/...` paths canonicalized 3 levels up).

## Build output
`build/windows-debug/Debug/{GFEngine.exe, GFEditor.exe}` with copied `data/` and `assets/`.

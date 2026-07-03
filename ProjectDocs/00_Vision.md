# 00 — Vision

## Intent
GFEngine is a small, specialised C++ game engine + companion editor (GFEditor) for a
Star Wars-style squad shooter (clones vs battle droids). It is a solo-developer
production codebase maintained across many sessions, not a demo.

## Core philosophy
- **Data-driven first.** As little as possible hardcoded in source; as much as possible
  defined in `data/*.json` and authored through the editor. New weapons/enemies/maps
  should require *data* changes, not *code* changes, wherever feasible.
- **Editor as a first-class tool.** GFEditor is the primary day-to-day tool. The runtime
  (GFEngine) is launchable directly into a match via `--direct-prematch` or into the
  training arena via `--sandbox` for fast iteration.
- **Two-binary separation.** GFEngine (runtime) and GFEditor (tool) are separate binaries.
  They communicate only through files (`data/*.json`, assets by path). The runtime never
  depends on editor code.

## Non-goals
- Not a general-purpose engine. No speculative plugin systems, no premature ECS/render
  abstractions.
- Not targeting portability beyond the current Windows + SDL2 + OpenGL 3.3 Compatibility
  Profile target (client-side arrays, Intel-driver workaround).

## Long-term direction
- Eliminate remaining hardcoded archetype ids and tuning constants from game modes.
- Grow the editor toward a complete tool suite: Entity, Weapon, Hitbox, Map, AI, Asset
  Manager, and a future UI/Interface Editor (menu text, layout, palette, fonts).
- Every "assign definition A to definition B" workflow uses dropdowns sourced from
  DefinitionRegistry, never free-text id entry.
- Unify hitbox authoring (see [08_KnownIssues](08_KnownIssues.md): inline vs profile split).

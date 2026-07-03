# 12 — Testing Strategy

No automated engine tests currently gate changes; verification is build + manual smoke tests.

## Build verification (always)
- `cmake --build build/windows-debug --config Debug` must be clean (0 errors). Warnings
  (fopen/strncpy/unref-param) are pre-existing and acceptable.

## Manual smoke tests (per meaningful change)
1. **Editor lists:** launch GFEditor -> open EntityEditor/WeaponEditor/HitboxEditor/MapEditor;
   verify definition lists load without duplicates or missing entries.
2. **Registry cross-refs:** an enemy's weapon/ai_profile/hitbox_profile dropdowns resolve to
   existing ids; renamed files don't orphan references.
3. **Runtime match:** GFEngine `--direct-prematch` -> loadout shows the same weapon set as the
   editor; enemies spawn (not floating), map geometry matches MapEditor.
4. **Sandbox:** GFEngine `--sandbox` -> firebase geometry loads, dummies spawn at team2 spawn,
   take damage, die, and respawn; player spawns at team1 spawn.
5. **Viewport:** models render un-corrupted; bones align to mesh; attach points visible as
   boxes+labels through the model; gizmo arrows move the selected object.

## What typically needs manual verification (cannot be traced statically)
- Actual OpenGL rendering / FBO output, gizmo drag feel, AI behavior at runtime, model scale
  in-scene. Flag these explicitly as unverified when not manually run.

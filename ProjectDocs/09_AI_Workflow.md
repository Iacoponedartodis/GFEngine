# 09 — AI Workflow (operating sequence in this repo)

For every non-trivial task:
1. **Read memory first:** 05_CurrentState, 06_Todo, 08_KnownIssues, 10_ProjectMemory, 13_ADR.
   For architectural tasks also read 00/01/03/11/12.
2. **Verify against live code.** Docs are a bridge; if code disagrees, code wins — fix the docs.
   (Le due vecchie "standing risks" — split hitbox inline/profile e fallback id ConquestMode —
   sono RISOLTE: ADR-006/012 e ADR-007. Nuove aree sensibili: movimento AI ora via crowd, non
   `aiMove`; Y transform = centro; telemetria eventi discreti.)
3. **Name the impacted subsystems** explicitly from {ECS (World + sistemi Movement/Combat/Ai/
   Crowd), game mode, render, DefinitionRegistry, physics, nav (NavManager/CrowdSystem),
   telemetria, EntityEditor, WeaponEditor, MapEditor, VehicleEditor, FreeCameraViewport, build,
   data JSON}.
4. **Trace global impact** for any id/schema/naming change: DefinitionRegistry loader + all
   editor UIs + all game-mode readers + cross-references (`EnemyDef.weaponIds/aiProfileId/
   hitboxProfileId`, `MapDef.enemyTypes/allyTypes`).
5. **Smallest safe change.** If migrating away from a hardcoded pattern, keep a documented
   fallback during transition.
6. **Implement in place**, then **build** (`cmake --build build/windows-debug --config Debug`)
   with GFEditor/GFEngine processes killed first (exe lock).
7. **Update ProjectDocs** (05/06/07/08 always as applicable; 13 ADR for structural decisions;
   10 for newly confirmed constraints).
8. **State verified vs unverified** (build-verified vs needs manual smoke test).

## Build/run notes
- Kill `GFEditor`/`GFEngine` before building (Windows file lock on the exe).
- Data is read from source `data/` (3 levels up from exe); no rebuild needed for pure data
  edits, but a build refreshes the copied output and is cheap.

# 11 — Development Workflow

## Build
- Preset: `windows-debug` -> `build/windows-debug`.
- Command: `cmake --build build/windows-debug --config Debug`.
- **Kill `GFEditor.exe` / `GFEngine.exe` first** (exe file lock on Windows, else LNK1168).
- Adding source files -> update the relevant CMake target (`CMakeLists.txt`); the editor and
  engine sources are listed there.

## File delivery
- Edits are applied **in place** on disk and verified by building. The on-disk file is the
  complete, authoritative artifact. Do not leave partial/unapplied diffs.
- If the developer explicitly asks for copy-paste, provide the **complete** file, not a patch.

## Migration discipline
- When changing an id/naming convention or a schema field, do a full sweep in one change set:
  DefinitionRegistry loader + every editor UI that reads/writes it + every game-mode reader +
  cross-references in other JSON. Never leave dangling references after a rename.
- Keep a documented fallback during any partial migration (comment in code + KnownIssues entry).

## Save/reload discipline (editors)
- After any editor save that other tools depend on, reload the affected registry so lists stay
  consistent. HitboxEditor reloads its registry after saving a profile.

## Data edits
- Pure `data/*.json` edits are picked up from source without rebuild; still build to refresh
  the copied output before shipping/running the packaged copy.

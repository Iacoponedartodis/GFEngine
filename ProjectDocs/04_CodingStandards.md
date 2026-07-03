# 04 — Coding Standards

## Observed conventions (keep)
- **RAII for GL resources** (FBO/textures owned by viewport, freed in dtor).
- **JSON via nlohmann::json** with the `gets/geti/getf/getb/getColor/getStrArray` helper
  pattern in DefinitionRegistry; editors use `j.value(key, default)` / `j.contains`.
- **id == filename stem** for every definition type. Loaders read `id` with the stem as
  default; treat the in-file `id`/`profile_id` as deprecated.
- **std::filesystem** for data/asset dir resolution (canonical `<exe>/../../../data|assets`).
- Namespaces: engine code in `mini::`, editor code in `editor::`.
- Italian comments/UI strings are the norm; keep consistent with surrounding code.

## Rules to uphold going forward
- Do **not** write `id`/`profile_id` back into JSON as authoritative (filename is canonical).
  (Current editors mostly avoid this; verify on any new save path.)
- No new hardcoded gameplay constants in game modes — put tunables in `data/` or
  `core/GameConfig.hpp` if truly global.
- No new hardcoded archetype/id strings in game modes; resolve via registry + MapDef.
- Definition assignment in editors uses **dropdowns from the registry**, never free-text ids.
- Respect the rendering constraint: client-side arrays only; do not introduce VAO/VBO.
- Keep `TINYGLTF_IMPLEMENTATION` in exactly one TU (`src/vendor/tinygltf_impl.cpp`).

## Change delivery
This repo is edited **in place** with direct file writes + a verifying build. Files on disk
are the source of truth and are always left complete and compiling. When the developer
explicitly needs copy-paste, provide the full file; otherwise in-place edits are preferred
(no partial diffs left unapplied). See [11_DevelopmentWorkflow](11_DevelopmentWorkflow.md).

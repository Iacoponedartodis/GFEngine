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

- **READ-MODIFY-WRITE obbligatorio per OGNI salvataggio JSON negli editor.** I file dati sono
  co-autorati da più moduli (es. mappa: BalanceEditor scrive stats, MapEditor geometry/posts;
  entità: BalanceEditor stats, EntityEditor attach/weapon_display). Costruire un `json j;`
  nuovo e scriverlo distrugge i campi degli altri moduli — è ESATTAMENTE il bug che il
  2026-07-08 ha cancellato geometry+command_posts da firebase.json (partita ingiocabile).
  Pattern corretto: leggi il file esistente in `j`, sovrascrivi solo i tuoi campi, salva.

- **Do not write `id`/`profile_id` back into JSON as authoritative** (filename is canonical).
  (Current editors mostly avoid this; verify on any new save path.)

- **No new hardcoded gameplay constants in game modes** — put tunables in `data/` or
  `core/GameConfig.hpp` if truly global.

- **No new hardcoded archetype/id strings in game modes**; resolve via registry + MapDef.

- **Definition assignment in editors uses dropdowns from the registry, ALWAYS — no exceptions.**
  This rule was previously stated only in 00_Vision as an intent ("Ogni workflow 'assegna
  definizione A a definizione B' usa dropdown dal DefinitionRegistry, mai id in testo libero").
  It is now a **binding coding standard**, not just a design preference:
  - Any UI field that assigns a weapon id, ai_profile id, hitbox_profile id, enemy/ally id,
    map id, or ability id to another definition MUST be a combo/dropdown populated from
    `DefinitionRegistry` (`registry.weapons()`, `registry.enemies()`, `registry.aiProfiles()`,
    `registry.hitboxProfiles()`, etc.), keyed by id, displaying `name` as label.
  - `ImGui::InputText` for an id field is a code review failure, not a style choice.
  - Rationale: free-text id fields are the root cause of two confirmed problems in production
    data — dead fallback ids (ADR-007) and near-duplicate weapon/enemy JSON files created by
    manual "rename via new file" workflows (see 08_KnownIssues #7, 06_Todo #16).
  - Reference implementation pattern: EntityEditor's weapon/ai_profile/hitbox_profile combos
    (see 03_SystemReference) — replicate this exact pattern for any new assignment field.

- **Renaming a definition MUST go through the in-editor rename command, never through
  manual file creation/deletion.** (See 13_ADR, ADR-010 — Proposed.) Manually duplicating a
  JSON file with a new filename to "rename" a weapon/enemy/etc. is explicitly disallowed
  once the rename command exists, because it leaves the old file as a silent duplicate and
  does not update cross-references. Until ADR-010 is implemented, manual renames must be
  followed by a full manual sweep: delete the old file, and grep every JSON in `data/` for
  the old id string (`weaponIds`, `aiProfileId`, `hitboxProfileId`, `enemyTypes`, `allyTypes`).

- **Any new JSON save path must use a shared RMW helper once introduced** (see 13_ADR,
  ADR-010 candidate scope). Until the helper exists, every new `save*()` function must
  manually implement read-modify-write and this must be called out explicitly in its own
  code review / changelog entry.

- Respect the rendering constraint: client-side arrays only; do not introduce VAO/VBO.

- Keep `TINYGLTF_IMPLEMENTATION` in exactly one TU (`src/vendor/tinygltf_impl.cpp`).

## Change delivery
This repo is edited **in place** with direct file writes + a verifying build. Files on disk
are the source of truth.
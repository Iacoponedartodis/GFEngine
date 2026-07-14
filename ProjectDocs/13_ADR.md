# 13 — Architecture Decision Records

One entry per structural decision. Newest last.

## ADR-001 — Canonical id = filename stem
- **Decision:** For every definition type the id is the JSON filename stem; the in-file
  `id`/`profile_id` field is non-authoritative.
- **Consequence:** Renaming = renaming the file; cross-references (`weaponIds`, `aiProfileId`,
  `hitboxProfileId`, `MapDef.enemyTypes/allyTypes`) are by that stem. Registry maps are keyed
  by stem. Status: **in force.**

## ADR-002 — Two-binary, file-only contract
- **Decision:** GFEngine (runtime) and GFEditor (tool) are separate binaries communicating only
  via `data/*.json` + assets by path. Editor launches runtime with `--direct-prematch` /
  `--sandbox`. Runtime never links editor code.
- **Consequence:** No circular src/editor deps; editor may read engine schemas. Status: **in force.**

## ADR-003 — Client-side vertex arrays (no VAO/VBO)
- **Decision:** Keep OpenGL 3.3 Compatibility Profile with client-side arrays as an Intel-driver
  workaround. Status: **in force** (do not migrate without a concrete driver justification).

## ADR-004 — Map is data-driven via MapDef.geometry (2026-07-03)
- **Decision:** Map collision/visual geometry lives in `MapDef.geometry` (authored in MapEditor),
  read by ConquestMode and SandboxMode. Hardcoded box layout retained only as an empty-geometry
  fallback.
- **Consequence:** New/edited maps are data changes. Status: **in force.**

## ADR-005 — SandboxMode shares the firebase map (2026-07-03)
- **Decision:** `--sandbox` loads the firebase `MapDef` (geometry + spawn points) and spawns
  respawning dummies, so map/spawn authoring is testable from the arena. Status: **in force.**

## ADR-006 — Hitbox single source of truth = PROFILE (2026-07-04)
- **Decision:** The hitbox PROFILE (`data/hitboxes/<id>.json`) is the only authoritative store,
  because it is what the runtime consumes. EntityEditor's Hitbox tab now reads/writes the
  profile referenced by `hitbox_profile` (auto-created with id = entity id if the reference is
  empty). Entity-inline `hitbox_zones` are **deprecated**: ignored when a profile exists, read
  only as a legacy fallback, erased from the entity JSON on the next save.
- **Consequence:** Zones authored in EntityEditor now reach the game. HitboxEditor and
  EntityEditor edit the same file — last save wins; both write the runtime schema
  (`damage_multiplier`, `bone`, `rotation`). Rationale for (a) over (b): zero runtime changes,
  one store instead of two. Status: **in force.**

## ADR-007 — Game-mode fallback ids come from the registry (2026-07-04)
- **Decision:** `ConquestMode::buildEnemySpawnList` no longer hardcodes archetype ids. When
  `MapDef.enemyTypes` is empty it falls back to the sorted list of ids actually registered in
  `data/enemies/`; if that is also empty, it spawns nothing and logs an error (no blind ids).
- **Consequence:** Renaming enemy files can no longer silently break spawning via dead
  hardcoded strings (KnownIssues #2 closed). Maps should still declare `enemyTypes` for
  intentional composition. Status: **in force.**

## ADR-008 — IGameMode interface + factory (2026-07-04)
- **Decision:** Tutte le modalità implementano `IGameMode`
  (`include/mini/game/game_modes/IGameMode.hpp`): applySettings/start/update, accessor
  player/spawn/tickets, `hasVictoryCondition()`. Le istanze si creano SOLO via
  `createGameMode(id)` (`src/game/game_modes/GameModeFactory.cpp`, id: "conquest",
  "sandbox"; sconosciuto → fallback conquest + log). Application detiene un
  `unique_ptr<IGameMode>` e non conosce le classi concrete (rimosse le lambda `useSandbox`).
- **Consequence:** Assalto/Difesa (Fase 1) = nuova classe + una riga nella factory; la
  logica win/lose usa `hasVictoryCondition()` invece di flag di modalità. `MeshCache` è
  definito una sola volta in IGameMode.hpp. Prossima evoluzione naturale: id modalità
  scelto da MapDef/PreMatch invece che dal flag CLI. Status: **in force.**

## ADR-009 — Command post come sistema riusabile, dati nel MapDef (2026-07-04)
- **Decision:** I punti di comando sono dati di mappa (`MapDef.commandPosts`, JSON
  `command_posts`: label/x/y/z/radius/team/capture_time), autorati nel Map Editor come i
  box/spawn. La logica (presenza esclusiva nel raggio → cattura in captureTime secondi,
  conteso/vuoto → decay; visual palo+piastra colorati per team) vive nella classe riusabile
  `CommandPosts` (`include/mini/game/CommandPosts.hpp`), NON nei game mode.
- **Consequence:** Ogni modalità configura e interroga il sistema per le proprie regole:
  Conquista applica ticket bleed (maggioranza post → -1 ticket avversario ogni 6s);
  la Sandbox li rende catturabili senza conseguenze (test dal vivo). Le future
  Assalto/Difesa riusano lo stesso blocco con regole diverse. KnownIssues #9 chiuso.
  Status: **in force.**

## ADR-010 — In-editor rename command + centralized RMW save helper (Proposed, 2026-07-09)

### Context
`id = filename stem` (ADR-001) is a sound convention, but the tooling to change an id safely
has never been built. Confirmed production incident (2026-07-09): renaming a weapon's
name/id by hand (creating a new JSON file instead of renaming the existing one) produced
duplicate near-identical entries in the loadout menu (08_KnownIssues #7, escalated to P0).
Separately, an unrelated but structurally similar incident occurred on 2026-07-08: an editor
module rewrote a JSON file from scratch instead of read-modify-write, destroying unrelated
fields (08_KnownIssues #15). Both incidents share a root cause class: **destructive or
incomplete file operations performed without a single, audited code path.**

### Options Considered
1. **Do nothing, rely on documented discipline.** (Current state.) Both incidents already
   happened despite discipline being documented in 04_CodingStandards before ADR-010 was
   drafted. Rejected — discipline alone has already failed twice.
2. **Decouple id from filename** (allow arbitrary in-file `id` distinct from filename).
   Rejected — reverses ADR-001, reintroduces the original problem ADR-001 solved (id/filename
   drift), and is a larger, riskier change than adding tooling on top of the current
   convention.
3. **Add an in-editor "Rinomina" command per definition type, that renames the physical file
   and sweeps cross-references, PLUS a centralized `saveJsonRMW` helper used by all save
   paths.** Chosen.

### Decision
Introduce two coupled but separable pieces of tooling, both living in editor-side shared
utility code (not the runtime):

1. **Rename command** (per module: EntityEditor, WeaponEditor, HitboxEditor, MapEditor, and
   any future AI Profile editor):
   - UI: text field for new id (filename-safe) + "Rinomina" button, shown next to the
     existing id/name display for the selected item.
   - Behavior on click:
     a. Validate new id (non-empty, filesystem-safe, not already in use in the same
        category).
     b. `std::filesystem::rename(oldPath, newPath)`.
     c. Update the in-memory definition's id to the new value.
     d. Sweep `data/` for every JSON file that references the old id in a known
        cross-reference field for that category (`weaponIds[]`/`aiProfileId`/
        `hitboxProfileId`/`enemyTypes[]`/`allyTypes[]`/map-level references) and rewrite
        those fields via the RMW helper (below).
     e. Reload the affected `DefinitionRegistry` instance(s).
   - Cross-reference field map per category must be maintained explicitly in the
     implementation (do not attempt a generic "find any string" sweep — false positives on
     coincidental string matches are worse than missed references; an explicit field map is
     auditable and matches the Migration Discipline already documented in
     11_DevelopmentWorkflow).

2. **Centralized `saveJsonRMW(path, patchFn)` helper** (e.g. in
   `editor/include/util/JsonSave.hpp`):
   - Reads existing file into `json j` (or `json::object()` if the file doesn't exist yet).
   - Calls `patchFn(j)` which mutates only the fields the calling module owns.
   - Writes `j` back.
   - Every existing `save*()` function in every editor module (BalanceEditor, EntityEditor,
     WeaponEditor, HitboxEditor, MapEditor) is migrated to call this helper instead of
     hand-rolled read-modify-write, removing the possibility of a future module forgetting
     the pattern.
   - Optional (not required for acceptance): automatic `.bak` backup of the file immediately
     before write, one rotation deep, as a last-resort recovery path.

### Reasoning
- Both confirmed incidents are process/tooling gaps, not architecture flaws — ADR-001
  (id=filename) and the RMW convention are both still the right design. The fix is to make
  the correct behavior the *only* behavior a developer (human or AI) can reach through the
  UI/API, rather than the behavior they must remember to perform correctly by hand each time.
- An explicit cross-reference field map (rather than a generic string-replace sweep) keeps
  the rename operation auditable and prevents accidental corruption of unrelated string
  fields (e.g. a `name` field that happens to contain the old id as a substring).
- This is consistent with 09_AI_Workflow's existing "Migration discipline" step ("do a full
  sweep in one change set... never leave dangling references after a rename") — ADR-010
  turns that manual discipline into an automated, single code path.

### Consequences
- **Positive:** Closes 08_KnownIssues #7 (near-duplicate data files) and hardens
  08_KnownIssues #15 (destructive saves) structurally rather than by convention. Removes a
  recurring class of manual-process bugs before more content (weapons, enemies, maps) is
  authored, which is the highest-leverage time to fix it.
- **Negative / cost:** Requires touching every existing editor save path once (migration
  cost), and requires maintaining the explicit cross-reference field map as new definition
  types or new reference fields are added — this map must be updated in the same change set
  as any new cross-reference field (see 04_CodingStandards, Migration Discipline).
- **Follow-up work implied:** 06_Todo #1 (rename tooling) and #8 (save-safety helper) track
  the implementation of this ADR. Once implemented, this ADR's Status must be updated to
  **Accepted**, and 08_KnownIssues #7 and #15 updated to RESOLVED with a smoke-test reference
  (per 12_TestingStrategy).

### Status
**Accepted (2026-07-09).** Implemented in code:
- `editor/include/util/JsonSave.hpp` — `saveJsonRMW(path, patchFn)` con backup `.bak`
  (una rotazione, best-effort) prima di ogni scrittura. patchFn ritorna false = no-op.
- `editor/include/util/DefinitionRename.hpp` + `editor/src/util/DefinitionRename.cpp` —
  `renameDefinition(dataDir, Category, oldId, newId)`: validazione, `fs::rename`, pulizia
  degli `id`/`profile_id` deprecati nel file rinominato, sweep cross-reference con **mappa
  esplicita per categoria** (Weapon → enemies/allies `weapons[]`/`weapon`/`weapon_display.id`;
  HitboxProfile → `hitbox_profile`; AiProfile → `ai_profile`; Ability → `abilities[]`;
  Enemy → maps `enemy_types[]`; Ally → maps `ally_types[]`; Map/Character → nessuno, con
  warning se si rinomina "firebase" perché i game mode la caricano hardcoded — residuo
  ADR-008).
- **Tutti** i save path migrati all'helper: BalanceEditor (saveWeapon/saveEnemy/saveAI/
  saveMap/saveAlly/savePlayerDef), EntityEditor::saveSelected (entità + profilo hitbox),
  WeaponEditor::saveSelected, HitboxEditor::saveProfile, MapEditor::saveMap. Nessuna
  scrittura JSON fuori dall'helper. In più: i campi `id`/`profile_id` deprecati vengono
  rimossi dai file a ogni salvataggio (ADR-001).
- UI "Rinomina" in WeaponEditor, EntityEditor (reload deferito al frame successivo per lo
  stack ImGui), HitboxEditor, MapEditor.
- Verifica: build pulita; smoke runtime ok (registry/mappa integri). **Smoke GUI del rename
  (rinomina reale + verifica cross-ref) da eseguire manualmente** — il duplicato del
  2026-07-09 era già stato ripulito a mano prima dell'implementazione.

## ADR-012 — Hitbox authoring solo nell'Entity Editor (Accepted, 2026-07-09)

### Context
Dopo ADR-006 il profilo hitbox era editabile da DUE moduli (EntityEditor tab Hitbox e
HitboxEditor standalone) sullo stesso file, con "last save wins" (KnownIssues #1 residuo)
e duplicazione di UI. Richiesta utente: consolidare tutto nell'Entity Editor, dove il
contesto (modello, ossa, attach point) rende l'authoring più comodo.

### Decision
- L'Entity Editor è l'UNICO luogo di authoring delle hitbox (zone, moltiplicatori danno,
  rotazioni, bone binding, debug_visible — gap colmato prima della rimozione).
- Il modulo HitboxEditor è RIMOSSO (file, CMake, EditorApp, HomeScreen). Il profilo
  (`data/hitboxes/<id>.json`) resta il formato runtime (ADR-006 invariato); segue
  la convenzione profileId = id entità.
- Rimosso l'ultimo fallback hardcoded `"grunt"` in `ConquestMode::spawnUnit` (l'id profilo
  è sempre risolto a monte; se assente → fallback sferico del CombatSystem).
- Dati: eliminati i profili orfani `grunt/heavy/sniper` da data/hitboxes (nessun riferimento).
- BalanceEditor ripulito dai tab vestigiali Nemici/Alleati (redirect-only) e dai relativi
  saveEnemy/saveAlly.

### Consequences
- Un solo flusso di authoring: niente più "last save wins" tra moduli (KnownIssues #1
  residuo chiuso). Perso l'editing di profili NON legati a un'entità: caso d'uso
  inesistente oggi (ogni profilo appartiene a un'entità); se servirà, si aggiunge una
  vista dedicata nell'Entity Editor, non un modulo separato.
- Limitazione nota: rinominare un profilo hitbox standalone non ha più UI (il rename
  entità non rinomina il profilo referenziato — accettato, tracciato in KnownIssues #16).

## ADR-013 — Sistema di telemetria e debugging estremo (Accepted, 2026-07-09)

### Context
I bug runtime (crash, memoria, modelli non renderizzati) non erano diagnosticabili senza
osservazione manuale: serviva un sistema che renda il motore trasparente via file di log
passabili tra sessioni di debug.

### Decision
Modulo `mini::telemetry` (`src/core/Telemetry.{hpp,cpp}`, header leggero senza dipendenze
esposte), linkato a ENTRAMBI i binari. Tutti gli artefatti in `_telemetry_data/` (root
progetto, risolta come `data/`; creata automaticamente all'avvio; in .gitignore):
1. **Logging** — spdlog v1.14.1 (FetchContent): `engine_run.log` con livelli
   TRACE/INFO/WARN/ERROR; file sink a TRACE, console a WARN+; flush su warn + ogni 3s.
   Strumentati: Application (avvio/flag/registry/mode), Window, Renderer, battito memoria
   ogni 600 frame.
2. **Dump stato** — tasto **F12** in partita → `game_state.json` (frame, timestamp,
   memoria MB, camera pos/forward, stato gioco, entità, hp/arma/heat player, ticket).
3. **Input recorder** — `InputManager::processEvent` → `input_history.log`
   (KEY_DOWN/UP, MOUSE_DOWN/UP con coordinate, ognuno col numero frame).
4. **Crash net** — cpptrace v0.7.3: `SetUnhandledExceptionFilter` (SEH, incl. access
   violation) + `std::set_terminate` → stack trace su terminale E `crash_report.txt`
   (con frame e motivo).
- **Sanitizers**: opzione CMake `GF_ENABLE_ASAN` (default OFF — troppo lenti per il
  playtest quotidiano): MSVC → `/fsanitize=address` (+ `/INCREMENTAL:NO`);
  **UBSan non esiste su MSVC** → aggiunto solo per toolchain non-MSVC.

### Consequences
- Diagnosi post-mortem senza riprodurre a mano: log + input history + crash report.
- Verificato: build pulita, `_telemetry_data/` creata, `engine_run.log` popolato.
  Da verificare con evento reale: crash report (serve un crash vero) e dump F12 (serve
  input in finestra). Il frame counter correla i tre file.
- Costo: due dipendenze FetchContent in più (tempo di primo configure).

## ADR-014 — Assalto/Difesa come configurazioni; esito partita deciso dal mode
## (Accepted, 2026-07-09)

### Context
La Fase 1 richiede Conquista/Assalto/Difesa come configurazioni dello stesso core
(00_Vision). ADR-008 (IGameMode) e ADR-009 (CommandPosts) erano i prerequisiti; mancavano
le modalità e la vittoria/sconfitta era hardcoded in Application (ticket team2 + nessun
nemico vivo), inestendibile a regole a obiettivi.

### Decision
1. **`MatchOutcome` in IGameMode**: `virtual outcome(const World&)` (default Ongoing) —
   Application chiede l'esito al mode invece di hardcodare la regola. Il lose per morte
   del giocatore senza ticket resta in Application (riguarda il giocatore, non gli
   obiettivi). ConquestMode implementa la regola storica.
2. **Hook `updateObjectiveRules(World&, float dt)`** (protected virtual in ConquestMode):
   Conquista = maggioranza post drena i ticket avversari (invariato). Le derivate
   sostituiscono SOLO questo + outcome + ownership iniziale dei post.
3. **AssaultMode / DefenseMode** (`game_modes/ObjectiveModes.{hpp,cpp}`), registrate in
   factory come "assault"/"defense":
   - Assalto: post iniziali ai nemici (`forceAllOwners(2)` — la mappa li autora neutrali,
     la modalità decide); i ticket alleati calano nel tempo (ogni post catturato allunga
     l'intervallo); vittoria = TUTTI i post; sconfitta = ticket alleati a 0.
   - Difesa: speculare (post agli alleati, ticket nemici calano; sconfitta se i nemici
     prendono tutto; vittoria a ticket nemici 0 + campo pulito).
4. **Selezione nel PreMatch**: `MatchSettings.modeIndex` + riga "Modalita' di gioco" nella
   pagina Regole (Row esteso con etichette enum); `startGame` crea il mode da
   `matchModeId(modeIndex)`. Chiude il residuo ADR-008 (mode non più solo da CLI).
5. **HUD stato post (Todo #6)**: `CommandPosts::status()` (label/owner/capturing/progress)
   → `IGameMode::commandPosts()` → barra in alto: quadrato colorato per proprietario con
   lettera + barra di cattura del team che sta catturando.

### Consequences
- Fase 1 "tre modalità come configurazioni" è reale: una modalità nuova = classe che
  overrida 2-3 hook + riga in factory. La cattura è finalmente VISIBILE in gioco.
- Nota bilanciamento: gli intervalli di bleed derivano da m_bleedInterval (6s); tarare
  con playtest. I preset partita NON salvano ancora modeIndex (persistenza preset da
  estendere — minor, tracciato).

## ADR-011 — Split-screen feasibility verification as a gating spike (Proposed, 2026-07-09)

### Context
Local split-screen for 2 players is stated in 00_Vision as a **non-negotiable functional
requirement**, not an aspiration. 08_KnownIssues #12 records that no evidence has been found
in the current camera/input/rendering pipeline that more than one active local
viewport/input source is supported simultaneously. Development continues to build systems
(HUD, weapon-in-hand rendering, command post UI) that implicitly assume exactly one local
player and one local camera. The longer this assumption goes unverified, the more code will
need retrofitting if split-screen turns out to require structural changes to
`Camera`/`InputManager`/`Renderer`.

### Options Considered
1. **Keep deferring verification indefinitely, address split-screen "when we get there."**
   Rejected — every new single-player-assuming system built in the meantime increases the
   retrofit cost if split-screen requires structural change; this is explicitly against the
   Vision's own stated risk ("da verificare prima di espandere ulteriormente sistemi che
   assumono un solo giocatore attivo").
2. **Design a full split-screen input/camera/render architecture now, speculatively.**
   Rejected — violates the "Things to Avoid" principle (no generic architecture without a
   concrete, tested need) and risks premature complexity if the actual constraint turns out
   to be narrower than assumed (e.g. only `Camera`/viewport rect needs duplication, not the
   full input stack).
3. **Run a small, time-boxed feasibility spike:** a minimal second local `Camera` + second
   viewport rect rendering the same scene simultaneously (no new gameplay), to empirically
   determine which layers (Window/viewport, Camera, InputManager, Renderer, HUD) already
   support duplication and which don't. Chosen.

### Decision
Before any further HUD, progression, or command-layer system is built on a single-local-
player assumption, run a feasibility spike:
- Add a second `Camera` instance and a second screen-space viewport rect (e.g. left/right
  half split) rendering the same live scene.
- Do not implement second-player input, gameplay, or UI scaling yet — the spike's only goal
  is to determine whether the render/camera layer can produce two simultaneous views without
  structural rework.
- Record the outcome as a finding in this ADR's Consequences section and in 05_CurrentState,
  explicitly as one of: (a) feasible with current architecture, minor changes only; (b)
  feasible but requires a documented refactor (list the affected systems); (c) not feasible
  without a significant `Renderer`/`Window` redesign.

### Reasoning
A time-boxed empirical spike is cheaper than either extreme (ignoring the risk, or designing
a speculative full solution) and directly follows the project's stated Decision Framework
principle: prefer value/complexity over technical complexity, and avoid building generic
architecture "without a concrete, tested need." The spike itself produces the concrete need
(or lack thereof).

### Consequences
- **Positive:** Converts an open, vague risk (08_KnownIssues #12) into a concrete, documented
  finding that future work can be planned against, with minimal upfront cost.
- **Negative / cost:** Small amount of throwaway/prototype code if the outcome is (a) or (c)
  and the spike code doesn't survive into the real implementation.
- **Gating effect:** Per 06_Todo #16, further systems that assume a single local player
  should not expand significantly until this spike's finding is recorded. This is a
  soft-gate (documented risk), not a hard build block — use judgment for low-risk additions.

### Finding (spike eseguito 2026-07-09)
**Esito: (a) — fattibile con l'architettura attuale, modifiche minori.** Lo spike (toggle
F9 in partita) renderizza la stessa scena live in due viewport affiancati con una seconda
`Camera` (copia della principale, offset laterale). Costo reale:
- `Renderer`: +2 metodi (`drawMeshFrom(const Camera&, ...)` — `drawMesh` ora vi delega —
  e `setViewportRect`; più `getDrawableSize`). Nessun cambiamento a shader, mesh o frame
  lifecycle.
- `Camera` è un value type copiabile: nessun refactor necessario.
- `Application`: loop entità estratto in una lambda `drawScene(const Camera&)`, chiamata
   1 o 2 volte. HUD/menu invariati (viewport ripristinato full prima del 2D).
Ciò che lo spike NON copre (lavoro vero della feature, quando arriverà): secondo input
locale (`InputManager` è single-source), secondo PlayerController/HUD scalato per metà
schermo, split della cattura mouse. Questi sono lavoro additivo, non redesign.

### Status
**Accepted (finding registrato).** Spike build-verified 2026-07-09; conferma visiva manuale
(F9 in partita) in carico allo sviluppatore. 08_KnownIssues #12 risolto come caso (a);
il codice spike resta nel binario come strumento debug innocuo (F9).

## ADR-015 — Tracy come profiler opt-in (Accepted, 2026-07-13)

### Context
Prima di qualsiasi ottimizzazione (fixed-timestep, layout data-oriented, AI time-slicing/
spatial hashing) serve una misura affidabile a precisione sub-millisecondo: Task Manager e
timer manuali non bastano per attribuire il costo a singole funzioni/frame. È la Fase 1 di
un piano di ottimizzazione a 4 fasi. Vincolo di questo repo: due binari con contratto
solo-file (ADR-002) e build riproducibili con tag pinnati (KI #27).

### Decision
1. **Tracy (`wolfpld/tracy`, tag pinnato `v0.11.1`) via FetchContent**, coerente col pattern
   delle altre 8 dipendenze. Nessun submodule.
2. **Opzione CMake `USE_TRACY_PROFILER` (default OFF)** che pilota l'opzione `TRACY_ENABLE`
   di TracyClient. OFF ⇒ TracyClient compila come stub e le macro `ZoneScoped`/`FrameMark`
   sono no-op: le build normali (Debug/Release) restano **identiche e a costo zero**. Gli
   header restano sempre includibili in entrambi gli stati (`<tracy/Tracy.hpp>` non va
   protetto da `#ifdef`). `TRACY_ON_DEMAND=ON`: cattura solo quando la GUI è connessa.
3. **Deviazione consapevole dal piano**: il piano chiedeva "default ON in RelWithDebInfo".
   Col generatore multi-config di Visual Studio `CMAKE_BUILD_TYPE` è vuoto e TracyClient è
   compilato una sola volta → un default per-configurazione non è affidabile. Si usa quindi
   un'opzione esplicita; il workflow di profiling è una build RelWithDebInfo dedicata con
   `-DUSE_TRACY_PROFILER=ON`.
4. **Linkato SOLO a GFEngine** (`TracyClient`), mai a GFEditor (ADR-002: l'editor non deve
   dipendere dal runtime). I TU strumentati (`Application.cpp`, `World.cpp`, `AiSystem.cpp`,
   `CombatSystem.cpp`) non sono compilati in GFEditor, quindi Tracy resta confinato al runtime.
5. **Punti di misura** (mappati sui nomi reali, non sui `Application::tick/render` del piano
   che non esistono): `FrameMark` a fine loop dopo lo swap; `ZoneScoped` in `World::tick`
   (equiv. tick), `AiSystem::update` (AI loop), `CombatSystem::update` (combat/collision);
   `ZoneScopedN("render.drawScene")` nella lambda di rendering (equiv. render). Le zone si
   annidano da sole: `World::tick` ⊃ `CombatSystem::update` + `AiSystem::update`.

### Consequences
- **Positive:** base di misura pronta per le Fasi 2-4; overhead zero quando il profiler è
  spento; nessuna violazione del contratto due-binari.
- **Costo:** un clone shallow in più al primo configure anche con profiler OFF (coerente col
  modello FetchContent esistente); il profiling vero richiede una build RelWithDebInfo a parte.
- **Verifica:** entrambi i path (OFF e ON) build-verified 2026-07-13, zero warning. La cattura
  live (connessione col Tracy profiler GUI) è uno smoke manuale ancora **da eseguire**.

## ADR-016 — Sink telemetria strutturato JSONL (LLM-observable), estende ADR-013 (Accepted, 2026-07-14)

### Context
Serve una telemetria parsabile perfettamente da agenti LLM per diagnosticare fallimenti
silenziosi (AI bloccata — 06_Todo #1) e sistemi invisibili. Un piano proponeva di
"rifattorizzare il basic logger in AITelemetry, senza log testuali". **Audit del codice
live:** non esiste un basic logger — esiste già il sistema completo ADR-013 (`mini::telemetry`:
spdlog a livelli, frame counter `frame()`, dump JSON `dumpGameState`, input recorder, crash
net), usato in centinaia di call site. Il piano assumeva anche `Application::tick()` e
`EntityManager` che **non esistono** (loop inline in `run()`; entità in `World`).

### Decision
1. **Additivo, non distruttivo.** Rimuovere i log testuali contraddirebbe ADR-013 (che manda
   `engine_run.log` leggibile) e romperebbe ogni call site: RIFIUTATO. Si **aggiunge** un sink
   JSONL accanto, dentro lo stesso modulo `mini::telemetry`. `engine_run.log` = umani,
   `session_latest.jsonl` (`editor_session.jsonl` per l'editor) = LLM.
2. **API** (`Telemetry.hpp`): `enum class Level {Trace,Debug,Info,Warn,Error,Fatal}`;
   `event(Level, const char* system, const std::string& msg, const nlohmann::json& data)` +
   overload senza data; `flushEvents()`. Riusa **nlohmann/json già in casa** (come già fa
   `dumpGameState`) e il **frame counter esistente**.
3. **Formato:** una riga = un oggetto JSON valido:
   `{"frame":N,"time":T,"system":...,"level":...,"msg":...,"data":{...}}`. `time` = secondi
   dallo start (steady_clock). `j.dump()` compatto; passato come ARGOMENTO a spdlog (`"{}"`),
   non come format-string, così le graffe del JSON non vengono interpretate.
4. **Buffering/flush:** riusa l'infrastruttura spdlog (sink file dedicato, pattern `%v`).
   `flush_on(err)` + flush esplicito su ERROR/FATAL; `flushEvents()` a fine frame nel main loop.

### Consequences
- **Positive:** parsing perfetto per LLM senza toccare ADR-013 né i call site; zero nuove
  dipendenze (nlohmann/spdlog già presenti); frame counter correla JSONL / engine_run.log /
  input_history / crash_report.
- **Verifica:** build-verified 2026-07-14, zero warning; `session_latest.jsonl` prodotto,
  riga validata con parser JSON. Phase 2-4 (hook GameMode/CommandPost/AI + dump stato) da
  fare dopo verifica dell'utente.
- **Nota di scope:** Phase 4 del piano (`dumpFullState`) è in gran parte già coperta da
  `dumpGameState` (F12, ADR-013); andrà estesa/ri-hookata, non creata da zero.

## ADR-017 — Navigazione con Recast/Detour (fasata) (Accepted — Phase A+B+C, 2026-07-14)

### Context
L'AI non ha alcun pathfinding: `aiMove` muove il bot lungo il vettore verso il target con
collision-sliding + anti-stuck. Qualsiasi ostacolo *tra* bot e goal lo blocca (la telemetria
Phase 3/ADR-016 mostra gli stuck addensati sul "Cover Centro N" a z≈-6, alto e non
scavalcabile). Inoltre l'avoidance tra decine di bot manca. Serve pathfinding + crowd.

### Decision
Adottare **Recast/Detour/DetourCrowd** (standard di settore), integrato in modo idiomatico e
**fasato** per de-riskare. Vincoli rispettati: ADR-004 (navmesh generato a runtime da
`MapDef.geometry`, resta data-driven), ADR-002 (Recast linkato SOLO a GFEngine), KI #27 (tag
pinnato v1.6.0). Deviazione dai numeri del piano-utente chiarita: "06_Todo #1" reale è il
rename tooling (fatto); l'AI-stuck non è un item numerato; "#11" = metadata AI (Todo #15/KI
#11, lato dati fatto, consumer runtime pendente → gancio Phase C).
- **Phase A (FATTA):** `NavManager` (`src/game/nav/`, header leggero, tipi Detour
  forward-declared) costruisce un `dtNavMesh` single-tile dai box collider di `MapDef.geometry`
  (box→12 triangoli con `ry`, pipeline Recast solo-mesh) al load mappa in `initWorld`; API
  `findPath` (Detour). Parametri: `walkableClimb=STEP_HEIGHT (0.55)` scavalca scalini bassi,
  muri/cover alti → non walkable → path INTORNO. **Zero cambi di comportamento** (AI ancora su
  `aiMove`). Validazione via telemetria JSONL (ADR-016): eventi `navmesh built` + `sample path`.
- **Phase B (FATTA):** `dtCrowd` (max 128 agenti). `CrowdSystem` (registrato DOPO `AiSystem`)
  registra le AI come agenti, fa il reap dei morti (mappa idx→entità + generazione navmesh per
  il reset su restart), ticka il crowd una volta per step fisso, write-back `npos`→transform.
  `World::nav` (puntatore opaco). `AiComponent::crowdAgentIdx`. Movimento in `AiSystem`:
  **traversata (Hunt/Search/Patrol) → `requestMoveTarget`** (il ramo traversal impone
  `moveDX/DZ = dest − pos`, quindi target = `pos + moveDX`); **Alert/roll → `requestMoveVelocity`**
  (velocità tattica + avoidance). Fallback su `aiMove` se il navmesh manca. `requestMoveTarget`
  salta la ri-pianificazione se il target è ~invariato (chiamarlo ogni frame rendeva il moto
  lento). Sensing (SoA+K-cap+time-slicing) e player/proiettili/veicoli invariati. Salto
  anti-ostacolo e stuck-telemetry disattivati col crowd/in Alert (falsi positivi).
- **Phase C (FATTA):** aree semantiche (Todo #15/#11) dai metadata caricati a runtime
  (`DangerZoneDef`/`CoverPointDef`; `MapGeometryBox` non ha `type`/`label`, sono editor-only).
  In `build()`, DOPO l'erosione e PRIMA delle regioni, `rcMarkCylinderArea` marca le danger
  zone come area `DANGER` e i cover point come `COVER` (id 1/2; ground=0). Il filtro del crowd
  (`getEditableFilter(0)`) assegna costo `DANGER=10` → il pathfinding **aggira le danger zone**;
  `GROUND`/`COVER` neutri. Le aree extra sono comunque WALKABLE (attraversabili se non c'è
  alternativa). Per-ruolo = estensione banale (filtri aggiuntivi + `queryFilterType` per agente).

### Consequences
- **Positive:** pathfinding reale (aggira gli ostacoli → risolve l'AI-stuck alla radice) +
  crowd-avoidance non-O(N²); base per navigazione scalabile a 40+ bot.
- **Verifica (Phase A):** build-verified 2026-07-14, zero warning (Recast esterno silenziato
  via `/external:W0`). Su firebase: navmesh 74 poligoni da 264 triangoli (bounds mappa
  corretti); `findPath` spawn1→spawn2 trova 8 waypoint, 40.0m vs ~32m in retta → **aggira**
  la geometria centrale. Fix CMake: `CMAKE_POLICY_VERSION_MINIMUM=3.5` (v1.6.0 usa un
  `cmake_minimum_required` rifiutato da CMake 4.0).
- **Verifica (Phase B):** build-verified 2026-07-14, zero warning. `--stress 20` (40 bot):
  agenti registrati e on-mesh (40/40), traversano e combattono (168 colpi), muoiono/respawnano
  (reap ok), 0 crash. **Stuck: da ~80 (Phase 3) a 35**, e la distribuzione Z si è spostata via
  dal cover z=-6 → l'obstacle-stuck è risolto dal pathfinding; il residuo è **congestione del
  crowd in mischia** (Hunt/Search), non il bug degli ostacoli.
- **Verifica (Phase C):** build-verified 2026-07-14, zero warning. Su firebase: navmesh tagga
  **5 poligoni DANGER** (la danger zone a (9.4,-5,r=4)) e **7 COVER** (i 3 cover point); costo
  filtro impostato; AI combatte normalmente (166 colpi), 0 crash, stuck ~33 (invariato = il
  costo danger non introduce regressioni). L'avoidance del danger è il meccanismo standard
  Detour (area cost); il tagging è confermato via telemetria.
- **Aperti/rischi:** micro-movimento di combattimento via `requestMoveVelocity` (feel leggermente
  diverso dal direct-position, con avoidance in più — da validare a mano); veicoli come ostacoli
  dinamici non ancora nel navmesh (concern futuro); il residuo di congestione è tarabile
  (separationWeight, soglia stuck); costi/filtri per-ruolo non ancora cablati (struttura pronta).
  **Smoke manuale utente:** partita reale (non solo sim), feel del combattimento, mappa outpost.
  ADR-017 completo (A+B+C) — la navigazione Recast è in force.
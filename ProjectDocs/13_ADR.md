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
- **Aggiornamento 2026-08-04 (misura + decisione dell'utente).** Il profiler ha quantificato il
  prezzo di questa scelta: senza VBO i vertici risalgono alla GPU a **ogni** draw call, quindi il
  costo del frame segue i **vertici spediti** (misurati: 1,45 M/frame, scena 3D al 95% del frame —
  KI #87). Non è un argomento per migrare *adesso*: la causa dominante è un **asset** da 161k
  vertici, non il metodo di upload, e si affronta prima quella (doc 43, R1).
  **Decisione esplicita dell'utente**: per ora va bene che la build sia ottimizzata per **questa**
  macchina; la **compatibilità universale su Windows** è un obiettivo successivo e separato, ed è
  quello — non la performance — il momento in cui questo ADR andrà riaperto. L'utente ha inoltre
  dichiarato che il PC attuale verrà sostituito a breve: qualunque riapertura di ADR-003 va decisa
  **sull'hardware nuovo**, perché il workaround esiste per il driver di quello vecchio.

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

## ADR-010 — In-editor rename command + centralized RMW save helper (Accepted — IMPLEMENTATO 2026-07-09)

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

## ADR-011 — Split-screen feasibility verification as a gating spike (Accepted — SPIKE ESEGUITO 2026-07-09, esito (a): fattibile)

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
## ADR-018 — Gate di validazione contenuti condiviso runtime/editor (Accepted — in force, 2026-07-15)

### Context
Il progetto ha pagato ripetutamente la stessa classe di problema: **dati sbagliati che non
falliscono, ma degradano in silenzio.** Casi confermati: near-duplicate da rename manuale
(KI #7, P0); campi editabili mai consumati (KI #25, mitigato a mano); dati/fallback morti
(KI #26); zone hitbox scritte dal profilo sbagliato (2026-07-09 #2, salvato dal `.bak`); id di
fallback hardcoded (ADR-007, rimossi dopo aver fatto danno). Il pattern è invariante: un
riferimento rotto non blocca nulla e il sintomo appare lontano dalla causa.
ADR-010 ha reso strutturale la **scrittura** sicura (`saveJsonRMW`); nulla presidia la
**correttezza** del contenuto.

### Options Considered
1. **Continuare con la disciplina documentata.** Rifiutato: è esattamente la scelta che ADR-010
   ha già dovuto abbandonare dopo due incidenti — la disciplina da sola ha già fallito.
2. **Validazione dentro i loader.** Rifiutato: i loader devono restare semplici e tolleranti
   (pattern `gets/geti/getf`, 04_CodingStandards); mescolare parsing e regole di dominio rende
   entrambi più difficili da cambiare, e non dà un punto unico per l'editor.
3. **Validatore separato nell'editor.** Rifiutato: l'editor avrebbe una copia più debole delle
   regole → contenuto accettato dall'editor e rifiutato dal runtime. È il bug che vogliamo togliere.
4. **`validateContent(registry)` nel layer game/data, linkato da entrambi i binari.** Scelto.

### Decision
Un'unica funzione `validateContent(registry) -> vector<Diagnostic>` nel layer **game/data**
(accanto a `DefinitionRegistry`), che gira **dopo** `loadAll()` e non fa I/O nuovo. Tre
consumatori dello **stesso** codice: runtime (blocca il load su Error), editor (pannello
cliccabile), headless (`--validate`, exit code ≠ 0). Ogni `Diagnostic` porta severity, categoria,
file, messaggio e **suggerimento azionabile**, ed è emessa anche come evento JSONL (ADR-016).
Contenuto critico invalido **blocca**; il non critico è Warning loggato; i fallback documentati
(es. geometria firebase, ADR-004) restano ma **devono loggare di essere stati usati**.

### Consequences
- Positive: chiude strutturalmente la classe di bug di KI #7/#25/#26; l'errore diventa
  azionabile e LLM-observable; l'editor non può più divergere dal runtime.
- Costi: va deciso con cura cosa è "critico" (bloccare su un cosmetico renderebbe l'authoring
  ostile); serve manutenzione dell'elenco dei gate quando nascono nuovi tipi di riferimento.
- Non viola il contratto two-binary (ADR-002): vive sotto il gameplay, l'editor lo linka come già
  fa con gli header engine.
- Dettaglio in **24_ContentValidation.md**.

### Stato implementazione (2026-07-15) — in force
`core/Result.hpp` (`Diagnostic` con **suggestion** azionabile) + `game/data/ContentValidation`
(`validateContent` / `validateMission` / `reportDiagnostics`). Tre consumatori dello stesso
codice: runtime (Error → **blocca l'avvio**), editor (*Moduli → Validazione contenuti*, linka la
stessa funzione — `ContentValidation.cpp` è nella source list di **entrambi** i target), headless
(`--validate`, exit code ≠ 0 + JSONL). Gate attivi: riferimenti, asset su disco, sanità armi,
unità, mappe, near-duplicate sui nomi visualizzati, missioni/obiettivi, orfani.
Verificato con guasti deliberati (6 errori / 3 warning / exit 1) e sui dati reali (0/0).
**Campi fantasma** (aggiunto 2026-07-15, opzione (a)): i loader registrano in
`DefinitionRegistry::unknownKeys()` le chiavi che non leggono, mentre il JSON è ancora in mano —
zero I/O nuovo. Gli elenchi delle chiavi note stanno **accanto al parser**, l'unico posto dove
non possono divergere da ciò che il codice legge davvero. Ha trovato subito un caso reale
(`profile_id` residuo in `data/ai/B1 Heavy Droid.json`, rimosso).

### Vincoli scoperti implementando
1. **Un rilevatore che non è mai stato visto fallire non è verificato.** `--validate` dava
   0 errori sui dati reali: risultato corretto, ma indistinguibile da un gate che non gira. La
   prova è stata iniettare guasti deliberati e vederlo fallire con exit 1. Vale per ogni futuro
   gate: aggiungerne uno **senza** un caso di guasto che lo attiva è aggiungere una spia verde.
2. **Il near-duplicate si vede sui NOMI, non sugli id.** Con `id = filename stem` (ADR-001) due
   file hanno per forza id diversi: è il nome visualizzato che rende il duplicato invisibile
   all'utente (KI #7). Un gate sugli id non avrebbe trovato nulla.
3. **Un gate sui DATI non può vedere un fatto sul CODICE.** "Campo dichiarato ma non consumato"
   (KI #25) sono due problemi diversi che il doc 24 confondeva: le chiavi che il *loader ignora*
   (refusi → default silenzioso) sono rilevabili e ora lo sono; i campi che il loader *legge* ma
   nessun sistema consuma (`min_range`, `fov_deg`) non lo sono per costruzione — servirebbe
   analisi statica. Prima di promettere un gate, verificare che l'informazione che gli serve
   esista ancora nel punto in cui gira.

## ADR-019 — Framework obiettivi generico; il command post ne è una configurazione (Accepted — Phase A in force, 2026-07-15)

### Context
La Fase 2 (00_Vision) chiede **obiettivi stratificati** (principali/strategici/tattici). Oggi
l'unico obiettivo è il command post (ADR-009), e le regole vivono dentro i mode. Aggiungere
"distruggi il relè" o "scorta il convoglio" richiederebbe una modalità nuova per ogni obiettivo —
esattamente il fork che ADR-008/ADR-014 hanno evitato per le modalità.

### Options Considered
1. **Un `IGameMode` per ogni tipo di missione.** Rifiutato: moltiplica la simulazione, contro la
   verità guida "un solo core, molte configurazioni" (00_Vision).
2. **Estendere `CommandPosts` a coprire tutto.** Rifiutato: modella *una* meccanica (cattura a
   presenza + bleed); non ha attivazione, dipendenze, tier.
3. **`ObjectiveDef` generico + `MissionDef`, con il command post come configurazione.** Scelto —
   generalizza il pattern che ADR-009 ha già dimostrato corretto (obiettivo = dati nel MapDef).

### Decision
`ObjectiveDef` (type, target, activation, success, failure, **tier**, reward, consequence,
linkedObjectives) e `MissionDef` (map, mode, obiettivi, regole di successo **e** fallimento)
diventano definizioni nel registry (id = filename stem, ADR-001). Nuovo `ObjectiveSystem` dopo
`AiSystem`/`CrowdSystem`. `IGameMode` resta e decide le **regole** (ticket, `outcome()`); gli
obiettivi decidono **cosa fare**. `MapDef.commandPosts` resta valido: ADR-009 viene **avvolto,
non riscritto**.

### Consequences
- Positive: un obiettivo nuovo = un JSON; la stratificazione della Fase 2 diventa un campo
  (`tier`) invece di tre sistemi paralleli; il fallimento parziale abilita decisioni tattiche
  emergenti invece di firefight lineari.
- Costi: un sistema ECS in più nel tick; l'assorbimento dei mode esistenti va fatto
  gradualmente ("smallest safe change", 09_AI_Workflow), non in un big-bang.
- Dettaglio in **25_ObjectivesAndMissions.md**.

### Stato implementazione (2026-07-15)
**Phase A in force**: `ObjectiveDef`/`MissionDef` nel registry (id = filename stem);
`ObjectiveSystem` dopo Ai/Crowd; tipi `ReachArea`/`EliminateTarget`/`HoldAreaForDuration`;
attivazione dichiarativa (immediate/after_objective/after_time) → dipendenze senza scripting;
`tier` come campo; regole di missione dichiarate; **gate** che rifiuta con causa le missioni
invalide; mailbox `World::activeMission`/`objectiveDefs`; flag `--mission <id>`.
**Inerte senza missione** → i mode esistenti sono intatti (verificato).
**Restano**: `CaptureZone`/`DefendZone` (l'avvolgimento di ADR-009 — serve pubblicare gli stati
dei command post in una mailbox, oggi vivono nel mode), gli altri 4 tipi, l'HUD obiettivi, la
selezione della missione fuori da `--mission`, l'economia dei Punti Comando (doc 26).

### Vincoli scoperti implementando
1. **Una mailbox deve trasportare ciò che serve a chi la legge, non un riferimento.**
   `killedThisTick` portava il solo `EntityId`, ma l'entità è già distrutta: `EliminateTarget`
   non poteva filtrare per team e avrebbe contato anche i propri morti. Ora porta `{entity, team}`.
   Regola generale: se il produttore distrugge il soggetto, la mailbox è l'**unica** fonte di
   verità e deve essere autosufficiente.
2. **Un dato invalido si respinge, non si interpreta.** Una regola di missione con stringa
   sconosciuta non diventa un default silenzioso: la missione viene rifiutata con causa. Vale
   anche per gli id non risolti e per il tier incoerente — è lo spirito di ADR-018 applicato
   prima che il gate condiviso esista.

## ADR-020 — Squad & Command: SquadSystem fra AiSystem e CrowdSystem (Accepted — Phase A+B in force, 2026-07-15)

### Context
"La squadra è una risorsa, non decorazione" è un pilastro del GDD, ed è **l'unico senza alcuna
traccia nel codice**: non esistono squadre né ordini. Il GDD è esplicito: la vittoria deve nascere
da decisioni tattiche e gestione della squadra, non dalla mira. Oggi nasce solo dalla mira.
Le fondamenta però esistono già: profili tattici AI (doc 16), pathfinding + crowd (ADR-017),
consumo dei metadata di mappa (doc 18), telemetria osservabile (ADR-016).

### Options Considered
1. **Ordini che scrivono direttamente la destinazione/transform dell'AI.** Rifiutato: viola il
   vincolo confermato sul codice reale (10_ProjectMemory / doc 22) — con navmesh presente il
   movimento passa per `NavManager::requestMove*` e lo scrive il `CrowdSystem`.
2. **SquadSystem DOPO AiSystem (override della decisione).** Rifiutato: renderebbe gli alleati
   telecomandati; il GDD vuole che l'AI resti autonoma *dentro* il vincolo dell'ordine.
3. **SquadSystem PRIMA di AiSystem** (l'ordine è un vincolo sulla decisione). Scelto.

### Decision
Nuovo `SquadSystem` in `World::tick` con ordine
`MovementSystem → CombatSystem → SquadSystem → AiSystem → CrowdSystem`: la squadra assegna il
task, l'AI individuale sceglie autonomamente movimento/copertura/micro-combattimento, il crowd
esegue. `SquadComponent` sui membri; l'input del giocatore arriva via **pattern mailbox** sul
World (10_ProjectMemory), senza accoppiare `ecs/` al codice di gioco. Set di ordini iniziale
volutamente piccolo (Follow, HoldPosition, MoveTo, TakeCover, FocusFire, Revive, Regroup).
Ogni ordine termina **completato o fallito con causa esplicita** — mai in silenzio.
L'economia tattica (Punti Comando) si guadagna **completando obiettivi, non uccidendo**.

### Consequences
- Positive: realizza il pilastro mancante; riusa nav/crowd/metadata già in force; verificabile
  headless in `--sim` via telemetria.
- Costi: un sistema in più nel tick (impatto da misurare, doc 20); lo stato "a terra"+rianimazione
  tocca `CombatSystem`.
- Rischio: il comando deve funzionare **durante** il firefight — un sistema che obbliga a fermarsi
  ha già fallito il requisito di design.
- Dettaglio in **26_SquadAndCommand.md**.

### Stato implementazione (2026-07-15)
**Phase A in force**: `SquadComponent` + `SquadSystem` registrato fra Combat e Ai; squadra alleata
formata a runtime (i respawn creano entità nuove, quindi si ri-arruola ogni tick); ordine di default
`Follow`; ciclo di vita completo con telemetria (`order issued/completed/failed`).

**Phase B in force**: comando contestuale a un tasto (`Action::SquadOrder`, default G) risolto dal
raycast del mirino — nemico → `FocusFire`, cover point reale del MapDef entro 4 m → `TakeCover`,
altrimenti → `MoveTo`; intenzione via mailbox `World::squadOrder`; **raggiungibilità verificata
prima di impartire** (`findPath` ritorna un path *parziale*, non un errore, quindi si confronta
l'arrivo col punto chiesto → gli ordini impossibili sono rifiutati con causa); HUD con pannello
SQUADRA (membri/ordine/distanza) letto dallo stato reale dei membri + esiti nel feed.
**Restano**: la **ruota di comando** (livello 2 del doc 26: Regroup/Hold/Advance) e **Phase C**
("a terra" + rianimazione, tocca CombatSystem). Revive e Regroup falliscono con causa esplicita.

### Vincoli scoperti implementando (non erano previsti nel design)
1. **L'ordine è un guinzaglio, non una destinazione continua.** Vincolare il movimento in ogni
   frame telecomanda l'AI e ne annulla il comportamento tattico; escludere del tutto lo stato
   `Alert` invece rende l'ordine **inerte**, perché in una sim densa le AI sono in Alert quasi
   sempre (misurato: un vincolo Alert-escluso produceva distanze identiche al baseline, effetto
   zero). Soluzione in force: l'ordine ha precedenza **solo fuori dal raggio di soddisfazione**
   (Follow 8 m, HoldPosition 2 m, MoveTo 1.5 m); dentro il raggio l'AI è libera — è il senso
   letterale di "autonoma dentro il vincolo".
2. **Un ordine che fa percorrere distanza DEVE passare per il pathfinding, anche in Alert.** Il
   ramo Alert usa `requestMoveVelocity`, che non pianifica: è pensato per lo strafe tattico a
   corto raggio. Con un ordine di viaggio l'agente spingeva contro i muri senza aggirarli. In
   force: flag `orderTravel` in `AiSystem` → `requestMoveTarget` anche in Alert. Il facing **non**
   viene toccato in Alert: l'AI continua a mirare al nemico mentre si riposiziona.
3. **Il movimento è vincolato, il combattimento no.** Mira e fuoco non passano dal ramo di
   movimento: restano autonomi per costruzione. È ciò che soddisfa il requisito "il comando deve
   funzionare *durante* il firefight". `FocusFire` è l'eccezione speculare: vincola il **bersaglio**
   e NON il movimento.
4. **Un sistema dopo `CombatSystem` non può osservare una morte** (Phase B). Combat distrugge
   l'entità nello stesso update in cui la uccide: il ciclo di vita di FocusFire interrogava la
   salute di un bersaglio già inesistente e riportava `failed` **proprio sul successo**. Serve la
   mailbox `World::killedThisTick`. Regola generale: *un ciclo di vita che dipende da un evento
   che il sistema non può osservare è codice morto* — verificarlo con la telemetria, non a vista.

## ADR-021 — Save di carriera: snapshot di dominio + scrittura atomica (Proposed, 2026-07-15)

### Context
La Fase 3 richiede persistenza della carriera. Il progetto ha già una storia di **salvataggi
distruttivi**: 2026-07-08 `BalanceEditor::saveMap` ha distrutto `geometry`/`command_posts` di
firebase.json (partita ingiocabile, KI #15); KI #19 — ogni build cancellava i preset perché
stavano dentro `data/`. ADR-010 ha risolto per gli editor con `saveJsonRMW`. Un save di carriera
è la stessa classe di rischio ma **il danno è peggiore: non è un file rigenerabile, è il
progresso dell'utente.**

### Options Considered
1. **Serializzare il World/ECS.** Rifiutato: il World contiene stato derivato e puntatori opachi
   (`World::nav`, `crowdAgentIdx`, mailbox) — insalvabili per costruzione, e fragilissimi fra versioni.
2. **Solo RMW come per gli editor.** Insufficiente: l'RMW protegge i campi altrui nello stesso
   file, ma non dal crash **a metà scrittura**. Su una carriera servono entrambi.
3. **Snapshot di dominio immutabile + temp → validate → `.bak` → rename atomico.** Scelto.

### Decision
Serializzare lo **stato di dominio** (`ProfileSave` / `CareerSave` / `CampaignSave` /
`MissionSnapshot`) e **ricostruire** le entità da definizioni + stato persistente al load. Ogni
scrittura: snapshot immutabile → file temporaneo → validazione → flush/close → rotazione `.bak` →
**rename atomico** → evento `SaveCompleted`/`SaveFailed` (ADR-016). I save vivono in
`<exe>/user_saves/`, **fuori da `data/`** (lezione KI #19). `nlohmann/json` resta il formato.

### Consequences
- Positive: perdere una carriera diventa strutturalmente difficile; save leggibili e diff-abili;
  la serializzazione post-snapshot è l'unico candidato sano a un futuro async (mai leggere il
  World live — coerente con la policy di threading del progetto).
- Costi: ricostruire le entità al load richiede che ogni stato persistente sia esprimibile in
  termini di definizioni + dominio (vincolo sano, ma va rispettato da chi aggiunge sistemi).
- Versioning/migrazioni **volutamente fuori scope** finché non esistono save reali da migrare
  (evita architettura speculativa, 00_Vision non-goals): aprire un ADR allora.
- Dettaglio in **28_Persistence.md**.

## ADR-022 (PRIMA STESURA — SUPERATA, vedi ADR-022 RISCRITTO in fondo) — Le classi sono professioni, non preset di armi (2026-07-16)

> ⚠️ **Questa stesura è superata**: coglieva solo la meta' NPC del modello e ignorava che il
> giocatore NON sceglie una classe (GDD 11.3). Lasciata come storico.

### Context
Il GDD originale è entrato nel repo il 2026-07-16 (`Galactic_Front_GDD.docx`, sorgente di
autoring; copia operativa leggibile in `29_GDD.md`, vedi doc 23). Al primo confronto,
il **cap. 12** contraddice **14_ClassSystem** su cosa *sia* una classe:

- **GDD 12, prima riga:** *"Rappresentare professioni militari, non semplici categorie di armi.
  Ogni classe cambia comportamento sul campo, contributo alla squadra, approccio tattico."*
- **GDD 12.3:** le classi definiscono *"le composizioni degli NPC, il loro comportamento IA e
  loadout"*; una squadra mista (Trooper + Heavy + Recon + Engineer + Leader) *"deve essere più
  efficace di una monoclasse, e deve comportarsi diversamente"* → lega le classi **direttamente
  all'IA** (cap. 8) e al Sistema di Squadra (cap. 7, pilastro #4).
- **GDD 12, Parametri:** una classe è *loadout base + abilità/perk sbloccabili + curva XP di classe
  + comportamento IA associato (NPC) + affinità equipaggiamenti + requisiti di sblocco*.
- **14_ClassSystem** modella `primaryWeaponId` + `secondaryWeaponId` + `abilityIds[]` + un `role`
  esplicitamente **descrittivo e non consumato**, e mette fra gli Out of Scope: *"Do not couple
  enemy AI archetypes to ClassDef without a separate ADR"*.

Quindi il `ClassDef` implementato (Phase A, 2026-07-15) copre **1 dei 6 parametri** e **vieta**
proprio il legame che il GDD indica come essenziale. Confermato dall'utente in modo indipendente:
*"le classi nel design della mia idea del gioco non sarebbero preset di armi"*.
La regola di precedenza di **23_GameDesignBridge** è netta: sull'**intento di design il GDD vince**,
e i conflitti aprono un ADR. Questo è quel caso.

### Options Considered
1. **Lasciare `ClassDef` com'è e chiamarla "classe".** Rifiutato: il nome più importante del
   sistema significherebbe una cosa che il GDD nega nella sua prima riga. È il tipo di deriva
   nome↔concetto che questo progetto paga da mesi (KI #7/#25/#35).
2. **Cancellare `ClassDef` e ridisegnare da zero a Fase 3.** Rifiutato: il *loadout base* È uno dei
   sei parametri del GDD. Il codice non è sbagliato, è incompleto: cancellarlo distrugge lavoro
   valido e la Fase 3 lo ricostruirebbe identico.
3. **Rinominare l'attuale in `LoadoutDef` e creare più tardi una `ClassDef` vera.** Possibile, ma
   produce due tipi dove il GDD ne vuole uno: la classe *contiene* il loadout, non lo affianca.
4. **Tenere `ClassDef` come seme e farlo crescere verso il GDD, un parametro alla volta.** Scelto.

### Decision (proposta, da approvare)
`ClassDef` resta il tipo, e cresce nell'ordine dettato dal valore, non dallo schema:
1. **`aiProfileId`** — il parametro che trasforma un elenco di armi in una professione: la classe
   dice *come si comporta* chi la indossa. Sblocca GDD 12.3 (composizione NPC) e alimenta il
   sistema di squadra già in force (ADR-020): una squadra Trooper+Heavy+Recon si comporta
   diversamente da una monoclasse. **Richiede di superare l'Out of Scope del doc 14** — è
   esattamente l'ADR che quel doc pretendeva.
2. **`role` diventa un enum consumato** (assault/heavy/recon/engineer/support/leader) invece di un
   tag libero: è ciò che il SquadSystem userà per assegnare i task per ruolo.
3. **Perk/XP/sblocchi** (classi d'élite: ARC Trooper, Clone Commando, ruoli di comando) → **Fase 3**,
   insieme a 27_Progression: non anticiparli (00_Vision, no architettura speculativa).
4. **`EnemyDef` può referenziare una classe** — ma solo dopo (1) e (2), e senza rimuovere
   `weaponIds[]`: additivo, come ogni altra migrazione di questo progetto.

### Consequences
- Positive: il concetto più importante del gioco torna a significare ciò che il GDD dice; le classi
  alimentano il pilastro #4 (squadra) invece di essere un menu armi; la progressione (Fase 3) trova
  un'unità di sblocco già viva.
- Costi: tocca l'IA (cap. 8) e il SquadSystem; `role` come enum è un cambio di schema sui dati
  esistenti (oggi 2 classi, costo nullo — farlo ora è molto più economico che dopo).
- Rischio: la tentazione di implementare tutti e 6 i parametri insieme. Il valore è quasi tutto
  in (1)+(2); il resto è Fase 3.
- **14_ClassSystem va riscritto** su questa base: oggi il suo Problem Solved è falso su due punti
  (vedi le note già inserite lì) e il suo Scope è più piccolo del GDD.

## ADR-022 (RISCRITTO) — Le classi sono DUE sistemi in una definizione; le specializzazioni sono un terzo (Accepted — modello, 2026-07-16)

> **Sostituisce integralmente la prima stesura di ADR-022** (2026-07-16, sopra), che proponeva
> "far crescere `ClassDef` verso la professione partendo da `aiProfileId`". Era **parziale**:
> coglieva metà del modello e ignorava che **il giocatore non sceglie affatto una classe**.
> La correzione arriva da una spiegazione diretta dell'utente + **GDD 11.3**, che non avevo letto
> (avevo letto il cap. 12 sulle classi, non l'11 sulla progressione — errore di metodo: il
> concetto era spiegato in DUE capitoli).

### Context — il modello reale, in tre parti
Chiarimento dell'utente (2026-07-16), che **rifinisce il GDD** e vale come intento autoritativo.

**1. `ClassDef` per i CLONI ALLEATI (NPC) — istanziata.**
Indica *"le abilità, il comportamento, il loadout e in caso l'aspetto (variazioni di armatura)"*.
È il modello "professione" (GDD 12.1/12.3): una squadra Trooper+Heavy+Recon+Engineer+Leader **deve
comportarsi diversamente** da una monoclasse. Qui la classe si **instanzia** su un'unità.

**2. `ClassDef` per il GIOCATORE — livellata, MAI scelta.**
> *"Per il personaggio è diverso, **non ne sceglie una**: le classi sono tipo un albero delle
> abilità. Ogni classe esiste **contemporaneamente** per il giocatore e può essere **livellata** —
> per esempio usando determinate armi, o completando determinati tipi di obiettivi che rispecchiano
> la filosofia di quella classe. Salendo di livello si sbloccano **perk**: se salgo di livello su
> Heavy avrò magari un bonus per le armi pesanti come lo Z-6, o una versione migliore di un'arma, o
> equipaggiamenti particolari. Il **gameplay decide** quali classi vengano livellate, e quindi fa
> crescere il personaggio in una direzione."*

**GDD 11.3 lo conferma alla lettera**: *"la classe **non è una scelta rigida all'inizio**, ma
un'identità che **emerge dal comportamento**. Un clone che usa spesso armi pesanti sviluppa capacità
da Heavy; uno che completa missioni di ricognizione sviluppa tratti da Recon. Ogni classe ha una
propria progressione, con esperienza ottenuta tramite **azioni coerenti**"* — con gli assi già
elencati (Heavy: armi pesanti, distruzione mezzi, difesa posizioni; Medic: cure, rianimazioni;
Recon: ricognizione, eliminazioni precise; Engineer: hacking, sabotaggio).

**3. SPECIALIZZAZIONI (ARC Trooper, Clone Commando) — sbloccate, NON livellate.**
> *"Le classi sono differenti dalle specializzazioni, che fanno riferimento agli ARC trooper o ai
> Clone Commando — diciamo delle forze speciali, che si sbloccano portando a termine una serie di
> **obiettivi specifici** e che forniscono perk, nuove armi, armature, abilità, ma che **non si
> livellano**."*

**Nota terminologica**: il GDD 12.2 le chiama *"classi d'élite"* e il 12.4 le descrive come
evoluzione di una classe (*"Clone Trooper → Heavy avanzato → ARC Heavy"*). La spiegazione
dell'utente le rende un **asse separato** (sblocco a obiettivi, nessun livello). Vale la
spiegazione più recente: sono un **terzo concetto**, non un ramo delle classi.

### Cosa è stato costruito che CONTRADDICE il modello
`MatchSettings.classId` + la riga **"Classe" nel PreMatch** = *il giocatore sceglie una classe che
gli assegna il loadout*. È esattamente ciò che GDD 11.3 nega (*"non è una scelta rigida
all'inizio"*) e che l'utente nega (*"non ne sceglie una"*).
Non è un dettaglio: fa sì che **"classe" significhi due cose contraddittorie nello stesso codice** —
la deriva nome↔concetto che questo progetto paga da mesi (KI #7, #25, #35).
Per il giocatore quel selettore è in realtà un **preset di loadout**: utile, ma va chiamato col suo
nome. La classe del giocatore non si sceglie: si **livella**.

### Decision
1. **`ClassDef` resta UNA definizione, usata in due modi** — è il modello dell'utente (*"la stessa
   classe esiste sia per i cloni alleati, sia per il personaggio"*), e tenerne una sola evita che
   le due metà divergano.
2. **Metà NPC — implementabile ORA**: `ClassDef` guadagna `aiProfileId` (e poi l'aspetto); le unità
   alleate/nemiche possono **referenziare una classe** invece di ripetere loadout+profilo+abilità.
   Sblocca GDD 12.3 e alimenta il sistema di squadra (ADR-020). Supera l'Out of Scope del doc 14
   (*"non accoppiare gli archetipi AI a ClassDef senza un ADR separato"*): **questo è quell'ADR**.
3. **Metà giocatore — Fase 3 (doc 27)**: `classXp[classId]` + livelli + perk, alimentata da **azioni
   coerenti**. Le fondamenta esistono già: `World::missionStats` conta kill/obiettivi/tempo, e gli
   obiettivi hanno `tier`/`type` → "completare obiettivi di un certo tipo" è **già osservabile**.
   **Non anticiparla**: richiede il sistema perk, che non esiste (e KI #32 — nemmeno le abilità del
   giocatore esistono).
4. **Il selettore di classe del giocatore va rimosso o rinominato "Loadout"**: non può restare a
   chiamarsi "Classe". Serve la decisione dell'utente su quale delle due.
   → **DECISO E APPLICATO 2026-07-17: RIMOSSO** (scelta dell'utente). Il fatto che ha sciolto il
   dubbio: le righe **"Arma primaria"/"Arma secondaria" esistevano già** nello stesso menu, e la
   riga "Classe" le **sovrascriveva in silenzio** (`Application`: `primaryId = cls->primaryWeaponId`).
   Non era quindi solo un nome sbagliato: era la stessa trappola "due posti decidono lo stesso dato,
   uno vince senza dirlo" che ADR-018 combatte. Rinominarla "Loadout" avrebbe conservato la
   trappola cambiandole etichetta. Rimossa la riga **e** `setClassList`/`getSelectedClassId`/
   `ClassEntry`: senza i metodi la regola è **strutturale** (stesso ragionamento della rimozione di
   `consumeTeam1Ticket()` in KI #39). `--class` resta come **override di test**, dichiarato tale.
   Trappola evitata nel farlo: lasciare `m_settings.classId = getSelectedClassId()` con la riga
   rimossa avrebbe azzerato `--class` a ogni passaggio dal menu — **KI #36 in miniatura**.
5. **Specializzazioni**: terzo tipo di definizione (`SpecializationDef`), sbloccato da una lista di
   obiettivi, senza livelli. **Non progettarlo ora** — dipende da perk e progressione (Fase 3).

### Consequences
- Positive: il concetto più importante del gioco smette di significare due cose opposte; la metà NPC
  è sbloccata e alimenta subito il pilastro #4 (la squadra come risorsa); la metà giocatore troverà
  le fondamenta già pronte.
- Costi: `role` dovrà diventare un enum consumato quando la metà NPC userà i ruoli tattici; il
  selettore PreMatch va toccato.
- ~~**14_ClassSystem è da riscrivere su questa base**: oggi descrive solo un pacchetto di armi.~~
  → **FATTO 2026-07-17**: doc 14 riscritto. Stato dichiarato **MISTO** (metà NPC = Current
  Implementation; metà giocatore = Planned). Questo **sblocca doc 27**, il cui criterio di
  accettazione #1 è *"14_ClassSystem implementato prima di iniziare"*: è la **metà NPC** a
  soddisfarlo, e ora il doc lo dice invece di dichiararsi "not yet implemented" mentre è in
  produzione.

### Vincolo di metodo che ne esce
**Un concetto può essere specificato in più capitoli del GDD.** Le classi vivono nel cap. 12
(cosa sono) *e* nell'11.3 (come si ottengono) — leggere solo il capitolo omonimo ha prodotto un ADR
sbagliato e una feature contraria al design. Prima di decidere su un sistema, cercare il concetto
in **tutto** il GDD (`grep`), non solo nel suo capitolo.

## ADR-023 — Entità = corpo, Classe = professione istanziabile su un corpo (con moltiplicatori di stat) (Accepted — in force, 2026-07-19)

> **Raffina la metà NPC di ADR-022.** Non la sostituisce: ADR-022 stabilisce *cosa* è una classe
> (professione, non preset d'armi; una def usata in due modi). Questo ADR stabilisce *come si
> istanzia*: chi porta il **corpo** e chi la **professione**, e come i roster referenziano le unità.
>
> **Nota 2026-08-02 (KI #88).** Runtime e gate `--validate` rispettavano questo ADR dal primo giorno, ma il
> **dropdown del roster nel BalanceEditor elencava solo le entità-corpo**: per oltre due settimane l'unico
> alleato schierabile è stato il Clone Trooper, e le classi (Marksman, Heavy Trooper) erano di fatto
> irraggiungibili. Un ADR non è "in force" finché ogni **dropdown che lo espone** offre tutto ciò che
> ammette — un combo che mostra un sottoinsieme è una capacità che non esiste.

### Context — la distinzione che manca
Chiarimento dell'utente (2026-07-19), intento autoritativo che rifinisce il modello:
- Un'**entità** deve rappresentare un **corpo reale**: modello, stat base (hp, velocità), hitbox,
  metadata/attach. Si usa **solo quando il modello è davvero diverso** — B1 vs B2 Super vs Droideka.
- Una **classe** rappresenta una **professione** che gira **sullo stesso corpo**: differisce per
  **armi, abilità, gadget** e segni distintivi d'armatura (che appaiono sul modello ma non lo
  cambiano), più **moltiplicatori** sulle stat base (hp, velocità). Trooper / Sniper / Medic / Heavy
  sono **un corpo (clone) + quattro classi**, non quattro entità.

Il doc 14 già dichiara metà del principio (*"hitbox/mesh/scala restano dell'entità: è il corpo, non
il mestiere"*) e cita il caso reale (`Clone Trooper`/`Heavy Clone Trooper` = un corpo, due
professioni). Ma il modello è **incompleto**: oggi è l'**entità** a referenziare una classe
(`EnemyDef.classId`), quindi l'entità resta il **tipo-unità** nei roster → per avere uno "Sniper
Clone Trooper" servirebbe COMUNQUE una seconda entità. È la duplicazione che il modello voleva
eliminare. Mancano tre cose: (1) la classe che referenzia un **corpo base**; (2) i **moltiplicatori**
di stat; (3) i roster che referenziano **classi** come tipo-unità.

### Decision
1. **`ClassDef` guadagna `baseEntityId`** — il corpo da cui prende modello, hitbox, stat base,
   attach/metadata. Una classe con `baseEntityId` è **istanziabile da sola** (è un tipo-unità).
   Dropdown dal registry (mai testo libero, 04).
2. **`ClassDef` guadagna moltiplicatori di stat**: `hpMult`, `speedMult`, `damageMult` (default 1.0).
   Applicati alle stat base del corpo alla risoluzione. Additivo: 1.0 = corpo invariato.
3. **I roster (`MapDef.allyTypes/enemyTypes`, e le missioni) possono referenziare una CLASSE** come
   tipo-unità, non solo un'entità. Istanziare una classe = caricare il corpo (`baseEntityId`) +
   applicare loadout/abilità (ADR-022) + moltiplicatori. Un'entità referenziata direttamente resta
   valida (un corpo "nudo" senza professione) → additivo, niente rompe.
4. **Le entità si riservano ai corpi veri.** I varianti di truppa (Heavy, Sniper, Medic) diventano
   **classi**; le entità restano per differenze di modello (B1/B2/Droideka; clone/ARC se il modello
   cambia). `hitbox_profile`, mesh, scala, attach, fazione **restano del corpo** (già vero, ADR-022).
5. **Aspetto/segni d'armatura**: previsti dal modello dell'utente e da ADR-022 (*"in caso
   l'aspetto"*); schema additivo quando servirà (non in questo ADR).

### Migrazione (contenuto)
- `Heavy Clone Trooper` (entità) → **classe** `heavy` con `baseEntityId: Clone Trooper` + arma Z-6 +
  `hpMult`/`speedMult` a gusto; poi l'entità ridondante si elimina.
- `Heavy B1 Battle Droid` (entità) → **classe** su `baseEntityId: B1 Battle Droid`.
- I roster che citavano quelle entità citano le nuove classi.
Le classi `trooper`/`marksman` esistenti restano; guadagnano `baseEntityId` (Clone Trooper).

### Consequences
- **Positivo**: un corpo, molte professioni senza duplicare entità (GDD 12.3 finalmente esprimibile
  pulito); authoring più semplice (crei una classe, non un'entità intera); meno dati da mantenere in
  sync (una hitbox per corpo, non per variante).
- **Costo**: tocca `ClassDef`, la risoluzione (`resolveUnitArchetype`/`classres`), il caricamento
  roster, il ClassEditor (dropdown corpo + moltiplicatori) e l'EntityEditor (marcatura campi decisi
  dalla classe). Migrazione dati. Da fare in passi **additivi** (i moltiplicatori e `baseEntityId`
  prima, additivi; poi i roster-referenziano-classi; infine la migrazione).
- **Gate (ADR-018)**: nuovi controlli — `baseEntityId` deve esistere; moltiplicatori > 0; un roster
  che referenzia un id deve risolverlo o come entità o come classe.

### Status
**Accepted — in force (2026-07-19).** Implementato e verificato: `ClassDef.baseEntityId`/`hpMult`/
`speedMult`/`damageMult`; `effectiveUnit` in ConquestMode (id-roster → corpo+classe); moltiplicatori
in `resolveUnitArchetype`; roster referenziano classi; gate esteso (base_entity esiste, mult > 0,
roster risolve come entità o classe). Migrazione fatta: `Heavy Clone Trooper`→classe `Heavy Trooper`
(corpo `Clone Trooper`); `B1 Heavy Battle Droid`(entità)→classe omonima (corpo `B1 Battle Droid`,
ai `B1 Heavy Droid`, hp_mult 1.125); entità ridondanti eliminate; firebase/outpost roster espliciti.
Verificato via `--sim`+telemetria: il Heavy B1 risolve `unit=B1 Battle Droid` (corpo) + `class=B1
Heavy Battle Droid` → `ai=B1 Heavy Droid`, `weapon=E-5C`. ClassEditor: dropdown corpo + moltiplicatori.
La metà giocatore (livelli/perk, doc 27) non è toccata: là la classe si livella, non si istanzia — Fase 3.

---

## ADR-024 — Comando nemico: il Droide Tattico è un COMANDANTE (controparte del giocatore), non un buff (Accepted — in force v0, 2026-07-20)

> **Riscritto il 2026-07-20** su chiarimento autoritativo dell'utente. La **prima stesura**
> (aura di accuratezza locale) è **superata**: un buff ad area non è ciò che è il Droide Tattico.
> Il concetto corretto è uno **stratega** — la controparte nemica del comando del giocatore — con
> influenza **globale**, non locale. Vedi memorie [[droide-tattico-concept]] / [[design-coherence-principle]]
> e la Planned Feature **doc 32 (Comando Nemico)** per lo scope v0.

### Context — cos'è davvero il Droide Tattico
Chiarimento dell'utente: il Droide Tattico serie T **non combatte** in prima linea. Sta **nascosto e
protetto** e **gestisce la strategia** — è il "generale" dei droidi. Senza di lui esiste già una
tattica di base dettata dallo stato partita (obiettivi disponibili/distrutti, metadata, conteggio
truppe per zona); **con** lui si aggiunge uno **strato strategico** che rende gli ordini più coerenti
e coordina **TUTTE** le truppe separatiste (non un raggio). È la **controparte** del giocatore: come io
do ordini ai cloni (ADR-020/doc 26), lui ne dà ai droidi. Ucciderlo → **conseguenza** (i droidi
perdono coordinamento), come la torre comunicazioni. La meccanica "buff locale ai soldati vicini"
appartiene invece al **futuro sistema di gradi** (ufficiali a cascata, [[command-rank-system]]), non
qui. Questa **v0** costruisce solo la **base**: il sistema completo (gradi, strati, entità a sé) verrà
progettato dopo (§5: base minima, scope in doc 32).

### Decision (v0)
1. **`AbilityDef.type = "command"`** (marker): assegna il ruolo di comandante. Nessun parametro
   rilevante in v0. Additivo: nessun tipo esistente cambia. (Sostituisce la bozza `command_aura`.)
2. **`CommanderComponent`** (marker ECS). Allo spawn `ConquestMode::spawnUnit` traduce l'ability
   `command` in `CommanderComponent` (pipeline ability→componente dello scudo).
3. **Direttiva strategica globale** (`World::enemyCommand`, mailbox — controparte di `squadOrder`).
   **v2 (2026-07-20, correzione utente): il comandante dà un INTENTO, non una destinazione.**
   Prima pubblicava un *punto* verso cui tutti i droidi camminavano → era un **cervello unico che
   pensava al posto delle singole AI**, esattamente ciò che NON serve. Ora `AiSystem` a inizio tick,
   se ≥1 comandante di **team 2 vivo**: (a) **identifica l'obiettivo** (command post non separatista
   più vicino) e (b) **decide uno `stance`** analizzando la situazione — euristica v1 sul rapporto di
   forze: molto in inferiorità → `Retreat`, in inferiorità → `Hold`, altrimenti → `Advance`. Pubblica
   obiettivo + stance. Ogni cambio di intento va nel feed ("Ordine del Droide Tattico: AVANZATA —
   obiettivo Alpha"). Nessun comandante vivo → `active=false` + messaggio-conseguenza.
4. **Consumo — l'intento modula, l'AI decide il COME** (movimento, non combattimento). Nel ramo
   Patrol (pre-contatto), per i droidi di team 2:
   - `Hold` → **presidia**: ognuno pattuglia la sua route / la sua area (è così che i percorsi
     autorati vengono davvero usati);
   - `Advance` → la **forza di manovra** (unità senza route) spinge sull'obiettivo identificato;
     chi è su una route continua a presidiare — un comandante non manda tutti sullo stesso punto;
   - `Retreat` → si ripiega verso lo spawn separatista.
   In ogni caso **percorso, coperture e ingaggio restano decisioni della singola AI** (navmesh,
   cover intelligence, peek/hide): il comandante non li sceglie. Il combattimento resta autonomo —
   identico al guinzaglio-ordine del giocatore (l'ordine vincola il movimento, mai mira/fuoco).
5. **Il Droide Tattico è una CLASSE (ADR-023)** sul corpo `B1 Battle Droid`: ability `Tactical
   Command` (+ `Shield`), `hp_mult 1.5` (più coriaceo), **tinta scura**. L'entità a sé è **futura**
   (serve un modello proprio; [[droide-tattico-concept]]).
6. **Singolo obiettivo vivente, nelle retrovie** (chiarimento utente 2026-07-20): NON è una truppa
   del roster (`enemy_types` ne spawnerebbe molti). Nuovo campo mappa `commander { unit, x, z }`
   (`CommanderSpawnDef`): ConquestMode ne spawna **uno solo** alla posizione autorata, **stationary**
   → AiSystem non lo muove mai (ogni ramo di movimento è sotto `!ai->stationary`); si limita a
   fronteggiare e sparare a chi vede (autodifesa). **Non rispawna** (`RespawnEntry.respawns=false` →
   fuori da `m_trackedUnits`, come i bersagli strategici): resta uno per partita. È l'**autorità strategica più alta** dei
   separatisti: dirige, non combatte. Gate: il comandante deve risolvere e portare l'ability
   `command`; **warning** se un comandante finisce in `enemy_types` (previene il bug "ce ne sono molti").

### Consequences
- **Positivo**: riusa la pipeline ability→componente e le mailbox esistenti; **non** piega il
  `SquadSystem` (solo-giocatore) né inventa un sistema parallelo; "muore → coordinamento sparisce" è
  **gratis** (la direttiva si ricalcola per tick). Coerente col mondo (uno stratega dà senso allo Sniper
  e all'assalto all'HVT) e con una vera catena di comando ([[design-coherence-principle]]).
- **Costo/limiti (v0)** — tutti **dati o futuro**, non bug: (a) combatte ancora col profilo `B1 Battle
  Droid` invece di **restare protetto** (KI #58: profilo AI "retrovie" da autorare). (b) **Un solo tipo
  di direttiva** (concentra sul post di fronte); ventaglio di ordini → espansione. (c) Il calcolo vive
  nel precompute di AiSystem: quando cresce va **estratto** in uno `StrategicAiSystem` dedicato (§5.3).
  (d) Entità a sé, gradi, controparte-cloni → futuro (doc 32 Out of Scope).
- **Gate (ADR-018)**: `command` è un tipo valido (nessuna whitelist di tipi-abilità); la classe che
  porta l'ability risolve come le altre. Nessun controllo nuovo.

### Status
**Accepted — in force v1 (2026-07-20).** v0 (direttiva strategica) + **v1** (singolo obiettivo
vivente, retrovie, stationary). Implementato: `AbilityDef.type "command"`; `CommanderComponent`
(rinominato dalla bozza `Aura`); mailbox `World::enemyCommand`; calcolo focus + feed + consumo in
`AiSystem`; **campo mappa** `MapDef.commander` (`CommanderSpawnDef`) + loader (DefinitionRegistry) +
gate (ContentValidation: risolve, è comandante, warning se nel roster); **spawn singleton stationary**
in `ConquestMode::start`; ability `data/abilities/Tactical Command.json`; classe `data/classes/Tactical
Droid.json` (profilo AI autorato `Tactical Droid`); `firebase.commander` (retrovie, non più in
`enemy_types`). BalanceEditor: tipo `command`. Verificato: build 0/0; `--validate` 0/0; `--sim` senza
crash, **esattamente 1** `class=Tactical Droid` risolto (prima erano molti). **Manca smoke manuale**:
in-game, comandante fermo nelle retrovie, droidi che convergono sul post-focus, e alla sua morte il
messaggio + ritorno alla pattuglia. **Limiti/futuro**: UI di piazzamento nel MapEditor (ora JSON a
mano, preservato da RMW); ingaggio "solo-se-attaccato" vero; ordini più ricchi; gerarchia gradi.

---

## ADR-025 — World Intelligence Layer: seam di query + Fase 0 dei metadata tattici (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Prima fase del piano doc 33** (World Tactical Intelligence). Filosofia: *"AI semplici in un mondo
> intelligente"* ([[world-tactical-intelligence]]). Fase 0 = fondamenta a basso rischio che abilitano
> tutte le fasi successive, **senza cambiare il comportamento AI**. Ogni fase futura avrà il suo ADR.

### Context
Doc 33 §2 elenca i problemi architetturali. La Fase 0 ne affronta tre, i più bloccanti:
- **#1 Consumo sparso**: `AiSystem` scandisce direttamente `MapDef.coverPoints` (`pickCover`) e le
  `dangerZones` (`applyDangerRepulsion`); non c'è un punto unico dove interrogare la conoscenza del
  mondo → ogni nuovo consumatore ri-scriverebbe scansioni, non ottimizzabili né testabili.
- **#2 Doppia verità sul pericolo**: `applyDangerRepulsion` (repulsione manuale) è ridondante col
  costo DANGER del navmesh (doc 22), che già fa aggirare le zone via pathfinding.
- **#9 Lacune editor**: il fronte dei cover point (e altri campi metadata) non è regolabile col gizmo
  (KI #60) — l'utente (2026-07-20) ha chiesto di abilitare ruota/scala sui metadata dove ha senso.

### Decision (Fase 0)
1. **Query layer `mini::worldintel`** — nuovi `include/mini/game/ai/WorldIntel.hpp` +
   `src/game/ai/WorldIntel.cpp`. È il seme del World Intelligence Layer: **solo dati+query pure** su
   `MapDef` (nessuna logica AI). API iniziale:
   - `const CoverPointDef* nearestCoverToward(const MapDef&, float x, float z, float towardX, float towardZ, float maxDist)` — sposta qui la logica di `AiSystem::pickCover`.
   - `float dangerAt(const MapDef&, float x, float z)` — livello di pericolo aggregato in un punto.
   `AiSystem` chiama il layer invece di scandire da sé. **Comportamento invariato** (stessa logica di
   `pickCover`), cambia solo *dove* vive. Scansione lineare per ora; il seam permette di aggiungere un
   indice spaziale dopo senza toccare i chiamanti.
2. **Consolida la doppia verità danger**: `applyDangerRepulsion` diventa **fallback** — applicata solo
   quando il crowd/navmesh NON è attivo (`!useCrowd`). Col crowd, il costo DANGER del navmesh già
   aggira le zone → niente doppia repulsione. Nessuna regressione (in gioco reale il navmesh c'è sempre).
3. **Editor: ruota/scala sui marker metadata dove esiste un campo** (KI #60 + richiesta utente):
   - **Cover point** → **ruota** Y (scrive `facing`).
   - **Vehicle spawn** → **ruota** Y (scrive `ry`).
   - **Danger zone** → **scala** (scrive `radius`, uniforme, con minimo).
   - **Command post** → **scala** (scrive `radius`).
   Gli altri marker (spawn, route point, target) restano solo-sposta (nessun campo mappabile).
4. **Doc-accuracy**: 15/18 aggiornati (l'AI consuma già i metadata; il navmesh marca DANGER/COVER).

### Out of Scope (Fase 0 — arrivano nelle fasi successive di doc 33)
- Dato ricco di copertura (protezione/visibilità/idoneità) → Fase 1. Tactical Points → Fase 2. Rete
  di navigazione tattica + filtri navmesh per-ruolo → Fase 3. Settori/Combat Areas → Fase 4. Squadre
  AI → Fase 5. Overlay di visualizzazione completo → fase editor dedicata (Fase 0 fa solo groundwork).
- Indice spaziale: non ora (scansione lineare); il seam lo rende aggiungibile senza toccare i chiamanti.

### Consequences
- **Positivo**: seam unico creato **senza cambiare il comportamento** (nearestCoverToward ≡ pickCover);
  una sola verità sul pericolo; authoring direzionale abilitato. Tutto additivo, basso rischio.
- **Costo**: nuovi file (WorldIntel) + CMake; AiSystem chiama game/ (già lo fa: include Definitions.hpp).
- **Gate**: nessun campo dati nuovo → gate invariato.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `mini::worldintel`
(`game/ai/WorldIntel.hpp/.cpp`, aggiunto a CMake) con `nearestCoverToward` + `dangerAt`; `AiSystem`
usa il query layer (rimossa la static `pickCover`); `applyDangerRepulsion` gated a fallback
(`!navActive`); editor ruota/scala sui metadata (cover→`facing`, veicolo→`ry`, danger/post→`radius`).
Build 0/0; `--validate` 0/0; `--sim` senza crash, AI viva (state change nella telemetria), navmesh
costruito. **Manca smoke manuale editor**: ruotare un cover point e scalare una danger zone/post col
gizmo. Prossimo: Fase 1 (Cover Intelligence) — doc 33.

---

## ADR-026 — Cover Intelligence: copertura come dato tattico + auto-generazione (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Fase 1 del piano doc 33.** La copertura smette di essere "posizione+fronte+altezza" e diventa un
> dato tattico che permette a un'AI semplice di scegliere bene ("protegge ma limita la visuale").
> Prima applicazione della filosofia [[world-tactical-intelligence]] sul World Intelligence Layer
> (ADR-025). Additivo: le mappe esistenti restano identiche (default = comportamento attuale).

### Context
`CoverPointDef` oggi è solo `{x,y,z,facingDeg,height}` (doc 15): l'AI può solo prendere la copertura
più vicina che guarda il nemico — non può distinguere una buona copertura da una scarsa (problema #3
doc 33). E l'authoring è 100% manuale (problema #7): serve generazione automatica + correzione.

### Decision (Fase 1)
1. **`CoverPointDef` guadagna** (additivi, default = oggi): `float protection = 0.5` (0..1, quanto
   ripara) e `bool canShoot = true` (si può sparare/peekare da qui vs solo nascondersi). `facingDeg`
   e `height` restano. Loader: chiavi `protection`, `can_shoot` (clamp protection in [0,1]).
2. **Query layer**: `worldintel::nearestCoverToward` → **`bestCoverToward`** (scoring): fra le
   coperture entro `maxDist` che guardano verso il bersaglio, sceglie quella col **punteggio** più
   alto = `protection` pesata meno una penalità di distanza. Con protezione tutta a 0.5 (mappe vecchie)
   degenera nella "più vicina" → **retrocompatibile**. `AiSystem` usa la nuova query.
3. **Editor — dato ricco**: `CoverEntry` guadagna `protection`+`canShoot`; slider protezione +
   checkbox "spara da qui" nel pannello copertura; load/save (RMW).
4. ~~**Editor — auto-generazione** (bottone "Genera coperture da geometria")~~ — **RIMOSSA
   (2026-07-20, feedback utente).** L'euristica "un cover per faccia di box" produceva coperture
   senza senso e su mappe strutturate sarebbe un disastro. Una generazione *buona* richiede analisi
   di linea-di-vista/direzioni di minaccia/spaziatura — un approccio **diverso**, di cui il codice
   naive non era una base utile. Le mappe sono **fortemente handcrafted** → valore basso. Funzione e
   bottone eliminati. La generazione automatica di metadata dalla geometria è **de-scoped per ora**
   (doc 33 §6): se un giorno servirà, va rifatta con analisi tattica, non con euristiche sui box.
5. **Robustezza**: `protection` **clampata a [0,1] al load** (il valore runtime è sempre valido → nessuna regola gate separata necessaria).

### Out of Scope (Fase 1 → fasi successive)
- **Pose alle coperture** (crouch, mira-da-copertura, peek-over/around da `height`): **bloccate** su
  animazioni ([[animations-blocked]]). Si autora il DATO ora; l'esecuzione della posa dopo.
- **Idoneità per ruolo** + **link fra coperture** (grafo): servono la tassonomia ruoli e i Tactical
  Points → Fase 2. `canShoot` è autorato ora, consumo pieno (fuoco-da-copertura) più avanti.
- **Riduzione danno dietro copertura** in combattimento: integrazione CombatSystem → più avanti.
- Qualità auto-gen basata su LOS/spazio aperto: la v1 è euristica su facce dei box.

### Consequences
- **Positivo**: l'AI sceglie coperture *migliori*, non solo vicine (effetto osservabile); authoring
  molto più veloce (genera + rifinisci); il query layer incapsula il trade-off → l'AI resta semplice.
- **Costo**: schema + loader + editor + query. Tutto additivo, retrocompatibile.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `CoverPointDef` += `protection`
(clamp [0,1] al load) + `canShoot`; loader `protection`/`can_shoot`; `worldintel::bestCoverToward`
(scoring protezione−distanza) usato da `AiSystem`; editor `CoverEntry` += campi, slider protezione +
checkbox "spara da qui". L'**auto-generazione è stata rimossa** su feedback utente (vedi Decision §4).
Build 0/0; `--validate` 0/0; `--sim` senza crash, AI viva. Retrocompatibile (protezione 0.5 di default
→ "più vicina" come prima). **Manca smoke manuale editor**: regolare protezione/canShoot e verificare
il salvataggio. Prossimo: Fase 2 (Tactical Points) — doc 33.

---

## ADR-027 — Tactical Points: punti d'interesse tattici autorabili (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Fase 2 del piano doc 33.** La mappa può esprimere **posizioni tatticamente rilevanti** oltre alle
> coperture: punti sopraelevati, difensivi, di osservazione, strettoie. È il dato che gli strati
> Squad (Fase 5) e Comandante/settori (Fase 4) useranno per posizionare e dirigere. Authoring
> **manuale** (mappe handcrafted, doc 33 §6), niente auto-gen. Additivo.

### Context
Oggi la sola conoscenza tattica posizionale sono i cover point (riparo) e le danger zone. Manca il
concetto generale di **"posizione che conta"**: un tetto da cui dominare, un ingresso da presidiare,
un punto da tenere. Doc 33 §4.1 lo chiama Tactical Points. Serve il DATO ora (handcrafted), il consumo
pieno arriva con Squad/settori.

### Decision (Fase 2)
1. **`TacticalPointDef { x, y, z, facingDeg, type, importance, radius }`** su `MapDef.tacticalPoints`
   (array, chiave JSON `tactical_points`). `type` (stringa dal set editor): `vantage` (sopraelevato/
   dominante), `defensive` (da tenere), `chokepoint` (strettoia/ingresso), `observation` (osservazione).
   `importance` 0..1 (priorità), `radius` = area d'influenza (difensiva/chokepoint). Additivo, vuoto
   di default → zero impatto sulle mappe esistenti.
2. **Loader** (DefinitionRegistry): parse `tactical_points` (+ chiave nella whitelist). `importance`
   clampata [0,1].
3. **Editor**: lista + property panel (dropdown `type`, slider importanza/raggio/fronte/posizione) +
   marker nel viewport (colore per tipo + naso del fronte) + gizmo (sposta + ruota→facing). **Il fronte
   è editabile anche via slider** → l'authoring non dipende dal gizmo. Save RMW.
4. **Query layer**: `worldintel::nearestTacticalPoint(map, x, z, type, maxDist)` — il seam per i
   consumatori futuri (nessun consumo AI cablato ora: è groundwork per Fase 4/5).

### Out of Scope (Fase 2 → fasi successive)
- **Consumo** da AI/squadra/comandante: Fase 4 (settori) e Fase 5 (squad). Ora il dato è autorato e
  interrogabile, non ancora usato — come height/canShoot prima (authoring-ahead onesto).
- **Unificazione** cover ↔ tactical point in un'unica struttura: doc 33 §4.1 la prevede, ma rifattorare
  la Cover Intelligence funzionante è rischioso e senza guadagno ora → i due coesistono; unificazione
  = cleanup futuro. `TacticalPointDef` è per i punti NON-copertura.
- **Link fra punti** (grafo) + **visibilità/rischio calcolati**: Fase 3 (rete tattica).
- **Auto-generazione**: de-scoped (mappe handcrafted, doc 33 §6).

### Consequences
- **Positivo**: il designer può marcare la struttura tattica delle mappe handcrafted ORA, pronta per
  gli strati che la useranno; query seam unico (worldintel) → i consumatori futuri non riscansionano.
- **Costo/onestà**: dato autorato senza consumatore immediato (documentato). Rischio "metadato
  decorativo" mitigato dal fatto che è groundwork esplicito per fasi vicine e già interrogabile.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `TacticalPointDef` +
`MapDef.tacticalPoints`; loader `tactical_points` (whitelist + clamp importance); `worldintel::
nearestTacticalPoint` (seam, non ancora consumato); editor completo (`TacticalEntry`, load/save,
lista + / -, pannello con dropdown tipo + slider, marker colorati per tipo nel viewport, gizmo
sposta+ruota). Build 0/0; `--validate` 0/0 (firebase: "0 tactical", additivo ok); `--sim` senza
crash, AI viva. **Manca smoke manuale editor**: creare un tactical point, cambiarne tipo, salvare e
ricaricare. Consumo AI = Fase 4/5 (documentato). Prossimo: Fase 3 (rete di navigazione tattica).

---

## ADR-028 — Le pattuglie seguono la ROUTE, non un segmento (Fase 3a) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Fase 3 del piano doc 33, primo incremento.** Il pezzo con valore immediato e osservabile:
> oggi le route autorate sono in gran parte **sprecate**. Gli altri elementi della Fase 3 (filtri
> navmesh per-ruolo, grafo tattico fra Tactical Points, `purpose` delle route) restano pianificati
> ma NON in questo ADR — vedi Out of Scope.

### Context
`AiComponent` ha **due soli waypoint** (A/B): ConquestMode appiattisce tutte le `patrolRoutes` in
segmenti e ne assegna **uno** per unità (doc 18, limite documentato). Risultato: su firebase (16
segmenti autorati) ogni droide fa avanti-indietro su un singolo tratto e la **sequenza** del percorso
non viene mai percorsa. Il dato è autorato ma non produce il comportamento previsto — è il caso peggiore
di "metadato quasi decorativo".

### Decision (Fase 3a)
1. **`AiComponent` guadagna `patrolRoute` (indice route, -1 = nessuna) e `patrolSeg`** (segmento
   corrente). Additivo: `-1` → comportamento legacy A/B invariato.
2. **Avanzamento lungo la route**: quando l'unità completa il segmento corrente (arrivo, fine sosta,
   o stuck), invece di invertire A↔B passa al **segmento successivo** della sua route (wrap alla fine)
   e ricalcola A/B dai punti autorati. Nuovo helper `advancePatrol(ai, map)` in AiSystem: un solo
   punto di verità, sostituisce i tre `goingToB = !goingToB`. Senza route → inversione legacy.
3. **Assegnazione allo spawn**: ConquestMode assegna a ogni unità una **route** (round-robin) e un
   **segmento di partenza** diverso (le unità si distribuiscono lungo il percorso invece di ammassarsi).
   `RespawnEntry` porta i due campi → **anche i respawn** mantengono la loro route.
4. **Presidio vs manovra (rivisto 2026-07-20 su feedback).** Prima stesura: la direttiva del Droide
   Tattico vinceva sempre sulla pattuglia — ma con un comandante vivo questo annullava **tutte** le
   route e i percorsi autorati non venivano mai percorsi. Ora: le unità **con una route restano in
   pattuglia**; il comandante dirige la **forza di manovra** (unità senza route). Lo spawn divide la
   forza (metà su route, metà libera). Coerente col mondo: un comandante assegna l'obiettivo a una
   parte delle forze e lascia le altre a presidiare, non manda tutti sullo stesso punto.

### Out of Scope (Fase 3, incrementi successivi)
- **Filtri navmesh per-ruolo** (doc 22 li dà "pronti, non cablati"): incremento 3b.
- **Grafo tattico fra Tactical Points** (archi con esposizione/copertura/aggiramento): ha senso quando
  esistono i consumatori (Fase 4 settori / Fase 5 squad) → non ora, eviterebbe dato decorativo.
- **`purpose` delle route** (avanzamento/ritirata/flanking/rinforzo): additivo, ma senza consumatori
  distinti oggi ogni route è "pattuglia" → rimandato con il grafo.
- Waypoint per-unità arbitrari (lista libera): non serve, la route autorata è la sorgente.

### Consequences
- **Positivo**: le route autorate diventano finalmente **percorsi veri** (le pattuglie girano la mappa
  come progettato dal designer); le unità partono sparse lungo il percorso; costo nullo (2 int per AI,
  nessuna allocazione); i respawn conservano la route. Sblocca level design tattico sulle mappe handcrafted.
- **Costo**: tocca AiComponent, RespawnEntry, spawn di ConquestMode e il ramo Patrol di AiSystem.
  Tutto additivo e con fallback legacy (`patrolRoute = -1`).

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `AiComponent.patrolRoute/patrolSeg`;
helper `advancePatrol(ai, map)` in AiSystem (sostituisce i tre `goingToB = !goingToB`); `RespawnEntry`
+ `mkUnitWithMesh` portano route/segmento (respawn inclusi); `genPositions` assegna una route intera
per unità (round-robin) con segmento di partenza sfalsato; fallback legacy invariato quando non ci
sono route. Build 0/0; `--validate` 0/0; `--sim` senza crash, AI in pattuglia. Il presidio/manovra è
stato poi rivisto (§4) e l'obiettivo in Advance reso individuale (ADR-029/giro 24).

---

## ADR-035 — Manovra in combattimento: l'AI si riposiziona invece di stare ferma (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Prima fase AI**, dopo il completamento dei metadata. È il consumo che mancava: le query esistono,
> sono economiche, ma nessuno le usava *durante* lo scontro.

### Context
Il difetto strutturale: appena l'AI ha contatto diretto entra in `Alert` e **azzera l'approccio**
(`flankActive = false`). Da lì in poi strafa, fa peek/hide e usa la copertura solo come *nascondiglio*
in fase evasiva. Conseguenza: **tutto il lavoro sui metadata è usato solo PRIMA del contatto**, e la
battaglia degenera in due gruppi che si sparano da fermi — esattamente ciò che l'utente descrive come
"finto e meccanico". Le query pronte e mai chiamate: `bestFlankingPosition`, `bestOverwatchFor`,
`positionExposure`; `bestFiringPosition` solo in avvicinamento.

### Decision
1. **Riposizionamento tattico in combattimento.** Un'AI ingaggiata valuta **periodicamente** (timer
   sfasato dal `bias`, quindi non tutte insieme) se spostarsi in una posizione migliore:
   - **aggiramento** — `bestFlankingPosition` (peso ∝ `flank_chance`): colpire da un'altra direzione;
   - **posizione di tiro** — `bestFiringPosition` (peso ∝ `cover_preference`): sparare da coperto;
   - **restare** e combattere com'è ora (peso ∝ `aggression`).
   La scelta è pesata dal profilo: i profili del BalanceEditor governano *come* si manovra.
2. **Muoversi NON smette di combattere.** Durante il riposizionamento l'AI continua a mirare e
   sparare: si vincola il MOVIMENTO, mai il fuoco — lo stesso principio del guinzaglio-ordine
   (ADR-020) e della direttiva del comandante (ADR-024). Il tragitto usa il **pathfinding**
   (`requestMoveTarget` via il flag `orderTravel`), non lo steering diretto: deve aggirare gli ostacoli.
3. **Bounding overwatch EMERGENTE**: un tetto al numero di unità della stessa squadra che si
   riposizionano contemporaneamente. Alcune si spostano, le altre restano a fare fuoco — l'effetto
   "ci copriamo a vicenda" **senza macchinari di coordinamento**, coerente con "AI semplici in un
   mondo intelligente". Chi non può muoversi ora, si muoverà al prossimo ciclo.
4. **Quando NON ci si riposiziona**: in ritirata (sotto `retreat_hp_threshold`), con ordine
   `CoveringFire` ("stand and deliver", doc 26), se `stationary` (il comandante non lascia la sua
   posizione), o senza bersaglio.
5. **Fine manovra**: arrivo, perdita del bersaglio, o timeout — poi cooldown prima di rivalutare, così
   non oscilla.

### Out of Scope
- Coordinamento esplicito (assegnare chi aggira e chi copre): qui è **emergente** dal cap di
  concorrenza e dal `bias`. Il coordinamento negoziato è lo Squad layer (doc 33 Fase 5).
- Uso del grafo `positionCovers` per scegliere coppie overwatch precise: possibile evoluzione.
- Comandante che usa `sectorStates` per stance per-settore: incremento successivo.

### Consequences
- **Positivo**: il combattimento smette di essere statico; i metadata vengono finalmente usati *quando
  conta*; i profili diventano leve reali sul comportamento in mischia; l'effetto "alcuni avanzano,
  altri coprono" nasce da due regole semplici.
- **Rischio**: unità che si spostano troppo diventano bersagli facili e sembrano indecise → mitigato da
  cooldown, cap di concorrenza e pesi dal profilo. Va tarato provando.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `AiComponent` +=
`repositionTimer/Active/X/Z`; valutazione periodica in Alert (sfasata dal `bias`) fra **aggiramento**
(`bestFlankingPosition`, ∝ `flank_chance`) e **posizione di tiro** (`bestFiringPosition`, ∝
`cover_preference`), con soglia minima di 3 m per non sembrare indecisi; durante la manovra si viaggia
col **pathfinding** (`orderTravel`) **continuando a mirare e sparare**; cap di concorrenza per squadra
→ bounding overwatch emergente; esclusi ritirata, `CoveringFire`, `stationary`; `enterHunt` azzera la
manovra. Build 0/0; `--validate` 0/0; `--sim` 25 s: **33 cambi di stato** (erano 11 → AI molto più
attiva), **`stuck` 1** (le manovre non creano ingorghi), 0 errori.
**Da valutare in partita**: se il movimento risulta naturale o eccessivo. Leve: `flank_chance` e
`cover_preference` nei profili, i timer di rivalutazione, il cap di concorrenza.

---

## ADR-034 — Settori (Combat Areas): il livello su cui ragiona il comandante (M5) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Chiude il percorso metadata** (doc 33 §5-bis) e lo ricollega all'obiettivo iniziale: dare al
> Droide Tattico qualcosa su cui ragionare davvero.

### Context
Il comandante (ADR-024 v2) sceglie l'obiettivo guardando **solo l'owner dei command post**: un dato
binario e puntiforme. Non sa dove si sta combattendo, dove è in inferiorità, quale zona conta di più.
Doc 33 §4.4 identifica il livello mancante: **aree** con importanza, controllo, presenze e pressione.
È anche il livello che una futura simulazione fuori-visuale userebbe (doc 33 §9), quindi va tenuto
**astratto**: dati puri, nessuna dipendenza da entità vive o rendering.

### Decision
1. **`SectorDef` autorato** (`MapDef.sectors`): `label`, posizione, `radius`, `importance` (0..1).
   Aree circolari come danger zone e command post — coerenti con l'authoring esistente e sufficienti
   per mappe handcrafted. Autorate a mano: sono poche e sono **scelte di design**, non dati derivabili.
2. **Stato runtime `World::sectorStates`** (parallelo a `map.sectors`), ricalcolato ogni tick nel
   precompute di `AiSystem` con **una sola passata sulle entità**: `allies`, `enemies`,
   `controllingTeam` (0 = conteso/vuoto), `pressure` (0..1, quanto è realmente conteso). È **stato di
   partita**, quindi vive nel World come le altre mailbox, non nel MapDef.
3. **Il comandante ragiona sui settori**: l'obiettivo non è più "il post non-separatista più vicino"
   ma il **settore di maggior valore** — importanza, pressione, e non già saldamente controllato.
   L'annuncio nel feed nomina il settore.
4. **I droidi restano padroni del COME** (vincolo ADR-024 v2): in `Advance` ciascuno sceglie il post
   catturabile più vicino **dentro/vicino al settore-obiettivo** se ce n'è uno, altrimenti il più
   vicino a sé. Il comandante indirizza la forza su una zona; i singoli scelgono il punto.
5. **Senza settori autorati nulla cambia**: se `sectors` è vuoto il comandante torna alla regola
   precedente. Additivo, nessuna mappa si rompe.

### Out of Scope
- Simulazione fuori-visuale: qui si predispone soltanto (§9). `SectorState` è dato puro, riusabile.
- Pressione basata su danni/perdite reali: la v1 la deriva dalle **presenze contrapposte**, che è
  economico e già informativo. Raffinabile.
- Ordini per-settore (stance diversa per zona): il comandante ha ancora un intento globale.

### Consequences
- **Positivo**: il comandante passa da un dato binario puntiforme a una **lettura della situazione**;
  i settori sono pochi e autorati, quindi il costo di authoring è basso e il controllo del designer
  alto; la struttura è pronta per la simulazione futura.
- **Costo**: una passata per tick sulle entità (già se ne fa una nel precompute) + schema/editor.
- **Rischio**: settori mal autorati danno decisioni strane — mitigato dal fatto che sono pochi e
  visibili nel viewport.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `SectorDef` + `MapDef.sectors`
(loader + whitelist + clamp); `World::sectorStates` con `updateSectorStates` (una passata per tick,
solo truppe — strutture/veicoli/comandante esclusi); `pickObjectiveSector` (importanza + contesa,
salta le zone già saldamente separatiste); il comandante pubblica il **settore** come obiettivo
(x/z/raggio/label) e in `Advance` ogni droide sceglie il **proprio** post catturabile dentro quella
zona (fallback: il più vicino in assoluto) — direzione al comandante, punto al droide;
editor: lista + pannello (nome/area/importanza) + disco viola nel viewport, scala → raggio.
Build 0/0; `--validate` 0/0; `--sim` senza crash. **Senza settori autorati il comportamento è
invariato** (firebase non ne ha ancora: vanno autorati per vederne l'effetto).
**Chiude il percorso metadata** (doc 33 §5-bis).

---

## ADR-033 — Esposizione derivata e aggiramento: le corsie senza autorarle (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> Chiude il pezzo "corsie di avvicinamento / aggirare restando coperti" (doc 33 §5-bis) **senza**
> introdurre un nuovo tipo di dato da autorare a mano.

### Context
La richiesta è: *"aggirare… sparando da luoghi più coperti e tattici"*, con **corsie di avvicinamento
a bassa esposizione**. La strada ovvia — far disegnare al designer delle corsie — significherebbe
centinaia di dati a mano su ogni mappa: esattamente ciò che si è deciso di evitare (doc 33 §6).
Ma dopo ADR-032 esiste il **grafo "chi copre chi"**, e da lì due cose sono **derivabili**:
- **quanto un punto è esposto** = da quante posizioni può essere battuto;
- **da dove si attacca di fianco** = con che angolo si colpisce il bersaglio rispetto a chi lo
  ingaggia già.
Derivare da dati autorati a mano non è l'auto-gen fallita (che *inventava* posizioni dalla geometria):
qui il designer autora le posizioni e i settori, il resto si calcola.

### Decision
1. **`MapDef.positionExposure` (derivato, 0..1)**: per ogni posizione, quanto è esposta = frazione
   delle altre posizioni che la possono battere, ottenuta **invertendo** il grafo di ADR-032. Costo
   nullo (si riusa una computazione già fatta). Ricalcolato al load come il grafo → mai stale.
2. **`worldintel::bestFlankingPosition(map, fromX, fromZ, targetX, targetZ, threatX, threatZ, maxDist)`**:
   fra le posizioni che possono battere il bersaglio (settore + gittata + linea di tiro, ADR-031/032),
   sceglie quella che lo attacca **da una direzione diversa** da dove il bersaglio è già ingaggiato
   (`threat`) e che è **meno esposta**. Punteggio: angolo di fianco + protezione + (1 − esposizione)
   − distanza. È la corsia d'aggiramento, espressa come destinazione invece che come tracciato.
3. **Editor**: l'esposizione della posizione selezionata è mostrata **in sola lettura** ("quanto è
   allo scoperto"). È un dato derivato: non si autora, ma vederlo guida l'authoring — un punto molto
   esposto probabilmente non è una buona posizione di tiro.

### Out of Scope (e perché)
- **Corsie autorate a mano** (polilinee con esposizione per tratto): non servono finché il grafo dà
  lo stesso risultato senza costo di authoring. Se un giorno servirà controllo fine sul *percorso*
  (non solo sulla destinazione), si aggiungono allora — la porta resta aperta.
- **`purpose` delle route**: rimandato di nuovo. Senza consumatori distinti resterebbe decorativo, e
  la funzione "da dove attacco" è ora coperta dalle posizioni.
- **Consumo AI** (usare `bestFlankingPosition` per manovrare davvero): fase AI.

### Consequences
- **Positivo**: l'aggiramento diventa una **query**, non un dato da disegnare; zero authoring
  aggiuntivo; l'esposizione rende visibile al designer quali posizioni sono allo scoperto.
- **Limite dichiarato**: l'esposizione è misurata *rispetto alle posizioni autorate*, quindi riflette
  la qualità della copertura di quella mappa — su una mappa con poche posizioni è poco informativa.
  È un'euristica utile, non una verità fisica.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `MapDef.positionExposure` derivata
invertendo il grafo di ADR-032 (costo nullo, ricalcolata al load); `worldintel::bestFlankingPosition`
(angolo di fianco + protezione + basso-esposizione − distanza, con gittata/settore/linea di tiro);
editor mostra l'**esposizione in sola lettura** sulla posizione selezionata, calcolata con la **stessa
funzione del runtime** (regola in un posto solo, niente duplicazione). Build 0/0; `--validate` 0/0
(638 link / 60 posizioni, 1.2 ms); `--sim` senza crash. **Manca**: consumo AI di
`bestFlankingPosition` → fase AI.

---

## ADR-032 — Rete tattica: linea di tiro reale + grafo "chi copre chi" (M3+M4) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **M3 e M4 vengono fatti INSIEME**, per la stessa ragione che ha portato a unificare prima (ADR-030):
> il grafo dei link e il precalcolo della visibilità sono **la stessa computazione**. Farli separati
> significherebbe costruire un grafo geometrico (sbagliato attraverso i muri) e poi rifarlo.

### Context
- M1 (ADR-031) ha dichiarato un limite: il settore di tiro è **geometrico**, quindi una posizione può
  "battere" un bersaglio dietro un muro. Finché resta così, ogni scelta costruita sopra è inquinata.
- La richiesta "coprirsi a vicenda mentre si avanza" (bounding overwatch) richiede di sapere **quale
  posizione copre quale**. Con i settori di tiro già autorati, questa relazione è **derivabile** — non
  va autorata a mano (sarebbero centinaia di link) né inventata dalla geometria come la fallita
  auto-gen: si **calcola da dati autorati a mano**, che è tutt'altra cosa.
- L'utente ha scelto il **calcolo al load**: sempre coerente con la geometria, nessun dato da tenere
  aggiornato.

### Decision
1. **`worldintel::hasLineOfFire(map, ax,ay,az, bx,by,bz)`** — segmento contro i box `collider` della
   mappa (slab test nel frame locale del box, gestisce `ry`). Lavora su `MapDef`, non sul `World`:
   serve al load e in editor, dove il World non esiste. È il mattone che mancava.
2. **`bestFiringPosition` verifica la linea di tiro** → cade il limite dichiarato in ADR-031: una
   posizione non "batte" più un bersaglio attraverso un muro.
3. **Grafo "chi copre chi", calcolato al load**: per ogni posizione, l'elenco delle posizioni che
   **copre** (dentro settore + gittata + linea di tiro libera). Sta in `MapDef.positionCovers`
   (indici paralleli a `tacticalPositions`): **dato derivato**, non autorato e non salvato nel JSON —
   si ricalcola a ogni load, quindi non può diventare stale.
4. **Query di appoggio `bestOverwatchFor(map, fromX, fromZ, advanceX, advanceZ, maxDist)`**: la
   posizione di tiro raggiungibile che **copre il punto verso cui un compagno sta avanzando**. È il
   dato che rende possibile il bounding overwatch quando arriverà la fase AI.
5. **Costo**: O(n²) sulle posizioni (60 → 3600 coppie) × box collider, una volta al load. Da misurare
   e riportare; se su mappe grandi diventasse pesante, il rimedio è una griglia spaziale, non un
   cambio di modello.

### Out of Scope
- **Corsie di avvicinamento** (route con esposizione per arco): concetto distinto dai link fra
  posizioni, va con l'evoluzione delle `patrolRoutes` → incremento successivo.
- **Consumo AI** (bounding overwatch vero, avanzare a sbalzi): siamo in fase metadata; qui si produce
  il dato e la query. L'unica eccezione è il punto 2, che *corregge* un consumo già esistente.
- Visualizzazione dei link nell'editor: i link sono **derivati**, non autorati, quindi non servono per
  autorare; utili per verificare → follow-up.

### Consequences
- **Positivo**: le posizioni di tiro diventano corrette (niente muri); nasce la relazione che serve a
  coprirsi a vicenda; tutto precalcolato una volta invece che a runtime per ogni NPC — la filosofia
  "meccaniche pesanti spostate nel mondo" diventa concreta.
- **Costo/rischio**: il costo al load va misurato. Il grafo è derivato, quindi non può essere
  incoerente col resto — ma dipende dalla qualità dei settori autorati.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `worldintel::hasLineOfFire`
(segmento vs box collider, slab test nel frame locale, gestisce `ry`); `bestFiringPosition` ora
verifica la linea di tiro → **cade il limite geometrico di ADR-031**; `MapDef.positionCovers` (grafo
derivato) costruito da `buildTacticalLinks` al load; `bestOverwatchFor`. `WorldIntel.cpp` aggiunto
anche al target GFEditor (il loader è condiviso). Build 0/0; `--validate` 0/0; `--sim` senza crash.
**Costo misurato**: firebase **638 link su 60 posizioni in 2.4 ms**; outpost 2 link in 0.01 ms —
trascurabile. Scala O(n²·box): su una mappa molto più grande il rimedio è una griglia spaziale, non
un cambio di modello (previsto). **Manca**: consumo AI del grafo (bounding overwatch) → fase AI;
visualizzazione dei link in editor → follow-up; corsie di avvicinamento → incremento successivo.

---

## ADR-031 — Settore di tiro: la copertura diventa una posizione di combattimento (M1) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Il cambio che rende le coperture offensive** (doc 33 §5-bis, M1). Costruito su ADR-030, quindi il
> campo nasce una volta sola sulla posizione tattica unificata.

### Context
Oggi una copertura risponde solo alla domanda *"dove mi sottraggo al fuoco?"*. In `AiSystem`, al
contatto diretto si azzera l'approccio (`flankActive = false`) e la copertura serve unicamente nella
fase evasiva del ciclo peek/hide: è **un posto dove nascondersi**, mai **un posto da cui combattere**.
Manca il dato che permette la domanda opposta — *"dammi una posizione riparata da cui BATTO quella
zona"* — e senza quel dato nessuna intelligenza aggiuntiva potrà far attaccare o aggirare in modo
credibile (richiesta utente: "cover che non servono solo a nascondersi ma anche ad attaccare e
aggirare il nemico sparando da luoghi più coperti e tattici").

### Decision
1. **`TacticalPositionDef` guadagna il settore di tiro**: `fireArcDeg` (ampiezza del settore centrato
   su `facingDeg`, default 120°) e `fireRange` (gittata utile, default 25 m). Additivi: i default
   danno un settore ampio e generoso, quindi le posizioni già autorate restano utilizzabili.
2. **Nuova query `worldintel::bestFiringPosition(map, fromX, fromZ, targetX, targetZ, maxDist)`**:
   fra le posizioni raggiungibili entro `maxDist` restituisce la migliore che **può battere** il
   bersaglio, cioè: `canShoot`, bersaglio entro `fireRange`, e bersaglio **dentro il settore**
   (angolo con `facingDeg` ≤ `fireArcDeg/2`). Punteggio: premia la **protezione**, penalizza la
   distanza da chi cerca. È la domanda "posizione riparata da cui batto quella zona".
3. **Due domande distinte, due query** (la separazione concettuale è il punto):
   - `bestCoverToward` → *"dove mi riparo dalla minaccia"* (difensiva, ciclo peek/hide);
   - `bestFiringPosition` → *"da dove la colpisco restando coperto"* (offensiva, approccio/manovra).
4. **Consumo AI**: nella scelta dell'approccio (ADR-029) l'opzione "copertura" diventa **posizione di
   tiro**: non più una copertura qualsiasi rivolta verso il bersaglio, ma una da cui si può davvero
   fare fuoco su di esso. Il resto della logica AI resta invariato (siamo in fase metadata).
5. **Editor**: campi autorabili + **visualizzazione del settore** nel viewport **solo per la posizione
   selezionata** (con 60 posizioni disegnarli tutti sarebbe illeggibile). Senza vederlo, il settore
   non è autorabile con cura — ed è il dato più delicato di questa fase.

### Out of Scope
- **Visibilità reale precalcolata** (M4): qui il settore è geometrico (arco + gittata), non tiene conto
  degli ostacoli fra posizione e bersaglio. M4 aggiungerà la verifica vera; il settore resta il filtro
  economico di primo livello.
- **Riposizionamento in combattimento** (spostarsi su una posizione di tiro durante l'ingaggio): è
  lavoro della fase AI, non di questa.
- Link fra posizioni / overwatch (M3).

### Consequences
- **Positivo**: esiste finalmente il dato per attaccare da coperto; la query è economica (arco +
  distanza, nessun raycast); i due significati di "copertura" smettono di essere confusi.
- **Costo**: 2 campi + 1 query + editor. Additivo; con i default il comportamento resta simile a oggi.
- **Limite dichiarato**: un settore geometrico può includere bersagli dietro un muro finché M4 non
  aggiunge la visibilità — accettabile come primo livello, va detto nell'authoring.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `TacticalPositionDef` +=
`fireArcDeg`/`fireRange` (clamp al load, default ampi → le 60 posizioni già autorate restano valide);
`worldintel::bestFiringPosition` (canShoot + gittata + settore, punteggio protezione−distanza);
`AiSystem` usa la posizione di TIRO al posto della copertura generica nella scelta dell'approccio;
editor: slider Ampiezza/Gittata + **visualizzazione del settore** (due raggi gialli) sulla sola
posizione selezionata. Build 0/0; `--validate` 0/0 (60 posizioni); `--sim` senza crash.
**Manca smoke manuale**: selezionare una posizione nell'editor, vedere il settore giallo, regolarlo,
salvare e ricaricare. Prossimo: M3 (link fra posizioni + corsie di avvicinamento) — doc 33 §5-bis.

---

## ADR-030 — Una sola "posizione tattica": unificazione cover ↔ tactical point (M2) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> **Primo incremento del completamento metadata** (doc 33 §5-bis). Scelta dell'utente: **unificare
> prima**, poi aggiungere il settore di tiro (M1) — così il campo nuovo nasce una volta sola e la rete
> tattica (M3) e il precalcolo visibilità (M4) si costruiscono su una base unica invece che doppia.

### Context
Esistono **due tipi paralleli** che descrivono "un posto che conta":
- `CoverPointDef` (ADR-026): posizione, fronte, altezza, `protection`, `canShoot` — usata dall'AI.
- `TacticalPointDef` (ADR-027): posizione, fronte, `type`, `importance`, `radius` — non ancora consumata.
Erano stati tenuti separati di proposito (non rifattorare la Cover Intelligence funzionante), con
l'unificazione già prevista come cleanup (doc 33 §4.1). **Ora è il momento**: M3 (link fra posizioni)
e M4 (visibilità precalcolata) andrebbero altrimenti implementati due volte, e i link dovrebbero
collegare tipi diversi. Inoltre l'authoring è confuso: due liste per un concetto solo.

### Decision
1. **`TacticalPositionDef`** sostituisce entrambi. Campi: posizione, `facingDeg`, **`role`**
   (`cover` | `vantage` | `defensive` | `chokepoint` | `observation`), `height`, `protection`,
   `canShoot`, `importance`, `radius`. `MapDef.tacticalPositions` sostituisce `coverPoints` +
   `tacticalPoints`.
2. **Il ruolo è descrittivo, le CAPACITÀ sono nei campi.** Le query non filtrano per `role` ma per ciò
   che serve: una copertura è una posizione con `protection > 0`; una posizione di tiro (M1) sarà una
   con un settore di tiro. Così una `vantage` che ripara vale anche come copertura, senza casi speciali.
3. **Migrazione trasparente nel loader**: si legge `tactical_positions` e **anche** le chiavi legacy
   `cover_points` (→ `role: cover`) e `tactical_points` (→ `role` dal vecchio `type`). Le mappe
   esistenti funzionano **senza toccarle**. L'editor salva la chiave nuova e **cancella le legacy**:
   aprire+salvare una mappa la migra definitivamente. I file `data/maps/*.json` vengono migrati subito.
4. **Consumatori aggiornati** (impatto tracciato, CLAUDE.md §1.4): `DefinitionRegistry` (parse +
   whitelist + log), `worldintel` (`bestCoverToward` → `TacticalPositionDef`; `nearestTacticalPoint` →
   `nearestPositionByRole`), `AiSystem` (3 punti), **`NavManager`** (marcatura area COVER nel navmesh),
   `Application` (ordine contestuale TakeCover del giocatore), `MapEditor` (una sola lista/pannello).

### Out of Scope (arriva dopo, su questa base)
- **Settore di tiro** (M1): è il passo successivo — questa unificazione serve proprio a farlo una volta sola.
- **Link fra posizioni** (M3) e **visibilità precalcolata al load** (M4, scelta utente).
- Ruoli aggiuntivi o gerarchie di ruolo: si aggiungono come stringhe quando servono.

### Consequences
- **Positivo**: un solo concetto da autorare e da interrogare; M1/M3/M4 si costruiscono una volta;
  authoring più chiaro (una lista con un ruolo, non due liste).
- **Costo**: migrazione di schema che tocca 8 punti + i JSON mappa. Mitigato dalla lettura legacy nel
  loader (nessuna mappa si rompe) e dal fatto che i dati autorati sono pochi (firebase 17 cover).
- **Rischio**: se un consumatore restasse indietro leggerebbe una lista ormai vuota → l'impatto è stato
  tracciato prima di scrivere codice, e la build lo rileva (i campi vecchi non esistono più).

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `TacticalPositionDef` +
`MapDef.tacticalPositions` (rimossi `CoverPointDef`/`TacticalPointDef` e i due vettori); loader con
migrazione trasparente delle chiavi legacy; `worldintel` (`bestCoverToward` filtra per
`protection > 0`, `nearestTacticalPoint` → `nearestPositionByRole`); `AiSystem` (3 punti);
`NavManager` (marca COVER solo ciò che ripara); `Application` (ordine contestuale TakeCover);
`MapEditor` (una sola lista/pannello/marker, salva `tactical_positions` e cancella le legacy).
Build 0/0; `--validate` 0/0 con **60 posizioni migrate** su firebase e 4 su outpost; `--sim` senza
crash. **Nota sui dati**: i `data/maps/*.json` NON sono stati riscritti a mano — l'utente li sta
autorando attivamente e la migrazione è trasparente al load; la conversione definitiva del file
avviene al primo salvataggio dall'editor. **Manca smoke manuale**: aprire firebase nell'editor,
verificare la lista unica con le coperture migrate, salvare e ricaricare.

---

## ADR-029 — Approccio tattico all'ingaggio + personalità individuale (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> Primo passo **costruttivo** verso "AI più intelligenti" (dopo aver rimosso ciò che *impediva*
> l'indipendenza, giro 24). Vale per **entrambe le fazioni**: è lo stesso sistema, non due
> implementazioni separate (direttiva utente 2026-07-20).

### Context
Due difetti osservati in partita:
1. **All'ingaggio l'AI va sempre addosso.** Vede/riceve un contatto e punta la posizione nota. Non
   valuta le alternative che il mondo ORA offre — coperture con `protection` (ADR-026), tactical
   point `vantage` (ADR-027), aggiramenti — pur avendo già `flank_chance`, `cover_preference` e
   `aggression` nel profilo (autorabili nel BalanceEditor: l'utente vuole differenziare i profili).
2. **Decidono tutte allo stesso modo.** Stesso profilo + stessi input ⇒ stesse scelte: si muovono in
   gruppo anche senza nemici, e la squadra alleata si ammassa/immobilizza attorno al leader (peggiorato
   dal fix del giro 24 che, per togliere l'oscillazione, le faceva stare ferme). Risultato: AI
   "meccaniche e finte".

### Decision
1. **`AiComponent.bias`** — valore per-unità in [0,1), assegnato allo spawn (hash dell'entity id:
   deterministico ma diverso per ogni unità). È la **personalità**: rompe le parità, scaglia i tempi,
   sceglie lato e distanza degli aggiramenti. Senza questo nessuna variazione è possibile.
2. **Scelta dell'approccio all'ingaggio** (`enterHunt` → valutazione pesata). All'acquisizione del
   contatto l'unità costruisce le opzioni che il **mondo** offre e ne sceglie una **pesata dal
   PROFILO** (mondo intelligente + AI semplice):
   - **Diretto** (peso ∝ `aggression`);
   - **Aggiramento** laterale (peso ∝ `flank_chance`; lato e distanza 5-9 m dal `bias` → unità diverse
     aggirano da lati diversi);
   - **Copertura** che guarda il bersaglio (`worldintel::bestCoverToward`; peso ∝ `cover_preference`
     × protezione della copertura);
   - **Punto dominante** vicino al bersaglio (`worldintel::nearestTacticalPoint("vantage")`; peso ∝
     importanza + inclinazione tattica del profilo).
   Riusa il meccanismo esistente `flankActive/flankX/flankZ` (waypoint di approccio, poi prosegue
   sulla posizione nota) → nessuna macchina a stati nuova.
3. **La squadra si DISPONE invece di ammassarsi**: un membro in `Follow` entro il guinzaglio prende
   una posizione attorno al leader con angolo/raggio derivati dal `bias` (anello 3-5.5 m) invece di
   stare fermo. Toglie sia l'oscillazione sia l'immobilità.

### Out of Scope
- Coordinamento esplicito fra unità (chi aggira a sinistra vs destra concordato): qui la varietà è
  **emergente** dal bias, non negoziata. Il coordinamento vero è lo Squad layer (doc 33 Fase 5).
- Riscrittura della macchina a stati AI: si riusa Hunt + waypoint di approccio.

### Consequences
- **Positivo**: attacchi da più punti, uso reale dei metadata autorati, profili finalmente
  differenzianti (il BalanceEditor conta davvero), fine del "tutti la stessa decisione".
- **Costo**: 1 campo in AiComponent + logica di scelta in `enterHunt`. Additivo; con
  `flank_chance = cover_preference = 0` il comportamento resta quello diretto di prima.

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `AiComponent.bias` (hash entity id,
assegnato in `spawnUnit`); `enterHunt` valuta 4 opzioni (diretto / aggiramento / copertura via
`bestCoverToward` / punto dominante via `nearestTacticalPoint("vantage")`) con pesi dal profilo e
scelta decorrelata dal bias; membri in `Follow` prendono una posizione in formazione attorno al leader
(anello 3-5.5 m, angolo dal bias). Build 0/0; `--validate` 0/0; `--sim` 25 s senza crash, unità sparse
su tutti i quadranti, `stuck` 3. **Vale per entrambe le fazioni** (stesso AiSystem). **Da valutare in
partita**: naturalezza e varietà degli approcci; le leve sono i profili nel BalanceEditor
(`flank_chance`, `cover_preference`, `aggression`).

---

## ADR-036 — Le strutture strategiche sono oggetti SOLIDI, autorati e per fazione (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

> Retro-documenta la decisione presa nel changelog (33). Il numero era già citato in memoria e nei
> documenti: senza questo ADR restava un riferimento senza contenuto.

### Context
`StrategicTargetDef` (doc 25, `DestroyTarget`) nasceva come "obiettivo da distruggere" e basta. Nel
codice questo si traduceva in tre limiti concreti:
1. **Nessun `ColliderComponent`**: il bersaglio aveva Transform/Team/Health/MeshRenderer/Hitbox ma
   nessuna collisione — AI e giocatore lo attraversavano. Segnalato dall'utente sulla torre di
   comunicazione.
2. **`team` cablato a 2**: qualunque struttura nasceva **separatista**. Una torre dei CLONI era
   quindi impossibile da autorare, non per scelta ma per un letterale nel game mode.
3. **Nessuna rotazione né scala**: la struttura era piazzabile ma non orientabile né dimensionabile.

Il vincolo è arrivato dal design, non dall'ingegneria: le torri di comunicazione servono a
**entrambe le fazioni** (memoria `command-rank-system`), quindi la struttura strategica non può
essere "una cosa dei droidi".

### Decision
1. **La struttura strategica è una struttura solida**: `ColliderComponent` sempre, con semiassi
   autorabili (`half_x/y/z`); `0` = ricavati dalla scala della mesh. L'altezza usa il semiasse
   **pieno** per compensare l'offset di grounding della mesh.
2. **La fazione è un dato autorato**, non un letterale: `StrategicTargetDef.team` (1 Repubblica /
   2 Separatisti). Il game mode legge `t.team`.
3. **Orientamento e dimensione autorati**: `ry` + `mesh_scale`, con gizmo ruota/scala abilitato sui
   bersagli nell'editor (la stessa richiesta che l'utente aveva fatto per i metadata).

### Out of Scope
- **Effetto funzionale** della torre (accelerare/rendere più efficaci i comandi dall'alto): qui si
  costruisce solo il *corpo* autorabile. L'effetto è la torre di comunicazione vera e propria, che
  richiede il sistema di comando — vedi memoria `command-rank-system`.
- Geometria non-box (mesh Blender → collisione/navmesh/hitbox): registrato, non in questo ADR.

### Consequences
- **Positivo**: le strutture di entrambe le fazioni diventano autorabili con lo stesso schema; le AI
  ci sbattono contro invece di attraversarle (la copertura che offrono diventa reale); prerequisito
  sbloccato per torre di comunicazione e torre di controllo dei cloni.
- **Costo**: 5 campi additivi in `StrategicTargetDef` con default retrocompatibili — le mappe
  esistenti si caricano invariate (`team` default 2, semiassi 0 = dalla scala).

### Status
**Accepted — in force (2026-07-20).** Implementato e verificato: `StrategicTargetDef` con
`ry`/`team`/`halfX`/`halfY`/`halfZ`; `ConquestMode` spawna con `ColliderComponent` e legge il team
autorato; editor con pannello e gizmo ruota/scala. Build 0/0. **Manca smoke manuale**: autorare una
torre di team 1, verificarne in partita collisione e appartenenza.

> ⚠️ **Correzione 2026-07-21 — questo Status era in parte FALSO** (KI #71/#72). Lo smoke manuale
> mancante ha poi rivelato che: il gizmo ruota/scala **non** era abilitato sui bersagli (la barra
> permetteva Ruota/Scala solo per i box); il viewport li disegnava con rotazione 0 e lato fisso; il
> runtime **ignorava** `mesh_scale` col box di fallback; e il `ColliderComponent` **non rende una
> struttura solida per le AI**, che camminano sul navmesh — quello andava costruito includendo le
> strutture. Erano stati aggiunti i campi, non l'effetto. Corretto nel changelog (37).

---

## ADR-037 — Lo stato privo di ordini: le truppe sono indipendenti per default (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

### Context
`SquadSystem` imponeva un default: **chi non ha un ordine attivo riceve `Follow` sul leader**. Non
era una scelta di design, era un placeholder di Phase A rimasto in piedi. Le conseguenze misurate:
- La telemetria mostrava `sq_follow` 4-9 su 9 membri: **tutti i cloni permanentemente al guinzaglio**.
- Da lì discendeva quasi tutto ciò che l'utente riportava: "si muovono tutti insieme", "sempre le
  stesse strade", "finiscono tutti aggregati". Non erano difetti dell'AI: erano **l'ordine Follow**
  che faceva esattamente il suo mestiere su tutta la squadra, tutto il tempo.
- Era anche la causa della rianimazione troppo efficace (KI risolto): il Follow teneva i membri
  ammassati, quindi c'era sempre un soccorritore a portata.
- E rendeva i **cloni meno indipendenti dei droidi**, che non hanno squadra e quindi nessun
  guinzaglio — l'opposto della differenza di fazione voluta.

### Decision
1. **Nessun ordine di default.** Un membro senza ordine resta `OrderType::None` e ricade sul
   comportamento AI normale (Patrol/Alert/Hunt): si muove per la mappa come **truppa indipendente**.
   Questo è lo stato **normale**, non un fallback degradato.
2. **`Follow` diventa un ordine come gli altri**, impartito dal giocatore dalla **ruota di comando**
   (4° settore). Se la squadra sta già seguendo, lo stesso settore legge **LIBERI** e **revoca**
   l'ordine — il giocatore può sempre tornare allo stato indipendente.
3. **La revoca è un ordine valido**: `SquadOrderRequest` con `order == None` azzera l'ordine dei
   membri. Serve un ramo esplicito, perché il blocco di assegnazione filtra su `isImplemented()`,
   che `None` non soddisfa — senza il ramo la revoca sarebbe stata silenziosamente ignorata.
4. **L'HUD dichiara lo stato**: senza ordini la squadra legge `LIBERI`, non una riga vuota. Uno stato
   di design deve essere visibile, altrimenti si legge come un bug.

### Out of Scope
- **Maggiore indipendenza dei cloni rispetto ai droidi** oltre a questa: qui si rimuove il vincolo
  che li rendeva *meno* indipendenti. La differenza di fazione vera nasce dall'asimmetria
  torre di controllo (segnala) ↔ Droide Tattico (ordina) — prossimo passo, memoria
  `command-rank-system`.
- Comportamento di squadra autonomo (i membri che si coordinano fra loro senza il giocatore): è lo
  Squad layer, doc 33 Fase 5.

### Consequences
- **Positivo**: i cloni si distribuiscono sulla mappa invece di seguire il giocatore in fila; il
  Follow torna a essere una **scelta tattica** con un costo (li togli dal resto del campo) invece di
  uno stato ambientale; le manovre tattiche costruite in ADR-029/035 possono finalmente esprimersi su
  tutta la squadra, non solo sui droidi.
- **Costo/rischio**: il giocatore perde la scorta gratuita — se vuole compagni addosso deve
  chiederla. È voluto, ma va verificato in partita che non risulti "la squadra mi ignora". La leva è
  la ruota di comando, che ora ha un settore in più.

### Status
**Accepted — in force (2026-07-20).** Implementato: rimosso il default in `SquadSystem`; ramo di
revoca per `order == None`; 4° settore della ruota (`Application` angoli sulle diagonali + `Hud`
etichetta SEGUI/LIBERI con `wheelFollowActive`); HUD squadra che dichiara `LIBERI`. Build 0/0;
`--validate` 0/0.

**Misurato (`--sim` 10v10 su firebase, ~550 s):** `sq_follow` **0 per tutta la partita** (era 4-9);
a t=4 s `sq_senza_ordine` **10/10** e **21 unità in patrol contemporaneamente** — la squadra si
distribuisce invece di incolonnarsi. Al picco d'ingaggio (t=64 s): `in_alert` 9, `manovra_valutata`
6 → `manovre_avviate` 3, `tiro_trovato` 3, `fianco_trovato` 1 — i cloni ora **manovrano davvero**,
non solo i droidi. `stuck` costante ~4-5 per minuto, invariato rispetto a prima (non è una
regressione: prima erano 2-3 su finestre da 25 s).

**Da verificare in partita (smoke manuale)**: che la squadra senza ordini non risulti "che ti
ignora"; ruota di comando a 4 settori (SEGUI impartisce, LIBERI revoca); il toast e la riga HUD.

---

## ADR-038 — La torre di comunicazione degrada, non spegne (doc 34) (Accepted — in force, stato corretto nell'audit 2026-08-04; proposto 2026-07-20)

### Context
Direttiva dell'utente: *"le torri di comunicazione se distrutte non devono bloccare i rinforzi, però
senza torre di comunicazioni tutte le informazioni, gli ordini e i rinforzi vengono rallentati,
quindi anche per esempio il fatto che un'AI avverte gli alleati nelle vicinanze quando trova un
nemico, senza torre di comunicazione il raggio entro il quale può avvertire i suoi alleati è ridotto
e la comunicazione arriva in ritardo."*

Il codice esistente andava nella direzione opposta su due fronti:
1. **La conseguenza era un interruttore**: `block_enemy_reinforcements` azzera i rimpiazzi. Binaria e
   definitiva — una volta scattata la partita è decisa, non resa più difficile. E incoerente: perdere
   le comunicazioni non impedisce ai rinforzi di partire, li rende disorganizzati e in ritardo.
2. **La comunicazione non era modellata affatto**: `AI_CONTACT_SHARE_RADIUS` era una costante uguale
   per tutti e per sempre, e un avvistamento si propagava **istantaneamente** (i contatti erano
   ricostruiti da zero ogni tick). Non esisteva alcuna grandezza su cui una torre potesse agire.
   Anche la direttiva del comandante si ricalcolava **ogni tick**: un comando istantaneo, quindi
   impossibile da rallentare.

### Decision
1. **`StrategicTargetDef.role`** (`"generic"` | `"comms"`, whitelist nel loader): solo le strutture
   con ruolo `comms` alimentano la rete della loro fazione. Autorabile in editor con un **combo**, mai
   testo libero.
2. **`World::comms[team]`** (mailbox): `hadTower` / `towerAlive` + quattro moltiplicatori — raggio di
   condivisione, ritardo dell'informazione, periodo di decisione del comando, ritardo dei rimpiazzi.
   Scritto dal game mode (che possiede le strutture), letto da `AiSystem` e dal mode stesso.
3. **Regola di non-regressione**: si degrada **solo chi una torre l'aveva e l'ha persa** (`hadTower`).
   Una fazione senza torre autorata comunica normalmente → le mappe esistenti non cambiano.
4. **I contatti diventano persistenti e datati** (`SharedContact{x,z,team,age}`). Un'unità adotta un
   contatto solo nella finestra `[shareDelay, shareDelay + FRESH]`: con la torre viva la finestra è
   `[0, 1s]`, cioè in pratica gli avvistamenti correnti — **comportamento nominale invariato**. Senza
   torre la finestra **si sposta**, non si allarga: non si sa di più, si sa più tardi, e la posizione
   è quella di allora → si accorre dove il nemico **era**.
5. **Il comando acquista una cadenza** (`COMMAND_DECISION_PERIOD`, 3 s), che il degrado moltiplica.
   La **morte** del comandante resta rilevata subito: è un fatto, non un ordine.
6. **I rimpiazzi tardano** (moltiplicatore sul timer di respawn). **Mai bloccati.**

### Out of Scope
- Consequence type `degrade_comms`: **tolto in implementazione** — lo stato si ricalcola dal vivo
  dalle torri, quindi una conseguenza che scrivesse lo stesso campo creerebbe due sorgenti di verità
  che si sovrascrivono. Se servirà, va composto come modificatore separato (doc 34).
- Bonus attivo della torre sopra la baseline; riparazione; comunicazione per-unità; effetti sull'HUD
  del giocatore. Vedi doc 34 Out of Scope.
- **La torre di CONTROLLO** (coordinazione e visione d'insieme per i cloni) è un sistema diverso: la
  torre di comunicazione riguarda la **qualità del canale**, quella di controllo **cosa passa nel
  canale**. Non vanno fusi.

### Consequences
- **Positivo**: la comunicazione diventa una risorsa attaccabile e graduale; nasce una differenza di
  fazione reale e simmetrica (entrambe hanno una torre da difendere); il comando smette di essere
  onnisciente e istantaneo; e si stabilisce il principio "una struttura distrutta **degrada** una
  capacità, non la rimuove" per tutte le strutture future.
- **Costo**: i contatti ora persistono → serve deduplica (misurati **1066** contatti vivi prima di
  introdurla, ~112 dopo) e un TTL. Un tick di AI fa qualche confronto in più.
- **Rischio**: il degrado potrebbe risultare impercettibile o, al contrario, punitivo. I valori sono
  in `GameConfig.hpp` (`COMMS_LOST_*`) proprio per essere rifiniti provando.

### Status
**Accepted — in force (2026-07-20).** Implementato: `role` nello schema + loader (whitelist) +
editor (combo, marker `[COM]` in lista); `World::comms` con la regola `hadTower`; contatti persistenti
e datati con deduplica; cadenza di decisione del comando; moltiplicatore sul respawn. Build 0/0;
`--validate` 0/0.

**Misurato.** In `--sim` normale le torri **non vengono distrutte** (le AI non le prendono di mira e i
colpi vaganti non bastano in 160 s — vedi KI #70), quindi il ramo degradato è stato verificato su una
**mappa di prova usa-e-getta** con torri a 5 HP, poi rimossa:
- `strategic target destroyed` (team 2, `comms: true`) a t=29.8 s → da t=35 s in poi la telemetria
  segna `comms_droidi: "degradate"` mentre `comms_cloni` resta `"ok"`: il degrado è **per fazione**,
  come previsto, e non tocca chi la torre ce l'ha ancora.
- Rinforzi: `respawn in 6.4s (COMUNICAZIONI DEGRADATE)` contro i 4.0 s nominali — ×1.6, e i rimpiazzi
  **continuano ad arrivare**, che era il punto della direttiva.
- Contatti vivi: 1066 → **112** al picco dopo la deduplica, ~64-80 a regime.

**Da verificare in partita (smoke manuale)**: che il degrado si *senta* — compagni che non accorrono
da lontano e arrivano dove il nemico era, droidi che insistono su un ordine vecchio. I valori sono in
`GameConfig.hpp` (`COMMS_LOST_*`).

---

## ADR-039 — Le strutture sono un fatto tattico autorabile (doc 35) (Accepted — in force, implementato e verificato 2026-08-04; 2026-07-20)

### Context
KI #70: in `--sim` una torre da 300 HP **non veniva mai distrutta**, nemmeno piazzata dentro lo spawn
avversario — cadeva solo per fuoco vagante. Direttiva dell'utente: *"il modo in cui le AI si
relazionano con strutture e bersagli va autorato per bene, per adesso magari intanto crea il sistema
per farli interagire... e il sistema per autorare l'interazione, poi ci penso io ad autorarlo"*, con
la nota che **queste informazioni devono essere considerate anche dalla torre di controllo e dal
Droide Tattico**.

Indagando, il difetto non era di design ma **tre bug in fila**, ognuno dei quali da solo bastava a
rendere le strutture inattaccabili:
1. **`hasLineOfSight` non escludeva il bersaglio**: il collider aggiunto da ADR-036 **bloccava la
   visuale verso il centro della struttura stessa**. Regressione introdotta senza accorgersene.
2. **Si mirava all'origine del transform**, che per una struttura sta **a terra**: il segmento
   raschiava il collider del pavimento → LOS fallito comunque.
3. **Il controllo LOS al momento del tiro** aveva entrambi i difetti: dopo aver corretto la
   *selezione*, la telemetria mostrava 396 ingaggi per finestra e **zero danni** — le AI sceglievano
   la torre e poi non le sparavano.

### Decision
1. **`hasLineOfSight(from, to, world, ignore = 0)`** — un bersaglio non si occlude da sé.
2. **Punto di mira = CORPO del bersaglio** (`y + hy/2` se ha un collider), sia nella selezione sia
   nel tiro. Per un'unità, priva di collider, resta il transform: nessun cambiamento.
3. **Due campi autorati** su `StrategicTargetDef`: `priority` (0..1) e `engage_radius`
   (**0 = mai ingaggiata di iniziativa**, default). Editor: slider + spiegazione.
4. **Le strutture ESCONO dalla lista dei bersagli-unità.** Corretto il LOS, sarebbero state ingaggiate
   per semplice vicinanza come un soldato qualsiasi — scavalcando il raggio autorato e facendo sparare
   i droidi a un edificio invece che a chi gli spara addosso. Rientrano **solo** dal percorso
   opportunistico, e solo quando l'unità **non ha un bersaglio-unità**: una struttura non spara, e
   preferirla a chi ti sta uccidendo sarebbe stupido.
5. **`World::strategicTargets` è la sorgente unica di intel sulle strutture** (posizione, fazione,
   ruolo, priorità, raggio): la leggono l'AI, il comando nemico e — quando esisterà — la torre di
   controllo dei cloni. Nessuno se le ricostruisce per conto proprio.
6. **Il comando considera le strutture** nella scelta dell'obiettivo, pesate da `priority`, con un
   premio alla torre di comunicazione (è un moltiplicatore di tutta la fazione, non un edificio
   qualsiasi). Indica **dove**, non il singolo colpo.
7. **Le strutture non contano come truppe** nel rapporto di forze del comandante (stesso difetto di
   KI #61 sull'HUD): gonfiavano `nFoes` e falsavano la stance.

### Out of Scope
Ordine di squadra "distruggi quella struttura"; restrizioni per classe/ruolo; strutture che
reagiscono; danno anti-struttura differenziato; **autorazione dei valori sulle mappe reali** (l'utente
ci pensa lui — su firebase i campi restano ai default conservativi). Vedi doc 35.

### Consequences
- **Positivo**: una struttura diventa un obiettivo vivo per entrambe le fazioni, non solo un bersaglio
  che aspetta il giocatore; il sistema di comunicazione (doc 34) acquista un antagonista reale; e i
  livelli di comando presenti e futuri leggono **una sola lista**.
- **Costo/rischio**: `engage_radius` mal tarato può far deviare truppe su un edificio mentre il fronte
  cede. Mitigato dalla precedenza assoluta ai bersagli-unità e dal default `0`.
- **Non-regressione**: con `engage_radius = 0` ovunque (default) il comportamento delle mappe esistenti
  è invariato — tranne la correzione dei tre bug, che vale sempre.

### Status
**Accepted — in force (2026-07-20).** Build 0/0; `--validate` 0/0.

**Misurato** su una mappa di prova usa-e-getta (`engage_radius 45`, `priority 0.9`, HP 300 reali), poi
rimossa — su firebase i valori restano ai default:
- Il comandante droide sceglie **"Torre Comunicazioni Repubblica"** come obiettivo e lo mantiene:
  le strutture sono entrate nella sua lettura della situazione.
- Ingaggi opportunistici: **46** per finestra al picco (erano **0**: prima non era possibile).
- **Torre separatista distrutta dalle AI a t=41.5 s** — la prima volta che una struttura cade per
  fuoco mirato invece che vagante → `comms_droidi: "degradate"` da t=54 s. La catena
  struttura → ingaggio → distruzione → degrado della rete gira **end-to-end**.

**Da verificare in partita**: i valori giusti di `priority`/`engage_radius`, che sono authoring
dell'utente, non ingegneria.

---

## ADR-040 — La torre di controllo SEGNALA, il comandante ORDINA (doc 36) (Accepted — in force, implementato e verificato 2026-08-04; 2026-07-21)

### Context
Direttiva dell'utente: la torre di controllo dei cloni *"si ferma ad un livello più basso per
lasciare più indipendenza, al massimo può segnalare i vari possibili obiettivi, ma non indirizzare i
cloni in un punto specifico o dare direttamente ordini"*, mentre il Droide Tattico *"oltre a questo
dà anche ordini"*.

Dopo ADR-037 i cloni sono truppe indipendenti ma **cieche**: nessun equivalente della lettura della
situazione che il comandante dà ai droidi. La strada facile — dare anche a loro un comandante —
avrebbe reso le due fazioni la stessa cosa con modelli diversi, cancellando la differenza che la
direttiva chiede di costruire.

### Decision
1. **Due canali SEPARATI, mai fusi**: `World::enemyCommand` (un intento unico, droidi) e
   `World::allyIntel` (una **lista** di segnali, cloni). Non condividono struttura né codice: è la
   separazione a impedire che uno diventi l'altro per deriva.
2. **`role: "control"`** sulla struttura strategica. Senza torre viva, `allyIntel` è spento e i cloni
   restano puramente autonomi.
3. **Segnali, non destinazioni**: settori non saldamente tenuti (peso = importanza × pressione) e
   strutture nemiche vive (peso = `priority`, premio alla torre di comunicazione). Ogni segnale dice
   *"qui conta qualcosa"*, non *"vai qui"*.
4. **La scelta è del singolo clone**: seleziona un segnale **decorrelato dal proprio `bias`**, non il
   migliore, e poi decide **da sé** il punto dentro l'area. Due cloni con la stessa informazione
   vanno in posti diversi.
5. **Il gate resta sull'indipendenza**: il ramo vale solo per un clone **senza ordini e senza route**.
   Un ordine del giocatore ha sempre la precedenza; chi presidia una route continua a presidiarla.

### Out of Scope
Qualunque forma di ordine dalla torre (sarebbe un altro sistema, altro ADR); marker sull'HUD;
effetti sul giocatore; torre di controllo per i droidi (hanno il comandante); degrado graduale alla
perdita. Vedi doc 36.

### Consequences
- **Positivo**: la differenza fra le fazioni diventa **strutturale e osservabile** — i droidi si
  muovono come forza diretta, i cloni come soldati informati che decidono. E i cloni smettono di
  essere ciechi senza smettere di essere indipendenti.
- **Costo/rischio**: un sistema che *sembra* fare poco. La tentazione futura sarà farlo "funzionare
  meglio" facendo scegliere a tutti il segnale migliore — che ricostruirebbe un comando unico e
  annullerebbe ADR-037. La scelta decorrelata dal bias **è** la feature, non un'approssimazione.
- **Non-regressione**: nessuna torre di controllo autorata → `allyIntel` spento → comportamento
  identico a prima.

### Status
**Accepted — in force (2026-07-21).** Implementato: `role: "control"`; `World::allyIntel`;
`updateAllyIntel` + `pickAllySignal` in AiSystem; ramo di pattuglia per cloni senza ordini né route;
editor con combo a 3 ruoli e marker `[CTRL]`; telemetria `torre_controllo` / `segnali_cloni` /
`segnali_seguiti`. Build 0/0; `--validate` 0/0.

**Misurato** (`--sim` 10v10 su firebase, torre autorata in posizione **segnaposto**): torre
`attiva`, **2-6 segnali** pubblicati e seguiti dai cloni in pattuglia (`segnali_seguiti` è un conteggio
per-tick, non per-unità: dice che il ramo è esercitato, non quanti cloni). Navmesh `input_tris` 300 =
(22 box + **3 strutture**) × 12. `stuck` 1, nessun crash.

**Da verificare in partita**: che si *veda* la differenza fra le due fazioni, e che i cloni non
risultino dispersivi. La leva è il peso dei segnali; il vincolo da non toccare è la scelta
decorrelata dal bias.

---

## ADR-041 — Il Droide Tattico è un'entità a sé, non una classe (SUPERSEDED da ADR-044, 2026-07-21)

> **Piano, non ancora implementato.** Nasce dalla direttiva dell'utente (2026-07-21) e da tre
> osservazioni convergenti: il Droide Tattico è al tempo stesso una **truppa**, un **bersaglio** e un
> **coordinatore**, e forzarlo nel modello "classe" (ADR-023) lo gestisce male su tutti e tre i piani.
> Vedi memorie [[droide-tattico-concept]] / [[command-rank-system]].

### Context
Oggi il comandante è una classe con ability `command` (ADR-024), spawnata dal campo `MapDef.commander`.
Funziona per il gancio v0, ma la direttiva dell'utente ne chiarisce la natura, che la classe non
sa esprimere:
1. **Non combatte**: sta in un punto sicuro, si difende solo se attaccato direttamente. Una classe è
   fatta per combattere — tutto il suo apparato (arma, hitbox offensiva, ingaggio) qui è rumore.
2. **È un bersaglio** con un ruolo strategico, come le torri (doc 34/35): distruggerlo ha una
   conseguenza. Ma le strutture sono `StrategicTargetDef`, le classi no: due modelli per la stessa
   idea di "nodo che, eliminato, degrada il nemico".
3. **Ha un raggio di movimento limitato**: idealmente sta in una struttura dedicata e si muove solo
   in un piccolo raggio attorno ad essa. Le classi non hanno un guinzaglio spaziale autorato.
4. **Coordina come la torre di controllo, MA dà anche ordini** (doc 36 vs ADR-024): è il livello
   sopra la torre di controllo, non un suo gemello.

### Decision (piano)
1. **Migrare il Droide Tattico fuori da `class`**, in una definizione/entità propria. Resta una
   truppa (ha corpo, salute, si difende), ma la sua definizione vive accanto a strutture e bersagli,
   non fra le classi combattenti.
2. **Spawn dedicato autorato nel MapEditor**, con un **raggio di leash**: un'area circolare da cui non
   può uscire. Riusa e generalizza l'idea di spawn: posizione + raggio autorabili. Dentro il raggio
   si muove (per coprirsi, ripiegare su una struttura); fuori non va mai.
3. **Unificare il concetto di "nodo strategico"**: il comandante è un bersaglio come le torri —
   `World::strategicTargets` lo elenca già come intel (doc 35), la sua morte degrada il coordinamento
   (già oggi: `enemyCommand` si spegne). Il piano allinea l'AUTHORING a questo: lo si autora dove si
   autorano torri e bersagli, non nel roster.
4. **Un editor dedicato "Strutture & Comando"** (o pannello del MapEditor) come casa d'autorazione di:
   torri (comunicazione/controllo), bersagli strategici, spawn del Droide Tattico + leash, e i
   **parametri globali collegati** che oggi non hanno casa (i moltiplicatori `COMMS_LOST_*`, doc 34).
   Risolve la domanda aperta dell'audit (§E) su dove vivono quei parametri: non nel BalanceEditor
   dell'AI, ma accanto alle strutture il cui effetto governano.

### Out of Scope
- **La gerarchia dei gradi intermedi** ([[command-rank-system]]): il grado sotto il Droide Tattico
  (un ufficiale per ~N truppe, che interpreta le direttive e coordina il proprio gruppo) è un sistema
  a sé, successivo. Questo ADR prepara il vertice, non la catena.
- **Migrazione dati automatica** delle mappe esistenti: `MapDef.commander` resta letto come fallback
  finché la nuova forma non è in produzione (transizione documentata, come da CLAUDE.md).

### Consequences
- **Positivo**: il Droide Tattico smette di essere una classe che non combatte; l'authoring diventa
  coerente (nodi strategici tutti nello stesso posto); il leash spaziale diventa un dato di design
  autorabile invece che un `stationary` hardcoded; e i parametri della rete di comunicazione trovano
  una casa sensata.
- **Costo/rischio**: tocca ADR-023 (entità=corpo, classe=professione) e ADR-024. Va fatto con una
  transizione, non con un big-bang: prima la nuova definizione + spawn/leash, poi lo spostamento
  dell'authoring, poi la rimozione del fallback `commander`-come-classe.

### Status
**Proposed — Fase 1 IMPLEMENTATA (2026-07-21).**

**Fase 1 (fatta): spawn dedicato con raggio di leash, autorabile.** `CommanderSpawnDef.leashRadius`
(+ loader `leash_radius`); `AiComponent.leashX/Z/Radius`; lo spawn passa da `stationary=true` a un
**leash** quando il raggio è > 0 (0 = fermo, retrocompatibile). In `AiSystem`: (a) un comandante con
leash NON insegue obiettivi/segnali — tiene la sua area; (b) **clamp universale** prima
dell'esecuzione del movimento: qualunque cosa voglia fare, non esce dal raggio. Editor completo:
marker viola + disco del raggio, pannello (classe dal registry, posizione, slider raggio), gizmo
Sposta/Scala, selezione dal viewport, save RMW. **Misurato** (leash 6 su firebase): la deriva del
comandante dalla casa resta **0 → 5.5 → 6.0**, mai oltre il raggio — si muove per difendersi ma non
esce. Build 0/0; `--validate` 0/0. **Chiude anche l'item audit "UI del commander nel MapEditor".**

**Fasi successive:** **✅ migrazione fuori da `class` FATTA (ADR-044, 2026-07-22)**; **✅ stance v2
multi-settore FATTA (ADR-042)**; **✅ editor "Comando" FATTO (2026-07-22, changelog 50)** — tab nel
BalanceEditor che autora i CommanderDef (dropdown corpo/arma/AI, abilità, hp/tinta) e i parametri
`COMMS_LOST_*` (§4). Le strutture restano per-mappa nel Map Editor (sono istanze, non def globali).
**Resta**: il ruolo comando implicito nel tipo (raffinamento) e il **grado intermedio**
([[command-rank-system]]). Con questo il rework del Droide Tattico è sostanzialmente completo.

---

## ADR-042 — Comando nemico v2: più fronti insieme, stance per-settore (doc 32) (Accepted — in force, implementato e verificato 2026-08-04; 2026-07-21)

> Realizza la "Direzione v2" del doc 32, su direttiva dell'utente: il Droide Tattico deve essere
> coerente con ciò che è in Star Wars — **analizza la situazione, imposta priorità e impartisce ordini,
> anche più alla volta, gestendo più settori/fronti insieme e dividendo le truppe** — restando al
> livello delle RISORSE e del COORDINAMENTO, non del micro delle singole truppe.

### Context
La v1 (ADR-024) produceva **un** intento globale: un solo obiettivo, una sola stance calcolata dal
rapporto di teste `nDroids`/`nFoes`. A forze pari usciva quasi sempre "avanzata" (KI/audit): sembrava
leggere la situazione, in realtà **contava le teste**. E tutti i droidi convergevano sullo stesso
obiettivo — un solo fronte, quando una battaglia ne ha diversi.

### Decision
1. **`World::EnemyCommand` diventa una LISTA di direttive** (`Directive{x,z,radius,stance,weight,label}`)
   invece di un intento singolo. Il comandante gestisce **più fronti insieme**.
2. **Stance PER-SETTORE dal bilancio LOCALE** (`sectorStates.allies/enemies`): dove i droidi
   controllano ma sono pressati → **TIENI**; settori contesi o in mano nemica → **SPINGI**. La stance
   varia per settore e nel tempo — fine del "sempre avanzata".
3. **Il comandante CONCENTRA**: valuta tutti i settori + le strutture nemiche, li pesa (importanza ×
   contesa + priorità autorata), e tiene i **top-K (=3)** fronti. Non disperde su tutto.
4. **I droidi si DISTRIBUISCONO sui fronti** (`pickEnemyDirective`, scelta pesata ma decorrelata dal
   `bias`, stesso meccanismo della torre di controllo): la forza si divide invece di convergere. Ogni
   droide segue la **stance del suo fronte** ma sceglie da sé il punto/percorso/ingaggio.
5. **Ripiegamento GLOBALE come override**: se i droidi vanno in netta inferiorità (≤ 50% dei cloni),
   un'unica direttiva Retreat verso lo spawn. È l'unica decisione che resta globale — ed è giusto che
   lo sia.
6. **Fallback** senza settori autorati: una singola direttiva sul post non-separatista più vicino
   (comportamento v1) → le mappe senza settori funzionano come prima.

### Out of Scope
- **Saturazione hard dei fronti** (un fronte "pieno" smette di attirare): la distribuzione pesata dal
  bias già divide; se in playtest i droidi si ammassano si aggiunge (come KI #73 per i cloni).
- **Il grado intermedio** che interpreta le direttive e coordina il micro del suo gruppo
  ([[command-rank-system]]): resta il livello sotto, futuro. Questo ADR è il vertice.
- **Migrazione del comandante fuori da `class`** (ADR-041 fasi successive): indipendente da questo.

### Consequences
- **Positivo**: il Droide Tattico gestisce davvero una battaglia su più fronti con posture diverse; la
  stance smette di essere un termostato sul conteggio; la forza si divide. È percepibilmente "un
  comandante che ragiona" invece di "tutti addosso allo stesso punto".
- **Costo**: `enemyCommand` da POD a lista + un helper di scelta. Superficie contenuta (solo AiSystem
  + World). I lettori del vecchio `.stance/.x/.z` sono stati riscritti.
- **Rischio**: senza saturazione i droidi potrebbero pesare troppo sul fronte top; da verificare.

### Status
**Accepted — in force (2026-07-21).** Build 0/0; `--validate` 0/0. **Misurato** (`--sim` 10v10):
il comandante gestisce **3 fronti**; le posture variano nel tempo — a t=33 s **2 AVANZATA + 1 TIENI**
insieme (tiene "Settore Enemy 2" mentre spinge altrove), l'obiettivo prioritario cambia (Ally 1 →
Enemy 2 → Charlie), e a t=73 s scatta il **RIPIEGAMENTO globale** quando i droidi calano sotto metà.
`stuck` 7 (varianza normale). **Da valutare in partita**: distribuzione della forza fra i fronti e
naturalezza; le leve sono i pesi dei settori e la soglia di ripiegamento.

---

## ADR-043 — Bilanciamento globale data-driven: `data/config/gameplay.json` (Accepted, 2026-07-21)

### Context
L'audit (doc 37 §E) aveva contato **17 costanti di gameplay** introdotte fra ADR-035 e ADR-040, tutte
`constexpr` in `GameConfig.hpp`, **zero autorabili**: per tarare la rianimazione bisognava
ricompilare. Le case dei dati erano già state decise con l'utente (doc 37/06): rianimazione base +
degrado comunicazioni → **globali autorabili**; `hunt_timeout` → nei profili AI (già fatto); soglie
tecniche dei contatti → **non esposte**.

### Decision
1. **`data/config/gameplay.json`** + `mini::GameplayBalance` (header-only,
   `include/mini/game/data/GameplayBalance.hpp`): struct con i **default = vecchie costanti**, load
   all'avvio che sovrascrive **solo le chiavi presenti**. File assente o invalido → default →
   comportamento invariato (non-regressione per costruzione).
2. **Migrati 10 parametri**: i 6 della squadra/rianimazione (`squad_bleedout_time`,
   `squad_revive_radius`, `squad_revive_time`, `squad_revive_hp`, `squad_down_lethal_hit_frac`,
   `squad_max_revives`) e i 4 del degrado comunicazioni (`comms_lost_*`). Le costanti in
   `GameConfig.hpp` sono state RIMOSSE (restano commenti-puntatore): una costante morta ma compilabile
   è una trappola.
3. **Tab "Gameplay" nel BalanceEditor**: slider con spiegazioni, salvataggio via `saveJsonRMW`,
   ripristino default. Stessa load del runtime → stessi valori.
4. Header-only con `inline` + static locale: **nessuna modifica a CMake**, nessuna nuova dipendenza
   fra i due binari (ADR-002 rispettato — condividono il FILE, non il codice di uno dei due).

### Out of Scope
- **Rianimazione per-classe** (medico): il layer globale è la base; il per-classe sarà un
  moltiplicatore sopra, con il sistema classi.
- Hot-reload in partita: si carica all'avvio (il tab lo dichiara: "salva e riavvia la partita").
- Le altre costanti di GameConfig (fisica, AI interne): restano compile-time di proposito.

### Consequences
- **Positivo**: la taratura chiesta dall'utente ("rendere tutto il più autorabile possibile") smette
  di richiedere una ricompilazione; runtime ed editor leggono la stessa fonte.
- **Trappola scoperta verificando** (e ora in memoria): `getDataPath()` del runtime **preferisce la
  `data/` SORGENTE** (3 livelli su dall'exe) e usa la copia accanto all'exe solo come fallback.
  Editare la copia in `build/.../Debug/data/` non ha alcun effetto quando si lancia dalla build tree.
  Tre run di test sprecate prima di capirlo — la diagnosi iniziale (BOM) era sbagliata.

### Status
**Accepted — in force (2026-07-21).** Build 0/0; `--validate` 0/0. **Verificato end-to-end in modo
deterministico**: `squad_max_revives = 0` nel JSON sorgente → log `max_revives=0` → **zero** eventi
`member downed/revived` in 110 s di sim (chi cade muore); ripristinato 1 → baseline identica
(8 downed / 5 revived / 3 bled out, tutti a `revives_used: 1`). **Manca smoke manuale**: il tab
Gameplay nell'editor (slider → salva → riavvia partita → effetto).

---

## ADR-044 — Il Droide Tattico è un CommanderDef, non una classe (Accepted, 2026-07-22)

> Realizza la **Fase 2 di ADR-041**: la migrazione del comandante fuori dal sistema classi. Chiude la
> parte più architetturale del rework del Droide Tattico.

### Context
Il Droide Tattico viveva in `data/classes/Tactical Droid.json`. Ma una **classe è una professione
istanziabile su più corpi** (ADR-023): Trooper, Heavy, Sniper. Il comandante non è nulla di ciò — è
un'**unità UNICA a ruolo strategico** (uno per mappa), che **non combatte** (si difende soltanto),
sta al sicuro, dà ordini, e ucciderla ha una conseguenza (come una torre). Forzarlo in `class` aveva
tre effetti concreti: (a) compariva nel roster delle classi giocabili (la sandbox spawna tutte le
classi → il comandante come truppa); (b) l'authoring era in mezzo alle professioni; (c) il suo essere
comandante dipendeva da un'ability "command" su una classe, non dal suo tipo.

### Decision
1. **Nuovo tipo `CommanderDef`** (`data/commanders/<id>.json`): `name`, `base_entity` (il CORPO da cui
   prende mesh/hitbox/proiettile), `self_defense_weapon`, `ai_profile`, `abilities`, `hp` **assoluti**,
   `speed_mult`, `mesh_scale`, `color_mult`, `team`. Loader + accessor nel registry, come le altre
   definizioni.
2. **Riuso del corpo, niente duplicazione**: `resolveCommanderArchetype` delega a
   `resolveUnitArchetype(base_entity)` per il corpo e vi sovrascrive gli override del comandante. La
   risoluzione di arma/proiettile/profilo resta l'unica esistente.
3. **`MapDef.commander.unit` referenzia un CommanderDef**; editor con dropdown da `data/commanders/`.
4. **Fallback documentato (transizione, CLAUDE.md)**: se `commander.unit` è ancora una classe
   con ability "command", lo spawn e la validazione la accettano. Rimosso `data/classes/Tactical
   Droid.json` una volta migrate le due mappe (firebase, Training Ground) → il fallback resta per
   sicurezza ma non è più esercitato.
5. **Validazione** (ADR-018): il gate accetta un CommanderDef (base_entity valido + ability "command")
   o una classe-comandante legacy; altrimenti errore/warning con rimedio.

### Out of Scope
- **Ruolo comando IMPLICITO** (senza ability "command"): per ora il CommanderComponent arriva ancora
  dall'ability, come nel modello classe — comportamento identico, rischio zero. Renderlo implicito nel
  tipo è un raffinamento successivo (richiede lo spawn che ritorna l'EntityId).
- **Entità-a-sé completa** (corpo proprio invece del `base_entity` B1): il CommanderDef riusa un corpo
  esistente; un modello dedicato è futuro (dipende dal tooling mesh dell'utente).
- **Editor "Strutture & Comando" dedicato** (ADR-041 §4): il comandante si autora nel MapEditor
  (pannello già esistente); i parametri globali `COMMS_LOST_*` vivono per ora nel tab Gameplay (ADR-043).

### Consequences
- **Positivo**: il comandante esce dal roster delle classi (non più spawnabile come truppa in
  sandbox); l'authoring è coerente col suo ruolo; il tipo è espandibile con parametri commander-specifici
  senza toccare il sistema classi. La macchina del corpo non è duplicata.
- **Costo/rischio**: un nuovo tipo di definizione + loader + gate + un path di spawn. Contenuto dal
  fallback e dal riuso di `resolveUnitArchetype`.

### Status
**Accepted — in force (2026-07-22).** Build 0/0; `--validate` 0/0. **Verificato end-to-end**: il
comandante spawna da `data/commanders/tactical_droid.json` (registry: "Commander: tactical_droid"),
**dirige** (`cmd_fronti` 3), resta nel **leash** (`cmd_deriva_m` 1.0 ≤ raggio 2), e continua a farlo
**dopo la rimozione della classe** dal roster. firebase e Training Ground migrate. **Manca smoke
manuale**: autorare/cambiare il comandante dal MapEditor (dropdown da commanders/).

---

## ADR-045 — Route fluide e obbedienti al comando (Accepted, 2026-07-22)

> Realizza P1+P2 dell'audit doc 38: le due frizioni più grosse fra route e resto dei sistemi. È
> consolidamento (far lavorare insieme pezzi esistenti), non un sistema nuovo.

### Context
Due problemi verificati (doc 38 C1/C2):
1. **Le route ignoravano il comando**: il ramo che leggeva il comandante/torre richiedeva
   `patrolRoute < 0`, quindi **metà** della forza (quella con una route autorata) era **sorda** alle
   direttive — su un binario fisso mentre il resto manovrava.
2. **Le route erano rigide** (osservazione dell'utente): `advancePatrol` faceva `(seg+1) % segCount`,
   solo in avanti, ciclico con un salto-teletrasporto dal fondo all'inizio; l'unità non poteva
   raccogliere una route dal punto più vicino, percorrerla al contrario, né cambiarla.

### Decision
1. **Il comando SOVRASCRIVE la pattuglia (P1)**: `Advance`/`Retreat` valgono ora per **tutti**, route
   o no. `Hold` (o nessun comando) → si pattuglia. Sblocca la metà della forza prima sorda. Vale per
   i droidi (direttiva del comandante) e per i cloni (segnali della torre, che ora raggiungono anche
   i cloni su route).
2. **Route bidirezionali (P2)**: `advancePatrol` percorre il tracciato avanti **e indietro**
   (`patrolReverse`), invertendo agli estremi — niente più salto-wrap. `patrolSeg` è ora l'indice del
   PUNTO-obiettivo, non del segmento.
3. **Raccolta dal punto più vicino (P2)**: `joinNearestRoute` aggancia la route più vicina dal punto
   più vicino (non solo dagli estremi). Le route diventano una **rete condivisa**.
4. **Cambio route (P2)**: uscendo da Search verso Patrol l'unità si sgancia (`patrolRoute = -1`) e
   rientrando raccoglie la route più vicina — così cambia tracciato invece di tornare al suo di
   partenza.

### Out of Scope
- **Punti di join intermedi con proiezione sul segmento** (agganciarsi esattamente al punto più
  vicino SU un segmento, non al vertice): qui ci si aggancia al vertice più vicino, sufficiente.
- **`purpose` delle route** (pattuglia vs assalto vs ripiego): resta futuro (doc 33).
- P3 dell'audit (ruoli decorativi, cover-evita-danger): prossimo giro.

### Consequences
- **Positivo**: metà forza non è più sorda al comando; le route sono percorse in modo naturale e
  condiviso; la varietà aumenta (versi e agganci diversi). Le route e il comando ora sono **un solo
  sistema** invece di due mondi.
- **Costo/rischio**: restructure di un ramo centrale (pattuglia). Contenuto: la logica di comando è la
  stessa di prima, solo senza il gate `patrolRoute < 0`; la route è la stessa, con verso + join.

### Status
**Accepted — in force (2026-07-22).** Build 0/0; `--validate` 0/0. **Misurato** (`--sim` 10v10):
`su_route` **5-10** unità agganciate a una route lungo tutta la partita (raccolta/cambio dinamici);
il comando sovrascrive (a t=93 s stance "Ripiegamento" → le unità lasciano le route); `cmd_fronti` 3,
obiettivo che varia; `stuck` 9 (varianza normale, nessuno spike dal restructure). **Da verificare in
partita**: che le route si vedano percorrere avanti e indietro e che i droidi su route rispondano
davvero all'avanzata (leggibile: durante Advance le unità puntano l'obiettivo invece del tracciato).

## ADR-046 — Ai ruoli tattici il loro comportamento; la copertura evita il pericolo (Accepted, 2026-07-22)

> Realizza P3 dell'audit doc 38: dà significato ai tre ruoli tattici finora decorativi, chiude
> l'ultimo dato inerte (`dangerAt`) e rimuove il codice morto residuo. È consolidamento, non un
> sistema nuovo — nessun nuovo schema, solo query e rami che leggono metadata già autorabili.

### Context
Tre attriti verificati (doc 38 B1/B2/B3):
1. **Tre ruoli decorativi (B1)**: `defensive`, `chokepoint`, `observation` si autoravano nell'editor e
   si disegnavano nel viewport, ma **nessun ramo dell'AI li leggeva**. Cover e vantage avevano
   comportamento; questi tre no — metadata a costo zero di manutenzione ma senza effetto.
2. **`dangerAt` inerte (B2)**: la query esisteva ed era testata, ma **nessuno la consumava**. Le danger
   zone si disegnavano ma non spostavano una sola decisione: l'AI poteva scegliere una copertura in
   piena zona di fuoco/mine.
3. **Codice morto (B3)**: `bestOverwatchFor` (variante vecchia non-Position, superata da
   `bestOverwatchForPosition`) e `pickObjectiveSector` (residuo del comando v1, superato da ADR-042)
   erano non referenziati — rumore che genera warning e confonde chi legge.

### Decision
1. **`observation` → vista estesa**: un'unità entro 10 m da una posizione `observation` vede più
   lontano (`aggroRange × 1.5`). Il punto di osservazione diventa un **moltiplicatore di sensori
   locale** — chi lo presidia ingaggia prima. Telemetria `obs_vista_estesa`.
2. **`defensive`/`chokepoint` → posizioni da tenere**: nuova query `bestHoldPosition(map, x, z, area…)`
   e nuovo ramo: quando il comandante emette `Hold` su un settore, le unità di quel fronte puntano la
   miglior posizione difensiva/di strozzatura dentro l'area (protezione + importanza, vicina, **fuori
   dalle danger zone**) invece di fermarsi su un punto qualunque. I due ruoli ora **significano**
   "presidiami". Telemetria `hold_su_posizione`.
3. **La copertura evita il pericolo (chiude B2)**: `bestCoverToward` e `bestFiringPosition` sottraggono
   ora `dangerAt(map, c.x, c.z)` dal punteggio. A parità di protezione l'AI preferisce la copertura
   **fuori** dalla zona pericolosa. Sempre attivo, non solo sotto comando. `dangerAt` non è più inerte.
4. **Pulizia (B3)**: rimossi `bestOverwatchFor` (da .cpp e .hpp) e `pickObjectiveSector`.

### Out of Scope
- **Pesare `dangerLevel` per tipo** (fuoco vs mine vs artiglieria): oggi è uno scalare unico, basta.
- **`observation` che alimenta l'intel di squadra/comando** (vedo un nemico → lo segnalo al fronte):
  qui la vista estesa è solo locale a chi presidia. Estensione naturale futura (doc 33).
- **`chokepoint` che genera comportamento di imbottigliamento attivo** (attirare il nemico dentro):
  qui è solo una posizione da tenere. Resta futuro.

### Consequences
- **Positivo**: i cinque ruoli tattici hanno ora tutti un comportamento; le danger zone spostano
  davvero le decisioni; `dangerAt` è vivo; meno codice morto. I metadata autorabili nell'editor e la
  loro lettura dall'Ani sono di nuovo allineati (nessun campo "che non fa niente").
- **Costo/rischio**: minimo — tre query pure e tre rami di lettura, nessun cambio di schema né di save.
  Il ramo `Hold` scatta solo quando il comandante tiene un settore, quindi in un 10v10 bilanciato si
  osserva di rado (vedi Status).

### Status
**Accepted — in force (2026-07-22).** Build 0/0; `--validate` 0/0. **Misurato** (`--sim --map
firebase`, che ha 3 punti `observation` e ruoli difensivi): `obs_vista_estesa` **333–1069** per
finestra — la vista estesa scatta in continuazione per chi passa vicino ai punti di osservazione. C3
(cover-evita-danger) è sempre attivo e verificato per build. `hold_su_posizione` era **0** in questa
partita perché il comandante emetteva `Hold` di rado.
**Aggiornamento 2026-07-22 (changelog 60)**: il `Hold` è stato reso più frequente e coerente (scatta
sugli obiettivi catturati e minacciati) e, soprattutto, il ramo droide è stato reso efficace anche
DURANTE il combattimento (opzione A): un droide in TIENI si àncora alla posizione difensiva/chokepoint
(`holdX/Z/Radius` + clamp) e ci combatte da lì senza inseguire. **`hold_su_posizione` ora scatta**
(318/225 su Training Ground quando il Hold è su un obiettivo con chokepoint) — i ruoli `defensive`/
`chokepoint` sono finalmente attivi. Combattimento sano, `fermi=0`, nessuna passività.
**Nota harness**: le misure `--sim` con id mappa contenente spazi vanno quotate ([[powershell-quote-args-with-spaces]],
KI #77): una prima misura su "Training Ground" girava su un'altra mappa per via dello spazio non quotato.

---

## ADR-047 — La geometria a BOX è la verità tattica; Blender fornisce il visivo (Accepted — in force 2026-08-05; 2026-07-27)

### Context
Le mappe future saranno prodotte con un flusso ibrido (editor + Blender). La tentazione naturale è
importare le mesh Blender come geometria del livello. Verificato sul codice (2026-07-27): `MapGeometryBox`
è **solo primitive** (nessun `meshPath`); il **navmesh si costruisce dai box collider**
(`NavManager`: "box collider → triangle soup"); `worldintel::hasLineOfFire` è uno **slab test sui box**.
La LOS è il costo dominante dell'AI ed è veloce *proprio perché* la geometria è analitica.

### Decision
La geometria di collisione, di navigazione e di analisi tattica resta **a box**. Le mesh importate da
Blender sono **esclusivamente visive** e non entrano mai in collisione, LOS o navmesh. Ogni asset esterno
porta con sé un **proxy di collisione** composto da box.

### Consequences
- **Positivo**: si preservano LOS analitica veloce, navmesh pulito, editing immediato e tutta l'analisi
  tattica esistente (grafo coperture/esposizione, visuale verticale). È il pattern AAA standard
  (visual mesh + collision proxy).
- **Costo**: l'autore deve fornire il proxy (poche box) accanto alla mesh. Costo reale basso, e
  ripagato dal fatto che il proxy È anche il modello tattico.
- **Rischio**: la scorciatoia "usiamo la mesh anche per la collisione, tanto per provare" degraderebbe
  LOS e navmesh in modo difficile da tornare indietro → va impedita con un controllo in `--validate`.

### Status
**Accepted — in force (2026-08-05).** La condizione dichiarata ("quando la pipeline prefab sarà
implementata e verificata") è soddisfatta da **ADR-048**, Accepted dal 2026-08-04. Da allora la
decisione è stata anche **usata come vincolo** in due punti: ADR-053 respinge il pitch sul box e la
collisione a mesh proprio perché romperebbero lo slab test analitico, e le rampe si risolvono per
scalettatura per la stessa ragione. Era rimasta Proposed per inerzia mentre il resto ci si appoggiava
sopra — trovata nel controllo di coerenza del 2026-08-05.

**Resta aperto** il controllo in `--validate` contro la scorciatoia "uso la mesh anche per la
collisione": oggi nessun gate lo impedisce.

---

## ADR-048 — Il significato tattico si autora per ASSET, non per ISTANZA (prefab) (Accepted — in force, implementato e verificato 2026-08-04; 2026-07-27)

### Context
Training Ground ha **167 posizioni tattiche piazzate a mano**. Le mappe "profonde" previste dal GDD ne
richiederebbero 1000+: l'authoring per istanza non scala per un team di una persona. Il tentativo di
generare automaticamente le coperture dalla geometria è già stato fatto e **rimosso** (ADR-026) perché
produceva risultati insensati. Serve una terza via fra "tutto a mano" e "tutto automatico".

### Decision
Introdurre i **prefab**: un asset (`data/prefabs/<id>.json`, id = filename stem per ADR-001) dichiara
insieme la mesh visiva, il proxy di collisione (box in coordinate LOCALI), le posizioni tattiche locali e
i volumi interni. La mappa contiene **istanze** (id + trasformazione); il motore le **espande al load**.
Corollario: la macchina **analizza e valida**, l'uomo **crea e decide** — non si torna alla generazione
automatica di posizioni dalla geometria.

Le posizioni espanse da prefab sono **dati derivati** (non salvati, rigenerati); quelle piazzate a mano
sono **autorate** (salvate). La distinzione è esplicita nel campo `source`.

### Consequences
- **Positivo**: il significato tattico si autora una volta e si moltiplica per istanza → mappe profonde
  diventano possibili. Coerente col principio dei dati derivati (ADR-033): non possono diventare stale.
- **Costo**: nuovo formato dati + espansione al load + UI di piazzamento (fase successiva).
- **Rischio**: aggiornare un prefab potrebbe cancellare modifiche manuali → mitigato da `source`, che
  permette di rigenerare solo ciò che viene dal prefab.

### Status
**Proposed.** Diventa Accepted a implementazione + verifica (`--validate` e confronto con mappa a mano).

---

## ADR-049 — Scheletro comune dei moduli editor: COMPOSIZIONE, non ereditarietà (Accepted — in force, implementato e verificato 2026-08-04; 2026-08-02)

### Context
L'editor cresce col progetto ed è già a 7 moduli / ~7400 righe (MapEditor da solo 2787). L'audit di coerenza
(doc 39, 2026-08-02) ha misurato la deriva: *Elimina* manca in 5 moduli su 7, *Duplica* in 3, il
ridimensionamento del pannello in 2 — e i quattro bug del changelog 106 (gizmo assente, pannello non
riallargabile, scroll tagliato) sono sintomi della stessa causa: **ogni modulo rifà la struttura a modo suo**.

Verificato però che le UTILITY sono già fattorizzate e vanno preservate: `FreeCameraViewport` (viewport 3D +
gizmo sposta/ruota/scala, usato da 4 moduli), `DefinitionRename` (ADR-010), `JsonSave::saveJsonRMW`,
`UiWidgets`. Il buco non è nelle utility: è nello **scheletro** che le mette insieme.

### Decision
Introdurre uno scheletro comune **per COMPOSIZIONE** (componenti riusabili che un modulo adotta), **non per
ereditarietà** (una classe base con virtual per ogni fase):
- **`ModuleShell`** — layout standard *lista | contenuto | proprietà* con splitter espliciti, clamp e scroll
  corretti (regole R5/R6 di doc 39), in un posto solo.
- **`AssetBrowser`** — ciclo di vita completo di una definizione su file: Crea, Duplica, Rinomina (comando
  ADR-010), Elimina (R1) — parametrizzato su cartella e contenuto di default.
- Il viewport resta `FreeCameraViewport`: già condiviso, non si tocca.

**Perché composizione e non una base class**: i moduli sono strutturalmente diversi (MapEditor = viewport 3D
con molti tipi selezionabili; BalanceEditor = tabelle di numeri; ClassEditor = form). Una gerarchia con
virtual per tutto diventerebbe un framework rigido da combattere al primo modulo che non ci rientra, e
imporrebbe di riscrivere tutti e sette insieme. I componenti si adottano **uno alla volta**, dove servono.

### Consequences
- **Positivo**: una funzione migliorata migliora tutti i moduli che la usano; le regole di doc 39 diventano
  strutturali invece che disciplina da ricordare; i moduli nuovi partono già coerenti.
- **Costo**: i componenti vanno progettati abbastanza generali da servire, abbastanza specifici da non essere
  vuoti. Rischio mitigato dall'adozione incrementale.
- **Rischio principale — l'editor è GUI e NON è verificabile con `--sim`**: un refactor big-bang dei sette
  moduli non sarebbe testabile in un colpo solo. Perciò la migrazione è **un modulo alla volta**, ognuno con
  il suo smoke test manuale, partendo da un pilota semplice. Nessun modulo viene toccato "per allineamento"
  senza un motivo funzionale.

### Status
**Proposed.** Diventa Accepted quando `ModuleShell` sarà adottato dal modulo pilota e verificato a mano.

## ADR-050 — Ogni sistema nasce con la sua OSSERVABILITÀ, pensata per l'agente AI (Accepted — in force, 2026-08-02)

### Context
Il progetto aveva già la regola "chi costruisce un sistema costruisce l'authoring". Mancava la
metà che serve a **diagnosticare**: nessuna regola imponeva strumenti per vedere cosa un sistema
sta realmente facendo mentre gira.

Il costo si è visto su KI #86. **Tre diagnosi consecutive sono state fuorviate da metriche
aggregate**:
1. gli eventi di combattimento confrontati fra run che divergono (un fix corretto sembrava
   peggiorare del 17%, uno sbagliato sembrava migliorare);
2. la classificazione dei bloccanti con una soglia fissa di 3 m dal centro di oggetti larghi
   fino a 31 m → il falso "57% di geometria muta";
3. l'ipotesi "FOV senza scandaglio", smentita dal funnel (il campo visivo costa il 5-8%).

Ogni volta la risposta è arrivata **solo** guardando una singola unità: la scatola nera
(`AiTrace.cpp`) ha trovato in un colpo un difetto che tre giri di aggregati avevano mancato —
un'unità immobile 3 s con la manovra accesa, leggibile come una riga di cronaca.

Il punto non è la comodità dell'utente. **L'osservabilità è il canale sensoriale dell'agente
AI su questo codice.** L'agente non vede lo schermo, non sente il "feel", non nota che un
fucile è piccolo: tutto ciò che non è strumentato, per lui non esiste — e viene sostituito da
ipotesi plausibili e sbagliate.

### Decision
Un sistema non è completo finché non risponde a *"cosa sta facendo, adesso, questa singola
entità, e perché"*. Servono tre livelli, non uno:

| livello | domanda | esempio |
|---|---|---|
| **Sintomo** | il comportamento è rotto? | `evasivo_durata_max_s`, `stalli per causa` |
| **Funnel** | dove muore il processo, su quale base? | `occ_in_raggio → occ_nel_cono → occ_acquisito`, `gate_*` |
| **Singola entità** | cosa ha fatto, tick per tick? | evento `stallo` (chi guardare) + `--trace-ai <id>` (cosa ha fatto) |

Vincoli:
- **L'osservatore non decide.** Nessun ramo di comportamento legge i dati di osservazione; se
  li leggesse sarebbe un sistema, e varrebbe la regola sui sistemi nuovi (CLAUDE.md §5).
- **Lo stato di osservazione sta sul COMPONENTE**, non nel sistema (sopravviverebbe a
  `initialize()`).
- **Guardie permanenti vs sonde temporanee**: la guardia costa un incremento e resta; la sonda
  costa un raycast o un'allocazione, si rimuove appena ha risposto, e nel codice resta scritto
  **quale risposta ha dato** — così nessuno la rifà.
- **Misura il sintomo, non l'esito**, e per confronti fra varianti calcola entrambe nella
  STESSA run: fra run diverse la simulazione diverge e la differenza non è attribuibile.
- **`--validate` è l'osservabilità dell'authoring**: se un dato può essere sbagliato in
  silenzio, il gate lo dice, con l'azione concreta per correggerlo.

### Consequences
- Costo reale in righe di codice e in tempo per ogni sistema nuovo. Accettato: il conto già
  pagato in diagnosi sbagliate è più alto.
- **Verifica del rispetto**: se per rispondere a una domanda su un sistema devo aggiungere
  strumentazione *dopo*, quel sistema è stato consegnato incompleto.
- Non retroattivo su tutto: i sistemi esistenti si strumentano quando li si tocca. Oggi
  l'AI è coperta (funnel + scatola nera); **non lo sono** navigazione, game mode, missioni,
  ability e veicoli.

---

## ADR-051 — La conoscenza tattica del mondo vive su TRE livelli, non uno (Proposed, 2026-08-04)

**Contesto.** Oggi tutta la conoscenza tattica di una mappa sta in un solo tipo di dato:
`TacticalPositionDef`, posizioni discrete autorate a mano (169 su Training Ground, che è
71,3 × 92,4 m — **una posizione ogni 39 m²**). La ricerca (doc 45) ha mostrato che questo è il
modello di Arma 3 — annotare gli **oggetti** — e che il suo limite noto è esattamente il nostro:
**lo spazio fra gli oggetti resta muto**, e l'AI non sa ragionare sul terreno aperto. Non è un
difetto che si tappa aggiungendo posizioni: le posizioni descrivono **cose**, e all'AI servono i
**luoghi**.

Nessuno dei sistemi che funzionano usa una granularità sola: Killzone ha waypoint *e* aree,
CryEngine punti *e* navmesh, Arma `buildingPos` *e* (nei mod) griglie di pericolo.

**Decisione.** La conoscenza tattica si articola su **tre livelli**, assegnati secondo la **natura
della domanda**, non secondo comodità:

- **A — Griglia d'influenza** (celle 2 m, dinamica, anonima, nessun authoring): *"com'è messa
  quest'area, adesso?"*
- **B — Poligoni del navmesh** (statico, denso, derivato al load, nessun authoring): *"che tipo di
  luogo è questo, e come ci si arriva?"*
- **C — Posizioni tattiche** (statico, rado, semantico, autorabile): *"dove mi metto esattamente, e
  cosa ci faccio?"*

**Regola d'oro**: *un dato vive nel livello più basso che può calcolarlo, e in nessun altro.* È
questa regola — non i tre livelli — che impedisce la divergenza fra verità parallele, che è il
difetto che ci è costato di più (changelog 77).

L'accesso resta esclusivamente via `worldintel` e i buffer runtime: nessun sistema AI legge
direttamente le strutture di mappa.

**Conseguenze.**
- L'autore continua a scrivere **solo l'intento** (`role`, `importance`, `destructible`, tag), e
  **non una riga in più di oggi** — ma la mappa passa da 169 dati tattici a decine di migliaia.
- Il livello B è quasi gratuito: Recast produce già poligoni, vicini e bordi, e oggi li buttiamo via.
- `distToObjective` come campo di Dijkstra per obiettivo dà la **distanza di cammino** ovunque e il
  suo gradiente **è** la via d'accesso: è la risposta strutturale alla verticalità (KI #95).
- Ogni livello nasce con la sua osservabilità (ADR-050): overlay per A, colorazione viewport per B e
  C, funnel di query, `--trace-ai` esteso a *"fra cosa stavo scegliendo"*.

**Status: Proposed** finché M1 (annotazione poligoni) non è implementata e verificata. Piano completo
e criteri di accettazione: **doc 46**.

---

## ADR-052 — Le query tattiche si scrivono in tre sezioni: Generazione / Condizioni / Pesi (Proposed, 2026-08-04)

**Contesto.** Le 7 query di `worldintel` mescolano nella stessa funzione ciò che **scarta** un
candidato e ciò che lo **ordina**. Conseguenza misurata sul lavoro reale: non so dire *perché* una
query ha scelto quel punto, e ogni modifica rischia di cambiare silenziosamente il filtro invece del
punteggio (è già successo: i pesi di `kHold` finiti su `bestAdvantageInArea`).

Il Tactical Point System di CryEngine separa esplicitamente `Generation` / `Conditions` / `Weights`.

**Decisione.** Ogni query tattica si struttura in tre sezioni separate, **in C++, senza introdurre un
linguaggio di query a dati** (sarebbe un interprete in più da scrivere, mantenere e osservare, per un
guadagno che una persona sola non incassa). Condizioni e pesi diventano primitive riusabili.

Si aggiunge il **sensore per-agente** (modello F.E.A.R.): ogni agente mantiene una lista corta di
candidati vicini, aggiornata a ~2 Hz sfalsata via indice spaziale; le decisioni interrogano solo
quella. Il livello squadra non analizza la mappa: sceglie fra ciò che l'agente sa e **rivendica**
(`allyTac.claimed`, già esistente).

**Conseguenze.**
- Il **funnel con denominatori** che ADR-050 richiede viene **gratis** dalla struttura: candidati
  generati → sopravvissuti a ogni condizione → punteggio dei primi tre.
- Il costo per decisione passa da `O(posizioni)` a `O(20)`: **smette di dipendere dal numero di
  posizioni**, che è la condizione perché la mappa grande e la generazione automatica siano possibili.
- Criterio di accettazione: **invarianza di comportamento** (come A5, ±5% sugli eventi nella stessa
  run) prima di qualunque cambio di punteggio.

**Status: Proposed.** Dipende da ADR-051. Dettaglio: doc 46 §5.

---

## ADR-053 — Le forme complesse sono PRIMITIVE PARAMETRICHE che si espandono in box (Accepted — in force, implementato e verificato 2026-08-05; 2026-08-04)

**Contesto.** `MapGeometryBox` ha `ry` e basta: **nessun pitch, nessun roll**. Una superficie
inclinata è *inesprimibile*, mentre il navmesh dichiara di accettare pendenze fino a 45°
(`kAgentSlope`) — una capacità che nessun dato può attivare (la stessa forma di difetto di ADR-023,
applicata alla geometria). Le scale si costruiscono impilando box a mano, ed è così che sono nate le
alzate di **0,68-1,21 m** contro uno `STEP_HEIGHT` di **0,55** (KI #95): l'autore le ha disegnate
credendo fossero scale, e niente gliel'ha detto. Le alzate reali stanno fra 0,10 e 0,18 m: quelle di
Training Ground sono **da 4 a 8 volte** una scala vera.

**Decisione.** Scala, rampa, muro, stanza e piattaforma diventano **primitive parametriche**:
l'autore dichiara l'intento (*"da qui a lì, larga 4 m"*), il motore **espande in
`MapGeometryBox`** rispettando `STEP_HEIGHT`. **L'alzata sbagliata diventa inesprimibile.**

I parametri **si salvano**; i box espansi **no** — si rigenerano al load, come i prefab (ADR-048) e
come tutti i dati derivati (ADR-033).

Le pendenze si risolvono per **scalettatura fine** (alzata 0,20 m = multiplo esatto di
`kCellHeight` 0,10; pedata 0,30 → 33,7°, dentro la banda 30-35° della letteratura), **non**
aggiungendo pitch al box. Motivo: le pedate orizzontali aggirano del tutto il limite di pendenza, il
campo di altezza di Recast le rappresenta senza arrotondamenti, e il visivo liscio resta a carico di
Blender (ADR-047).

**La piattaforma dichiara i propri accessi** come parte della sua definizione: non si verifica dopo
che sia raggiungibile, si rende irraggiungibile-per-costruzione impossibile.

**Alternative respinte.**
- *Pitch/roll sul box*: romperebbe lo slab test analitico della LOS, `appendBox` del navmesh e la
  collisione — cioè la fondazione tattica (ADR-047) per una feature di authoring.
- *Collisione a mesh arbitrarie*: contraddice ADR-047 frontalmente.
- *CSG / brush (Hammer, TrenchBroom)*: altra rappresentazione del mondo. Se ne prende il **flusso di
  lavoro** (undo, livelli, gruppi, duplicatore con offset), non la geometria.
- *Alzare `STEP_HEIGHT` a 0,9*: non è un gradino, è un salto; cambierebbe il movimento ovunque per
  tappare un difetto di authoring.

**Conseguenze.**
- Zero modifiche a collisione, LOS, navmesh e render: a valle dell'espansione ci sono solo box.
- `MapGeometryBox` guadagna `type` (`floor`/`wall`/`platform`/`cover`/`decoration`), che l'editor
  **già scrive e il runtime già scarta** — canale semantico gratuito per doc 46.
- Le mappe esistenti continuano a funzionare identiche: le primitive sono una sezione **nuova**.
- Serve una tabella di **metriche normative** (doc 47 §4), oggi inesistente — ed è la ragione
  strutturale per cui le scale erano sbagliate: non c'era un numero giusto da rispettare.

**Status: Accepted — in force** (2026-08-05). Implementata in `mini/game/MapStructures.hpp`
(espansione) + `StructureDef` in `Definitions.hpp` (ricetta) + authoring nel Map Editor.
**Verificata come richiesto, per connettività e non a occhio:**
- alzata richiesta **2,0 m** → limitata a `STEP_HEIGHT`, 6 gradini da 0,50, **nessuna segnalazione**;
- piattaforma a 3 m con **un solo** lato di accesso, command post in cima → il navmesh trova il
  percorso: **`found:true`, arrivo a 10 cm**;
- la stessa piattaforma con tutti gli accessi disattivati **viene segnalata** dal gate.

Nota emersa dall'implementazione: il gate contraddiceva le metriche che deve far rispettare — la sua
soglia minima di gradino era 0,6 m scelti a mano, e scartava le scale prodotte dalle primitive
(pedate da 0,30, la misura normativa). La soglia ora **è** `mapmetrics::STAIR_TREAD`. Lezione
generale: quando una regola diventa codice, ogni soglia scelta a mano che la riguarda va ricondotta
alla regola, altrimenti i due si contraddicono in silenzio.

Piano completo: **doc 47**.

---

## ADR-054 — L'editor costruisce il NAVMESH VERO, non una sua approssimazione (Accepted — in force, 2026-08-05)

**Contesto.** Il Map Editor aveva una spunta "Area navigabile" che coloriva di verde i box di tipo
`floor`. Mostrava l'**intenzione dell'autore**, non ciò su cui l'AI può camminare — e le due cose
divergono in modo che nessun controllo sui dati può vedere. Fra i box e il navmesh ci sono quattro
filtri di Recast: **erosione** (`kAgentRadius` per lato), **sfoltimento dei cigli**
(`rcFilterLedgeSpans`), **altezza libera** (`walkableHeight`) e **area minima di regione**
(`minRegionArea`).

Il caso che ha forzato la decisione è **KI #97**: su Training Ground `--validate` dichiara
**0 problemi** — per i dati le alzate sono 0,10-0,20 m, tutte a norma — mentre un **recinto intero**
è irraggiungibile. Il difetto non è nella geometria dichiarata: è nella voxelizzazione.

**Decisione.** Il **GFEditor linka `NavManager` + Recast/Detour** e costruisce il navmesh **vero**,
con lo stesso codice del runtime, sullo stato in editing (box a mano **più** i box generati dalle
primitive). Nessun calcolo "approssimato ma economico" solo per l'editor.

È lo stesso principio di **ADR-018** (l'editor usa lo stesso `ContentValidation` del gioco) e di
**ADR-032** (una sola `hasLineOfFire`): *una verità sola sul mondo*. Un secondo calcolo darebbe prima
o poi un verdetto diverso da quello del gioco — il difetto che ci è costato di più (changelog 77).

**Non viola ADR-002**: il contratto è che *GFEngine non deve mai linkare codice dell'editor*. Il
verso opposto è già la norma (`DefinitionRegistry`, `ContentValidation`, `WorldIntel`, `Telemetry`).

**Cosa ne esce, oltre alla visualizzazione.** `NavManager` guadagna due metodi che servono anche al
runtime:
- `debugTriangles()` — i poligoni con la loro **componente connessa**;
- `componentAt(p)` — la componente di un punto, cioè *"è raggiungibile da qui?"* ridotto a un
  confronto fra due interi.

La componente connessa **è** il `componentId` che doc 46 M1 vuole come dato di primo livello: nasce
qui, e resta una sola implementazione.

**Verificato per controincrocio**, non per fiducia: su Training Ground il conteggio delle posizioni
irraggiungibili dà **1** sia con `isReachable` (pathfinding Detour) sia con il confronto di
componente (analisi del grafo). Due metodi indipendenti, stesso verdetto.

**Costo**: la costruzione è **su richiesta**, non a ogni frame — 0,11 s su Training Ground, ~1,4 s su
una mappa 300 × 200. Il risultato invecchia da solo tramite un'**impronta della geometria**
ricalcolata a ogni frame: un flag da alzare a mano nei venti punti che modificano la mappa prima o
poi resta basso, e mostrare un navmesh stantio come buono è peggio che non mostrarlo.

## ADR-055 — Le strutture hanno un TIPO autorabile; i vincoli fisici restano nel codice (Proposed, 2026-08-05)

**Contesto.** ADR-053 ha reso le forme complesse **ricette parametriche**, e ha funzionato: un'alzata
sbagliata è diventata inesprimibile. Ma i **vincoli** di quelle ricette (`minWidthFor(kind)`, i clamp
di `STEP_HEIGHT` e `STAIR_TREAD`) sono murati in `MapStructures.hpp`, e le nove primitive sono
anonime: non esiste un posto dove *"la passerella stretta di questa mappa"* sia **definita**. Per la
mappa 300 × 200 servono forme nominate, riusabili e già verificate — non nove ricette da
riparametrizzare a memoria ogni volta, che è il modo in cui una forma sbagliata si ripete.

**Decisione.** Si introduce un livello **TIPO** sopra le primitive, preso da **Revit**: nelle sue
famiglie, *i parametri di tipo e di istanza si cambiano senza aprire la famiglia*.

- Un **tipo** (`data/structures/<id>.json`, id = filename stem per ADR-001) dichiara: quale
  primitiva, i valori predefiniti, e per ogni parametro se è **modificabile** e con quali
  **min/max**.
- Un'**istanza** (`MapDef.structures[]`) resta ciò che è oggi — posizione, rotazione, valori — e
  guadagna un campo `type` **opzionale**.

**Il pavimento fisico non è autorabile.** `minWidthFor(kind)`, il clamp di `STEP_HEIGHT` sull'alzata e
di `STAIR_TREAD` sulla pedata **restano nel codice** e restano invalicabili: non sono preferenze, sono
conseguenze dell'erosione di Recast e di `minRegionArea` (doc 47 §4). Un tipo può essere **più
severo**, mai più permissivo; un `min` autorato sotto il pavimento viene alzato e l'editor lo dichiara.
È la stessa logica per cui ADR-053 esiste: ciò che rompe la struttura deve restare **inesprimibile**,
e renderlo autorabile sarebbe restituire all'autore esattamente l'errore che gli avevamo tolto.

**Fallback durante la transizione** (CLAUDE.md §2): un'istanza **senza** `type` si comporta
esattamente come oggi, con i minimi per primitiva. Nessuna mappa esistente cambia comportamento; il
tipo è additivo.

**Conseguenza da rendere visibile — la lezione di AutoCAD.** In REFEDIT, ridefinire un blocco
**ridefinisce tutte le sue inserzioni**. Qui vale lo stesso: stringere il `min` di un tipo può
invalidare istanze già piazzate, che verranno riportate nei limiti al caricamento. L'editor deve dire
**quante istanze** un tipo ha, *prima* della modifica — non dopo.

**Alternativa scartata: spostare anche i vincoli fisici nei dati.** Sarebbe stato più "configurabile"
e sbagliato: quei numeri non sono opinioni, e un dato che può essere sbagliato in silenzio è
esattamente ciò che il gate esiste per impedire. Un tipo con una passerella da 0,80 m si salverebbe
senza un lamento e produrrebbe una struttura che il navmesh non genera — cioè KI #97 di nuovo, ma
autorato.

**Osservabilità** (ADR-050): un tipo che non supera la verifica navmesh si può salvare, ma resta
**marcato non verificato**, e il menu `+ Struttura` lo mostra come tale. Il sintomo misurato è
`superficie_persa_%` sulla struttura **isolata** — non un esito lontano, ma la cosa che si rompe.

**Status: Proposed.** Passa ad Accepted quando i tipi sono implementati, un tipo reale è in uso su una
mappa e la verifica navmesh isolata è stata confrontata con il comportamento in partita.

## ADR-056 — Assemblaggio e prefab sono UN SOLO sistema, non due (Accepted — approvato dall'utente 2026-08-06; **revisionato 2026-08-08**: l'annidamento per riferimento è ammesso, vedi in fondo)

**Contesto.** ADR-055 ha dato alle strutture un livello di **tipo**: preset vincolati di **una**
primitiva. L'utente ha subito segnalato il limite (2026-08-06): *"l'editor strutture mi permette di
modificare le strutture ma non di crearne di nuove … per fare anche magari strutture un po' più
complesse"*. Le nove primitive esprimono **elementi**, non **edifici**: una torre con scala interna,
un bunker con feritoie, un magazzino su due piani sono **assemblaggi**.

Ma un assemblaggio somiglia moltissimo a un **prefab** (ADR-048), che già oggi è *un insieme di box +
posizioni tattiche piazzato come unità*. Le differenze reali sono due: il prefab contiene box
**fissi**, il tipo struttura una **ricetta** rigenerata al caricamento; e il tipo ha **vincoli**.

**Decisione.** **Un solo sistema di assemblaggi.** Un assemblaggio è un insieme di **parti** —
primitive parametriche **o** box liberi — più i parametri e i vincoli propri. Il **prefab diventa il
caso degenere**: un assemblaggio di soli box liberi, senza parametri.

Costruire un "editor assemblaggi" *e* un "editor prefab" separati significherebbe costruire due volte
la stessa cosa, con due formati, due editor e due verifiche destinati a divergere — esattamente ciò
contro cui mette in guardia CLAUDE.md §5 sulla responsabilità dei sistemi. E la divergenza fra due
implementazioni della stessa regola è il difetto che è costato di più su questo progetto
(changelog 77, ADR-018, ADR-032).

**Modello preso da AutoCAD (blocchi dinamici)**: geometria + **parametri** + **azioni** + **vincoli**,
con l'opzione *elenco* che limita i valori ammessi. È la stessa scala di problema, già risolta.

**Limite esplicito, preso da Revit**: la pratica delle famiglie annidate avverte di **non eccedere
con l'annidamento**, perché diventa impossibile da gestire e da diagnosticare. Qui:
**un assemblaggio NON può contenere altri assemblaggi.** Le parti sono primitive o box, punto.
Annidare moltiplicherebbe i modi in cui il navmesh si rompe senza che si capisca dove — e la verifica
isolata (doc 48) perderebbe il suo potere diagnostico.

**Conseguenza sulla verifica.** L'assemblaggio è precisamente il punto in cui il navmesh si rompe:
giunzioni che si sfiorano invece di sovrapporsi, altezza libera sotto un solaio, accessi che non si
toccano. La verifica navmesh **sull'insieme** (non parte per parte) è quindi un requisito, non un
accessorio: è la ragione per cui l'utente ha chiesto *"strutture che rispettino il navmesh"*.

**Migrazione.** I prefab esistenti (ADR-048) continuano a funzionare come sono: il formato attuale
resta leggibile e diventa il caso "assemblaggio senza parametri". Nessuna mappa cambia comportamento.

**Status: Accepted** come DECISIONE (approvata esplicitamente dall'utente). L'implementazione è doc 50
C1 e non è ancora iniziata; questo ADR passerà a "implementato e verificato" quando un assemblaggio
reale sarà in uso su una mappa.

### ADR-056 — aggiornamento 2026-08-06: implementato e verificato
Assemblaggi implementati (changelog 171). `StructurePart` (primitiva **o** box libero, posa locale),
`StructureTypeDef::parts`, `mapstructures::expandAssembly` / `expandInstance` come **unico** punto in
cui si decide fra assemblaggio e primitiva — ci passano registry, editor e gate.

Il limite dell'ADR è rispettato nel codice: **una parte non può essere un assemblaggio**, solo una
primitiva o un box. Non c'è annidamento, come prescritto.

Verificato su un assemblaggio reale a tre parti (ripiano + parapetto + insegna): la verifica navmesh
**sull'insieme** ha trovato che l'insegna toglieva l'altezza libera sopra l'arrivo della scala e
isolava il ripiano — con tutte e tre le parti legali singolarmente. È la conferma sul campo della
tesi dell'ADR. Status resta **Accepted**, ora anche implementato.

### ADR-049 — CORREZIONE 2026-08-07: la premessa sul viewport era sbagliata
L'ADR affermava: *"Il viewport resta `FreeCameraViewport`: già condiviso, non si tocca."*

**Vero per la classe, falso per la capacità.** Il viewport è condiviso da 4 moduli, ma tutto ciò che
lo rende un EDITOR — selezione, wiring del gizmo, traduzione dei delta in modifiche, undo, filtri di
vista — vive nel CHIAMANTE, riscritto ogni volta. È da lì che nasce la divergenza segnalata
dall'utente (*"la viewport del map editor è evidentemente molto più avanti di quella dell'editor
strutture"*), e la prova è che il ray-picking **esisteva già** nel viewport condiviso e al tab
strutture mancava solo la riga che lo chiama.

Va aggiunto un componente **`ViewportEditing`** all'elenco dei pezzi condivisi (doc 52 F1), insieme a
`UndoStack` (F2), `DirtyGuard` (F3) e `Dialogs` (F4).

**Stato della migrazione, misurato**: `ModuleShell`/`AssetBrowser` sono adottati da **1 modulo su 7**
(VehicleEditor, 349 righe — il più piccolo proprio perché li usa; MapEditor ne ha 6258 senza).
La migrazione "un modulo alla volta" prevista dall'ADR si è fermata dopo il pilota e va ripresa:
è il piano di doc 52.

### ADR-056 — precisazione 2026-08-08: inserire una composita è COPIARE, non annidare
L'utente ha chiesto di poter mettere strutture composite dentro un assemblaggio. L'ADR **vieta
l'annidamento**, per la ragione presa dalle famiglie Revit: un assemblaggio dentro un altro
moltiplica i modi in cui il navmesh si rompe senza che si capisca dove, e toglie alla verifica
isolata il suo potere diagnostico.

**Il divieto resta, e la richiesta è soddisfatta lo stesso**: il comando `+ Composita` **copia le
parti** dell'altro tipo dentro questo, appiattite, mantenendone le posizioni relative. Si riusa il
lavoro di authoring — che è ciò che serve davvero — senza creare un RIFERIMENTO che possa annidarsi
all'infinito. Le parti restano primitive o box, come prescritto, e la verifica continua a poter
indicare la singola parte che non produce superficie.

**Conseguenza dichiarata all'utente nella UI**: sono copie, non istanze. Modificando la torre
originale, le copie già inserite non cambiano. È il compromesso di ogni sistema che appiattisce
(le esplosioni di blocco in CAD si comportano così), ed è preferibile a una gerarchia che nessuno
riesce più a diagnosticare.

> **Superata il 2026-08-08 dalla revisione qui sotto.** L'utente, messo davanti a questa
> conseguenza, ha scelto il contrario. La precisazione resta scritta perché il ragionamento è
> corretto e il rischio che descrive è reale: è quello che la revisione deve pagare.

### ADR-056 — REVISIONE 2026-08-08: l'annidamento è AMMESSO, per riferimento

**Decisione dell'utente**, testuale: *"per quanto riguarda le composite, preferirei le lasciassi
normali, mettendo al massimo la limitazione per cui puoi aggiungere composite solo se sono state
verificate"*. Il divieto di annidamento cade.

**Perché il divieto era sbagliato nella pratica.** Appiattire risolve il problema dell'espansione
e ne crea uno peggiore a monte: la libreria diventa un archivio di copie che divergono. Una torre
corretta una volta va corretta in ogni posto dove è stata copiata, a mano, ricordandosi quali
sono. È esattamente il difetto che ADR-001 (id = filename stem) e il comando di rinomina esistono
per impedire altrove — una verità duplicata è una verità che si sfalda. Il costo si paga al primo
ripensamento su un pezzo riusato, cioè sempre.

**Cosa cambia nello schema.** `StructurePart` acquista `refType` (chiave JSON `ref`). Non vuoto =
la parte **È** un altro tipo composito, non una copia delle sue parti. La posa vive in
`prim.x/y/z/ry`: riusare quei campi evita un terzo blocco di coordinate che poi qualcuno dimentica
di leggere o di scrivere. Assente = niente cambia, e nessun file esistente si tocca.

**Il rischio del divieto originale resta vero, e si paga in quattro modi** invece che vietando:

1. **Catena anti-ciclo + tetto di profondità** (`kMaxAssemblyDepth = 4`) dentro `expandAssembly`.
   Due strutture che si contengono a vicenda producevano un'espansione infinita: l'editor che si
   pianta senza un messaggio. La catena tiene i tipi che si stanno espandendo *adesso* e rifiuta
   di rientrarci; il tetto è la rete sotto, per i tipi senza id (un tab mai salvato).
2. **Solo composite VERIFICATE** si possono riferire — il limite chiesto dall'utente, e il più
   efficace: un riferimento porta dentro geometria di cui si sa già che il navmesh la attraversa.
   Nel menu le altre si vedono comunque, in grigio, **col motivo** (ADR-023: una voce che sparisce
   insegna che la capacità non esiste).
3. **Il gate `--validate` non lascia niente di muto**: riferimento a un tipo inesistente (Error —
   a runtime la parte sparisce e basta), riferimento a un tipo non composito (Error), a un tipo
   non verificato (Warn), ciclo (Error), annidamento oltre il tetto (Warn). Più, in mappa,
   l'istanza con un `type` che non esiste più. Erano tutti errori **silenziosi**: la struttura
   nasce, si vede, e le manca un pezzo.
4. **`Esplodi`** è la via d'uscita in entrambe le direzioni (vedi sotto): quando l'annidamento
   diventa difficile da diagnosticare, lo si scioglie e si torna a parti piatte.

**Un solo serializzatore per le parti.** Le parti avevano due serializzatori — lettore nel
registry, scrittore nell'editor: la stessa configurazione che aveva già perso il campo `type` e
causato una perdita dati permanente. Sono diventati uno (`mini::structjson::partFromJson` /
`partToJson`, più `boxFromJson`/`boxToJson`) **prima** di aggiungere `ref`, perché `ref` sarebbe
stato il quarto campo ad arrivare in un lettore su due.

### ADR-056 — ESPLODI / RAGGRUPPA (2026-08-08)

Richiesta testuale: *"una funzione per le composite che ti permette di passare da un oggetto
unico, alla struttura come insieme di parti, funzione utile anche nel map editor normale in caso
di bisogno di modifiche ad hoc per delle situazioni specifiche"*.

È il complemento necessario del riferimento, non un extra. Il riferimento è la forma giusta
finché la struttura va bene com'è; quando serve cambiarne un pezzo **in quel punto e solo lì** —
la barricata storta perché c'è una roccia — senza `Esplodi` l'unica strada era duplicare l'intero
tipo in libreria per una modifica di mezzo metro. È lo stesso gesto di *Explode* in CAD e di
*Unpack Prefab* in Unity, e per lo stesso motivo.

- **Nell'editor strutture**: `Esplodi` su una parte-riferimento la sostituisce con le parti vere
  del sottotipo, alla stessa posa. Da lì in poi sono parti di questa struttura.
- **Nel Map Editor**: `Esplodi in parti` su un'istanza composita la scioglie negli elementi della
  mappa. Le parti primitive **restano primitive** (conservano la ricetta e i vincoli di ADR-053:
  appiattirle a box butterebbe via la garanzia sulle alzate); le parti-riferimento restano
  composite, un livello più in basso. Esplodere è un passo, non una demolizione fino ai box.
- **Il ritorno**, sempre nel Map Editor: `Raggruppa in una composita...` su una selezione multipla
  crea un TIPO con quegli elementi (origine al loro baricentro) e li sostituisce con una sola
  istanza. Senza, `Esplodi` sarebbe una porta a senso unico e la modifica ad hoc di oggi
  resterebbe per sempre geometria sciolta. Il tipo si scrive con lo **stesso** `saveStructType`
  dei tab, e nasce non verificato.

**Invariante collaudato** (`--editor-selftest`): esplodere non sposta la geometria di un
millimetro, in entrambi gli editor. Uno strumento che rompe la mappa mentre la aiuta se ne
accorgerebbe solo chi guarda.

**Conseguenza dichiarata**: `Esplodi` **scioglie il legame col tipo**. È ciò che si vuole in quel
momento, ma da lì in poi correggere l'originale non cambia più quelle parti — l'esatto contrario
della proprietà per cui esiste il riferimento. Ctrl+Z lo annulla.

### ADR-056 — PARTI LOCALI: modificare UNA copia sola (2026-08-10)

Richiesta testuale: *"uso la composita Tactic Bunker, ne piazzo 4 diverse, ma su una devo fare
una modifica specifica, quindi la seleziono e apro l'editor per quella singola composita, che poi
appare sempre come Tactic Bunker ma con magari un segnetto per indicare che è una versione
modificata"*.

**Perché `Esplodi` non bastava.** Esplodere risolve "voglio cambiare un pezzo qui", ma al prezzo
di perdere il confine e il nome: dopo, non è più un Tactic Bunker, sono dodici box sciolti. Si
perde la capacità di dire *"questo è un bunker, con una modifica"* — che è precisamente
l'informazione che serve fra tre settimane, guardando la mappa. Le altre due strade erano
peggiori: duplicare il tipo in libreria (varianti quasi identiche di cui nessuno ricorda la
differenza) o modificare il tipo (rovinare gli altri tre bunker per sistemarne uno).

**Decisione.** `StructureDef::localParts` e `StructurePart::localParts` (chiave JSON
`local_parts`). Non vuoto = **queste parti vincono sul tipo**; `type`/`refType` resta scritto, e
serve a due cose: dire da cosa deriva ("Tactic Bunker *") e poterci tornare
("Ripristina dall'originale"). Assente = niente cambia, e nessuna mappa esistente si tocca.

È il modello degli **override d'istanza** di Unity, adottato per la stessa ragione: le decisioni
che valgono per un punto solo devono vivere nel documento di quel punto — cioè nel file della
mappa, non nella libreria.

**Un solo posto in cui la regola è scritta**: `expandParts`, estratta da `expandAssembly`. Le
parti da espandere ora vengono da tre sorgenti (un tipo, le parti locali di un'istanza, le parti
locali di un riferimento isolato) e la funzione è una sola — tre copie divergerebbero al primo
caso nuovo, che è la storia di questo sottosistema.

**Le due strade, dichiarate una accanto all'altra.** Nel pannello di una composita in mappa:
`Modifica solo QUESTA...` e `Modifica il TIPO (tutte le copie)`. Sono la stessa azione con due
portate opposte, e sbagliare porta costa tre bunker o quattro correzioni ripetute: stanno vicine
apposta, con la portata scritta **nel testo del pulsante**, non solo nel tooltip.

**Lo stesso editor, non un secondo semplificato.** Il tab si apre in modo `Instance`: cambia il
bersaglio (`Applica alla struttura` invece di `Salva`), non gli strumenti. Un secondo editor
sarebbe rimasto indietro di qualche funzione per sempre.

**`Isola e modifica`** è la stessa cosa un livello dentro: si entra in una parte-riferimento, si
modifica *quella copia*, e alla chiusura si richiude in un oggetto solo. Differenza da `Esplodi`,
che è la domanda giusta da farsi: **esplodere demolisce il confine, isolare lo tiene e cambia cosa
c'è dentro.** Servono tutti e due, e il modo di dirlo è tenerli accanto con due verbi diversi.

**Guardia sull'identità posizionale (KI #100).** `applyInstanceTab` scrive per indice, e fra
l'apertura del tab e l'applicazione la struttura può essere sparita o essere stata sostituita.
L'indice **più** il tipo di origine sono la controprova: se non combaciano, si rifiuta con un
messaggio invece di modificare la struttura sbagliata.

**Invarianti collaudati** (`--editor-selftest`): quattro copie, se ne modifica una, le altre tre
restano identiche; il tipo di libreria non cambia; `local_parts` sopravvive al giro su disco; il
ripristino riporta all'originale; l'espansione isolata usa le parti locali e non il tipo.

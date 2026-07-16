# 06 — Todo (reality-based, prioritized)

## PROSSIMO SALTO — derivato da GDD + master plan (2026-07-15)

Contesto: la **Fase 1 è essenzialmente completa** (05_CurrentState) e sopra ci sono già sistemi
di respiro Fase 2/3 (nav ADR-017, telemetria ADR-016, ottimizzazione ADR-015). Il confronto con
il GDD (nuovo doc 23) dice che il gap non è più tecnico: **è di design**. Ordine proposto —
discutibile, ma questa è la logica.

**N1. Squad & Command (doc 26, ADR-020) — il salto più grande. → Phase A+B FATTE 2026-07-15.**
Stato: `SquadComponent` + `SquadSystem` (fra Combat e Ai) in force; squadra alleata, default
`Follow`, ciclo di vita con telemetria, ordini non implementati che falliscono con causa. **Phase B**:
comando contestuale a un tasto (G) risolto dal mirino → `FocusFire`/`TakeCover`/`MoveTo`, mailbox
`World::squadOrder`, raggiungibilità verificata prima di impartire, HUD pannello SQUADRA + esiti nel
feed. 4 criteri di accettazione del doc 26 su 5 soddisfatti (vedi 07_Changelog per i numeri).
**Restano:** la **ruota di comando** (livello 2: Regroup/Hold/Advance) e **Phase C** ("a terra" +
rianimazione, tocca `CombatSystem`). L'economia tattica (Punti Comando) resta bloccata su N2 —
come il 5° criterio di accettazione ("la missione è più difficile senza ordini"), che richiede una
missione vera: **è l'argomento più forte per fare N2 adesso**.
→ **Playtest confermato dall'utente 2026-07-15** (tasto G, contesto dal mirino, pannello HUD:
"funziona perfettamente"). N1 è chiuso per quanto costruibile senza obiettivi.

È **l'unico pilastro del GDD senza una riga di codice**, ed è quello che decide se Galactic Front
è "uno sparatutto competente" o "il gioco del GDD": la vittoria deve nascere da decisioni
tattiche e gestione della squadra, non dalla mira. Oggi nasce solo dalla mira.
Perché **adesso** e non prima: tutte le fondamenta sono appena state completate —
AI con profili tattici (16), pathfinding+crowd (22: un ordine "vai lì" è finalmente eseguibile),
consumo dei metadata di mappa (18: "prendi copertura" ha dati veri sotto), telemetria
osservabile (21: gli ordini si verificano in `--sim` senza giocare a mano). Prima di ADR-017
questo sistema non era costruibile bene; ora sì.
Nota: include lo **stato "a terra" + rianimazione** — è ciò che dà peso alle perdite.
→ Lega con l'iterazione **"is it fun"** ancora aperta della Fase 1: è il candidato numero uno a
farla passare.

**N2. Framework obiettivi (doc 25, ADR-019) → Phase A + B FATTE (07-15 / 07-16).**
**Phase B (07-16) — collegamento, scelto leggendo il GDD 21.2 "evitare i sistemi isolati"**: il
framework era completo ma per il giocatore **non esisteva** (KI #37). Ora: esito missione → esito
partita (con precedenza al mode); **HUD OBIETTIVI**; la missione impone la sua mappa; rebind al
riavvio (KI #38). Verificato in partita vera: fallimento → SCONFITTA, successo → VITTORIA.
**Selezione dal PreMatch FATTA (07-16)**: righe Missione e Classe nel menu; la missione impone
mappa/modalita' e il menu le mostra.
**CaptureZone/DefendZone FATTE (07-16)**: ADR-009 **avvolto** via mailbox `commandPostStates`
(zero duplicazione della logica di cattura); riferimento per label validato dal gate nella mappa
della missione. Missione esempio: `firebase_alpha`. Ora il framework sa esprimere la meccanica
principale del gioco.
**Restano**: `DestroyTarget`/`EscortEntity`/`SurviveWave`/`InteractHack` (nessuno urgente: nascono
quando serve una missione che li usa) e i **Punti Comando**.
→ **Punti Comando — serve una decisione di design prima del codice.** Il GDD 5.4 dà la direzione
("guadagnati completando obiettivi, NON con le kill; spesi per rinforzi/veicoli/supporto orbitale")
ma lascia aperti **sink e prezzi**, e il GDD 21.4 vieta di deciderli scrivendo codice.
**Chiarimento dell'utente (07-16): i ticket SONO già il sistema di rinforzi** (cap di AI in campo +
riserva che entra man mano) → "comprare rinforzi" coi Punti Comando sarebbe un **doppione**. Se il
sistema si farà, il sink dovrà essere altro (veicoli? supporto orbitale?). Implementare solo il
guadagno = numero sull'HUD che non si spende (sistema isolato, GDD 21.2).

**Statistiche di missione + debrief — FATTE 2026-07-16** (la domanda di design l'ha sciolta
l'utente: *"narrativo"* = il giudizio nasce dall'**insieme dei fattori**, non da un voto unico).
`World::missionStats` accumulato da chi conosce il fatto; debrief sulle schermate Win/Lose +
evento JSONL `match end`. **Nessun punteggio calcolato di proposito**: i pesi → esperienza sono
progressione (doc 27) e restano design.
**`consequence` degli obiettivi — FATTO 2026-07-16.** 4 tipi agganciati a sistemi reali
(`block_enemy_reinforcements`, `enemy_accuracy`, `ally_reinforcements`, `unlock_spawn`) via
`World::battleState`; valori segnaposto da bilanciare provando; gate ADR-018 esteso.
Verificato con **effetto reale** (2× "RINFORZI INTERROTTI", 0 rimpiazzi).

## ⚠️ DEBITO PRINCIPALE — l'editor non conosce i sistemi nuovi
Direttiva utente 2026-07-16 (vedi 10_ProjectMemory): *"più cose posso modificare dall'editor meglio
è; quello rimane lo strumento principale che IO posso usare, deve essere un tool molto forte e
profondo"*. Oggi **obiettivi, missioni, classi e conseguenze si autorano a mano nei JSON**: nessun
modulo editor. Il doc 25 stesso prevedeva "prima lo schema e il runtime, poi l'authoring" — schema
e runtime **ci sono**, quindi l'authoring è il passo dovuto.
Da fare (in ordine): **modulo "Missioni"** (missione + obiettivi + conseguenze, dropdown-only dal
registry: post dalla mappa della missione, obiettivi dal registry, tipi da enum) e **modulo
"Classi"** (doc 14). Vincoli non negoziabili: `saveJsonRMW` (ADR-010), comando Rinomina,
dropdown mai testo libero, e il gate ADR-018 già pronto come pannello di verifica.

**Altro su questo filone:**
1. **`DestroyTarget`** — serve per "bersagli distrutti" (parametro del giudizio) e per i "bersagli
   strategici" che ogni mappa avrà.
2. **Pesi/esperienza** del giudizio → doc 27 (Fase 3), dopo aver provato.

**Storico — candidato ormai chiuso:**
L'utente lo indica come necessario: *"dopo ogni missione si ottengono punti grazie a un sistema di
giudizio basato su parametri e statistiche — obiettivi completati, alleati morti, kill, morti"*.
Il GDD lo specifica al **9.6**: *"la valutazione finale pesa obiettivi (primari/secondari/falliti),
prestazione tattica (gestione squadra, uso risorse, efficacia decisioni) e costi (perdite, tempo,
risorse). **Il risultato è narrativo, non un semplice voto**"* (vedi anche 5.2, "debrief: successo
militare + prestazione personale + costi; deve raccontare una storia").
**Già disponibile**: obiettivi con tier e stato (ADR-019), tempo di missione, morti/kill via
`killedThisTick` + telemetria, perdite alleate via `checkDeaths`.
**Serve una decisione di design PRIMA del codice** (GDD 21.4): quali parametri pesano e quanto, e
soprattutto cosa significa "narrativo invece che un voto". Raccogliere statistiche senza il
consumatore = ennesimo sistema isolato (GDD 21.2).

**Nota di metodo (07-16):** `--direct-prematch` **NON** avvia la partita in modo affidabile (a
volte sì, a volte no, stesso comando): non è un meccanismo di verifica. Per testare headless il
percorso PreMatch→partita serve una **sonda temporanea che chiami `startFromPreMatch()`** — la
stessa funzione del tasto ENTER, mai una copia della sua logica. `--stress`/`--sim` girano in
osservatore e **non finiscono mai**: inutili per testare gli esiti. Vedi 10_ProjectMemory.

**Storico Phase A (2026-07-15):**
Stato: `ObjectiveDef`/`MissionDef` nel registry, `ObjectiveSystem` dopo Ai/Crowd, 3 tipi
implementati, attivazione/dipendenze/tier, regole di missione dichiarate, gate che rifiuta con
causa, flag `--mission`. Inerte senza missione → mode intatti. Tutti e 5 i criteri di accettazione
del doc 25 soddisfatti (07_Changelog per i numeri). **Restano**: CaptureZone/DefendZone
(avvolgimento ADR-009, serve la mailbox degli stati dei post), gli altri 4 tipi, HUD obiettivi,
selezione missione, Punti Comando.

**N3. Gate di validazione contenuti (doc 24, ADR-018) — FATTO 2026-07-15.**
In force: `core/Result.hpp` + `game/data/ContentValidation`, tre consumatori dello **stesso**
codice (runtime blocca su Error, pannello editor *Moduli → Validazione contenuti*, `--validate`
con exit code ≠ 0 + JSONL). Verificato con guasti deliberati (6 errori/3 warning/exit 1) e sui
dati reali (0/0). Chiusa anche la duplicazione del gate missioni introdotta da N2:
`validateMission` è ora condivisa fra runtime ed editor.
Tutti i criteri del doc 24 soddisfatti, **campi fantasma incluso** (opzione (a): i loader
registrano le chiavi che non leggono → `unknownKeys()`, zero I/O nuovo). Ha trovato subito un
caso vero: `profile_id` residuo in `data/ai/B1 Heavy Droid.json`, rimosso.
Limite documentato: cattura le chiavi IGNORATE dal loader, non i campi letti-ma-non-consumati
(la lista storica di KI #25) — quelli sono codice, non dati, e restano annotati a mano.
→ **Smoke dovuto**: aprire GFEditor → Moduli → Validazione contenuti.

**N4. Class System — Phase A fatta, ma il doc 14 CONTRADDICE il GDD → ADR-022 (Proposed).**
Il GDD cap. 12 apre con *"professioni militari, non semplici categorie di armi"* e in 12.3 lega le
classi al **comportamento IA degli NPC** e alla composizione di squadra; il doc 14 modella una
categoria di armi e **vieta** il legame con l'IA. Il `ClassDef` implementato copre **1 dei 6
parametri** del GDD: seme valido, incorniciato male. **Decisione da approvare (ADR-022)**: far
crescere `ClassDef` verso il GDD partendo da `aiProfileId` + `role` come enum consumato — è ciò che
lo rende una professione e alimenta il sistema di squadra (ADR-020). Perk/XP/sblocchi → Fase 3.
**Il doc 14 va riscritto** su quella base.

**Dettaglio di quanto già in force (Phase A):**
`ClassDef` nel registry (id = filename stem) + `MatchSettings.classId` risolto in `startGame()`,
additivo (vuoto = loadout manuale invariato) + persistenza nei preset + flag `--class` + gate
ADR-018 sulle classi. Esempi: `trooper`, `marksman`.
**Il doc 14 partiva da due premesse false** (PlayerDef con `weaponIds`; `PlayerDef.classId`):
`PlayerDef` non e' letto da nessuno → **KI #35**. La classe e' andata su `MatchSettings`; doc corretto.
**Restano**: selettore classe nel PreMatch (oggi solo `--class`/preset) e modulo editor "Classi".
`abilityIds` e' trasportato ma senza effetto finche' esiste KI #32.
→ ~~Playtest dovuto~~ **FATTO dall'utente 2026-07-16: non funzionava.** Era un **bug reale**
(KI #36), non il design: all'ENTER il PreMatch azzerava `classId` e `characterId` sovrascrivendo
la struct intera — stessa classe di guasto della regola RMW (ADR-010), in memoria invece che su
file. Azzerava anche il personaggio, quindi **nemmeno KI #35 funzionava in partita** (verificato
solo in sandbox e generalizzato: errore di metodo). Corretto e verificato sul percorso reale:
marksman → DC-15X, trooper → DC-15A.

**KI #35 — `PlayerDef` morto → RISOLTO 2026-07-15** (decisione delegata: opzione (a), renderlo
vivo). Le stat del personaggio ora atterrano sul giocatore in `initWorld` (partita **e** sandbox);
default in codice = costanti storiche → variazione zero. Con un solo personaggio autorato viene
scelto da solo, senza UI. Rimossa la costante hardcoded `SPRINT_MULT` (viveva in
`PlayerController.cpp` contro CLAUDE.md); il dato è stato allineato al comportamento reale (1.65),
non viceversa. Gate ADR-018 esteso a personaggi e classi.

**Restano da fare (in ordine di valore):**
1. ~~Selettore PreMatch per classe~~ **FATTO 07-16** (riga "Classe" nel menu, insieme a
   "Missione"). Resta il **selettore personaggio**: serve solo quando esisterà un secondo
   personaggio autorato — con uno solo la scelta è automatica e il pannello dell'editor è già vivo.
2. **Modulo editor "Classi"** (doc 14): oggi le classi si autorano a mano nei JSON. Il gate
   ADR-018 protegge già i riferimenti, quindi l'urgenza è bassa ma il doc lo chiede (dropdown-only).
3. **KI #32 — abilità/gadget lato giocatore**: `ClassDef.abilityIds` è trasportato correttamente
   ma **nessuno lo consuma**. Volutamente NON affrontato ora: l'utente ha chiesto di non
   complicare abilità/gadget ("ci lavoreremo meglio in futuro"), ed è un sistema, non una patch.

**N5. Progressione (27) e Persistenza (28) — Fase 3, dopo N4.** Non prima.

**R8 (nuovo, 2026-07-15).** La risoluzione della data dir sorgente e' duplicata in ogni modulo
dell'editor (BalanceEditor, EntityEditor, MapEditor, ... e ora EditorApp per il pannello
validazione). Se una copia divergesse, quel modulo lavorerebbe su una `data/` diversa da quella
che il gioco carica — e il pannello di validazione mentirebbe. Estrarre in una helper unica.

**Debiti che restano validi in parallelo:** KI #31 (AI attraversano i veicoli — regressione nav),
KI #32 (abilità/gadget player-side), R2 (Application.cpp ~1250 righe → estrarre
VehicleDriver/SandboxSession), R7 (igiene `.bak`).
~~KI #7 (bonifica manuale near-duplicate)~~ → non è più un lavoro manuale: dal 2026-07-15 il gate
ADR-018 li rileva da solo, e sui dati attuali non ce ne sono.


## Done 2026-07-11 → 07-14 (ottimizzazione + telemetria + navigazione)
- **Ottimizzazione loop/AI (ADR-015, doc 20):** frame pacing doppia precisione + cap sicurezza;
  Tracy opt-in; ricerca target SoA; time-slicing sensing; cap LOS ai K vicini. Stress `--stress N`.
  Risultato: ~40 AI fluidi in sim (prima ~30).
- **Telemetria LLM-observable (ADR-016, doc 21):** sink JSONL `session_latest.jsonl` + hook
  GameMode/CommandPost/AI + dump stato completo su F12/fine-partita/crash.
- **Navigazione Recast/Detour (ADR-017, doc 22):** navmesh da MapDef.geometry, DetourCrowd muove
  le AI (pathfinding → AI-stuck su ostacoli RISOLTO; crowd-avoidance), aree danger/cover.
- **Fix:** spawn giocatore posato a terra (KI #28), glitch mouse primo-frame, piedi sottoterra AI
  (regressione crowd). Aperti nuovi: KI #31 (AI attraversano veicoli), KI #32 (abilità player).
- **Nuovi candidati a valore:** R2 (Application.cpp ~1250 righe → estrarre VehicleDriver/Sandbox),
  KI #31 (AI-veicolo), sistema abilità/gadget player-side (KI #32).

## Robustezza (audit codice completo 2026-07-10 — ordine = gravità)

A1. ~~FATTO 2026-07-10~~ **Preset partita: sopravvivenza alle build + formato robusto** (KI #19+#20).
    Spostati in `<exe>/user_presets/match` (fuori dalla `data/` che CMake azzera),
    serializzati con nlohmann (escaping ok), persistono `map_id` (non più l'indice
    fragile) e l'intero loadout. Migrazione legacy best-effort. Smoke: salvare un
    preset → rebuild → deve sopravvivere; caricare un preset → mappa e loadout giusti.
A2. ~~FATTO 2026-07-10~~ **Fallback `id`/`profile_id` in-file rimosso da TUTTI i loader**
    (KI #21): id = solo filename stem (ADR-001). Verificato zero mismatch nei dati
    prima del cambio; smoke `--sim` con registry completo.
A3. ~~FATTO 2026-07-10~~ **Heat reset allo switch arma** (KI #22): stato riscritto in
    `weapons[activeWeapon]` prima del cambio. Consolidare `weapon`/`weapons[2]` → A10.
A4. ~~FATTO 2026-07-10~~ **Spawn spec ConquestMode unificato**: `UnitTemplate` eliminato,
    `RespawnEntry` (con default sensati) è l'unico tipo per spawn/tracking/respawn —
    copia integrale, zero liste di campi da allineare. Le lambda `mkUnit*` restano
    (candidate a prendere direttamente una RespawnEntry — pulizia futura, bassa urgenza).
A5. ~~FATTO 2026-07-10~~ **Collider ruotati** (KI #23): SAT 2D esatto nel movimento +
    LOS in spazio locale del box; coerente col test OBB dei proiettili. Smoke manuale
    alla prima mappa con muri diagonali.
A6. ~~FATTO 2026-07-10~~ **Dipendenze CMake pinnate** (KI #27): stb 31c1ad3, imgui 6029ee3
    (i commit già in uso in _deps).
A7. ~~FATTO 2026-07-10 (completo)~~ **Dedup loader e resolve**: `parseUnitDef` condiviso
    tra loadEnemies/loadAllies; `resolveUnitArchetype` unico per nemici e alleati in
    ConquestMode (gli alleati ora prendono le stats proiettile dall'arma reale, non
    più 8/20/5 hardcoded).
A8. ~~FATTO 2026-07-10 (in parte)~~ **Igiene dati/fallback** (KI #24+#26): id hardcoded
    rimossi da SandboxMode; preset armi morti rimossi da Weapon.hpp (resta solo il
    fallback di ultima istanza documentato); `data/definitions/*` e cartelle vuote
    eliminati. RESTA: geometria fallback firebase (decisione ADR-004) e
    `data/versions.json` (verificare se serve, poi rimuovere).
A9. ~~FATTO 2026-07-10~~ **Campi fantasma marcati "(non attivo)"** negli editor
    (KI #25): min_range, mesh proiettile, fov_deg, hearing_range, reposition_chance,
    damage_scale (+ nota su move_speed vinto dal profilo AI). Quando un campo viene
    consumato dal runtime, togliere il suffisso nello stesso change set.
A10. **Vincoli architetturali da tracciare (non da fixare ora):** team 1/2 hardcoded
    trasversale (nessun supporto 3+ fazioni/FFA); timestep misto (world a fixedDt,
    player/sparo a dt variabile — rilevante per determinismo/replay/split-screen);
    ~~nessuna broad-phase spaziale (collision/LOS O(N²) — muro alla scala fase 2/3)~~
    → MITIGATO 2026-07-14: sensing AI ora SoA + time-slicing + cap LOS ai K vicini (doc 20),
    e il movimento AI usa il navmesh Detour invece di `hasCollision` (doc 22). Resta O(N²) solo
    il broad-phase collisione di player/proiettili (pochi collider, non un muro);
    `MatchSettings.hpp` include ancora SDL in header condiviso.
    ~~Doppia rappresentazione weapon/weapons[2]~~ → RISOLTA 2026-07-10 (28):
    accessor `weapon()` su `weapons[activeWeapon]`, nessuna copia da sincronizzare.

## Rifinitura (diagnosi 2026-07-10 — candidati per la fase di polish)

R1. ~~FATTO 2026-07-10 (18)~~ **Spread/gittata delle armi MAI applicati al giocatore.** `WeaponDef` ha 5 campi
    spread (base/ADS/move/sprint/jump) + effective_range/min_range, autorabili nel
    BalanceEditor con tanto di anteprima — ma PlayerController non li consuma (il
    player spara sempre perfetto, a gittata infinita fino a bullet_lifetime) e le AI
    usano solo `accuracy` del profilo. O si consumano (feel!) o si dichiara il gap.
R2. ~~PARZIALE 2026-07-10 (18): guida estratta in VehicleDrive.hpp (1120→1057);
    prossimi candidati: sim/sandbox session, viewmodel~~ **Application.cpp a 1120 righe** e in crescita: main loop + menu routing + guida
    veicoli + viewmodel + aim check + sim sandbox. Refactor candidato: estrarre
    VehicleDriver e SandboxSession in file propri (nessun cambio di comportamento).
R3. ~~FATTO 2026-07-10 (21): MatchSettings.mapId + selettore nel PreMatch + seconda
    mappa "Outpost" + flag --map~~ **Mappa "firebase" hardcoded nei mode.**
R4. ~~PARZIALE 2026-07-10 (19)~~ **Editor: parser divergenti degli stessi JSON.**
    VehicleEditor ora usa il DefinitionRegistry (duplicato eliminato). Analisi:
    Entity/Map editor restano LEGITTIMAMENTE su parser propri — leggono campi
    editor-only (label/type dei box mappa, buffer ImGui, zone in editing) che il
    runtime non deve caricare. Se lo schema runtime cambia, aggiornare comunque
    entrambi (regola in CLAUDE.md §1.4). Chiuso salvo nuovi duplicati.
R5. ~~FATTO 2026-07-10 (20): pilota saltato come bersaglio, il mezzo assorbe;
    danno a sagoma OBB piena~~ **Pilota colpibile "attraverso" il veicolo**: l'entità player segue la camera
    dentro il mezzo e il test entità avviene PRIMA del blocco collider → i colpi al
    centro del veicolo uccidono il pilota invece di danneggiare il mezzo. Da
    ridefinire con le hitbox veicolo (19 Fase B: il pilota dovrebbe prendere danno
    via veicolo, non direttamente).
R6. ~~FATTO 2026-07-10 (18): VehicleSpawn.hpp condiviso~~ **Spawn veicoli duplicato** in Conquest/Sandbox (2 copie identiche ~25 righe):
    estrarre helper condiviso (es. in MapQuery o un VehicleSpawn.hpp).
R7. **Igiene data/**: `.bak` accumulati (uno per arma) — sono il paracadute ADR-010,
    ma prima o poi serve una pulizia/rotazione; near-duplicate armi (KI #7) ancora
    da bonificare A MANO con il rename tool.

## Done 2026-07-04
- ~~Unify hitbox authoring~~ → ADR-006: profile = single source of truth; EntityEditor
  reads/writes the profile; inline zones deprecated + B1 data migrated.
- ~~ConquestMode dead fallback ids~~ → ADR-007: registry-derived fallback, empty-safe.
- ~~EntityEditor gizmo under scale/rotation~~ → toWorld()/deltaToLocal() at all call sites.
- ~~.gitignore hygiene~~ → .gitignore rewritten (was corrupted), build/ + imgui.ini +
  presets.cfg untracked.

## Done 2026-07-04 (later batches)
- ~~GameMode interface + factory~~ → ADR-008 (`IGameMode` + `createGameMode`); smoke test
  runtime `--sandbox` passato. KnownIssues #8 chiuso.
- ~~Editor professionalization~~ → gizmo 3 modalità, slider ovunque, camera Unreal-style,
  WeaponEditor attach point nel viewport.

## Done 2026-07-04 (5)
- ~~Objective / Command Post riusabile~~ → ADR-009: `CommandPosts` + MapDef + Map Editor
  authoring + ticket bleed in Conquista + test in sandbox. KnownIssues #9 chiuso.

## Done 2026-07-09 — ADR-010 implementato (messa in regola)
- ~~[P0] Rename tooling~~ → `DefinitionRename` (mappa cross-ref esplicita) + UI "Rinomina"
  in Weapon/Entity/Hitbox/Map editor. **Smoke GUI manuale pendente** (KnownIssues #7).
- ~~[P0] Dropdown-only audit~~ → PASSATO: gli unici InputText residui sono creazione nuovi
  id, nomi/etichette/descrizioni e path mesh — nessuna assegnazione di id via testo libero.
- ~~#8 Save-safety helper~~ → `saveJsonRMW` (util/JsonSave.hpp, con .bak) + TUTTI i save
  path migrati. KnownIssues #15 chiuso. In più: `id`/`profile_id` deprecati rimossi dai
  JSON a ogni save (ADR-001).

## P0 — Blocking (superato, riferimento storico)

1. **[P0] Rename tooling for all definition types (was #16, promoted from Low/future).**
   User-confirmed symptom (2026-07-09): renaming weapon names/ids without the tool produced
   duplicate-looking weapon entries in the loadout menu. Root cause: id=filename (ADR-001) +
   no rename command → manual "rename by creating a new file" leaves the old file as an
   orphaned near-duplicate. Scope:
   - Add a "Rinomina" action in EntityEditor, WeaponEditor, HitboxEditor, MapEditor (AI
     profile editor when it exists).
   - Implementation must: rename the physical file (`std::filesystem::rename`), update the
     in-memory id, sweep and rewrite every cross-reference in `data/` that pointed to the old
     id (`weaponIds[]`, `aiProfileId`, `hitboxProfileId`, `enemyTypes[]`, `allyTypes[]`,
     `MapDef` references), then reload the registry.
   - Must use the RMW save discipline (04_CodingStandards) for every file it touches.
   - See 13_ADR, ADR-010 (Proposed) for the full decision record.
   - Blocks: clean data authoring for the rest of Phase 1 content (more weapons/enemies).

2. **[P0] Dropdown-only enforcement audit.** Full audit of every editor module for any
   remaining free-text id input (`ImGui::InputText` on an id/reference field). Convert to
   registry-backed combo per 04_CodingStandards. This is a prerequisite for Todo #1 above:
   the rename tool is only fully effective if there is no remaining path to type a stale id
   by hand.

## High priority

0. ~~Sandbox: selettore armi~~ → FATTO 2026-07-09 (10): tasti 1-9 equipaggiano qualunque
   arma del registry (ordinate per nome, incluse separatiste), toast col nome + hint
   all'avvio, log in telemetria.

3. ~~AI: abilità + ruoli tattici~~ → FATTO 2026-07-10 (core, doc 16_AiBehavior);
   prima abilità ATTIVA (Combat Roll) 2026-07-10 (23) — restano jetpack/missile/
   command_aura sullo stesso binario:
   aggression→distanza d'ingaggio, retreat_hp_threshold→disimpegno, peek/hide da
   cover_preference, flank_chance in Hunt; ability "shield" runtime (ShieldComponent,
   assorbimento + regen in CombatSystem). Resta (16_AiBehavior Out of Scope): abilità
   attive (roll/jetpack/...), vera copertura geometrica (dipende da 15_MapMetadata).
4. ~~Assault/Defense mode~~ → FATTO 2026-07-09 (11), ADR-014. ~~modeIndex nei preset~~
   → FATTO 2026-07-10 (13).
5. ~~Runtime weapon-in-hand~~ → FATTO 2026-07-04 (8): WeaponAttach.hpp + attachMesh nel
   renderer. Resta: assegnare mesh alle armi Republic (DC-15A ecc.) nel Weapon Editor.
6. ~~HUD: stato command post~~ → FATTO 2026-07-09 (11): barra in alto con proprietario,
   lettera e progresso di cattura per ogni post.
7. ~~Dato mancante: profilo AI "grunt"~~ → FATTO 2026-07-09 (2): creato `data/ai/grunt.json`
   (infantry), log pulito, 3 profili caricati.
8. **Save-safety helper centralizzato.** Introdurre un helper condiviso
   (`saveJsonRMW(path, patchFn)`) usato da tutti i moduli editor per ogni salvataggio JSON,
   così la regola RMW (04_CodingStandards) diventa un vincolo strutturale e non solo una
   disciplina documentata. Motivato dall'incidente reale del 2026-07-08 (clobber di
   firebase.json). Vedi ADR-010 candidate scope in 13_ADR.

## Medium

9. **Clone Trooper scale.** Normalize oversized FBX-cm GLB (author `mesh_scale` per entity or
   pre-scale asset). (KnownIssues #5)
10. ~~Weapon attach-point grip/right_hand~~ → codice GIÀ implementato (WeaponAttach usa
    gripAttach, right_hand prioritario); resta SOLO autorare i punti nei GLB delle armi
    dal Weapon Editor (attività dati). Chiuso lato codice 2026-07-10 (14).
11. ~~Runtime weapon rendering~~ → FATTO: unità AI (weapon_display, 2026-07-04) e ora
    anche viewmodel prima persona del giocatore (2026-07-10 (14)).
12. **Commit the hygiene change** (untracked build/ + new .gitignore) — staged, needs a
    commit.
13. **Introduce explicit Objective and Command Post concepts.** Required by Vision Phase 1/2
    to express Conquest/Assault/Defense as data configurations instead of bespoke mode logic.
    (KnownIssues #9)
14. **Introduce explicit Class concept** (weapon + equipment + role composition) separate
    from a single weapon definition, ahead of Phase 3 progression work. (KnownIssues #10)
15. ~~Extend MapDef with AI-relevant metadata~~ → FATTO 2026-07-10 (6): schema + loader +
    authoring MapEditor (15_MapMetadata ora Implementato). Resta il CONSUMER runtime
    (AI tattica, parte del futuro lavoro fase 2 — da documentare a parte).
    (KnownIssues #11 chiuso lato dati)
16. ~~Verify split-screen/multi-viewport feasibility~~ → FATTO 2026-07-09 (12): spike
    ADR-011 eseguito, esito (a) — fattibile con modifiche minori. Toggle F9 in partita.
    KnownIssues #12 chiuso; il soft-gate ADR-011 decade.

## Low / future

17. AI Editor module (dropdown-driven, rename-capable — inherits Todo #1 tooling once built).
18. Asset Manager module.
19. UI/Interface Editor (centralize menu text/layout/palette/fonts — currently scattered).
20. Data hygiene pass to remove existing near-duplicate weapon JSONs (manual cleanup once
    rename tooling — Todo #1 — exists, to avoid re-creating the same problem while cleaning).
21. Define AI hierarchy extension points (squad/strategic tiers) even before implementing
    them, so individual-agent AI doesn't need a rewrite when Phase 2/5 AI is added.
22. Vehicle system as ECS-composable entities (multi-seat), not bespoke controllers.
    → **Fase A FATTA 2026-07-10 (8)** (doc 19_Vehicles): VehicleDef + spawn da MapDef +
    guida player come entità ECS. Resta Fase B: armi di bordo, multi-posto, AI alla
    guida, authoring vehicle_spawns nel MapEditor.
23. **Sistema shape/collision oltre i box** (richiesta utente 2026-07-10): geometria
    mappa, hitbox e collisioni sono limitate a parallelepipedi. Servirà un sistema più
    ricco (forme composte/mesh semplificate) per ambiente ed entità — grande, da
    progettare con un ADR quando diventa bloccante.
24. **Meccaniche FPS alle coperture** (richiesta utente 2026-07-10): crouch dietro
    copertura, mira da copertura, peek-over/around con pose — dipende da un sistema di
    pose/animazioni; il dato `coverPoints[].height` è già pronto a guidarle.
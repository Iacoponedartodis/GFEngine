# 25 — Objectives & Missions (Planned Feature)

**Status: Planned Feature — non ancora implementato in codice.**
Descrive un sistema che NON esiste ancora. Oggi l'unico "obiettivo" del gioco è il command post
(ADR-009), cablato nei mode. Non trattare nessun nome di campo/struct come presente finché
05_CurrentState non lo conferma. Vedi ADR-019.

## Overview
Un **framework obiettivi** generico e data-driven, di cui il command post diventa *una
configurazione* invece che un caso speciale. Sopra ci sta la **MissionDef**: una missione è dati
(mappa + modalità + obiettivi + regole di vittoria/sconfitta), non codice.

## Problem Solved
La Fase 2 della Vision chiede **"obiettivi stratificati (principali/strategici/tattici)"**. Oggi
ogni cosa che assomiglia a un obiettivo va scritta dentro un `IGameMode`: ConquestMode conosce i
command post, AssaultMode/DefenseMode conoscono le loro regole. Aggiungere "distruggi il relè",
"scorta il convoglio", "sopravvivi 3 minuti" significherebbe **una modalità nuova per ogni
obiettivo** — esattamente il fork che ADR-008/ADR-014 hanno evitato per le modalità.

ADR-009 ha già dimostrato il pattern giusto (command post = dati nel MapDef, sistema riusabile).
Questo doc lo generalizza: **il command post non è "il" sistema di controllo territoriale, è il
primo `ObjectiveDef`.**

## Goal
Aggiungere un obiettivo nuovo = **un JSON**, non una modalità nuova né un ramo `if` in un mode.

## Scope

### `ObjectiveDef` (nuovo tipo di definizione, id = filename stem per ADR-001)
```
id                  filename stem (ADR-001)
name                display
type                CaptureZone | DefendZone | EliminateTarget | DestroyTarget |
                    EscortEntity | ReachArea | HoldAreaForDuration | SurviveWave | InteractHack
target              riferimento risolto dal registry/MapDef (zona, entità, post) — dropdown
activation          quando diventa attivo (subito | dopo obiettivo X | a tempo | a evento)
success             condizione di successo
failure             condizione di fallimento (opzionale: non tutti gli obiettivi falliscono)
tier                primary | strategic | tactical      <- la stratificazione chiesta dalla Fase 2
reward              punti comando / effetto (vedi 26 economia tattica)
consequence         cosa cambia se riesce/fallisce (es. apre un altro obiettivo)
linkedObjectives    dipendenze fra obiettivi
```

### `MissionDef` (nuovo tipo di definizione)
```
mapId · modeId · briefing
primaryObjectives[] · optionalObjectives[]
successRules · failureRules            <- obbligatori entrambi (gate di validazione, doc 24)
rewardProfile · persistencePolicy      <- vedi 28
```

### Command post come configurazione
`MapDef.commandPosts` resta dov'è (ADR-009, funziona), ma diventa **generabile come
`ObjectiveDef` di tipo CaptureZone/DefendZone**. Il ticket bleed resta una regola del mode.
Non riscrivere ADR-009: **avvolgerlo**, mantenendo i dati esistenti validi.

### Intento di design (utente, 2026-07-16) — gli obiettivi hanno un VANTAGGIO tattico
> *"Le battaglie deriveranno dalla mappa: ogni mappa avrà posti di comando, bersagli strategici da
> difendere o distruggere, zone strategiche. Vari tipi di obiettivi, **ognuno con un suo
> vantaggio**."*

Esempi dati (illustrativi dell'intento, **non una specifica**):
- **Posto di comando catturato → sblocca un nuovo punto di spawn** (modello Battlefront).
- **Posizione strategica → più copertura**, permette di sfruttare meglio gli alleati.
- **Torre delle comunicazioni distrutta → nemici più disorganizzati.**
- **Base d'atterraggio nemica presa/distrutta → impedisce l'arrivo di nuovi rinforzi nemici.**

È esattamente il campo **`consequence`** dello schema qui sopra — **NON implementato** (vedi Stato).
Il punto di design: un obiettivo non è una casella da spuntare, è una **mossa che cambia la
battaglia**. È anche ciò che rende reale il "giudizio" finale (sotto): le piccole scelte tattiche
producono effetti diversi, quindi esiti diversi.
**Conseguenza architetturale da rispettare quando si implementerà:** ogni consequence tocca un
sistema diverso (spawn, IA, rinforzi nemici) → va espressa come **dato dichiarativo** consumato da
quei sistemi, mai come `if (objectiveId == ...)`. Diversamente si reintroduce il fork che ADR-019
esiste per evitare.

### Intento di design (utente, 2026-07-16) — il "giudizio" di fine missione
> *"Non è un semplice voto, è narrativo, perché c'è un insieme di fattori e di scelte che portano a
> dei risultati, valutati insieme a tutte le statistiche. Così si può avere un sistema di esperienza
> che risulta vero: piccole scelte tattiche influenzano il giudizio finale, proprio perché sono un
> insieme di cose."*

**"Narrativo" NON significa prosa generata**: significa che il giudizio nasce da un **insieme** di
parametri, non da un numero unico — è la combinazione a raccontare com'è andata.
Parametri indicati: obiettivi completati, **bersagli distrutti**, kill, alleati morti, **tempo per
completare la missione**, numero di morti del giocatore, "e così via".
Coerente con GDD 9.6 (*"pesa obiettivi (primari/secondari/falliti), prestazione tattica e costi
(perdite, tempo, risorse); il risultato è narrativo, non un semplice voto"*) e 5.2 (*"debrief:
successo militare + prestazione personale + costi; deve raccontare una storia"*).
**Alimenta la progressione** (doc 27, Fase 3): è da qui che arriva l'esperienza.

### `ObjectiveSystem`
Nuovo sistema ECS, dopo `AiSystem`/`CrowdSystem` in `World::tick` (ordine: gli obiettivi
valutano lo stato dopo che le unità si sono mosse). Emette eventi telemetria discreti
(`objective activated/completed/failed`, doc 21 — MAI per-frame).

## Out of Scope
- **Riscrivere i mode esistenti.** Conquista/Assalto/Difesa continuano a funzionare; il
  framework si affianca e li assorbe gradualmente (regola: "smallest safe change", doc 09).
- Mission editor nell'editor — prima lo schema e il runtime, poi l'authoring (nuovo modulo o tab).
- Generazione procedurale di missioni (è Fase 4/5, Galactic Conquest).
- Scripting di missione (Chronicles, Fase 4) — qui solo condizioni dichiarative.

## Architecture
`ObjectiveDef`/`MissionDef` sono definizioni pure nel `DefinitionRegistry` (stesso layer di
`WeaponDef`/`MapDef`). L'`ObjectiveSystem` legge lo stato dal `World` e comunica con
l'Application via il **pattern mailbox** già in uso (`combatFeedback`/`eventFeed`/`activeMap`/
`nav` — doc 10): niente include di gioco dentro `ecs/`.

L'`IGameMode` non sparisce: decide le **regole** (ticket, esito via `outcome()`), mentre gli
obiettivi decidono **cosa il giocatore deve fare**. Divisione netta.

## Technical Decisions
- **Perché non riusare direttamente `CommandPosts`:** è ottimo ma modella *una* meccanica
  (cattura a presenza + bleed). Gli obiettivi Fase 2 hanno attivazione, dipendenze e tier.
- **Perché `tier` è un campo e non tre sistemi:** la Vision chiede la stratificazione, ma tre
  sistemi paralleli sarebbero il fork che vogliamo evitare.
- **Perché `failure` è opzionale:** il fallimento parziale (un obiettivo tattico fallito che non
  chiude la missione) è ciò che produce decisioni tattiche emergenti invece di firefight lineari.

## Acceptance
Stato al 2026-07-15 (Phase A). Numeri e comandi in 07_Changelog.
- [x] Un obiettivo nuovo (es. "distruggi il generatore") si aggiunge con **solo dati**. — vero per
      i 3 tipi implementati (`ReachArea`, `EliminateTarget`, `HoldAreaForDuration`); gli altri 6
      sono dichiarati e falliscono con causa esplicita finché non esistono.
- [x] I command post esistenti continuano a funzionare senza modifiche ai dati. — il sistema è
      **inerte senza missione**; verificato: zero eventi Objective e mode intatti.
- [x] Una missione con obiettivi a tier diversi produce esiti diversi in `--sim`. — verificato:
      primario completato → `mission success`; un tattico completato non chiude la missione.
- [x] Nessun `if (missionId == ...)` in nessun sistema. — per costruzione, verificato per grep.
- [x] Gate di validazione: una missione senza regole di successo E fallimento non parte. —
      verificato, con causa esplicita in telemetria (ERROR) e a schermo. Rifiuta anche id di
      obiettivo inesistenti, tier incoerenti e missioni senza obiettivi.
      *Nota:* è un gate **locale** a ObjectiveSystem. Il gate condiviso runtime/editor resta N3
      (ADR-018): quando esisterà, questa validazione va spostata lì, non duplicata.

### Phase B.2 (2026-07-16) — `CaptureZone`/`DefendZone`: ADR-009 avvolto, non riscritto
Il framework non sapeva esprimere la meccanica principale del gioco: una missione non poteva dire
"cattura Alpha". Ora sì, e **senza duplicare una riga** della logica di cattura.
- **Mailbox `World::commandPostStates`**: i post vivono in `CommandPosts` (game mode), che `ecs/`
  non può includere. Application li pubblica **fra `mode->update()` e `world.tick()`** → gli
  obiettivi leggono lo stato dello stesso tick.
- `CaptureZone` = il post `target.post` è di `actor_team`. `DefendZone` = tenerlo per
  `hold_seconds`; **perderlo fallisce subito** (un post perso è perso, non un timer che si azzera).
- **Riferimento per LABEL**: i post non hanno id. Il gate ADR-018 risolve la label **nella mappa
  della missione** e segnala le label duplicate (ambigue) nella stessa mappa.
- Esempio in repo: missione **`firebase_alpha`** (`capture_alpha` → `hold_alpha` 20 s).
- Verificato: catena completa → VITTORIA; post perduto → fallimento con causa → SCONFITTA; post
  inesistente → gate + exit 1.

### Phase B (2026-07-16) — collegamento, non espansione
Fatto sulla base del GDD 21.2 ("evitare i sistemi isolati"): il framework era completo ma per il
giocatore **non esisteva** (KI #37).
- **Esito missione → esito partita**: `ObjectiveSystem::outcome()` era codice morto. Ora chiude la
  partita, **con precedenza al mode** (se i ticket hanno già deciso, la missione non ribalta) —
  rispetta la divisione di questo doc: il mode decide le *regole*, gli obiettivi *cosa fare*.
- **HUD OBIETTIVI**: pannello letto dallo stato reale del sistema; primari in evidenza; colore =
  stato; progresso solo dove esiste un conteggio reale; gli `Inactive` non si mostrano.
- **`MissionDef.mapId` è vincolante**: la missione impone la sua mappa (gli obiettivi sono
  coordinate in *quella* mappa); un `--map` contraddittorio viene segnalato.
- **Restart**: il sistema ora rileva il riavvio del mondo (KI #38).

### `consequence` — IMPLEMENTATO 2026-07-16 (il drift è chiuso)
`ObjectiveDef.on_success[]` / `on_failure[]` = liste di `{type, value, target}`.
`ObjectiveSystem` le applica scrivendo **solo** su `World::battleState`; ogni sistema competente
legge ciò che lo riguarda → **nessun `if (objectiveId == ...)`**, e aggiungere un tipo significa un
enum + un `case` + un lettore, senza toccare il resto.

| Tipo | Effetto | Letto da |
| --- | --- | --- |
| `block_enemy_reinforcements` | il nemico non rimpiazza le perdite | `ConquestMode::checkDeaths` |
| `enemy_accuracy` | moltiplicatore precisione nemica (<1 = disorganizzati; **moltiplicativo**, gli effetti si sommano) | `AiSystem` (solo team 2) |
| `ally_reinforcements` | riserve extra alla squadra (delta consumato dal mode) | `ConquestMode::update` |
| `unlock_spawn` | la squadra rinasce al post indicato | `battleState.allySpawnPost` |

**I valori nei dati sono segnaposto**, da bilanciare provando (direttiva utente 2026-07-16):
`capture_alpha` → `unlock_spawn` + 2 riserve; `hold_alpha` → taglia i rinforzi nemici + precisione
nemica 0.6. Cambiarli non richiede codice.
Gate ADR-018: type sconosciuto → **Error** (resterebbe `None` e non farebbe nulla: un obiettivo che
sembra avere un effetto e non ce l'ha); `enemy_accuracy` fuori da (0,1]; `unlock_spawn` senza
target o verso un post inesistente nella mappa della missione.
**Verificato con effetto reale**: col blocco attivo, 2 nemici uccisi → 2× "RINFORZI INTERROTTI",
0 rimpiazzi.
**Manca l'authoring nell'editor** (si scrivono a mano nei JSON) — è il debito principale.

### Storico: il drift ammesso (2026-07-16, ora chiuso)
Lo schema in cima elenca `consequence` ("cosa cambia se riesce/fallisce"), ma il `ObjectiveDef`
implementato **non ha quel campo** — e le note di stato precedenti non lo segnalavano fra le cose
mancanti: era doc↔codice drift, la classe di problema che questo progetto paga da sempre.
È il campo che esprimerebbe l'intento sopra (posto catturato → nuovo spawn; torre distrutta →
nemici disorganizzati; base presa → niente rinforzi nemici) — cioè **la ragione per cui un
obiettivo conta**. Oggi gli obiettivi sono caselle da spuntare: si completano e non cambia nulla
nella battaglia, solo l'esito della missione.
Non implementato di proposito: gli esempi dell'utente sono **illustrativi, non una specifica**, e
ogni consequence tocca un sistema diverso → serve un giro di design prima (GDD 21.4).

### Non ancora fatto (Phase B+)
- **`CaptureZone`/`DefendZone`**: l'avvolgimento di ADR-009. Serve pubblicare gli stati dei
  command post in una mailbox del World — oggi vivono dentro il mode e `ecs/` non può includerlo.
- Gli altri tipi: `DestroyTarget`, `EscortEntity`, `SurviveWave`, `InteractHack`.
- **HUD obiettivi**: oggi attivazione/completamento/fallimento passano solo dal feed.
- **Selezione della missione**: solo `--mission <id>` (l'authoring è fuori scope per questo doc).
- **Punti Comando** (doc 26): `reward` è letto dai dati ma non ancora speso da nulla.

## Interconnessioni
Consuma `MapDef` (doc 15) · valutata dai mode (`IGameMode`, ADR-008/014) · alimenta l'economia
tattica e gli ordini di squadra (doc 26) · sorgente della valutazione di fine missione per la
progressione (doc 27) · richiede i gate di 24 · è il prerequisito della Fase 2 tattica (00_Vision).

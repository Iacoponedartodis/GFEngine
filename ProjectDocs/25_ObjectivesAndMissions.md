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
- [ ] Un obiettivo nuovo (es. "distruggi il generatore") si aggiunge con **solo dati**.
- [ ] I command post esistenti continuano a funzionare senza modifiche ai dati.
- [ ] Una missione con obiettivi a tier diversi produce esiti diversi in `--sim`.
- [ ] Nessun `if (missionId == ...)` in nessun sistema.
- [ ] Gate di validazione (doc 24): una missione senza regole di successo E fallimento non parte.

## Interconnessioni
Consuma `MapDef` (doc 15) · valutata dai mode (`IGameMode`, ADR-008/014) · alimenta l'economia
tattica e gli ordini di squadra (doc 26) · sorgente della valutazione di fine missione per la
progressione (doc 27) · richiede i gate di 24 · è il prerequisito della Fase 2 tattica (00_Vision).

# 28 — Persistence & Career Save (Planned Feature)

**Status: Planned Feature — non ancora implementato in codice. Fase 3/4.**
Descrive un sistema che NON esiste. Oggi l'unica persistenza è `<exe>/user_presets/match`
(preset partita, KI #19/#20 risolti). Non trattare nessun nome come presente finché
05_CurrentState non lo conferma. Vedi ADR-021.

## Overview
Rendere persistenti i **due grandi stati** del GDD (doc 23):
- **Profilo del Clone** → `CareerSave`: grado, classe, specializzazioni, equipaggiamento,
  statistiche, roster della squadra.
- **Stato della Guerra** → `CampaignSave`: fronti, controllo, operazioni (Fase 4/5).

## Problem Solved
La Fase 3 non ha senso senza persistenza: una carriera che si azzera a ogni avvio non è una
carriera. Ma il problema **vero** che questo doc deve prevenire è un altro, e il progetto lo
conosce bene: **i salvataggi distruttivi.**

Storico confermato (doc 10):
- **2026-07-08:** `BalanceEditor::saveMap` riscriveva il JSON da zero → distrutti
  `geometry`/`command_posts`/`ally_*` di firebase.json (**partita ingiocabile**).
- **2026-07-09 (#2):** cambio di riferimento senza ricaricare → zone del profilo sbagliato scritte.
- **KI #19:** ogni build cancellava i preset dell'utente (erano dentro `data/`).

ADR-010 ha reso strutturale la regola per gli editor (`saveJsonRMW`). **Un save di carriera è
esattamente la stessa classe di rischio, ma il danno è peggiore: non è un file dati
rigenerabile, è il progresso dell'utente.** Questo doc esiste soprattutto per non ripetere quegli
incidenti su dati non recuperabili.

## Goal
Perdere una carriera deve essere **strutturalmente impossibile**, non "improbabile se si sta
attenti".

## Scope

### Tipi di save
| Save | Contiene | Quando |
|---|---|---|
| `ProfileSave` | Opzioni, sblocchi globali, estetica | Raro |
| `CareerSave` | Clone (grado/classe/XP/equip), squadra, cronologia | Fine missione/evento |
| `CampaignSave` | Stato guerra: fronti, controllo, operazioni | Fine operazione (Fase 4/5) |
| `MissionSnapshot` | Checkpoint controllato | Solo se serve davvero |

### Regola: serializzare il dominio, ricostruire le entità
**Non serializzare il World/ECS.** Salvare lo stato di dominio e **ricostruire** le entità da
definizioni + stato persistente al load. Il World è pieno di stato derivato (componenti,
crowd agent idx, mailbox, puntatori nav) che non ha senso e non deve essere persistito.

### Scrittura atomica (obbligatoria, senza eccezioni)
```
1. Costruisci uno snapshot IMMUTABILE del dominio
2. Serializza su file temporaneo
3. Valida l'output (rileggi e verifica)
4. Flush + close
5. Ruota il vecchio file in .bak
6. Rename ATOMICO temp → target
7. Emetti evento telemetria SaveCompleted / SaveFailed (doc 21)
```
È l'estensione naturale di `saveJsonRMW`/ADR-010 al runtime. **Ogni nuovo save path che non
segue questa sequenza è un bug**, non una scelta di stile.

### Collocazione
`<exe>/user_saves/` — **fuori da `data/`**, che le build azzerano (lezione di KI #19, già pagata).

### Persistenza a livelli della squadra (GDD)
- Personaggi importanti → persistenza completa (nome, storia, progressione).
- Soldati standard → semplificata (ruolo, esperienza, stato).
- Unità generiche → statistiche aggregate.
Un veterano perso in missione deve essere una **perdita reale**, non un respawn.

## Out of Scope
- Schema versioning / migrazioni: servirà, ma solo quando esisteranno save reali da migrare.
  Aprire un ADR allora. Prevederlo ora sarebbe architettura speculativa (00_Vision, non-goals).
- Cloud/steam saves. Compressione. Formati binari (`nlohmann/json` basta e resta leggibile).
- Persistenza dello stato guerra (Fase 4/5): qui solo la struttura, non il meta-layer.

## Architecture
Layer **game/data**, accanto al registry. Usa `nlohmann/json` come tutto il resto. La
serializzazione può avvenire **dopo** lo snapshot immutabile ed è quindi l'unico candidato sano
ad andare async in futuro — mai leggendo il World live.

## Technical Decisions
- **Perché non serializzare l'ECS:** il World contiene stato derivato e puntatori opachi
  (`World::nav`, crowd agent idx) — insalvabili per costruzione.
- **Perché atomico e non "RMW e basta":** l'RMW protegge i campi *altrui* nello stesso file; il
  rename atomico protegge dal crash **a metà scrittura**. Su una carriera servono entrambi.
- **Perché fuori da `data/`:** KI #19 è già successo una volta.

## Acceptance
- [ ] Un crash simulato a metà salvataggio lascia intatto il save precedente.
- [ ] Una carriera sopravvive a una rebuild (il test che KI #19 avrebbe fallito).
- [ ] Load ricostruisce le entità dalle definizioni, senza deserializzare componenti.
- [ ] Ogni save emette SaveCompleted/SaveFailed in `session_latest.jsonl`.

## Interconnessioni
Serializza la progressione (doc 27), la classe (14) e la squadra (26) · eredita la disciplina di
ADR-010 · emette telemetria (21) · realizza i "due grandi stati" del GDD (doc 23) · Fase 3/4.

# 16 — AI Behavior: profilo tattico completo + abilità runtime (Implementato)

Status: **Implementato (scope 1-5)** — 2026-07-10, Todo #3. Le voci in Out of Scope
restano pianificate. Template come 14/15.

## Overview
`AiProfileDef` definisce già un vocabolario tattico completo (`aggression`,
`cover_preference`, `retreat_hp_threshold`, `peek/hide`, `flank_chance`,
`reposition_chance`) e le unità dichiarano `abilities[]` nei loro JSON — ma il runtime
consuma solo una parte del profilo (seek/jump/accuracy/reaction) e ignora del tutto le
abilità. Questo documento definisce lo scope per chiudere il gap.

## Goal
Ogni campo del profilo AI caricato dal `DefinitionRegistry` deve avere un effetto
osservabile in partita, e `abilities[]` deve produrre comportamento runtime — senza
introdurre nuovi dati hardcoded nei game mode (regola CLAUDE.md §2).

## Problem Solved
- Profili AI diversi oggi si distinguono solo per velocità/precisione: "heavy" e
  "infantry" combattono in modo identico. I campi tattici sono documentazione morta.
- `data/abilities/Shield.json` esiste, è assegnabile dall'editor, ma non fa nulla:
  un'unità con scudo muore come una senza.

## Scope
1. **Ingaggio modulato da `aggression`** — distanza preferita di combattimento:
   aggressione alta → chiude la distanza; bassa → mantiene il raggio, arretra se il
   bersaglio si avvicina troppo.
2. **Ritirata da `retreat_hp_threshold`** — sotto la soglia HP l'AI si disimpegna
   (arretra dal bersaglio continuando fuoco di copertura).
3. **Ciclo peek/hide da `peek_*`/`hide_*`/`cover_preference`** — in ingaggio l'AI
   alterna finestre di esposizione (spara) e di evasione (non spara, strafe ampio).
   `cover_preference` è la probabilità di entrare in evasione a fine finestra.
   *Aggiornamento 2026-07-10:* con 18_AiMapConsumption la fase "hide" ora usa i cover
   point autorati (se presenti e orientati verso il nemico); lo strafe evasivo resta
   il fallback quando la mappa non offre copertura.
4. **Fiancheggiamento da `flank_chance`** — all'ingresso in Hunt, con probabilità
   `flank_chance` l'AI raggiunge la lastKnown da un punto laterale (~6m perpendicolare)
   invece che in linea retta.
5. **Abilità runtime — tipo `shield`** — `ShieldComponent` (ECS): assorbe il danno prima
   degli HP (`param1`=shield HP, `param2`=regen/s, `param3`=ritardo regen dopo un colpo).
   Assegnato allo spawn se `abilities[]` dell'unità referenzia un'ability con
   `type=="shield"`. Il danno assorbito è loggato in telemetria.

## Scope — estensione 2026-07-10: prima abilità ATTIVA ("roll")
Con lo shield passivo validato in gioco, prima abilità con decisione d'uso:
- **`type=="roll"`**: schivata rapida. L'AI la usa entrando in fase evasiva (hide)
  se il cooldown è pronto: scatto laterale a `param1` m/s per `param2` secondi,
  poi cooldown dall'AbilityDef. Stato runtime nello scaffold `AbilityComponent`
  (ora con storage in World). Telemetria/log chat: "ROLL #id".
- Trigger volutamente semplice (niente action-selector generico): l'ingresso in
  evading È il momento "sotto pressione" che il comportamento già calcola.

## Out of Scope (per ora)
- Altre abilità attive (jetpack, missile, command_aura): arriveranno sullo stesso
  binario (AbilityComponent + trigger dedicati), una per volta.
- Vera copertura geometrica (cover point / nav data): dipende da 15_MapMetadata.
- Ruoli come macro-preset (`role` del profilo resta descrittivo: il comportamento emerge
  dai parametri, non da branch per ruolo — evita hardcoding di archetipi).
- Shield per il giocatore (HUD dedicato necessario).
- Gerarchie squad/strategiche (Todo #21).

## Dependencies
- `AiProfileDef`/`AbilityDef` già caricati dal registry (nessun cambio schema JSON).
- `World` ECS: nuovo storage `ShieldComponent`.
- `CombatSystem`: punto unico di applicazione danno (già centralizzato).
- Nessun impatto editor: i campi sono già autorabili in BalanceEditor (tab AI) e
  `abilities[]` in EntityEditor.

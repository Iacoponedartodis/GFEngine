# 17 — Sandbox Tools: banco di prova completo (Implementato)

Status: **Implementato (scope 1-3)** — 2026-07-10. Le voci in Out of Scope restano
pianificate. Template come 14/15/16.

## Overview
La sandbox è il banco di prova del progetto ma è sotto-sfruttata: le armi si cambiano
solo con i tasti 1-9 (tetto a 9), gli eventi di gioco (colpi, scudi, kill) sono visibili
solo nel file di telemetria, e i parametri della prova (quante unità, HP) sono fissi nel
codice. Questo documento definisce gli strumenti in-game della sandbox.

## Goal
Poter verificare in sandbox qualunque contenuto/feature (arma, abilità, hitbox, AI)
in meno di un minuto, senza riavviare il processo e senza leggere file di log esterni.

## Problem Solved
- Tasti 1-9 → inutilizzabile oltre 9 armi (il registry ne ha già 7 e crescerà).
- "Lo shield non funziona": senza log visibile in-game non si distingue "non assegnato"
  da "assorbe senza feedback". La log chat rende osservabile ciò che accade.
- Prove specifiche (es. 5v5 AI) richiedevano modifiche al codice.

## Scope
1. **Menu sandbox (TAB in partita, solo sandbox)** — overlay Ui2D a due pagine:
   - *Armi*: lista completa dal registry, scrollabile, slot primaria/secondaria.
   - *Simulazione*: battaglia AI-vs-AI personalizzabile (modalità Conquista/Assalto/
     Difesa, truppe per team, ticket, respawn) con il giocatore come **osservatore
     neutrale in volo libero** (nessun bersaglio per le AI, nessun esito partita).
   La partita VERA non si avvia dentro la sandbox: la scorciatoia **P** apre il
   PreMatch classico (decisione 2026-07-10, dopo la confusione della pagina
   "Partita" che riavviava solo la sandbox).
2. **Log chat in-game** — mailbox `World::eventFeed` alimentata dai sistemi
   (CombatSystem: colpi con zona/danno, assorbimenti scudo, eliminazioni; Application:
   eventi sandbox). HUD: ultime righe sempre visibili in basso a sinistra (fade),
   tasto L apre il pannello con lo storico recente. Attiva in ogni modalità, non solo
   sandbox — i messaggi sono gli stessi della telemetria, in forma leggibile.
3. **Volo osservatore** — camera libera (WASD + SPAZIO/CTRL, velocità alta) sganciata
   dal PlayerController; il giocatore diventa team neutro (le AI lo ignorano).

## Out of Scope (per ora)
- Selezione abilità/gadget/equipaggiamento del giocatore: il runtime non ha ancora
  abilità player-side (16_AiBehavior copre solo le unità AI). Quando esisteranno,
  il menu sandbox è il posto dove esporle.
- Comandi console testuali (spawn arbitrari, teleport): eventuale estensione futura.
- Salvataggio dei parametri sandbox su file.

## Dependencies
- `Ui2D` (già usata da tutti i menu), `MatchSettings` (team1/2AiCount, playerHp già
  esistenti), factory `createGameMode` (ADR-008) per la simulazione, telemetria
  (ADR-013) — la log chat NON sostituisce il file, lo affianca.

## Stress test / profiling (Fase 3-4 ottimizzazione)
- Cap AI per team alzato a `config::MAX_AI_PER_TEAM` (=50) in sim e partita: slider
  SandboxMenu/PreMatch, clamp ConquestMode. Lo spawn resta nei limiti mappa (griglia
  `perRow` + `findFreeSpot`), non più fila singola.
- **`--stress N`** (CLI): forza sim AI-vs-AI con N (clampato a 50) AI per team, headless.
  Uso tipico per profilare a scala:
  `cmake -S . -B build/reldbg -DUSE_TRACY_PROFILER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`,
  build, poi `GFEngine.exe --stress 50` con il Tracy profiler GUI connesso a 127.0.0.1
  → zone `World::tick`/`AiSystem::update`/`CombatSystem::update` (ADR-015).

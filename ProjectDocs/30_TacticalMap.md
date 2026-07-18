# 30 — Tactical Map (in implementazione)

**Status: Phase 1 in implementazione — 2026-07-18.**
Phase 1 = **mappa top-down di selezione del respawn** (stile Battlefront II 2005): mentre si è a
terra, una vista dall'alto della mappa con i punti di respawn disponibili **selezionabili col mouse
SULLA mappa** (non da un elenco). Sostituisce l'overlay-lista provvisorio del giro 5 (07-18), di cui
è l'evoluzione naturale — la lista era esplicitamente "la fondazione, non la UI definitiva".

## Overview
La mappa tattica è una vista schematica dall'alto della mappa attiva, disegnata in 2D nell'HUD
(`Ui2D`), che proietta le posizioni mondo (XZ) su un pannello a schermo. Non è una telecamera 3D:
è un disegno 2D → **non tocca la pipeline di rendering OpenGL 3.3 Compat** (ADR-003).

## Goal
Dare al giocatore una scelta **spaziale** del punto di respawn: vedere dove sono i propri punti
sulla mappa e cliccare quello voluto, invece di scorrere un elenco astratto. È il primo mattone di
un sistema mappa più ampio (vedi Out of Scope).

## Problem Solved
L'overlay-lista del giro 5 comunica *quali* punti esistono ma non *dove sono*: senza posizione
relativa il giocatore non può fare una scelta tattica ("rientro sul fianco est, vicino al fronte").
BF2005 risolveva questo con la mappa cliccabile; BF2017 tornò all'elenco ed era un passo indietro
(direttiva utente 07-18).

## Scope (Phase 1 — questa)
- **Helper bounds mappa**: calcola l'AABB XZ della mappa dai box di `MapDef.geometry` (unione dei
  footprint), includendo i punti di respawn così sono sempre in-frame; padding. Nessun nuovo campo
  dati (i bounds si derivano, come la navmesh — ADR-004).
- **Render top-down nell'HUD** (`HUD::setRespawnMap` + render): pannello incorniciato, pareti come
  rettangoli tenui (contesto/orientamento), marker dei punti di respawn disponibili
  (`IGameMode::availableSpawns`) alle loro posizioni proiettate, marker evidenziato = selezionato,
  marker del luogo di morte per orientarsi. Titolo "SEI A TERRA" + countdown/prompt di schieramento.
- **Selezione col mouse**: mentre la mappa è aperta la cattura del mouse è rilasciata (cursore
  visibile); passare sopra un marker lo evidenzia, cliccarlo lo seleziona e — se l'attesa minima è
  finita — schiera lì (`deployPlayerRespawn`). `A/D`/frecce e Invio restano come fallback da tastiera.
- **Coerenza col respawn esistente**: usa lo stesso `respawnSel` + `deployPlayerRespawn` + de-clip
  `nudgeOutOfColliders` del giro 6. Con un solo punto disponibile resta il respawn automatico (KI #56).

## Out of Scope (fasi future, NON in Phase 1)
- **Mappa tattica generale** (tasto dedicato, mette in **pausa** il gioco): panoramica strategica
  fuori dal flusso di respawn. L'utente ha chiarito (07-18) che è un sistema *distinto* ma con base e
  concetto condivisi con questa mappa di respawn.
- Post nemici/neutrali e stato di cattura sulla mappa, icone unità/squadra, obiettivi, nebbia.
- Impartire **ordini** dalla mappa (muovi la squadra cliccando la mappa).
- Texture del terreno / minimappa sempre-visibile durante il gioco.
- Zoom/pan della mappa.

## Dependencies
- `IGameMode::availableSpawns()` (giro 5), `CommandPosts::ownedByTeam` (giro 5),
  `deployPlayerRespawn` + `nudgeOutOfColliders` (giro 6).
- `World::activeMap` (accesso a `MapDef.geometry` per bounds e pareti).
- `Ui2D` (rendering 2D dell'HUD), `Window::setMouseCaptured` (rilascio cursore per il click).

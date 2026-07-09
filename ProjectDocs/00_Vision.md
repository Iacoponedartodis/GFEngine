# 00 — Vision

## Intent
GFEngine è un engine C++ specializzato + editor companion (GFEditor) per "Star Wars: Galactic
Front": non un singolo gioco lineare, ma un **ecosistema di guerra modulare** ambientato nelle
Clone Wars, in cui più esperienze (FPS/TPS tattico, comando strategico, progressione RPG,
sandbox di battaglie) condividono lo stesso **core battlefield system**. È un codebase di
produzione solo-developer mantenuto su molte sessioni, non una demo.

## Core philosophy
- **Data-driven first.** Il meno possibile hardcoded in sorgente; il più possibile definito in
  `data/*.json` e autorabile via editor. Nuove armi/nemici/mappe/modalità devono richiedere
  cambi di *dati*, non di *codice*, ovunque sia possibile.
- **Editor come sistema di metadata, non solo di bilanciamento.** GFEditor non serve solo a
  tarare numeri: gestisce i **metadata** che permettono l'integrazione fluida tra sistemi —
  informazioni di mappa utili all'AI (cover point, spawn, in futuro danger zone/settori),
  metadata sui modelli di armi e personaggi (attach point per equipaggiare armi/armature sui
  bones corretti), rig/bones, animazioni, hitbox. È lo strumento che rende possibile aggiungere
  contenuto senza toccare il core engine.
- **Two-binary separation.** GFEngine (runtime) e GFEditor (tool) sono binari separati.
  Comunicano solo tramite file (`data/*.json`, asset per path). Il runtime non dipende mai dal
  codice dell'editor.
- **Un solo core, molte configurazioni.** I sistemi core (combattimento, unità/classi, AI
  gerarchica, obiettivi, veicoli, mappe modulari, spawn/command post) sono condivisi da tutte le
  modalità. Ogni modalità è una configurazione diversa di questi sistemi, mai una riscrittura.

## Non-goals
- Non un engine general-purpose. Nessun plugin system speculativo, nessuna astrazione
  ECS/render prematura.
- Nessuna portabilità oltre il target attuale Windows + SDL2 + OpenGL 3.3 Compatibility
  Profile (client-side arrays, workaround driver Intel).
- **Multiplayer online competitivo esplicitamente fuori scope.** Focus esclusivo su single
  player e co-op locale (split-screen).

## Requisiti funzionali dichiarati (non negoziabili)
- **Split-screen locale a 2 giocatori** con scaling flessibile di UI/input, configurabile senza
  codice. Stato attuale: non verificato nel codice se la pipeline camera/input regge un secondo
  viewport locale simultaneo — da verificare prima di espandere ulteriormente sistemi che
  assumono un solo giocatore attivo.

## Roadmap a fasi (vincolante per le priorità di sviluppo)
1. **Fase 1 — Core playable:** 1-2 mappe, fanteria (cloni/droidi), armi funzionanti, spawn,
   command post, AI base, veicoli base (preferiti), modalità Conquista/Assalto/Difesa come
   configurazioni condivise. Deve essere già divertente qui.
2. **Fase 2 — Sistema tattico:** fronti multipli, obiettivi stratificati (principali/
   strategici/tattici), AI con ruolo e comportamento tattico, veicoli completi, rinforzi/spawn
   dinamici, mappe più grandi.
3. **Fase 3 — Progressione:** carriera clone (gradi Trooper -> Marshal), sblocco classi
   avanzate, abilità di comando, ruolo comandante con gestione macro.
4. **Fase 4 — Modalità avanzate parallele:** Galactic Conquest RPG (strategia globale),
   Chronicles Mode (missioni scriptate), Battle Sandbox (editor di battaglie completo).
5. **Fase 5 — Galaxy War Ecosystem:** tutte le modalità sullo stesso core, AI multi-livello
   completa, veicoli completi, sistema eroi, mappe unificate.

## Long-term direction
- Eliminare gli id di archetipo/fallback hardcoded rimasti nei game mode (vedi KnownIssues #2).
- Unificare l'autoring hitbox (inline vs profilo — vedi KnownIssues #1) prima di costruire
  altri sistemi che dipendono dalle hitbox.
- Introdurre un'astrazione di **game mode** (interfaccia + registro) prima di aggiungere
  Assalto/Difesa, così che ogni nuova modalità sia una configurazione, non un fork di
  ConquestMode/SandboxMode.
- Introdurre concetti espliciti di **Obiettivo** e **Command Post** come sistemi riusabili tra
  modalità.
- Introdurre il concetto di **Classe** (composizione arma+equipaggiamento+ruolo) distinto
  dalla singola arma, in preparazione della Fase 3.
- Far crescere l'editor verso: Entity, Weapon, Hitbox, Map, **AI Editor**, **Asset Manager**,
  **Map Metadata** (cover point/danger zone/settori per l'AI), e un futuro UI/Interface Editor.
- Ogni workflow "assegna definizione A a definizione B" usa dropdown dal DefinitionRegistry,
  mai id in testo libero (pattern già in uso in EntityEditor/WeaponEditor).
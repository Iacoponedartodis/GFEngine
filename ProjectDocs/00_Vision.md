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
  informazioni di mappa utili all'AI (cover point, spawn, in futuro danger zone/settori —
  vedi 15_MapMetadata, Planned Feature), metadata sui modelli di armi e personaggi (attach
  point per equipaggiare armi/armature sui bones corretti), rig/bones, animazioni, hitbox. È
  lo strumento che rende possibile aggiungere contenuto senza toccare il core engine.
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
  codice. Stato attuale: **feasibility VERIFICATA** (spike ADR-011 eseguito 2026-07-09, esito
  (a)): due viewport + seconda Camera sulla stessa scena live funzionano con sole aggiunte
  minori al Renderer (`drawMeshFrom(const Camera&)`, `setViewportRect`); toggle debug F9.
  08_KnownIssues #12 chiuso, il soft-gate ADR-011 decade. **Resta da fare**: input e HUD del
  secondo giocatore (lavoro additivo, non strutturale).

## Roadmap a fasi (vincolante per le priorità di sviluppo)
1. **Fase 1 — Core playable:** 1-2 mappe, fanteria (cloni/droidi), armi funzionanti, spawn,
   command post, AI base, veicoli base (preferiti), modalità Conquista/Assalto/Difesa come
   configurazioni condivise. Deve essere già divertente qui.
2. **Fase 2 — Sistema tattico:** fronti multipli, obiettivi stratificati (principali/
   strategici/tattici), AI con ruolo e comportamento tattico, veicoli completi, rinforzi/spawn
   dinamici, mappe più grandi. Dipende da 15_MapMetadata (Planned Feature) per i dati spaziali
   che l'AI tattica consumerà.
3. **Fase 3 — Progressione:** carriera clone (gradi Trooper -> Marshal), sblocco classi
   avanzate, abilità di comando, ruolo comandante con gestione macro. Dipende da 14_ClassSystem
   (Planned Feature) come unità di composizione loadout/identità su cui costruire gli sblocchi.
4. **Fase 4 — Modalità avanzate parallele:** Galactic Conquest RPG (strategia globale),
   Chronicles Mode (missioni scriptate), Battle Sandbox (editor di battaglie completo).
5. **Fase 5 — Galaxy War Ecosystem:** tutte le modalità sullo stesso core, AI multi-livello
   completa, veicoli completi, sistema eroi, mappe unificate.

## Long-term direction
- ~~Eliminare gli id di archetipo/fallback hardcoded rimasti nei game mode~~ — RISOLTO
  (ADR-007, 2026-07-04).
- ~~Unificare l'autoring hitbox (inline vs profilo)~~ — RISOLTO (ADR-006, 2026-07-04).
- ~~Introdurre un'astrazione di game mode~~ — RISOLTO (ADR-008, 2026-07-04).
- ~~Introdurre concetti espliciti di Obiettivo e Command Post~~ — RISOLTO (ADR-009,
  2026-07-04).
- **Introdurre il concetto di Classe** (composizione arma+equipaggiamento+ruolo) distinto
  dalla singola arma, in preparazione della Fase 3. Documentato come Planned Feature in
  14_ClassSystem; non ancora implementato.
- ~~Estendere le mappe con metadata tattici (cover point/danger zone/patrol/settori) per
  l'AI~~ — FATTO 2026-07-10: dati + authoring MapEditor (15_MapMetadata) e consumo dall'AI
  (18_AiMapConsumption). Restano i settori e le pose alle coperture.
- ~~Unificare naming/id e introdurre rename tooling in editor~~ — FATTO 2026-07-09 (ADR-010
  Accepted): comando "Rinomina" con sweep cross-ref in Weapon/Entity/Map/Vehicle editor +
  `saveJsonRMW` centralizzato con `.bak`. Resta la bonifica manuale dei near-duplicate storici
  (08_KnownIssues #7) e, come presidio strutturale, il gate di validazione (ADR-018, doc 24).
- Far crescere l'editor verso: Entity, Weapon, Hitbox, Map, **AI Editor**, **Asset Manager**,
  **Map Metadata** (cover point/danger zone/settori per l'AI — vedi 15_MapMetadata), e un
  futuro UI/Interface Editor.
- **Ogni workflow "assegna definizione A a definizione B" usa dropdown dal DefinitionRegistry,
  mai id in testo libero.** Questo non è più solo una direzione dichiarata: è una regola
  vincolante in 04_CodingStandards, con audit P0 tracciato in 06_Todo #2.
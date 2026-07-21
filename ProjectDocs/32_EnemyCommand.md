# 32 — Comando Nemico (Droide Tattico come comandante) — Planned Feature / v1

> **Stato: v1 costruita (base espandibile).** Questo doc definisce lo scope di un sistema che sarà
> implementato *pienamente* più avanti, insieme al sistema di gradi/ufficiali e agli strati tattici.
> La v1 dà una **base coerente** su cui costruire, non la versione finale. **v1 (2026-07-20)**: il
> comandante è ora un **singolo obiettivo vivente per mappa**, piazzato nelle **retrovie** e
> **stationary** (non avanza, si difende soltanto). **v0** era la sola direttiva strategica. Vedi
> ADR-024 per la decisione e le memorie [[droide-tattico-concept]] / [[command-rank-system]].

## Overview
Il **Droide Tattico serie T** è la **controparte nemica del comando del giocatore**: come il
giocatore osserva la situazione e dà ordini ai cloni (squadra, ADR-020/doc 26), un comandante
droide osserva lo stato della partita e **dirige i droidi**. Non è un buff e non combatte in
prima linea: è uno **stratega**. La v0 realizza il gancio minimo — una **direttiva strategica**
condivisa che coordina il movimento pre-combattimento dei droidi — lasciando aperta l'espansione
verso ordini più ricchi, gerarchia e gradi.

## Goal
Rendere percepibile che **c'è qualcuno che comanda i droidi**, e che **eliminarlo conta**:
finché il comandante è vivo i droidi si coordinano (convergono sull'obiettivo che il comandante
sceglie); quando muore, tornano al comportamento individuale (pattuglia) — perdita visibile di
coordinamento. È la stessa logica narrativa della torre comunicazioni: un nodo che, distrutto,
degrada l'organizzazione nemica.

## Problem Solved
Oggi i droidi **non hanno un livello strategico**: pattugliano rotte autorate e ingaggiano a
vista (AiSystem: Patrol ↔ Alert/Hunt/Search). Non c'è un'entità che, letta la situazione,
concentri la forza. Manca inoltre una **controparte** all'unico lato che oggi riceve ordini (i
cloni via SquadSystem). Il Droide Tattico riempie entrambi i vuoti con **una** meccanica.

## Scope (v1 — ciò che è stato costruito)
- **Singolo obiettivo vivente per mappa** (v1): il comandante **non è nel roster** (`enemy_types`
  ne spawnerebbe molti come truppa). Nuovo campo mappa `commander { unit, x, z }` (`CommanderSpawnDef`):
  ConquestMode ne spawna **uno solo** alla posizione autorata. Il gate (ADR-018) verifica che risolva,
  che porti l'ability `command`, e **avvisa** se un comandante finisce per errore in `enemy_types`.
- **Sta nelle retrovie, si difende soltanto** (v1): spawna **stationary** → AiSystem non lo muove
  mai (ogni ramo di movimento è sotto `!ai->stationary`); se ha un nemico in vista lo fronteggia e
  spara (autodifesa). Profilo AI autorato (`Tactical Droid`: aggression 0, cover 1.0, retreat 0.52).
  Leva-dati: abbassare `sight_range` per fargli ingaggiare **solo minacce vicine**.
- **`CommanderComponent`** (marker ECS): assegnato allo spawn dall'ability `type: "command"`
  (pipeline abilità→componente, come lo scudo).
- **Direttiva strategica** (`World::enemyCommand`, mailbox): finché il comandante di team 2 è
  **vivo**, AiSystem calcola un **focus** = il command post **non ancora separatista** più vicino al
  comandante, e lo pubblica. Comandante morto → direttiva inattiva.
- **Consumo — v2 (2026-07-20): INTENTO, non destinazioni.** Il comandante **identifica l'obiettivo**
  e **decide uno stance**; i droidi scelgono da sé percorso, coperture e ingaggio:
  - `Hold` → ognuno **presidia** (pattuglia la sua route/area) — è qui che i percorsi autorati vivono;
  - `Advance` → la **forza di manovra** (unità senza route) spinge sull'obiettivo, chi è su route presidia;
  - `Retreat` → si ripiega verso lo spawn separatista.
  Il **combattimento resta autonomo** (come il guinzaglio-ordine del giocatore: vincola il movimento,
  mai mira/fuoco). **Perché**: la v1 pubblicava un *punto* verso cui tutti camminavano — un cervello
  unico al posto delle singole AI, l'opposto della filosofia "AI semplici in un mondo intelligente".
- **Conseguenza leggibile**: alla morte del comandante, messaggio nel feed ("i droidi perdono
  coordinamento") + ritorno alla pattuglia.
- **Contenuto**: il Droide Tattico è una **classe** sul corpo `B1 Battle Droid` (tinta scura,
  hp_mult 1.5, ability `Tactical Command` + `Shield`), piazzato in `firebase.commander` (retrovie).

## v2 IMPLEMENTATA (2026-07-21, ADR-042) — più fronti, stance per-settore
`enemyCommand` è ora una **lista di direttive** (`Directive{x,z,radius,stance,weight,label}`). Il
comandante valuta tutti i settori + le strutture nemiche, li pesa e tiene i **top-3 fronti**; per
ciascuno decide la stance dal **bilancio locale** (`sectorStates`): TIENI dove controlla ma è pressato,
SPINGI su conteso/nemico. I droidi si **distribuiscono** sui fronti (`pickEnemyDirective`, pesato dal
bias) e seguono la stance del proprio fronte, scegliendo da sé il punto/percorso/ingaggio. Il
**ripiegamento** resta l'unica decisione globale (droidi ≤ 50% dei cloni → tutti allo spawn). Misurato:
3 fronti insieme, posture miste (es. 2 AVANZATA + 1 TIENI), priorità che cambia nel tempo, ripiegamento
quando la forza cala. **Il micro fine** (quale copertura, come muoversi nel gruppo) resta alle AI e,
in futuro, al **grado intermedio** ([[command-rank-system]]).

## Direzione v2 (stesura originale) — coerente con un vero Droide Tattico
La v1 sceglie **un** obiettivo e **uno** stance globale calcolato dal solo rapporto di teste
(`nDroids`/`nFoes`) → a forze pari è quasi sempre "avanzata": sembra che legga la situazione, in
realtà conta soltanto. La direttiva dell'utente per la v2 è renderlo **coerente con ciò che un Droide
Tattico è in Star Wars**: analizza la situazione, **segnala obiettivi, condivide informazioni,
imposta priorità e impartisce ordini — anche più di uno alla volta**, gestendo **più settori/fronti
insieme** e dividendo le truppe in modo intelligente.

Il vincolo di responsabilità è netto e va tenuto: **gestisce risorse, priorità e coordinamento**, non
il micro delle singole truppe. Gli ordini specifici (quale copertura, quale percorso, quando sparare)
restano alle AI individuali — e, quando esisterà, al **grado intermedio** ([[command-rank-system]]):
un ufficiale ogni ~N truppe che **interpreta** le direttive del Droide Tattico e coordina solo il
proprio gruppo. Il Droide Tattico parla ai gradi intermedi, non ai singoli soldati.

Implicazione tecnica: `enemyCommand` come **singolo** intento globale è troppo povero per questa
visione — servirà una direttiva **per-settore/fronte** (una lista, non un campo), analoga a come la
torre di controllo pubblica una lista di segnali (doc 36). Studio dedicato prima dell'implementazione.
La ristrutturazione del comandante come **entità a sé** con spawn e leash autorabili è pianificata in
**ADR-041**.

## Out of Scope (v1) — arriva dopo, con studio dedicato
- **Entità a sé**: per ora gira sul corpo B1 (serve un modello proprio); la separazione in entità
  è futura ([[droide-tattico-concept]]).
- **Sistema di gradi / strati tattici**: gerarchia a cascata, ufficiali fino al fireteam (1+3),
  influenza locale sulla tattica immediata → è il [[command-rank-system]]. Il Droide Tattico resta
  l'**autorità strategica più alta** (situazione generale); gli ufficiali gestiranno le singole truppe.
- ~~**UI di piazzamento nel MapEditor**~~ → **✅ FATTA (ADR-041 Fase 1, 2026-07-21)**: marker + pannello
  (classe dal registry, posizione, raggio di leash), gizmo Sposta/Scala, selezione dal viewport, save
  RMW. Il comandante ha ora anche un **raggio di leash** autorato: area circolare da cui non esce
  (0 = fermo sul posto). Non è più `stationary` fisso.
- **Ingaggio solo-se-attaccato "vero"**: oggi il comandante spara a chi vede (regolabile con
  `sight_range`); "reagisci solo a chi mi attacca / ripiega in copertura sotto fuoco" → raffinamento.
- **Ordini ricchi / lettura più profonda**: v1 legge i command post e concentra la forza sul più
  vicino non-separatista. Esaminare anche obiettivi di missione, densità truppe per zona e posizioni
  nemiche per ordini differenziati (difendi/fiancheggia/ripiega) → espansione dello stesso `enemyCommand`.
- **Estrazione in un sistema dedicato**: in v1 il calcolo vive nel precompute di AiSystem; quando
  cresce va estratto in uno `StrategicAiSystem` con responsabilità propria (CLAUDE.md §5.3).
- **Controparte cloni**: ruoli di comando analoghi lato Repubblica → futuro, stesso binario.

## Dependencies
- **MapDef** `CommanderSpawnDef commander` (Definitions.hpp) + loader `commander{unit,x,z}`
  (DefinitionRegistry) + gate (ContentValidation: risolve, è comandante, non nel roster).
- **ConquestMode** `start()`: spawna il singolo comandante `stationary` alla posizione mappa;
  `spawnUnit` traduce l'ability `command` in `CommanderComponent`.
- **AiSystem** (precompute): calcolo/pubblicazione del focus + feed alla morte; (Patrol branch): consumo.
  Il **movimento stationary** è la base del "sta nelle retrovie".
- **World mailbox** `commandPostStates` (owner per post) + `activeMap->commandPosts` (posizioni per
  label): la fonte di "situazione" che il comandante osserva; `enemyCommand` è il canale d'uscita.
- **ADR-020 / doc 26** (comando del giocatore): il modello concettuale di cui questa è la controparte.
- **ADR-023** (classe su corpo): il Droide Tattico è una classe, non un'entità.

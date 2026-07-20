# 33 — World Tactical Intelligence & Metadata System — Piano architetturale

**Status: Proposed (piano, non implementazione).** Questo documento è la **strategia** per
evolvere i metadata attuali in una vera rappresentazione tattica del mondo. NON va implementato
tutto ora: definisce le fasi, l'ordine e i confini. Ogni fase, quando verrà eseguita, avrà il suo
ADR (Status: Proposed → Accepted dopo build+smoke) e aggiornerà 05/06/07/08. Alcune capacità
(pose alle coperture) restano **bloccate** finché non ci sono le animazioni ([[animations-blocked]]).

> **Filosofia guida (direttiva utente 2026-07-20):** *"AI semplici che operano dentro un mondo
> intelligente."* Non NPC costosi che analizzano da soli l'intero mondo, ma un **mondo ricco di
> informazioni tattiche** che permette a AI semplici di decidere in modo credibile e coerente.
> È il metro con cui valutare ogni scelta di questo piano.

---

## 1. Analisi: cosa esiste già (verificato sul codice, 2026-07-20)

Il progetto ha **più di una base** su cui costruire — l'errore da evitare è creare un sistema
isolato invece di collegarli.

- **Metadata mappa (doc 15)** — `MapDef.coverPoints` (`CoverPointDef {x,y,z,facingDeg,height}`),
  `patrolRoutes` (`PatrolRouteDef {id, points[]}`), `dangerZones` (`DangerZoneDef {x,y,z,radius,
  dangerLevel}`). Parse in `DefinitionRegistry::loadMaps`, autorati nel MapEditor (marker + gizmo).
- **Consumo AI (doc 18)** — `AiSystem` usa i cover point in fase hide, la repulsione dalle danger
  zone in Patrol/Hunt/Search, e ConquestMode assegna segmenti di `patrolRoutes` come waypoint.
- **Navigazione (doc 22, ADR-017)** — Recast/Detour/DetourCrowd **maturi**. Il navmesh è generato
  dalla geometria; le **aree semantiche** DANGER/COVER sono già **marcate nel navmesh**
  (`rcMarkCylinderArea`) e il costo DANGER fa aggirare le zone. **I filtri di costo per-ruolo sono
  "struttura pronta, non cablata"** (doc 22 §Phase C) — un gancio enorme già lì.
- **Comportamento AI (doc 16)** — macchina a stati Patrol/Alert/Hunt/Search, `AiProfileDef` con
  aggression/retreat/cover_preference/peek/hide/flank. `AiComponent` ha però **solo 2 waypoint**
  (A/B) → una route = un segmento per unità (limite documentato).
- **Squadra & comando (doc 26, ADR-020)** — `SquadComponent` + `SquadSystem` con ordini
  (Follow/MoveTo/HoldPosition/FocusFire/TakeCover/CoveringFire/Revive), modello a guinzaglio.
  **Oggi è solo lato giocatore** (leader = il giocatore).
- **Comando nemico (doc 32, ADR-024)** — `CommanderComponent` + mailbox `World::enemyCommand`: il
  Droide Tattico pubblica un focus strategico letto da `AiSystem`. È il **primo abbozzo** dello
  strato Squad/Strategico lato AI.
- **Obiettivi & territorio (doc 25/ADR-019, ADR-009)** — `commandPostStates` (owner+progress),
  `strategicTargets`, `battleState`. I command post partizionano già lo spazio in punti-obiettivo.
- **Supporto** — `mapquery::` (findFreeSpot, groundHeightAt) è il posto naturale per query spaziali;
  telemetria JSONL (doc 21) per il debug "perché l'AI ha deciso X"; gate contenuti (doc 24) per
  validare i metadata; mappa top-down (doc 30) come superficie di visualizzazione.

**Conclusione:** i tre livelli richiesti dall'utente esistono già in forma embrionale e vanno
**collegati ed estesi**, non reinventati:
`World Intelligence` ≈ metadata + aree navmesh · `Squad Tactical` ≈ SquadSystem + enemyCommand ·
`Individual AI` ≈ AiSystem.

---

## 2. Problemi architetturali identificati

1. **Consumo sparso, nessun seam.** `AiSystem` scandisce direttamente `MapDef.coverPoints`/
   `dangerZones`; ConquestMode legge `patrolRoutes`; `AiSystem` legge `commandPostStates` per
   `enemyCommand`. Ogni nuovo consumatore ri-implementa scansioni spaziali → niente indice
   spaziale, difficile da ottimizzare e testare. **Manca un livello di query unico.**
2. **Doppia verità sul "pericolo".** `applyDangerRepulsion` (AiSystem) è ridondante col costo
   DANGER del navmesh (doc 22 lo segnala già). Due sorgenti per lo stesso concetto.
3. **Modello di copertura troppo povero.** Un cover point è solo posizione+fronte+altezza: l'AI non
   può decidere "protegge bene ma limita la visuale" perché il dato non esiste.
4. **Route a segmento singolo.** `AiComponent` ha 2 waypoint → niente percorsi tattici veri
   (avanzamento/ritirata/aggiramento come sequenze).
5. **Strato squadra solo-giocatore.** Non esistono squadre AI: il Droide Tattico spinge un singolo
   focus globale come workaround, in assenza di un vero Squad Tactical Layer lato nemico.
6. **Nessuna astrazione a zone/settori.** I command post sono punti, non **aree** con stato di
   controllo/pressione → lo strato strategico (Droide Tattico) ha input situazionale sottile
   (solo owner dei post). È la lacuna che impedisce di renderlo davvero intelligente.
7. **Authoring 100% manuale.** Nessuna generazione automatica: mappe handcrafted grandi
   richiederebbero migliaia di dati a mano (esplicitamente da evitare).
8. **Filtri navmesh per-ruolo non cablati.** L'infrastruttura per navigazione role-aware c'è
   (doc 22) ma non è usata.
9. **Lacune editor.** Il fronte dei cover point non è ruotabile col gizmo (solo slider — è il bug
   notato dall'utente); nessuna visualizzazione/debug dei dati tattici.

---

## 3. Architettura target — tre livelli

```
World Intelligence Layer   (DATI + QUERY, nessuna logica AI)
  geometria tattica · tactical points · cover intelligence ·
  rete di navigazione tattica · settori/combat areas · linee di vista
        │  (query: "miglior copertura verso X", "settore più conteso", "rotta di aggiramento a Y")
        ▼
Squad Tactical Layer       (INTERPRETA il mondo, decide)
  SquadSystem (esteso a entrambi i team) + Droide Tattico/enemyCommand
  obiettivi · assegnazione ruoli · scelta posizioni · attacco/difesa/aggiramento
        │  (compiti specifici: "raggiungi cover Y", "tieni il settore Z", "fiancheggia da W")
        ▼
Individual AI Layer        (ESEGUE il compito assegnato)
  AiSystem: raggiungi posizione · usa copertura · spara · mantieni · reagisci
```

**Principio di decoupling (eredita da doc 15):** il World Intelligence Layer è **solo dati +
funzioni di query pure**. Non conosce l'AI. AiSystem/SquadSystem/Droide Tattico lo **interrogano**.
Così lo stesso layer serve sia l'AI completa sia una futura simulazione semplificata (§9).

---

## 4. Evoluzione dei metadata — i concetti

Tutti additivi (campi opzionali, default = comportamento attuale). Ogni concetto: **schema** →
**generazione automatica + correzione manuale** → **query nel World Intelligence Layer** → consumo.

### 4.1 Tactical Points (generalizzazione dei cover point)
Un `TacticalPointDef` come superset: posizione + **tipo** (cover, vantaggio/sopraelevato, finestra,
ingresso, corridoio, posizione difensiva, punto d'osservazione) + attributi decisionali:
importanza strategica, visibilità, uso consigliato, rischio, **collegamenti** ad altri punti.
Il `CoverPointDef` attuale diventa una specializzazione (retrocompatibile).

### 4.2 Cover Intelligence
La copertura non è un punto navigabile: porta **livello di protezione**, **direzione/e di
protezione**, **altezza**, **si-può-sparare-da-qui**, **idoneità per tipo di unità**, **posizioni
collegate**. Obiettivo: decisioni come *"protegge bene ma limita la visuale"* /
*"ideale per soppressione ma esposta ai fianchi"*. (Le **pose** — crouch/mira-da-copertura,
peek-over vs peek-around da `height` — restano **bloccate** su animazioni: si autora il DATO ora,
l'esecuzione della posa arriva dopo.)

### 4.3 Tactical Navigation
Evolvere le route in una **rete tattica**: non solo il percorso più breve, ma rischio, esposizione,
copertura lungo il tragitto, possibilità di aggiramento. Una route ha uno **scopo**
(`purpose`: avanzamento/ritirata/flanking/pattuglia/rinforzo). Sfrutta **due cose già pronte**: i
**filtri di costo per-ruolo del navmesh** (doc 22, da cablare) e un **grafo tattico** che collega i
Tactical Points. Richiede anche di superare il limite dei 2 waypoint in `AiComponent` (lista).

### 4.4 Combat Areas / Settori
Suddividere la mappa in **aree** con stato tattico: importanza, controllo (presenza alleata/nemica),
pressione del combattimento, risorse. È **il dato che manca al Droide Tattico** per essere davvero
intelligente (oggi vede solo l'owner dei post). I command post diventano ancore dei settori.
Questo layer alimenta sia l'AI locale sia la futura simulazione (§9).

### 4.5 Squad AI Foundation
La squadra è il **principale consumatore** dei metadata. Un comandante di squadra ragiona via query
del World Intelligence: scegli posizione difensiva, ordina aggiramento, assegna una copertura,
scegli direzione d'attacco, reagisci alla perdita di un settore. I membri ricevono **compiti
specifici** invece di decidere da soli — è la concretizzazione della filosofia guida.

---

## 5. Roadmap a fasi (ordine di valore, ognuna piccola e verificabile)

> Ogni fase: additiva, con ADR dedicato, build 0-errori + `--validate` + smoke, doc aggiornati.
> Non passare alla successiva finché la precedente non è verificata.

### Fase 0 — Fondamenta pulite (basso rischio, abilita tutto il resto) — **✅ FATTA (ADR-025, 2026-07-20)**
- **Query layer unico** `mini::worldintel` (nuovo `game/ai/WorldIntel`): un solo posto dove AI/squadra
  chiedono ("cover più vicina verso il nemico", "danger a (x,z)", …). Sposta qui la logica oggi sparsa
  in AiSystem (`pickCover`). **Nessun dato nuovo**, solo il seam — rende ottimizzabile (indice
  spaziale futuro) e testabile ciò che c'è, senza cambiare il comportamento.
- **Rimuovi la doppia verità sul pericolo** (problema #2): `applyDangerRepulsion` → **fallback**
  (solo senza crowd), coerente col costo DANGER del navmesh.
- **Editor: ruota/scala sui marker metadata** dove esiste un campo (KI #60 + richiesta utente
  2026-07-20): cover→ruota(`facing`), veicolo→ruota(`ry`), danger→scala(`radius`), post→scala
  (`radius`). Groundwork per visualizzazione/debug (overlay completo → fase editor dedicata).
- **Doc-accuracy**: aggiorna 15/18 (l'AI ORA consuma cover/danger; il navmesh marca le aree).

### Fase 1 — Cover Intelligence (dato ricco) — **✅ FATTA (ADR-026, 2026-07-20)**
- `CoverPointDef` += `protection` (0..1) + `canShoot`; `worldintel::bestCoverToward` sceglie per
  **protezione** pesata (non solo vicinanza); editor: slider protezione + checkbox. Retrocompatibile.
- **Auto-generazione da geometria RIMOSSA** (feedback utente 2026-07-20): l'euristica sui box faceva
  coperture insensate; mappe fortemente handcrafted → basso valore. De-scoped (vedi §6).
- Rimandati a fasi successive: **idoneità per ruolo** e **link** (servono ruoli e Tactical Points →
  Fase 2); `canShoot` autorato, consumo pieno (fuoco-da-copertura) più avanti; riduzione danno dietro
  copertura (CombatSystem) più avanti. **Blocco**: pose alle coperture (animazioni) → dato ora, posa dopo.

### Fase 2 — Tactical Points generalizzati — **✅ FATTA (ADR-027, 2026-07-20)**
- `TacticalPointDef {x,y,z,facing,type,importance,radius}` (type: vantage/defensive/chokepoint/
  observation) + loader + `worldintel::nearestTacticalPoint` (seam) + editor completo (lista, dropdown
  tipo, slider, marker colorati, gizmo). **Authoring manuale** (no auto-gen). **Consumo = Fase 4/5**.
- Nota: cover e tactical point **coesistono** (l'unificazione §4.1 è cleanup futuro, non rifattoro la
  Cover Intelligence funzionante). Link/visibilità calcolati → Fase 3.

### Fase 3 — Rete di navigazione tattica
- **Cabla i filtri per-ruolo del navmesh** (già pronti, doc 22). Grafo tattico fra Tactical Points
  con semantica di arco (esposizione/copertura/avanzamento/ritirata/aggiramento). Route con
  `purpose`. Supera il limite 2-waypoint (lista in `AiComponent`).

### Fase 4 — Combat Areas / Settori
- `SectorDef` (aree) + stato runtime (controllo/pressione/presenza) calcolato da presenze e post.
  Autorati a mano (ancorati ai command post); lo **stato runtime** è calcolato, non i confini.
  **È la fase che rende intelligente il Droide Tattico**: `enemyCommand` passa da "post più vicino" a
  decisioni su settori (rinforza il conteso, difendi il chiave, aggira il debole).

### Fase 5 — Squad Tactical Layer (entrambi i team)
- Estendi `SquadSystem` alle squadre AI (leader AI, dirette dal Droide Tattico via settori/ordini).
  Il comandante di squadra ragiona via query; i membri ricevono compiti specifici. Chiude il
  triangolo World→Squad→Individual. (Prerequisito del sistema gradi/ufficiali, doc 32/[[command-rank-system]].)

### Fase 6 — Predisposizione simulazione (solo design, non implementare ora)
- Verifica che `SectorDef`/stato-zone siano abbastanza astratti da servire una futura simulazione
  fuori-visuale (stato zone, movimento squadre, controllo territoriale, perdite, morale, risorse).
  Nessun codice di simulazione ora: solo non chiudere la porta (§9).

---

## 6. Editor & Workflow (trasversale a tutte le fasi)

**Aggiornamento 2026-07-20 (direttiva utente):** le mappe saranno **fortemente handcrafted**, quindi
la **generazione automatica di metadata dalla geometria è de-prioritizzata/de-scoped** per ora (il
primo tentativo — auto-gen coperture, ADR-026 §4 — è stato rimosso perché produceva dati insensati).
Il percorso primario è l'**authoring manuale reso veloce da buoni strumenti d'editor**. Se un giorno
servisse davvero auto-gen (mappe procedurali/grandi), va fatta con **analisi tattica vera** (linea di
vista, direzioni di minaccia, spaziatura, chokepoint), non con euristiche sui box — è un sistema a sé,
non un bottone. Priorità per ogni tipo di metadato:
- **Authoring manuale ergonomico**: liste, gizmo (sposta/ruota/scala, ADR-025), slider chiari, marker
  leggibili nel viewport. Rendere veloce la mano del designer è l'obiettivo #1.
- **Visualizzazione** nel viewport (qualità copertura, danger, settori, grafo tattico), attivabile.
- **Debug delle decisioni**: "perché questa AI ha scelto X" via telemetria (doc 21) + overlay — è ciò
  che rende iterabile il level design tattico.
- Authoring sempre via `saveJsonRMW` (ADR-010); il gate (ADR-018) valida i nuovi campi.
- **Auto-gen**: solo se e quando avrà un approccio serio; non un requisito delle fasi attuali.

---

## 7. Vincoli e principi (non negoziabili per questo piano)

- **Dati disaccoppiati dalla logica AI** (eredità doc 15): World Intelligence = dati + query pure.
- **Additivo e retrocompatibile**: ogni campo nuovo opzionale; le mappe esistenti continuano a
  funzionare identiche finché non si autora.
- **Estendere, non duplicare**: riusa navmesh/aree semantiche, SquadSystem, enemyCommand, command
  post. Un problema architetturale (#1, #2, #5, #6) si risolve collegando, non aggiungendo variabili.
- **Contratto due-binari** (ADR-002): metadata/navmesh/query nell'engine; l'editor autora file.
- **Un ADR per fase**; niente promozione automatica di questo piano a "in force".
- **Rispetta i blocchi**: pose/animazioni ferme finché l'utente non le sblocca.

---

## 8. Dipendenze

- `MapDef` + `DefinitionRegistry::loadMaps` (schema additivo per ogni fase).
- `NavManager` (aree semantiche + filtri per-ruolo, doc 22) — cardine della Fase 3.
- `AiSystem`/`AiComponent` (waypoint list, consumo via query) — Fasi 1/3/5.
- `SquadSystem`/`SquadComponent` (ADR-020) — Fase 5.
- `World::enemyCommand` + `CommanderComponent` (doc 32) — Fase 4/5.
- `commandPostStates`/`battleState`/`ObjectiveSystem` (ADR-009/019) — ancore dei settori, Fase 4.
- MapEditor + FreeCameraViewport (authoring/gizmo/visualizzazione) — trasversale.
- Telemetria (ADR-016) + gate (ADR-018) — debug e validazione, trasversale.

---

## 9. Predisposizione alla simulazione futura (non ora)

La base metadata deve essere abbastanza **astratta** da servire sia l'AI completa sia una
simulazione semplificata fuori-visuale. Concretamente: i **settori** (§4.4) e il loro **stato**
(controllo, pressione, presenze, risorse) sono l'unità con cui una simulazione futura potrà
muovere squadre e territorio senza istanziare ogni soldato. Progettare `SectorDef` + stato-zona come
dato puro (nessuna dipendenza dal rendering o dalle entità vive) tiene aperta la porta senza
implementare nulla ora.

---

## 10. Primo passo consigliato

**Fase 0**, perché è a basso rischio, sblocca l'authoring (bug gizmo) e crea il seam su cui poggia
tutto il resto. In parallelo si può decidere se il fronte dei cover point va autorato col gizmo
(fix immediato) o dentro la più ampia Cover Intelligence (Fase 1). Nessuna fase successiva parte
prima che la Fase 0 sia verificata.

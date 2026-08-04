# 40 — AI Architecture: review critica e piano direttore

Status: **Proposed** (documento di architettura, nessuna riga di codice implicata finché non approvato)
Data: 2026-07-27 · Scope: ecosistema AI completo di Galactic Front

> Questo documento NON è una rassegna teorica di architetture AI. È una review dell'architettura proposta
> **misurata contro il codice che esiste oggi in GFEngine** e contro gli esperimenti già falliti in questo
> progetto. Dove la proposta è peggiore di ciò che c'è, lo dico. Dove è migliore, lo dico. Dove è teoria che
> costerebbe anni-uomo a un solo sviluppatore, lo dico e propongo l'alternativa che compra il 90% del
> risultato al 10% del costo.

---

## 0. Verdetto esecutivo (leggi almeno questo)

1. **L'architettura proposta va MODIFICATA, non adottata né buttata.** La pipeline a 5 livelli
   (Mission Goal → Squad Planner → Strategic Brain → Tactical Executor → Animation) è **già in gran parte
   implementata** in GFEngine sotto altri nomi. Ma il modo in cui è disegnata — una **catena di comando
   top-down** dove ogni livello impone al successivo — è **esattamente il modello che questo progetto ha già
   provato e REVERTATO** (changelog 72-77), con dati: gli ordini che scavalcavano l'AI autonoma lasciavano le
   unità **senza bersaglio l'81% del tempo**.
2. **Il modello corretto è a DUE CANALI + agente locale sovrano** (§2). Il comando non spinge ordini verso il
   basso: pubblica **intento** e **fatti tattici pre-calcolati**; l'agente *legge* e decide da sé. È la
   filosofia già dichiarata del progetto — *"AI semplici in un mondo intelligente"* — ed è ciò che ha prodotto
   i risultati migliori (torre-hub, changelog 93).
3. **ML/ONNX: da RIFIUTARE per questo progetto** (§6). Non per snobismo tecnico: distrugge le tre cose su cui
   questo progetto ha appena costruito il suo metodo di lavoro — **determinismo** (appena conquistato con
   `--sim-ticks`, changelog 99), **debuggabilità** (misura → diagnosi → fix) e **autorabilità** (il designer
   regola numeri leggibili, non pesi di una rete).
4. **GOAP: da RIFIUTARE. Utility AI: già presente di fatto, va FORMALIZZATA.** (§6)
5. **Il buco più grave non è nel livello strategico: è nella PERCEZIONE.** `fov_deg` e `hearing_range` erano
   **autorati in ogni profilo AI ma mai letti dal codice** — e la cosa era **già tracciata in KI #25b dal
   2026-07-10**: non è stata scoperta qui, è stata trovata *ancora aperta* qui. Che un buco di gameplay di
   questa portata sia rimasto due settimane in una lista di "campi non consumati" dice che quella lista va
   riletta ogni volta che si progetta sopra quei sistemi. I soldati **vedono a 360° e
   sono sordi**. Nessun livello strategico, per quanto sofisticato, produrrà soldati credibili sopra una
   percezione onnisciente e sorda. **Questa è la priorità #1.**

---

## 1. Review critica dell'architettura proposta

### 1.1 Cosa è giusto nella proposta
- **La separazione fra "decidere" ed "eseguire" è corretta** ed è già realizzata: `AiCommandLayer.cpp`
  (cosa si decide a livello di teatro) vs `AiSystem.cpp` (come la singola unità esegue) — split fatto nel
  changelog 95 proprio lungo questa linea.
- **"Emergency overrides"** è corretto e già presente in forma primitiva (ritirata sotto soglia HP, reazione
  al fuoco). Va generalizzato (§7.5).
- **Cadenze diverse per livelli diversi** è corretto: il comando decide ogni `COMMAND_DECISION_PERIOD` (3 s),
  il quadro tattico ogni `TAC_PICTURE_PERIOD` (0.33 s), l'esecuzione ogni tick.

### 1.2 Il difetto strutturale: la freccia verso il basso
La proposta disegna:

```
Mission Goal → Squad Planner → Strategic Brain → Tactical Executor
```

Letta come **catena di comando**, questa architettura ha un fallimento noto e *già documentato in questo
progetto*: quando il livello superiore decide *dove* e *come* deve stare l'unità, il livello inferiore perde
la capacità di reagire a ciò che vede. Evidenza interna (KI #80b, changelog 77):

> Hold/Advance custom in `SquadSystem` con LOS e bounding propri, che sopprimevano il reposition autonomo →
> **unità in Advance senza bersaglio l'81% del tempo**. Diagnosi: la LOS parallela del planner
> (`hasLineOfFire` su MapDef) era **scollegata** da come l'AI acquisisce davvero (`hasLineOfSight` runtime +
> `aggroRange`). Il planner mandava i soldati in posti "tatticamente belli" e ciecamente vuoti.

La lezione non è "il planner è inutile", è: **il planner non deve possedere una seconda verità sul mondo.**

### 1.3 Rischio di produzione della proposta così com'è
| Rischio | Perché | Mitigazione |
|---|---|---|
| Doppia verità tattica | Ogni livello che calcola LOS/copertura per conto suo diverge dal runtime | Un solo query layer (`worldintel`), già esistente |
| Planner storm | Replanning a cascata a ogni cambio di stato | Cadenze + commitment (già in uso: `ORDER_COMMIT_TIME`) |
| Non-determinismo da async | Thread + ordine di esecuzione variabile | Jobify solo il precompute read-only (§11) |
| Costo di debug | 5 livelli = 5 posti dove il comportamento può nascere | Telemetria a funnel (già in uso, §12) |
| Costo di produzione | Solo sviluppatore | Roadmap a valore decrescente (§14) |

### 1.4 Verdetto
**Modificare.** Mantenere la stratificazione, **invertire la direzione del controllo**: da *push di ordini* a
*pull di informazioni*. Vedi §2.

---

## 2. Architettura raccomandata — due canali + agente sovrano

```
        ┌─────────────────────────── MONDO (MapDef + World state) ───────────────────────────┐
        │                                                                                    │
        │   CANALE A — INTENTO (lento, ~3 s)          CANALE B — FATTI TATTICI (~0.33 s)     │
        │   ┌────────────────────────────┐            ┌──────────────────────────────────┐   │
        │   │ Mission / Obiettivi        │            │ WorldIntel (query pure su MapDef)│   │
        │   │  ↓                         │            │  · posizioni, coperture, archi   │   │
        │   │ Comando di fazione         │            │  · grafo covers/exposure         │   │
        │   │  · Droide Tattico (team 2) │            │  · corsie (lateralCoord)         │   │
        │   │  · Torre controllo (team 1)│            │ AllyTactical (pre-calcolo torre) │   │
        │   │  ↓ pubblica DIRETTIVE      │            │  · canFire per posizione (LOS)   │   │
        │   │    (settore, stance, peso) │            │  · score, occupancy centrale     │   │
        │   └────────────┬───────────────┘            └───────────────┬──────────────────┘   │
        │                │                                            │                      │
        └────────────────┼────────────────────────────────────────────┼──────────────────────┘
                         │   (mailbox, sola lettura per l'agente)     │
                         ▼                                            ▼
                 ┌──────────────────────────────────────────────────────────┐
                 │  AGENTE (AiSystem) — SOVRANO sulle proprie decisioni     │
                 │  percezione → memoria → utilità → esecuzione             │
                 │  L'intento è un BIAS, mai un telecomando (ADR-020)       │
                 └────────────────────────┬─────────────────────────────────┘
                                          ▼
                         Movimento (Crowd/Detour) · Tiro · Animazione
```

**Regola non negoziabile** (già ADR-020, confermata dall'incidente del changelog 77):
> *L'ordine non scrive mai il transform e non sceglie mai il bersaglio. Vincola l'INTENZIONE
> (dove/quanto aggressivo), mai l'ESECUZIONE.*

Perché questo modello è superiore alla catena di comando:
- **Reattività preservata**: l'agente vede un nemico e reagisce, qualunque cosa dica il comando.
- **Costo di calcolo condiviso**: il lavoro pesante (LOS di tutte le posizioni contro tutti i nemici) si fa
  **una volta per fazione**, non per agente — già realizzato (`updateAllyTactical`, changelog 93).
- **Degradazione naturale**: se la torre/comandante muore, gli agenti continuano a funzionare con
  informazione locale ([[structures-degrade-not-block]]).
- **Debuggabile**: ogni canale è ispezionabile separatamente (telemetria + editor).

---

## 3. Inventario onesto: cosa esiste GIÀ

| Sezione richiesta | Stato in GFEngine | Riferimento |
|---|---|---|
| 1. Tactical Spatial DB | **Esiste, maturo** — `TacticalPositionDef` (ruolo, protezione, altezza, arco/gittata, importanza), grafo `positionCovers`/`positionExposure` al load, settori + `SectorState`, danger zones, route | ADR-030/032/033/034 |
| 2. Perception | **Completata** (A1, changelog 100) — LOS 3D ad altezza occhi, aggro, K-nearest (8), **FOV + fascia periferica + rivelazione da lampo**, udito event-driven, confidenza (A2). Misurato col funnel d'ingaggio: il FOV costa il 5-8%, **non** è il collo di bottiglia; lo è la LOS (61-72%, KI #86 causa 3) | §4 |
| 3. World Model & Memory | **Esiste** — `SharedContact` con età, TTL, merge; last-known; ricerca; ritardo informazione da comms | doc 34 |
| 4. Communication | **Esiste** — `World::comms[3]`, raggio/ritardo scalati, mailbox multiple | doc 34 |
| 5. Tactical Planner | **Esiste, due implementazioni** — Droide Tattico (direttive per settore, corsie, assegnazione spaziale) e Torre di controllo (segnali + quadro tattico + occupancy) | doc 32/36 |
| 6. Strategic Layer | **Utility esplicita** (changelog 117) — gli 8 bilanci di pesi vivono in `include/mini/game/ai/AiUtility.hpp`; `sectorTacticalWeight`/`bestOrderPosition` sono dichiaratamente funzioni di utilità. **Restano da tarare** (curve §6) | §6 |
| 7. Tactical Execution | **Esiste, maturo** — FSM (Patrol/Alert/Hunt/Search), reposition ADR-035, cover/peek, leash, crowd, gate di fuoco | ADR-035 |
| 8. Personality | **Parziale** — `aggression`, `accuracy`, `cover_preference`, `flank_chance`, `reposition_chance`, `retreat_hp_threshold`, `reaction_time`, peek/hide | data/ai/*.json |
| 9. Player Commands | **Esiste** — `OrderType`, mailbox, ruota 6 posture, `directedMembers` per-membro | ADR-020, changelog 98 |
| 10. Engine Architecture | **Esiste** — ECS + mailbox, split AiSystem/AiCommandLayer | ADR-002, changelog 95 |
| 11. Performance | **Esiste** — time-slicing, cap LOS, griglia broadphase, commitment, precompute | doc 20 |
| 12. Debug tools | **Esiste, forte** — telemetria JSONL a contatori, editor (esposizione, visuale verticale, settori), Tracy | ADR-016, changelog 97 |
| 13. Testing | **Esiste, ora deterministico** — `--sim-ticks N`, `--validate`, `--stress` | changelog 99 |

**Conclusione dell'inventario**: non serve costruire un'architettura nuova. Serve **chiudere tre buchi** e
**formalizzare** ciò che è implicito.

---

## 4. I buchi reali (in ordine di impatto sulla credibilità)

### 4.1 🔴 PERCEZIONE — il buco grave (priorità assoluta)
**Misurato oggi**: `hearingRange` compare **solo** nel parser (`DefinitionRegistry.cpp:181`); `fovDeg` solo nel
parser e nella Camera di rendering. **Nessuno dei due raggiunge l'AI.**

Conseguenze osservabili in gioco:
- un soldato **vede alle proprie spalle** come davanti → niente aggiramenti che funzionano, niente
  stealth, nessun valore posizionale nell'arrivare da dietro;
- uno **sparo non allerta nessuno** → niente reazione a contatto, niente "accorrere al rumore",
  le battaglie non si propagano.

Costo di chiusura: **basso** (i dati sono già autorati, la pipeline di parsing esiste, il punto di innesto è
un solo blocco in `AiSystem`). Impatto: **massimo**.

### 4.2 🟠 SOPPRESSIONE — assente come meccanica
Oggi "essere sotto tiro" non cambia il comportamento se non tramite HP. Un soldato vero **si abbassa quando i
colpi passano vicino**, anche se non colpito. È il singolo elemento che più fa leggere una sparatoria come
militare invece che come deathmatch.

### 4.3 🟠 RUOLI di squadra — assenti
Il planner assegna **aree**, mai **ruoli**. Non esiste "tu sopprimi mentre tu aggiri": il bounding overwatch
oggi è *emergente* da un cap di concorrenza (ADR-035), non *deciso*.

### 4.4 🟡 PREDIZIONE — assente
La memoria conserva l'ultima posizione nota, ma nessuno estrapola *dove sarà* il nemico. È ciò che distingue
"cercare dove l'ho visto" da "tagliargli la strada".

### 4.5 ⚪ Room/indoor, breaching, destructible cover — assenti
Coerente con mappe attuali prevalentemente aperte. **Da NON costruire ora** (§14).

---

## 5. Sezione per sezione — raccomandazioni

### §1 Tactical Spatial Database
**Stato**: maturo. **Non toccare la struttura dati.**

Sulla domanda "Spatial Grid vs BVH vs Quadtree": è una domanda che questo progetto **non deve porsi ora**.
Le posizioni tattiche sono **poche e autorate a mano** (167 su Training Ground), non milioni di voxel:
la scansione lineare costa microsecondi e il seam `worldintel` permette di inserire un indice **senza toccare
i chiamanti** quando (e se) servirà. Una griglia esiste già dove serve davvero: la broadphase della LOS
(`ensureGrid` in `Collision.cpp`), che è il vero costo.

> **Decisione**: nessun cambio di struttura spaziale finché un profiling non mostra che la scansione lineare
> è nel top-3 dei costi. Oggi non lo è.

Estensioni utili (basso costo, alto valore), da autorare in editor:
- `indoor: bool` per posizione/settore (abilita comportamenti diversi: distanze corte, granate);
- `chokepoint` già esiste come ruolo → usarlo per il controllo di transito;
- copertura **distruttibile**: riusare `strategicTargets` (già ha HP e collider) marcando la posizione come
  invalidata quando il box muore → la posizione esce dallo score. Costo basso perché `allyTac.score` è già
  ricalcolato periodicamente.

### §2 Perception System — **PRIORITÀ 1**
Modello raccomandato, in ordine di innesto:

```
                    ┌── stimoli ──┐
   VISTA:  dentro FOV? → dentro sight_range? → LOS 3D? → (tempo di reazione) → contatto
   UDITO:  evento sonoro (sparo/passo/esplosione) con RAGGIO → contatto DEBOLE (direzione, non identità)
```

**Vista — cosa aggiungere** (il resto esiste):
```cpp
// In AiSystem, dentro il loop dei candidati (dove oggi si testa solo distanza + LOS):
// FOV: un soldato non vede alle spalle. `facing` è già in TransformComponent (ry).
const float halfFov = ai->fovDeg * 0.5f * (PI/180.0f);
glm::vec2 fwd{ std::sin(et->ry*PI/180.0f), std::cos(et->ry*PI/180.0f) };
glm::vec2 to { tp.x - et->x, tp.z - et->z };
const float cosAng = (fwd.x*to.x + fwd.y*to.y) / std::max(0.001f, glm::length(to));
if (cosAng < std::cos(halfFov)) {
    // FUORI campo visivo → NON è un contatto visivo.
    // MA: se è vicinissimo o sta sparando, resta percepibile (visione periferica/udito).
    if (dist > PERIPHERAL_RADIUS && !targetIsFiring) continue;
}
```
Nota di design: **non** implementare il FOV come taglio netto. Un taglio netto produce il difetto classico
"il nemico mi sta di fianco e non mi vede": serve una fascia periferica (rilevamento senza identificazione) e
il fatto che **sparare rende visibili**.

**Udito — il sistema che manca del tutto**. Architettura minima e sufficiente:
```cpp
// World: mailbox degli stimoli sonori del tick (come le altre mailbox, doc 10)
struct SoundStimulus {
    float x, z;          // origine
    float radius;        // quanto lontano si sente (sparo > passo)
    int   team;          // chi l'ha prodotto (per non allertarsi da soli)
    float loudness;      // 0..1 → confidenza del contatto risultante
};
std::vector<SoundStimulus> soundsThisTick;   // svuotata a fine tick
```
Chi emette: `CombatSystem` allo sparo (raggio = gittata udibile dell'arma, **ridotto dal silenziatore** se
esisterà), esplosioni, `CrowdSystem` per i passi (a cadenza, non ogni tick).
Chi consuma: `AiSystem` → un suono entro `hearingRange` genera un **contatto a bassa confidenza**
(posizione approssimata, nessuna identità) che alimenta la ricerca (`pickSearchPoint` esiste già).

> **Perché questo è il miglior rapporto valore/costo di tutto il documento**: riusa `SharedContact` (che ha
> già età e TTL), riusa la ricerca, riusa i parametri già autorati. È l'unico intervento che trasforma
> qualitativamente la percezione della credibilità, e costa poche decine di righe.

**Frequenze di aggiornamento** (già impostate correttamente): sensing time-sliced, LOS cap a K=8 candidati.
L'udito è **event-driven** (costo zero quando nessuno spara), non polling.

### §3 World Model & Memory
**Esiste già più di quanto la proposta immagini.** `SharedContact` ha età; `comms` introduce **ritardo** →
gli agenti accorrono dove il nemico **era**. Questo è già "informazione incompleta".

Da aggiungere, in ordine di valore:
1. **Confidenza** esplicita sul contatto (`confidence 0..1`), decrescente col tempo:
   `c(t) = c₀ · e^(−t/τ)`, con τ per tipo di stimolo (vista diretta τ alto, udito τ basso).
   Consumo: sotto una soglia il contatto smette di essere un bersaglio e diventa un **punto da investigare**.
2. **Predizione** (§4.4): estrapolazione lineare smorzata dell'ultima velocità nota,
   `p̂(t) = p_last + v_last · min(t, t_max) · k`, con `k < 1` che degrada con la confidenza. Vietato
   estrapolare oltre `t_max` (~2 s): produce inseguimenti di fantasmi.
3. **Assunzioni false**: sono un *effetto emergente* dei punti 1-2 (accorrere dove non c'è nessuno). **Non
   costruire un sistema dedicato.**

**Individuo vs squadra**: già distinti — la memoria individuale è in `AiComponent` (lastKnown, search),
quella condivisa è `m_contacts` filtrata per raggio/ritardo di fazione. Mantenere questa separazione: è ciò
che rende possibile "chi ha visto sa, gli altri no".

### §4 Communication System
**Esiste** (mailbox + comms degradabili). Raccomandazione: **non aggiungere un "radio system" separato.**
Il canale radio è già modellato come *proprietà della fazione* (`comms[team]`: raggio di condivisione,
ritardo, periodo degli ordini). Aggiungere un secondo meccanismo creerebbe due verità.

Da aggiungere: **priorità** sul contatto condiviso (un "contatto + sotto tiro" deve propagarsi prima di un
"rumore lontano"), banalmente un campo `priority` usato nell'ordinamento del merge.

### §5 Tactical Planner
**Esiste in due varianti già allineate** (stessa formula `sectorTacticalWeight`, changelog 88).
Da aggiungere: **assegnazione di RUOLI**, che è il buco §4.3.

Modello raccomandato (minimo, dentro il canale A):
```
Per ogni fronte selezionato, il comando pubblica anche una COMPOSIZIONE desiderata:
   { suppress: n₁, flank: n₂, hold: n₃ }
L'agente sceglie il ruolo LIBERO più adatto al proprio profilo:
   score(ruolo) = affinità(profilo, ruolo) · (1 − saturazione(ruolo))
   affinità(suppress) ∝ (1 − flank_chance) · cover_preference
   affinità(flank)    ∝ flank_chance · aggression
```
Nota: l'occupancy centrale (`allyTac.claimed`) è **esattamente il meccanismo** che serve per la saturazione
dei ruoli — esiste già, va solo esteso da "posizioni" a "ruoli".

### §6 Strategic Decision Layer — la scelta dell'architettura
Confronto **fatto per questo progetto**, non in astratto:

| Approccio | Pro | Contro **per GFEngine** | Verdetto |
|---|---|---|---|
| **Utility AI** | Continua, pesabile, si fonde con metadata autorati, debuggabile (ogni score è un numero leggibile) | Curve da tarare | ✅ **ADOTTARE** (è già ciò che fa il codice) |
| Behavior Tree | Ottimo per *sequenze* di esecuzione, tooling maturo | Pessimo per scelte *continue e contese* ("quale delle 167 posizioni?"); esplosione di nodi | 🟡 Solo per l'esecuzione, se servirà |
| HFSM | Semplice, prevedibile; già presente (`AiState`) | Transizioni combinatorie quando gli stati crescono | ✅ Mantenere dov'è, non estendere |
| GOAP | Piani emergenti, celebre (F.E.A.R.) | Il piano qui è **piatto** (vai→posizionati→ingaggia): tutto il costo del planner senza il beneficio. Debug difficile, replanning storm | ❌ **RIFIUTARE** |
| Rule system | Semplicissimo | Non scala, rigido | ❌ |
| ML / ONNX | Comportamenti non ovvi | **Distrugge il determinismo** (appena conquistato, changelog 99), **non debuggabile** (il metodo di lavoro di questo progetto è misura→diagnosi→fix), **non autorabile** dal designer, costo di training/inferenza, dipendenza pesante, incompatibile con il replay | ❌ **RIFIUTARE** |

**Raccomandazione: Utility AI formalizzata + FSM per lo stato di combattimento + intento dal canale A.**

Formalizzare significa: rendere esplicito che `sectorTacticalWeight` e `bestOrderPosition` **sono** funzioni
di utilità, e dare loro **curve dichiarate e tarabili** invece di coefficienti sparsi.

> **Stato (changelog 117): primo passo FATTO.** I pesi sono raccolti in
> `include/mini/game/ai/AiUtility.hpp` — 8 bilanci (`kCover`, `kFlank`, `kOverwatch`, `kFiring`, `kHold`,
> `kAdvantage`, `kPicture`, `kSector`) commentati per *intento*, con i valori **esattamente** quelli
> preesistenti: refactor a comportamento invariato, verificato con `--sim-ticks` (128 = baseline).
> **Secondo passo FATTO e in gran parte NEGATIVO** (changelog 136): le curve raccomandate qui sotto
> sono state implementate e misurate con A/B. Il **valore del terreno concavo è stato RIFIUTATO** —
> più acquisizioni ma meno colpi a segno (265 → 211 eventi), e attenua il segnale dell.AUTORE, che è
> il modello del progetto. Il **rischio convesso è adottato ma inerte** (tocca aggiramento e
> overwatch: 5 occorrenze in 6000 tick). Le raccomandazioni sotto restano come teoria, con questa
> misura accanto.
Curve raccomandate (tutte normalizzate 0..1, componibili per prodotto pesato):
```
Minaccia          f(d) = 1 / (1 + (d/d₀)²)            iperbolica: crolla con la distanza
Urgenza numerica  f(Δn) = clamp(Δn / n_max, 0, 1)     lineare nella disparità
Valore terreno    f(imp) = imp^0.7                     concava: rendimenti decrescenti
Rischio           f(exp) = 1 − exp^1.5                 convessa: punisce l'alta esposizione
Prossimità fronte f(d) = 1/(1 + d/HALFDIST)            già implementata (COMMAND_PROXIMITY_HALFDIST)
Copertura         f(p) = p                             lineare (protection già 0..1)
```
Composizione: `U = Σ wᵢ·fᵢ` (additiva, come oggi) — **non** moltiplicativa, perché un fattore a zero
azzererebbe tutto e produce "unità che non fanno nulla", difetto già visto in questo progetto quando
l'importanza veniva schiacciata (KI #81).

**Async**: la proposta chiede un "Strategic Brain asincrono". **Sconsigliato adesso.** Ciò che serve —
non ricalcolare tutto ogni frame — è già ottenuto con **cadenze** (3 s / 0.33 s) a costo zero e **senza**
rischi di determinismo. Vedi §11 per cosa jobificare davvero.

### §7 Tactical Execution Layer
Maturo. Aggiunte raccomandate, in ordine:
1. **Suppression** (§4.2): stato con decadimento
   `s ← min(1, s + impact)` a ogni colpo entro `SUPPRESS_RADIUS`, `s ← s·e^(−dt/τ)` altrimenti.
   Effetti: ↑ probabilità di stare in copertura, ↓ accuratezza, ↓ propensione a esporsi.
   È l'anello che rende utile la soppressione **come ordine** (§9) e come ruolo (§5).
2. **Emergency overrides** come tabella di priorità esplicita, non `if` sparsi:
   `granata > soppressione pesante > HP critici > ordine del player > intento del comando > autonomia`.
3. **Animazione**: **BLOCCATA per decisione del progetto** ([[animations-blocked]]). Il band-aid attuale
   (mira al busto alto + muzzle stimato, KI #82) va sostituito quando le pose si sbloccano. Non progettare
   ora l'integrazione animation-driven: sarebbe design su ipotesi.

### §8 Personality & Skill
Parametri già esistenti: `aggression, accuracy, cover_preference, flank_chance, reposition_chance,
retreat_hp_threshold, reaction_time, peek/hide durations, sight_range, fov_deg, hearing_range`.

Da aggiungere solo se **consumati** (regola di questo progetto: mai authoring inerte — è esattamente
l'errore trovato su fov/hearing): `discipline` (tenuta sotto soppressione), `initiative` (propensione ad
agire senza ordini), `teamwork` (peso della saturazione dei ruoli).

**Recluta / veterano / élite** non devono essere tre valori di accuratezza, ma tre **profili coerenti**:
| | recluta | veterano | élite |
|---|---|---|---|
| reaction_time | 0.7 | 0.4 | 0.2 |
| discipline (panico) | bassa | media | alta |
| cover_preference | alta (si nasconde) | media (usa) | media-alta (sfrutta) |
| flank_chance | bassa | media | alta |
| accuracy | 0.45 | 0.65 | 0.8 |

### §9 Player Command System
**Esiste e ha il modello giusto** (comando = vincolo, non telecomando). I comandi richiesti mappano così:
| Richiesto | Stato |
|---|---|
| Hold Position, Regroup, Flank(→Advance), Secure Area(→Hold su settore) | ✅ esistono |
| Suppress | 🔜 dipende da §7.1 (soppressione) |
| Stack Up / Breach / Clear Room | ❌ richiedono metadata indoor (§1) — **posporre** |

### §10 Engine Architecture
Attuale, corretta: **ECS + mailbox**, `ecs/` indipendente dal codice di gioco (ADR-002/doc 10).
Non passare a OOP-con-gerarchie: perderesti il layout cache-friendly e il seam dei sistemi.

Direzione raccomandata (già iniziata col changelog 95):
```
src/ecs/systems/
  AiSystem.cpp          ← esecuzione per-agente (percezione, combattimento, movimento)
  AiCommandLayer.cpp    ← decisione di teatro (settori, torre, direttive, posizioni ordini)
  AiPerception.cpp      ← NUOVO (§2): vista con FOV + udito event-driven
  AiInternal.hpp        ← seam privato fra queste unità
```

### §11 Performance
**Non ottimizzare ciò che non è stato misurato.** Oggi esistono già: time-slicing, cap K=8 sulle LOS,
griglia di broadphase, commitment che bounda i `findPath`, precompute condiviso della torre.

Stima di scalabilità (per-tick, ordine di grandezza, con l'architettura attuale):
| Agenti | Costo dominante | Nota |
|---|---|---|
| 10-25 | trascurabile | stato attuale, nessun problema |
| 50 | LOS (K·N) + crowd | il precompute della torre **ammortizza** perché è per-fazione, non per-agente |
| 100 | LOS + pathfinding | serve AI LOD (agenti lontani dal player a cadenza ridotta) |
| 200 | pathfinding + crowd | serve sleeping agents + budget per-frame |

Ordine di intervento **quando** servirà (non prima):
1. **AI LOD**: cadenza di decisione in funzione della distanza dal player/telecamera.
2. **Sleeping agents**: chi è fuori contatto da N secondi e senza stimoli scende a 1 Hz.
3. **Job del precompute**: `updateAllyTactical` è **read-only sul mondo e write su buffer proprio** →
   è l'unico candidato *sicuro* alla parallelizzazione. Tutto il resto tocca stato condiviso.
4. SIMD: solo sui test di distanza in blocco. Ultimo, guadagno modesto.

**Determinismo**: da preservare come requisito di primo livello — è ciò che rende possibile
`--sim-ticks` e quindi ogni verifica seria (changelog 99). Qualunque proposta di async che lo comprometta va
respinta o resa opt-in per il solo profiling.

### §12 Debugging & Developer Tools
**Il progetto è già forte qui** e deve continuare su questa strada, perché è ciò che ha permesso di scoprire
che l'AI verticale *non* era rotta (KI #83).
Esistenti: telemetria JSONL a contatori (funnel), eventi tattici, editor con esposizione e **visuale
verticale**, `--validate`, Tracy, dump di stato F12.
Da aggiungere, in ordine di utilità reale:
1. **Ispettore di utilità** (perché *questo* agente ha scelto *quella* posizione): stampare i primi 3 score
   candidati nel dump di stato. Costo bassissimo, valore diagnostico enorme.
2. **Visualizzazione della memoria**: dove l'AI *crede* che sia il nemico (rende leggibile l'informazione
   invecchiata — altrimenti sembra "AI stupida" quando invece è "AI male informata").
3. Influence map: **solo se** si introdurrà. Oggi i settori pesati fanno lo stesso lavoro a costo minore.

### §13 Testing Strategy
Base già solida e appena rafforzata:
- `--sim-ticks N` → confronti **deterministici** (155/155/155 verificato);
- `--validate` → gate dei contenuti headless;
- telemetria a funnel → guardie di regressione *comportamentali*, non solo "non crasha".

Da aggiungere: **scenari tattici riproducibili** (mappa minima + posizioni fisse + asserzioni sui contatori,
es. "in 3000 tick, ≥N ingaggi cross-quota"). È la naturale evoluzione del funnel di verticalità.

---

## 6. Roadmap raccomandata

### Fase 1 — Percezione credibile *(la più alta priorità, costo basso)*
1. ✅ **FOV** con fascia periferica + "sparare rende visibili" — **FATTO (changelog 100)**.
2. ✅ **Udito event-driven** (spari) → contatti imprecisi — **FATTO (changelog 100)**, con correzione della
   semantica di `hearing_range` (sensibilità relativa) guidata dalla misura.
3. ▶ **Confidenza** sui contatti + decadimento — da fare (abilita §3 e la Fase 4).
   Rimangono fuori: rumore dei passi, esplosioni, silenziatori (stessa mailbox, costo marginale).
> Dipendenze: nessuna. Rischio: basso. Impatto: **massimo** sulla credibilità.
> Verifica: funnel di percezione (contatti per fonte: vista/udito) + `--sim-ticks`.

### Fase 2 — Soppressione e ruoli *(il salto "militare")*
4. **Suppression state** + effetti su accuratezza/copertura.
5. **Ruoli di squadra** (suppress/flank/hold) via saturazione, riusando `allyTac.claimed`.
6. Comando player **Suppress**.
> Dipendenze: 4 → 5 → 6. Rischio: medio (tarare senza paralizzare le AI).

### Fase 3 — Formalizzazione utility
7. Estrarre le curve in un modulo dichiarativo e **tarabile da editor**.
8. Ispettore di utilità nei tool.
> Rischio: basso (refactor a comportamento invariato, verificabile con `--sim-ticks`).

### Fase 4 — Predizione e memoria avanzata
9. Estrapolazione limitata; investigazione guidata dalla confidenza.

### Fase 5 — Scala *(solo quando servirà davvero)*
10. AI LOD, sleeping agents, job del precompute.

### Da NON costruire ora (e perché)
| Sistema | Motivo |
|---|---|
| ML / ONNX | Distrugge determinismo, debuggabilità, autorabilità. Costo altissimo, valore incerto |
| GOAP | Il piano è piatto: costo del planner senza beneficio |
| Room clearing / breaching | Le mappe attuali non hanno metadata indoor: si costruirebbe su un vuoto |
| Influence map dedicata | I settori pesati fanno già lo stesso lavoro |
| Nuove strutture spaziali | La scansione lineare non è nel top-3 dei costi |
| Integrazione animation-driven | Pose bloccate per decisione del progetto |

### Rischi tecnici principali
1. **Tarare la soppressione** (rischio: AI paralizzate) → mitigazione: funnel di telemetria + `--sim-ticks`.
2. **FOV troppo netto** (rischio: "mi sta di fianco e non mi vede") → mitigazione: fascia periferica.
3. **Regressione durante la formalizzazione utility** → mitigazione: refactor verbatim + confronto
   deterministico.
4. **Scope creep** (rischio maggiore per un solo sviluppatore) → mitigazione: la tabella "da NON costruire".

---

## 6-bis. Percorso evolutivo — tenere aperte le porte senza pagarle adesso

Richiesta utente (2026-07-27): *"ML/ONNX e GOAP ora non servono, ma in futuro potrebbero servirci per avere AI
veramente vive. Le mappe saranno molto profonde, con più fronti, obiettivi, bersagli, obiettivi secondari, e
una grande varietà di approcci possibili, ognuno coi suoi vantaggi e svantaggi."*

Obiezione accolta: il rifiuto in §6 è un rifiuto **all'adozione oggi**, non una chiusura permanente. Ma la
risposta onesta a quello scenario cambia la conclusione in tre punti.

### 6-bis.1 Il collo di bottiglia futuro NON è l'algoritmo di decisione
Lo scenario descritto ha tre requisiti distinti, e solo uno è un problema di algoritmo:

| Requisito | Cosa lo risolve davvero | Stato |
|---|---|---|
| Molte opzioni **significative** | Ricchezza del MONDO (metadata, obiettivi, vie alternative con costi diversi) | Base già forte; manca il legame AI↔obiettivi |
| Scegliere fra opzioni con **trade-off** | Utility multi-obiettivo (pesare valore vs rischio vs tempo) | Utility già presente, va estesa agli obiettivi |
| Piani **profondi** (A per fare B per fare C) | Planner gerarchico | Non serve finché i piani restano piatti |

> Se il mondo offre tre vie con vantaggi diversi e l'AI ha buoni metadata + utility, **sceglie già diversamente**
> a seconda di personalità e situazione. Un planner non aggiunge varietà se la varietà non è nel mondo:
> aggiungerlo prima significherebbe pianificare sul vuoto.

**Conclusione**: l'investimento che compra "AI vive su mappe profonde" è **collegare gli OBIETTIVI al livello
decisionale**, non sostituire il decisore.

### 6-bis.2 Se un giorno servirà un planner: **HTN, non GOAP**
Correzione esplicita alla proposta originale. Per questo progetto HTN è superiore su ogni asse che qui conta:

| | GOAP | **HTN** |
|---|---|---|
| Come produce il piano | Ricerca all'indietro nello spazio degli stati (A*) | Decomposizione in avanti di compiti, con metodi **autorati** |
| Controllo del designer | Basso: il piano emerge, si "scopre" cosa farà | **Alto**: il designer scrive i metodi ("assalto = fissa + aggira") |
| Debug | Difficile: perché ha scelto *questo* piano fra migliaia? | **Facile**: l'albero di decomposizione è leggibile |
| Costo CPU | Ricerca, potenzialmente esplosiva; replanning storm | Decomposizione limitata dalla profondità dei metodi |
| Determinismo | Sensibile a euristiche/ordinamenti | **Deterministico** per costruzione |
| Compone con Utility | Male (sostituisce) | **Bene**: l'utility sceglie QUALE metodo usare a ogni nodo |
| Precedenti | F.E.A.R. | Killzone 2/3, Horizon Zero Dawn, Transformers |

Il punto decisivo per GFEngine: **HTN si somma all'utility invece di sostituirla.** L'ibrido raccomandato per
il futuro è *"HTN per la struttura del piano, Utility per la scelta locale del metodo"* — nessuna riscrittura
del livello tattico, che resta com'è.

### 6-bis.3 ML: sì, ma **offline**, non a runtime
Il rifiuto di §6 riguarda l'inferenza a runtime (determinismo, debug, dipendenza). Esiste però un uso di ML
che **rispetta tutti i pilastri** e che questo progetto è già attrezzato a sfruttare:

> **Tarare le curve di utility a partire dalla telemetria.** `_telemetry_data/session_latest.jsonl` è già un
> dataset: contatori di funnel, esiti, distribuzioni. Un'ottimizzazione offline (anche solo una ricerca
> parametrica, senza reti neurali) può cercare i pesi che massimizzano metriche di gioco dichiarate
> ("ingaggi cross-quota", "fronti coperti", "durata media dello scontro").

Il risultato dell'addestramento è **una tabella di numeri leggibili** che finisce nei dati autorati: a runtime
non gira nessuna rete, il determinismo resta intatto, e il designer può correggere a mano. È ML che entra
come *strumento di sviluppo*, non come dipendenza di gioco. **Questa porta è già aperta e non costa nulla.**

### 6-bis.4 Le tre CUCITURE da rispettare per non chiudersi le porte
Non serve costruire nulla di futuro adesso. Serve non fare tre errori che renderebbero costoso il futuro:

**Cucitura 1 — Il contratto delle direttive (esiste già).**
`World::EnemyCommand::Directive` (posizione, raggio, stance, peso, etichetta) è il confine fra *chi decide* e
*chi esegue*. Finché il livello tattico consuma **solo** questo contratto, il decisore può essere sostituito
(utility → HTN → altro) **senza toccare l'esecuzione**. Regola: mai far leggere all'esecuzione lo stato
interno del decisore.

**Cucitura 2 — Gli OBIETTIVI come dato di primo livello (da fare, §Fase 4).**
Oggi il decisore ragiona su *settori* (importanza statica + pressione). Per mappe con obiettivi primari e
secondari serve che ragioni su **obiettivi** con: valore, scadenza, prerequisiti, fazione interessata, stato.
`ObjectiveSystem` (doc 25) esiste già ma **non alimenta il livello decisionale dell'AI**: è il collegamento
mancante, ed è il vero abilitatore dello scenario "mappe profonde".

**Cucitura 3 — Repertorio di AZIONI dichiarato (economico, abilita l'HTN).**
Un planner ha bisogno di azioni con **precondizioni ed effetti**. Oggi le azioni sono implicite nel codice
(avanza, copriti, aggira, sopprimi). Dichiararle come dati — anche mantenendo l'utility come selettore —
costa poco e rende un HTN futuro un'**aggiunta**, non una riscrittura:
```cpp
struct TacticalAction {
    const char* id;              // "suppress", "flank", "advance_bound", "regroup"
    uint32_t    requires;        // bitmask: HAS_TARGET | HAS_COVER | AMMO_OK | SQUAD_2PLUS
    uint32_t    provides;        // ENEMY_SUPPRESSED | POSITION_GAINED | CONTACT_RESTORED
    float       baseCost;        // tempo/rischio previsto
};
```
Con questo, "HTN in futuro" significa scrivere i **metodi** di decomposizione, non rifare l'AI.

### 6-bis.4-bis GOAP: c'è un uso legittimo, ma NON quello che sembra
Domanda utente: *"sicuro che GOAP non convenga per niente? magari come layer parallelo di supporto, per
reazioni meno programmate, o per aiutare la catena di comando a fare piani più vari e creativi."*

**Dove l'intuizione è corretta.** C'è una cosa che GOAP fa e che HTN e utility **non possono fare per
costruzione**: produrre piani che il designer non ha previsto. HTN decompone secondo metodi *autorati* — la
varietà è limitata da ciò che hai immaginato. GOAP cerca all'indietro nello spazio degli stati e **compone**
azioni in modi non previsti. Il "meno programmato" è reale e solo la ricerca lo dà.

**Dove il presupposto va corretto.** Il mito su F.E.A.R. è che l'intelligenza venisse dalla profondità dei
piani: i piani erano **profondi 2-3 azioni** su ~10 azioni totali, e — parole di Orkin — le **manovre di
squadra NON erano GOAP** (layer di coordinamento separato). Gran parte dell'intelligenza percepita veniva dai
**barks** (l'AI dichiara l'intento: *"sta aggirando!"*) e dal level design. Adottare GOAP aspettandosi piani
creativi profondi compra una promessa che non mantiene.

**Il valore vero, isolato**: il **concatenamento automatico su precondizione bloccata**.
```
"sparare al bersaglio" → manca LOS → [muovi a posizione con LOS] → spara
"sparare"              → munizioni esaurite → [copriti] → [ricarica] → [riesponi] → spara
```
Con l'utility ogni ripiego va **scritto a mano**; i casi combinatori (bloccato *e* a secco *e* soppresso)
degenerano in un albero di `if` ingestibile. Con la ricerca all'indietro emergono da soli.

**Raccomandazione differenziata**:
| Livello | Verdetto | Perché |
|---|---|---|
| Catena di comando (fronti, settori) | ❌ **NO** | Le decisioni di comando devono essere **leggibili** (debug + il giocatore le subisce). Un piano creativo ma incomprensibile si legge come bug. La varietà lì deve nascere dalla SITUAZIONE, non dalla ricerca. Determinismo e ispezionabilità valgono più della sorpresa |
| Singolo soldato, **solo su azione bloccata** | 🟡 **SÌ, condizionale** | Risolutore di precondizioni a profondità ≤3 su ~8-10 azioni. Non gira a ogni tick: **solo quando l'azione desiderata fallisce**. Deterministico, ispezionabile, costo ~zero quando nessuno è bloccato |

**Costo reale**: quasi nullo *se* si è fatta la **Cucitura 3** (azioni dichiarate con `requires`/`provides`):
quella tabella **è** il grafo su cui il risolutore cerca. Il risolutore sono ~150 righe, non un'architettura.

**Trigger oggettivo per aprirla**: quando ci si accorge di scrivere a mano il terzo/quarto ripiego annidato
("se non posso X allora Y, ma se anche Y è bloccato allora Z"). Quello è il segnale che il problema è
diventato di ricerca. Prima, l'utility con buona percezione fa lo stesso lavoro con meno pezzi.

> **Nota importante sulla percezione di "reazioni programmate"**: nella grande maggioranza dei casi NON nasce
> dall'algoritmo di decisione, ma da tre cose — reagire sempre uguale (nessuna varianza di personalità),
> assenza di memoria, informazione perfetta. Sono esattamente i tre bersagli della Fase 1.

### 6-bis.5 Aggiornamento della roadmap
Le fasi 1-3 restano invariate (percezione → soppressione/ruoli → formalizzazione utility). Si aggiunge:

- **Fase 4 (rivista) — Obiettivi nel decisore**: l'utility passa da "quale settore" a "quale obiettivo, e
  quale settore lo serve". È ciò che rende sensati approcci alternativi con trade-off reali.
- **Fase 6 (nuova, condizionale) — HTN**: solo **quando** i piani diventeranno davvero profondi (più passi
  interdipendenti). Trigger oggettivo per aprirla: *quando l'utility richiederà termini che codificano
  sequenze* ("vai lì per poi poter fare X") — quello è il segnale che il problema è diventato di pianificazione.
- **Trasversale — ML offline**: utilizzabile da subito come strumento di tuning, senza impegno architetturale.

> Principio guida: **si paga il futuro con le cuciture, non con le implementazioni.** Le cuciture costano
> poche ore; le implementazioni premature costano mesi e vanno mantenute anche quando sbagliate.

---

## 7. Verdetto finale

**Architettura proposta: MODIFICARE.**
- Mantenere la stratificazione e la separazione decisione/esecuzione: sono corrette e già realizzate.
- **Invertire la direzione del controllo**: il comando pubblica intento e fatti; l'agente resta sovrano.
  Non è preferenza stilistica — è la conclusione di un esperimento già fatto e revertato in questo progetto.
- **Rifiutare ML/ONNX e GOAP**; adottare **Utility AI formalizzata + FSM**, che è già la sostanza del codice.
- **Spostare la priorità dal livello strategico alla PERCEZIONE**: nessun cervello strategico rende credibile
  un soldato che vede a 360° e non sente gli spari. Oggi è così, e i parametri per ripararlo sono già
  autorati nei profili.

> La domanda giusta per questo progetto non è *"quale architettura AI adottare"* — quella c'è ed è sana.
> È *"quali informazioni ha il soldato, e quanto sono imperfette"*. La credibilità nasce lì.

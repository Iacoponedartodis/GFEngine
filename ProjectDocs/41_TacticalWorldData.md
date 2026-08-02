# 41 — Tactical World Data, pipeline geometrie/Blender ed evoluzione dell'editor

Status: **Proposed** · Data: 2026-07-27 · Companion di [40_AiArchitecture](40_AiArchitecture.md)
Scope: rappresentazione tattica del mondo, metadata, pipeline asset, editor, roadmap. **Nessun codice.**

---

## 0. Verdetto esecutivo

1. **Il "mondo a box" NON è un prototipo da superare: è il modello tattico del gioco.** Verificato:
   `MapGeometryBox` è solo primitive; il **navmesh si genera dai box**; `hasLineOfFire` è uno **slab test sui
   box**, veloce *proprio perché* sono box. Sostituirli con mesh importate romperebbe LOS, navmesh e ogni
   ragionamento tattico. → **Blender aggiunge il VISIVO sopra, non sostituisce la geometria tattica.**
2. **Il problema di scala non è la qualità dei metadata: è che si autorano per ISTANZA.** Oggi Training
   Ground ha 167 posizioni piazzate a mano. Mappe "molto profonde" ne vorrebbero 1000+: non scala.
   → **Autorare il significato tattico UNA VOLTA per ASSET (prefab), non una volta per istanza.** È la
   singola decisione che abilita mappe profonde con un team di una persona.
3. **Automatizzare l'ANALISI e la VALIDAZIONE, non la CREAZIONE.** L'auto-generazione di coperture dalla
   geometria è già stata provata e rimossa (ADR-026) perché produceva risultati insensati. Ciò che invece
   funziona è già dimostrato in questo progetto: lo strumento di **visuale verticale** (changelog 97) non
   crea nulla, *misura* e dice al designer cosa è sbagliato. È il modello da estendere.
4. **Poche cose si possono derivare automaticamente e bene**: indoor/outdoor, choke point, esposizione, link
   di copertura, punteggio di una posizione. **Molte no**: dove *ha senso* una posizione, cosa è "importante",
   dove passa il fronte. Quelle restano al designer.
5. **Sulla combinazione Utility+BT+HTN+GOAP**: sì, ma con una regola rigida — **una tecnica per tipo di
   domanda, mai due tecniche che rispondono alla stessa**. Senza questa disciplina la combinazione moltiplica
   la superficie di debug invece della qualità (§8).

---

## 1. Audit del progetto

### 1.1 Punti di forza da PRESERVARE (non riscrivere)
| Sistema | Perché è un asset | Implicazione |
|---|---|---|
| **Geometria a box** | LOS analitica velocissima (slab test), navmesh pulito, editing immediato | È la **verità tattica**: non toccarla |
| **`TacticalPositionDef` unificata** (ADR-030) | Ruolo + protezione + altezza + arco/gittata + importanza: già espressiva | Estendere, non rifare |
| **Grafo derivato** `positionCovers`/`positionExposure` (ADR-032/033) | Ricalcolato al load → **non può diventare stale** | Modello da replicare per ogni nuovo dato derivato |
| **Settori** (ADR-034) + `SectorState` | Astrazione di teatro già usata da comando e torre | Base per obiettivi (doc 40, Cucitura 2) |
| **Query layer `worldintel`** (ADR-025) | Seam unico: una sola verità tattica | Punto d'innesto di ogni analisi nuova |
| **Editor con dati derivati** (esposizione, visuale verticale) | Feedback immediato al designer | **Il modello di automazione giusto** |
| **Telemetria a funnel + `--sim-ticks`** | Verifica deterministica del comportamento | Come si validano i metadata, non a occhio |
| **Validazione contenuti** (`--validate`, ADR-018) | Gate headless già esistente | Estendibile ai difetti tattici |

### 1.2 Limiti reali (in ordine di gravità)
1. **Authoring per istanza → non scala.** 167 posizioni a mano su una mappa piccola. Nessun concetto di
   *prefab*: piazzare un edificio non porta con sé né collisione né significato tattico.
2. **Nessuna semantica di spazio**: non esiste "stanza", "interno/esterno", "porta", "corridoio". Senza,
   comportamenti come *breach*, *clear room*, *guarda l'ingresso* non sono nemmeno esprimibili.
3. **Nessuna pipeline per geometria esterna**: `MapGeometryBox` non ha `meshPath`. Il motore carica GLB per
   unità/armi/veicoli/bersagli, **non** per il livello.
4. **Metadata piatti**: importanza è un numero globale, non dipende dal contesto (chi attacca? da dove?).
5. **Nessun collegamento obiettivi ↔ AI** (già rilevato in doc 40, Cucitura 2).
6. **Scalabilità del bake O(N²)**: `buildTacticalLinks` è quadratico. A 167 posizioni costa ~2.4 ms; a 1500
   sarebbero ~2.25 M coppie → secondi. Va reso spazialmente limitato **prima** delle mappe profonde (§9).

---

## 2. Tesi centrale

> **(a) Il significato tattico si autora una volta per ASSET e si moltiplica per istanza.**
> **(b) La macchina ANALIZZA e VALIDA; l'uomo CREA e DECIDE.**

Corollario operativo: ogni volta che si è tentati di scrivere "genera automaticamente le coperture", la
domanda giusta è: *"posso invece far autorare la copertura una volta sull'asset, e poi far VERIFICARE alla
macchina che funzioni dove l'ho piazzato?"* La risposta è quasi sempre sì, ed è molto più economica e
robusta.

---

## 3. Architettura dei metadata — tre livelli

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ L1 — ASSET / PREFAB  (autorato UNA volta, riusato ovunque)                    │
│   · mesh visiva (GLB da Blender)                                             │
│   · proxy di COLLISIONE: 1..N box (la verità fisica e tattica)               │
│   · posizioni tattiche RELATIVE (cover dietro il muretto, vantage sul tetto)  │
│   · semantica: is_building, is_cover, occludes, indoor_volume                 │
├──────────────────────────────────────────────────────────────────────────────┤
│ L2 — ISTANZA  (autorato per piazzamento, poco e mirato)                      │
│   · trasformazione (pos, rot, scala uniforme)                                │
│   · override locali: importanza, disabilita-posizione, team owner            │
│   · annotazioni di teatro: settori, obiettivi, danger, route                  │
├──────────────────────────────────────────────────────────────────────────────┤
│ L3 — DERIVATO  (mai autorato, mai salvato, ricalcolato) ← già il modello ADR-033 │
│   · link copertura/esposizione · visuale verticale · indoor/outdoor           │
│   · choke point · punteggio posizione · quadro tattico runtime (allyTac)      │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Regola d'oro (già in vigore, da estendere)**: *un dato derivato non si salva mai su file.* Se si salva,
prima o poi diverge dalla geometria e mente. Il costo di ricalcolarlo è il prezzo della sua verità.

**Come l'AI vi accede**: **solo** attraverso `worldintel` (query pure) e i buffer runtime (`allyTac`,
`sectorStates`). Nessun sistema AI deve leggere direttamente le strutture di mappa: è la regola che ha già
evitato la "doppia verità" e che ha causato il fallimento del changelog 77 quando è stata violata.

---

## 4. Tactical Node System — verdetto

**Esiste già** ed è buono: `TacticalPositionDef` **è** un tactical node, con ruolo, capacità di tiro, arco,
gittata, protezione, importanza, più un **grafo di relazioni** derivato. Non va reimplementato.

Cosa aggiungere, in ordine di valore:
1. **Provenienza del nodo**: `source = { handPlaced, fromPrefab }`. Serve per non perdere le modifiche a mano
   quando un prefab viene aggiornato, e per sapere cosa si può rigenerare.
2. **Direzione di minaccia**: oggi `facing` è assoluto. Una copertura è buona *rispetto a da dove arriva il
   nemico*: il punteggio deve essere funzione della direzione di minaccia corrente (già parzialmente fatto
   nel tower-hub, va formalizzato).
3. **Capacità** invece di soli ruoli: `canShoot` esiste; servono `canCrouch`, `blocksMovement`,
   `entryPoint` — perché i comportamenti futuri (breach, guardia dell'ingresso) interrogano *capacità*.
4. **Validità dinamica**: se la copertura è distruttibile e muore, il nodo esce dal punteggio (§5.4).

### Come evitare il fallimento dell'auto-generazione (ADR-026)
Il tentativo precedente falliva perché generava candidati **dalla geometria** con euristiche sulle facce dei
box: nessun criterio tattico, nessuna verifica, nessun contesto → migliaia di posizioni prive di senso.
Il modello corretto è invertito:

```
   PREFAB (umano, una volta)          →   candidati SENSATI per costruzione
        ↓ piazzamento
   ANALISI automatica (macchina)      →   validazione + punteggio nel contesto reale
        ↓
   REPORT al designer                 →   "queste 12 posizioni sono cieche / ridondanti / esposte"
```
La macchina non inventa: **filtra, misura, segnala**. È esattamente ciò che fa già la visuale verticale.

---

## 5. Pipeline di analisi automatica — cosa è realistico

### 5.1 Fattibile, economico, alto valore ✅
| Analisi | Come | Costo |
|---|---|---|
| **Indoor / outdoor** | Per ogni poligono navmesh, raycast verso l'alto: se colpisce geometria entro ~6 m → interno | Bassissimo (una LOS per poly, a bake) |
| **Choke point** | Sul grafo del navmesh: archi la cui rimozione **sconnette** due regioni ampie, o poligoni di larghezza < soglia fra due aree grandi | Medio (analisi di grafo a bake) |
| **Esposizione** | Già implementata (ADR-033) | Già pagato |
| **Visuale verticale** | Già implementata (changelog 97) | Già pagato |
| **Ridondanza posizioni** | Due nodi che coprono lo stesso insieme di bersagli entro X m → uno è superfluo | Basso |
| **Punteggio contestuale** | Utility su (protezione, esposizione, LOS verso direzione di minaccia, quota) | Basso, già in `allyTac` |
| **Copertura del fronte** | Per ogni settore: esiste ≥1 posizione che batte le vie d'accesso? Se no → **buco tattico** | Basso |

### 5.2 Fattibile ma costoso 🟡
- **Stanze** come entità (non solo indoor/outdoor): componenti connesse di poligoni interni separate da
  choke point/porte. Utile solo *quando* serviranno breach/clear room.
- **Ingressi/uscite**: archi del navmesh che attraversano il confine indoor/outdoor. Deriva dalle stanze.

### 5.3 Da NON automatizzare ❌
- **Dove è tatticamente sensato stare** (l'intento del level design);
- **Importanza** di un settore/posizione (è scelta di gameplay, non proprietà geometrica);
- **Dove passa il fronte** (dipende dal design della battaglia);
- **Ruolo** di una posizione (una finestra può essere osservazione o postazione: lo decide il designer).

### 5.4 Copertura distruttibile
Non serve un sistema nuovo: `strategicTargets` ha già entità, HP e collider. Un prefab può dichiarare che le
sue posizioni dipendono da un pezzo distruttibile; quando muore, quelle posizioni escono dal punteggio al
successivo ricalcolo di `allyTac` (già periodico, 0.33 s). **Costo: quasi nullo.**

---

## 6. Pipeline geometrie Blender ↔ motore — la decisione più importante

### 6.1 Il principio
```
        BLENDER                              MOTORE
   ┌────────────────┐                 ┌──────────────────────────┐
   │ mesh VISIVA    │ ──── GLB ────▶  │ rendering (già esiste)   │
   │ (dettagliata)  │                 └──────────────────────────┘
   │                │                 ┌──────────────────────────┐
   │ proxy COLLISIONE│ ─ convenzione ▶│ MapGeometryBox[]         │ ← verità fisica E tattica
   │ (pochi box)    │   UCX_/BOX_     │  · LOS slab (veloce)     │
   │                │                 │  · navmesh (già)         │
   │ empties TATTICI│ ─ convenzione ▶ │ TacticalPositionDef[]    │ ← significato, autorato 1 volta
   │ (TP_cover_...) │   TP_<ruolo>    │                          │
   └────────────────┘                 └──────────────────────────┘
```

**Regola non negoziabile**: la mesh visiva **non** entra mai nella collisione, nella LOS o nel navmesh.
Motivo tecnico verificato: LOS e navmesh sono costruiti sui box; sostituirli con triangle soup significherebbe
LOS 1-2 ordini di grandezza più lenta (e il costo LOS è già il dominante), navmesh peggiore, e la perdita
dell'analisi tattica analitica. Questo è anche il pattern standard AAA (visual mesh + collision proxy).

### 6.2 Convenzioni raccomandate (semplici, testuali, versionabili)
| In Blender | Diventa nel motore |
|---|---|
| Mesh normale | Visivo (GLB) |
| Empty/cubo `UCX_box_*` | `MapGeometryBox` con `collider = true` |
| Empty `TP_cover_<n>` (con rotazione) | `TacticalPositionDef` role=cover, `facing` dalla rotazione |
| Empty `TP_vantage/observation/defensive/chokepoint_<n>` | idem, col ruolo corrispondente |
| Empty `ENTRY_<n>` | punto d'ingresso (per stanze/breach, futuro) |
| Custom property `indoor=true` sull'oggetto | volume interno |

Perché convenzioni per **nome** e non un formato custom: costano zero tooling, funzionano con qualsiasi
versione di Blender, sono leggibili in un diff, e sono lo standard de facto (UE usa `UCX_`).

### 6.3 Il PREFAB come unità di lavoro
```
data/prefabs/bunker_small.json
{
  "mesh": "assets/models/env/bunker_small.glb",
  "collision": [ {box}, {box}, {box} ],          // proxy, coordinate LOCALI
  "tactical":  [ {role, x,y,z, facing, ...}, ... ],  // LOCALI
  "indoor":    [ {box} ],
  "tags": ["building", "hard_cover"]
}
```
Il piazzamento in mappa diventa:
```
"prefabs": [ { "id": "bunker_small", "x":.., "y":.., "z":.., "ry":.. } ]
```
Al load il motore **espande** il prefab: box → `geometry`, posizioni → `tacticalPositions` (trasformate).

**Questo è il moltiplicatore**: autori le posizioni tattiche del bunker una volta; ogni bunker piazzato le
porta con sé. È ciò che rende possibile una mappa da 1000 posizioni con l'authoring di oggi.

**Punto critico da decidere ora**: le posizioni espanse dal prefab sono **derivate** (L3, non salvate,
rigenerate) mentre gli override a mano sono **autorati** (L2, salvati). Tenere le due cose distinte
(`source`) è ciò che permette di aggiornare un prefab senza perdere il lavoro manuale.

### 6.4 Cosa NON fare
- ❌ Importare l'intera mappa come singola mesh Blender (perdi collisione, navmesh, tattica, editing);
- ❌ Generare collisione automatica dalla mesh visiva (convex decomposition): risultati imprevedibili,
  costosi, e non danno *significato*;
- ❌ Sostituire i box con mesh "perché più belli": il bello lo fa il layer visivo.

---

## 7. Evoluzione del Map Editor — minimo indispensabile

Non ricostruire nulla. In ordine di valore:

1. **Piazzamento di prefab** (nuovo): lista, anteprima, gizmo, espansione visibile. È il 90% del valore.
2. **Pannello "Salute tattica della mappa"** (estende la visuale verticale): un solo pannello che elenca i
   difetti trovati dall'analisi — posizioni cieche, ridondanti, settori senza copertura, choke point non
   presidiati. **Cliccabile** → seleziona l'elemento. Questo è il vero salto di produttività.
3. **Visualizzazione dell'analisi**: indoor/outdoor, choke point, direzione di minaccia (già c'è esposizione
   e visuale verticale — stessa famiglia).
4. **Filtri di vista**: con 1000 posizioni la lista attuale diventa inusabile → filtro per ruolo/difetto.
5. **Estensione di `--validate`** ai difetti tattici: il gate headless esiste già (ADR-018), aggiungere le
   regole tattiche lo rende utilizzabile in CI e da un LLM senza aprire l'editor.

> Nota UX (vincolo del progetto, [[ui-no-clipping-use-dropdowns]]): con la crescita dei pannelli, raggruppare
> in sezioni/dropdown — mai far tagliare i comandi.

---

## 8. Allocazione delle tecniche AI (Utility / BT / HTN / GOAP)

Richiesta: *"la giusta combinazione di Utility, BT, HTN, GOAP in un ecosistema ben studiato"*. Accolta, con
una **regola di disciplina** senza la quale la combinazione peggiora il sistema invece di migliorarlo:

> **Una tecnica per tipo di domanda. Mai due tecniche che rispondono alla stessa domanda.**
> Ogni tecnica aggiunta moltiplica la superficie di debug: il costo non è la somma, è il prodotto.

| Domanda | Tecnica | Stato |
|---|---|---|
| *Quanto conta questo posto / bersaglio / fronte?* (scelta continua e contesa) | **Utility** | Già in uso, da formalizzare |
| *In che stato di combattimento sono?* (poche modalità grosse) | **FSM** | Già in uso — **non farla crescere** |
| *In che ORDINE eseguo questa manovra?* (sequenze autorate: ricarica→sporgi→spara, entra in stanza) | **Behavior Tree** | Da introdurre **solo** con manovre multi-passo (Fase 2-3) |
| *Come scompongo un obiettivo di missione in compiti di squadra?* | **HTN** | Fase 4+, quando gli obiettivi saranno profondi |
| *L'azione che voglio è bloccata: come la sblocco?* | **Risolutore stile GOAP**, profondità ≤3, solo su fallimento | Fase 6, condizionale |
| *Quali pesi usare?* | **ML offline** sulla telemetria → tabella di numeri leggibili | Strumento, non dipendenza |

Il punto di equilibrio: **Utility decide COSA, BT/HTN decidono COME e IN CHE ORDINE, GOAP interviene solo
quando il COME fallisce.** Nessuna delle quattro ricalcola ciò che un'altra ha già deciso.

---

## 9. Performance — bake vs runtime

| Quando | Cosa | Vincolo |
|---|---|---|
| **Editor (a modifica)** | Esposizione, visuale verticale, validazione, ridondanza | Interattivo: < ~100 ms |
| **Bake (a salvataggio)** | Indoor/outdoor, choke point, espansione prefab | Anche secondi |
| **Load** | Navmesh, `buildTacticalLinks` | Oggi 2.4 ms/60 pos — **attenzione: O(N²)** |
| **Runtime periodico** | `allyTac` (LOS posizioni×nemici), settori | 0.33 s, per fazione |
| **Runtime per-tick** | Query, decisioni | Micro |

**Il rischio di scala numerico**, da affrontare *prima* delle mappe profonde.
**Misurato 2026-07-27** (non stimato): Training Ground **167 posizioni → 7.9 ms**; firebase 60 → 0.43 ms.
La crescita è quadratica, quindi:
```
buildTacticalLinks: O(N²) LOS      [misure reali in grassetto]
    60 posizioni  →   ~3.6 K coppie →  **0.43 ms**
   167 posizioni  →   ~28 K coppie  →  **7.9 ms**   (oggi, accettabile)
   500 posizioni  →   ~250 K        →  ~70 ms       (si inizia a sentire)
  1000 posizioni  →   ~1 M          →  ~280 ms
  1500 posizioni  →   ~2.25 M       →  ~640 ms      (inaccettabile al load)
```
> **Correzione di una stima precedente**: questo documento indicava ~20 ms a 500 posizioni. Il dato reale
> dice ~70 ms, cioè **~3× peggio**. La soglia d'intervento va quindi abbassata da ~400 a **~300 posizioni**.

Mitigazione (semplice): **limitare le coppie alla gittata massima di tiro** con una griglia spaziale — due
posizioni oltre `fireRange` non possono coprirsi, quindi il test è sprecato. Porta il costo da O(N²) a
~O(N·k). *Non prima della soglia*: oggi 7.9 ms al load non è un problema.

Analogamente `allyTac` è O(posizioni × nemici): a 1500 posizioni × 20 nemici = 30 K LOS ogni 0.33 s → serve
il filtro per settore attivo (calcolare solo le posizioni nei settori contesi). Anche questo **dopo**, non ora.

**Multithreading**: un solo candidato sicuro, come già stabilito in doc 40 §11 — il precompute `allyTac`
(read-only sul mondo, scrive su buffer proprio). Il bake dell'editor è naturalmente parallelo ma non è nel
percorso critico. **Il determinismo resta un requisito di primo livello** (changelog 99).

---

## 10. Roadmap

| Fase | Obiettivo | Dipendenze | Difficoltà | Rischio |
|---|---|---|---|---|
| **0 — Audit** | ✅ **Fatto in questo documento** | — | — | — |
| **1 — Fondazione dati** | `source` sui nodi (hand/prefab); formato **prefab**; espansione al load | nessuna | Media | Basso: formato dati, verificabile con `--validate` |
| **2 — Editor: prefab** | Piazzamento prefab + anteprima + gizmo | F1 | Media | Medio: UX, e il rischio di perdere override manuali → mitigato da `source` |
| **3 — Analisi & validazione** | Indoor/outdoor, choke point, ridondanza, buchi di copertura + **pannello Salute tattica** + regole in `--validate` | F1 | Media | Basso: sono dati derivati, se sbagliati non rompono il gioco |
| **4 — Blender** | Convenzioni `UCX_`/`TP_`, importatore GLB→prefab | F1-F3 | Alta | **Il più alto**: pipeline esterna, versioni Blender, controllo qualità degli asset |
| **5 — AI su dati ricchi** | Obiettivi nel decisore (doc 40 Cucitura 2); capacità dei nodi; comportamenti indoor | F3, doc 40 F1-F3 | Alta | Medio |
| **6 — Dinamico** | Copertura distruttibile, danger dinamici, aggiornamento incrementale | F5 | Alta | Medio |

**Dipendenza critica da capire subito**: la Fase 4 (Blender) **dipende** dalla Fase 1 (formato prefab), non
viceversa. Iniziare da Blender senza il formato prefab significherebbe importare mesh senza sapere dove
metterne il significato — cioè il fallimento dell'auto-generazione, ripetuto.

---

## 11. Rischi tecnici principali

| Rischio | Impatto | Mitigazione |
|---|---|---|
| **Ricadere nell'auto-generazione** perché "il prefab è lento da autorare" | Alto: già fallito una volta | La macchina valida, non crea (§4). Se serve velocità, si fanno più prefab, non euristiche |
| **Perdere modifiche manuali** all'aggiornamento di un prefab | Alto: perdita di lavoro | `source` per nodo + override espliciti (§3) |
| **Mesh Blender nella collisione** "tanto per provare" | Alto: degrada LOS e navmesh irreversibilmente | Regola §6.1, e un controllo in `--validate` |
| **O(N²) al load** su mappe profonde | Medio, prevedibile | Griglia spaziale quando si superano ~400 posizioni (§9) |
| **Sovra-ingegnerizzare l'AI** combinando 4 tecniche | Alto per un solo sviluppatore | Regola "una tecnica per domanda" (§8) |
| **Editor ingestibile** con 1000 elementi | Medio | Filtri + pannello difetti (§7) |

---

## 12. Prime azioni raccomandate

Nell'ordine, e ognuna verificabile:
1. **Definire il formato prefab** (JSON, come sopra) e l'espansione al load. Nessuna UI ancora.
   *Verifica*: una mappa che usa un prefab produce le stesse strutture di una mappa a mano (`--validate`).
2. **Aggiungere `source` ai nodi tattici** (hand/prefab). Prerequisito per non perdere lavoro dopo.
3. **Indoor/outdoor derivato** (raycast verso l'alto sul navmesh): il dato semantico più economico e con più
   sbocchi futuri (comportamenti interni, stanze, breach).
4. **Pannello "Salute tattica"** in editor, che aggrega i controlli già esistenti (visuale verticale,
   esposizione) + ridondanza + buchi di copertura.
5. **Solo dopo**: convenzioni Blender e importatore.

> Motivazione dell'ordine: i primi quattro punti **aumentano la capacità di autorare e verificare** con
> l'editor attuale, senza dipendere da una pipeline esterna. Il punto 5 è quello a rischio più alto e va
> affrontato quando il resto è solido — altrimenti si importano asset senza saper dire se sono buoni.

# 23 — Game Design Bridge (GDD ↔ engine)

**Status: Reference — bridge document, not a feature spec.**
Questo documento non descrive un sistema da implementare: mappa il **game design** (il GDD
consolidato di "Galactic Front") sui sistemi dell'engine, e dice per ogni sistema di gioco
**dove vive nel codice**, **quale doc lo specifica** e **a quale fase appartiene**. Serve a
rispondere a una domanda che finora nessun doc copriva: *"il GDD chiede X — dove lo tocco?"*.

## Perché esiste
I ProjectDocs sono (giustamente) engine-centrici e verificati sul codice. Ma il **perché** dei
sistemi — la fantasia del clone, i pilastri di design, il ruolo tattico delle armi e dei nemici,
la gerarchia GAR — vive solo nel GDD, che non è nel repo. Senza questo ponte, chi implementa
vede *cosa* c'è ma non *cosa deve diventare*, e le decisioni di dettaglio (bilanciamento, ruoli,
priorità) finiscono per essere prese durante la scrittura del codice invece che dal design.

**Regola di precedenza:** per lo **stato reale** vincono sempre 05_CurrentState + il codice. Per
l'**intenzione di design** vince il GDD. Se il GDD chiede qualcosa che l'architettura non regge,
si apre un ADR — non si piega il codice in silenzio.

---

## I 5 pilastri del GDD (il metro di ogni scelta)
1. **Sei un clone qualunque che cresce** — la prospettiva *è* l'identità del gioco.
2. **Le decisioni tattiche contano più dei riflessi** — posizione, ordini, priorità dei bersagli.
3. **La guerra è viva e continua senza di te** — i fronti si muovono, gli alleati avanzano o cadono.
4. **La squadra è una risorsa, non decorazione** — gli alleati combattono davvero; gestirli vince.
5. **Le meccaniche generano storie** — i momenti memorabili nascono dall'interazione dei sistemi.

**La domanda che decide tutto:** *"questa scelta rende Galactic Front più coerente, più
costruibile e più vicino all'esperienza di essere un clone della Grande Armata della Repubblica?"*
Se la risposta non è chiaramente sì, non guida il progetto.

---

## Mappa sistema GDD → engine

| Sistema GDD | Dove vive oggi | Doc | Stato |
|---|---|---|---|
| Combattimento FPS/tattico | `CombatSystem`, `Weapon`, `PlayerController`, `physics/HitTest` | 03 | Implementato |
| Armi + ruoli | `WeaponDef`, BalanceEditor | 03 | Implementato (spread applicato R1) |
| AI individuale | `AiSystem` + `AiProfileDef` | 16 | Implementato (scope core) |
| AI: movimento/pathfinding | `NavManager` + `CrowdSystem` (Recast/Detour) | 22 | Implementato A+B+C |
| AI: uso dello spazio tattico | `AiSystem` ← `MapDef` metadata | 15, 18 | Implementato |
| Mappe + metadata tattici | `MapDef.geometry/coverPoints/patrolRoutes/dangerZones` | 15 | Implementato |
| Command post / controllo territoriale | `CommandPosts` (ADR-009) + HUD | 03 | Implementato |
| Modalità come configurazioni | `IGameMode` + factory (ADR-008); Conquista/Assalto/Difesa (ADR-014) | 03 | Implementato |
| Veicoli | `VehicleDef`, `VehicleDrive`, `VehicleComponent` | 19 | Fase A implementata |
| Fazioni (Repubblica/CSI) | `enemies/`, `allies/`, roster `MapDef` | 03 | Implementato (asimmetria: solo dati) |
| Telemetria / osservabilità | `mini::telemetry` (ADR-013/016) | 21 | Implementato |
| Scala / prestazioni | SoA + time-slicing + cap LOS (ADR-015) | 20 | Implementato (~40 AI) |
| **Classi / ruoli / specializzazioni** | — | **14** | **Planned (zero codice)** |
| **Squadra e comando** | — | **26** | **Planned (zero codice)** |
| **Obiettivi generici / missioni** | solo command post | **25** | **Planned** |
| **Progressione / carriera / gradi** | — | **27** | **Planned (Fase 3)** |
| **Persistenza carriera / stato guerra** | solo `user_presets` | **28** | **Planned (Fase 3/4)** |
| **Validazione contenuti** | sparsa nei loader | **24** | **Planned** |
| Galactic Conquest / Chronicles | — | — | Fase 4/5, non progettato |

---

## Elementi di design che il codice non esprime ancora

### 1. "Gli obiettivi contano più delle uccisioni" (pilastro 3)
Il GDD è esplicito: la vittoria nasce da decisioni tattiche e obiettivi, non dalla mira. Oggi
il gioco non ha né XP né valutazione, quindi il pilastro non è ancora violato — ma **quando
arriverà la progressione (27), l'XP dovrà pesare obiettivi/supporto/sopravvivenza, non le kill.**
Un run kill-focused non deve superare un run objective-focused. Questo è un criterio di
accettazione, non un'aspirazione.

### 2. L'economia tattica (Punti Comando)
Risorsa **in-missione**, guadagnata soprattutto completando obiettivi, spesa per rinforzi,
veicoli e supporto. È distinta dalla progressione di carriera. Non esiste; va con 25/26.

### 3. Ordini contestuali (il cuore del comando)
Il GDD descrive un sistema a due livelli ispirato a Republic Commando: **un tasto** puntando un
elemento del mondo → l'ordine implicito (copertura → prendi posizione; console → hackera;
nemico → fuoco concentrato; alleato a terra → rianima), più una **ruota di comando** per gli
ordini ampi. Il punto non è la quantità di ordini: è che la tattica stia **dentro il flusso
dell'azione**, non in un menu separato ("complessità accessibile"). Vedi 26.

### 4. Stato "a terra" e rianimazione
Messa fuori combattimento non letale + rianimazione da parte della squadra: è la meccanica che
rende la squadra una risorsa (pilastro 4) e dà peso alle perdite. Oggi le unità muoiono e basta.
Prerequisito di 26.

### 5. Il bestiario è un sistema di design, non lore
Ogni archetipo nemico deve **imporre una risposta tattica diversa** — non essere solo un sacco
di HP diverso. Il GDD lo definisce così:

| Unità | Comportamento | Risposta richiesta |
|---|---|---|
| B1 | Lenti, in formazione, imprecisi | Carne da cannone; pericolosi solo in massa |
| B2 Super | Avanza frontalmente, alta cadenza | Assorbe fuoco leggero → fuoco concentrato / Heavy |
| BX Commando | Agile, copertura, fiancheggia | Anti-camper: obbliga a muoversi |
| Droideka | Scudo deflettore, doppi cannoni | Area denial: EMP, aggiramento, soppressione |
| Droide Tattico serie T | Evita il combattimento, potenzia i droidi vicini | **HVT**: eliminarlo degrada l'AI nemica dell'area |

Oggi `AiProfileDef` ha i campi tattici giusti (aggression, retreat, cover_preference, flank) —
**il gap non è il codice, sono i dati**: gli archetipi vanno autorati per esprimere questi ruoli.
Il **Droide Tattico è l'unico che richiede codice nuovo** (un'aura che modifica l'AI vicina) e
va trattato come feature a sé.

### 6. Matrice dei ruoli d'arma
Le armi coprono quattro ruoli con **trade-off reali**, così nessuna è universalmente migliore:
fucili standard (DC-15A lungo/preciso, DC-15S corto/manovriero), pesanti (Z-6: volume di fuoco,
rallenta + spin-up), precisione (DC-15x: one-shot, basso rateo/surriscaldamento), specialistiche
(PLX-1: unica anti-veicolo, munizioni limitatissime). I campi esistono in `WeaponDef`; il lavoro
è **di bilanciamento dati**, non di codice.

### 7. Gerarchia GAR canonica (per la progressione, doc 27)
Squadra 9 → Plotone 36 → Compagnia 144 → Battaglione 576 → Reggimento 2.304 → Legione 9.216 →
Corpo 36.864 → Armata di Settore 147.456 → Armata di Sistema 294.912 → Grande Armata ~3M.
Il grado del giocatore determina il **livello di comando** (quante unità coordina), non solo un
numero. Il fireteam (~4) è un'astrazione di gameplay sotto la squadra.

### 8. I due grandi stati persistenti
Il GDD converge tutto in due contenitori: **Profilo del Clone** (grado, classe, specializzazioni,
equipaggiamento, statistiche, squadra) e **Stato della Guerra** (grafo galattico, controllo,
operazioni). Mappano su `CareerSave` e `CampaignSave` in 28.

---

## Vincoli del progetto che il GDD deve rispettare
Il GDD è la sorgente del design, ma **non scavalca i vincoli strutturali** già decisi:
- Two-binary, comunicazione solo via file (ADR-002).
- OpenGL 3.3 Compatibility + client-side arrays — vincolo Intel, **non "legacy da fixare"** (ADR-003).
- id = filename stem (ADR-001); dropdown-only, mai id in testo libero (04).
- `saveJsonRMW` per ogni salvataggio (ADR-010).
- Multiplayer online **fuori scope**; focus single-player + co-op locale split-screen.
- Data-driven first: nuova arma/nemico/mappa/modalità = dati, non codice.

Dove il GDD immagina scala galattica (Galactic Conquest), va ricordato che la "scala" qui è
**simulazione locale** (numero di AI/entità), non sessione di rete.

## Ordine di costruzione implicito nel GDD (coerente con 00_Vision)
Verticale minimo (combat + mappa + una classe) → livello tattico (squadra + AI + obiettivi) →
carriera (progressione + equipaggiamento + persistenza) → meta-gioco (veicoli pesanti, guerra
dinamica). **Il progetto è già oltre il primo strato**: il prossimo salto di *design* è il
livello tattico (25 + 26), non la progressione.

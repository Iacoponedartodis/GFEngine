# 45 — Metadata tattici: ricerca sullo stato dell'arte

> **Documento di RICERCA, non di decisione.** Nessuna riga di codice, nessuna scelta presa. Serve a
> rispondere a una domanda sola: *come risolvono questo problema i giochi che lo hanno risolto bene,
> e cosa conviene a noi.* Le decisioni si prendono dopo, con l'utente, e finiranno in doc 44 §W2.
>
> Richiesto dall'utente il 2026-08-04: *"partire facendo una ricerca... senza farti spaventare da
> eventuali complessità, perché questo è uno dei sistemi più importanti di tutto il gioco"*.

---

## 0. La domanda, posta bene

Non è *"quali campi mettiamo in una cover"*. È: **quali fatti sul mondo deve poter interrogare
un'AI per comportarsi bene, e quali di questi può ricavarseli da sola.**

La differenza è sostanziale. Un campo autorato è un costo permanente per l'autore e una fonte di
errore silenzioso (l'abbiamo pagata: `facing_deg` sbagliato nel prefab, 169 posizioni a mano su
Training Ground, scale che non erano scale). Un campo derivato costa una volta in codice e poi è
**gratis e sempre coerente** — ma solo se è ispezionabile, altrimenti diventa magia che nessuno
può correggere.

---

## 1. Cosa fanno gli altri

### 1.1 Killzone 2/3 — Guerrilla (GDC 2005/2009)
Il riferimento più vicino a noi: sparatutto a coperture, squadre, mappe autorate.

- **Waypoint annotati a mano** con ruoli: *regroup areas, hiding areas, sniper locations, defend
  locations* per obiettivo, *retrieve locations* per la bandiera, *assassination target areas*.
- **Punteggio a pesi additivi** per scegliere la posizione d'attacco: linea di tiro sulla minaccia
  primaria (peso **40** se copertura parziale, **20** altrimenti), copertura dalle minacce
  secondarie (peso **20**), posizione dentro il raggio di combattimento preferito. I contributi si
  **sommano**.
- **Grafo strategico**: i waypoint si raggruppano in *aree*, e le aree formano un grafo di livello
  superiore su cui ragiona la strategia.

> **Per noi**: è quasi esattamente la nostra architettura — pesi additivi (`AiUtility`), aree
> (settori ADR-034), ruoli autorati. La differenza è che loro annotano **a mano** e noi vogliamo
> derivare. Conferma però che i **pesi additivi** sono la scelta giusta: nessun fattore azzera gli
> altri.

### 1.2 CryEngine — Tactical Point System (TPS)
Il sistema più esplicito e documentato. Una query ha **tre sezioni**:

| sezione | cosa fa | esempi reali |
|---|---|---|
| **Generation** | da dove nascono i candidati | `hidespots_from_attentionTarget_around_puppet` (+ distanza) |
| **Conditions** | filtri booleani, scartano | `coverSoft`, `visible_from_player`, `max_distance_from_puppet`, `min_distance_from_puppet` |
| **Weights** | punteggio continuo, ordinano | densità di copertura, distanza dall'agente |

Test disponibili come primitive componibili: `visible`, `towards`, `reachable`, `canReachBefore`.
Proprietà di copertura come booleani: *soft cover*, *superior cover*, *inferior cover*.

> **Per noi**: la separazione **condizione (scarta) / peso (ordina)** è una lezione importante che
> oggi non abbiamo — le nostre query mescolano i due (`bestFiringPosition` filtra arco e gittata e
> poi pesa). Renderla esplicita renderebbe le query leggibili e componibili.
> `canReachBefore` è notevole: un test **relativo al nemico**, non assoluto.

### 1.3 Generazione automatica da navmesh (Arma Reforger, letteratura dal 2005)
Il filone che ci interessa di più.

- I candidati nascono dai **bordi del navmesh**: ogni bordo o buco indica un dislivello, un muro o
  un oggetto — cioè un potenziale riparo. Si cammina lungo i bordi e si campiona.
- Per ogni candidato si verifica che **l'oggetto davanti abbia dimensioni adeguate** a riparare.
- Rigenerazione **per tile** agganciata agli aggiornamenti del navmesh: se compare un oggetto
  dinamico, si ricalcolano solo le tile toccate.

> **Per noi**: è la strada per W3, e si sposa col nostro navmesh Recast. Nota importante: il bordo
> del navmesh è già **calcolato** — la copertura viene "gratis" dalla stessa struttura che serve a
> camminare, senza una seconda verità sul mondo (il difetto che ci ha morso nel changelog 77).

### 1.4 F.E.A.R. — Monolith (GDC 2006, Jeff Orkin) — *aggiunto su indicazione dell'utente*
Il pezzo che conta non è GOAP. È il **sensore per-agente**, testuale dalla talk:

> *"the squad behavior does not need to analyze the map and determine where there is available
> cover. This is because **each A.I. already has sensors keeping an up to date list of potentially
> valid cover positions nearby**. All the squad behavior needs to do is select one node that the
> A.I. knows about, and ensure that other A.I. are not ordered to go to the same node."*

E la filosofia di authoring, esplicita: *"the designer's job is to create interesting spaces for
combat, packed with opportunities for the A.I. to exploit... **Designers are not responsible for
scripting the behavior of individuals**"*. I livelli portano **decine di migliaia** di hint node
piazzati a mano — sostenibile per Monolith nel 2005, non per noi.

> **Per noi**: tre lezioni. (a) il candidato è **per-agente e continuo**, non ricalcolato a ogni
> decisione — è ciò che tiene il costo lineare; (b) la squadra **non analizza la mappa**, sceglie e
> **rivendica** (che è il nostro `allyTac.claimed`, già esistente); (c) la validità di una copertura
> è **relativa alla minaccia corrente**, non assoluta — noi oggi usiamo uno scalare, e per questo
> consideriamo "coperto" chi è esposto di fianco.

### 1.5 Arma 3 — Bohemia — *aggiunto su indicazione dell'utente*
Le posizioni utilizzabili dentro un edificio (`buildingPos`) sono **annotate nel modello**, non nella
mappa: piazzi l'edificio e le posizioni vengono con lui. È **esattamente la nostra ADR-048**, e che
il gioco militare con le mappe più grandi del mercato abbia scelto questa strada è la conferma più
forte che avessimo preso la direzione giusta.

Ma Arma è soprattutto il **controesempio più istruttivo**. La critica storica alla sua AI non è sul
combattimento ravvicinato: è sul **ragionamento in terreno aperto** — unità che attraversano campi
scoperti, che non usano le creste, che non capiscono che una collina domina una valle. Il motivo
strutturale: **l'edificio è annotato, il terreno no**. Fuori dagli edifici l'AI non ha alcuna
descrizione dello spazio, e i grandi mod (LAMBS Danger, Vcom, ASR) esistono in gran parte per
costruirla a posteriori.

> **Per noi, ed è la lezione singola più importante della ricerca**: se l'unico livello di metadata è
> "posizioni su oggetti autorati", **lo spazio fra gli oggetti resta muto**. Non è un difetto
> marginale e non si tappa aggiungendo posizioni. Serve un livello che descriva i **luoghi**, non le
> **cose**. È la ragione tecnica per cui la granularità non può essere una sola.

### 1.6 The Last of Us — Naughty Dog (GDC 2014)
La talk *Human Enemy AI* mette al centro la **"real-time level analysis"**: il livello viene
analizzato **durante il gioco**, non solo al caricamento, per alimentare combattimento, ricerca e
stealth. È il modello "il mondo si ri-descrive mentre la partita evolve".

> **Per noi**: conferma che parte dei dati non può essere statica. Noi lo facciamo già in piccolo
> (quadro tattico della torre, ricalcolato ogni 0,33 s). La domanda vera diventa **quale dato è
> statico e quale è dinamico**, non "derivato o autorato".

### 1.7 Spatial Query Systems e Influence Map (Game AI Pro)
- Flusso canonico: **genera campioni → applica test → calcola punteggi → scegli**. È lo stesso
  schema del TPS, generalizzato.
- **Influence map** (Dave Mark): mappe di influenza modulari usate anche per *scegliere una
  destinazione di movimento* — l'agente chiede alla mappa "dove conviene andare", non calcola da sé.
- *Dishonored 2* usa il **flooding** dell'influence map per l'inseguimento.

> **Per noi**: l'influence map è la struttura che **ci manca completamente** e che risolverebbe di
> netto problemi che oggi affrontiamo con euristiche puntuali (dove si sta combattendo, da dove
> arriva la pressione, quale zona è pericolosa *adesso*). È il candidato numero uno fra le cose
> nuove.

---

## 2. Le due assi che contano davvero

Tutta la letteratura si dispone su due assi indipendenti. Confonderli è l'errore tipico.

|  | **STATICO** (dalla geometria, al load) | **DINAMICO** (dallo stato di partita) |
|---|---|---|
| **DERIVATO** | protezione, altezza, direzioni riparate, chi copre chi, esposizione, quota, vie d'accesso, chokepoint, stanze | occupazione, pressione, minaccia percepita, influenza, esposizione *al nemico attuale* |
| **AUTORATO** | *intento*: obiettivi, importanza di un fronte, danger zone narrative, tipo di copertura (distruttibile?) | — (nulla: l'autore non tocca lo stato di partita) |

Tre conseguenze pratiche:
1. **L'autore vive solo in una casella.** Tutto ciò che non è intento dovrebbe uscire dalle sue mani.
2. Il derivato-statico è **precalcolabile al load** (ADR-033: mai salvato, mai stantio).
3. Il derivato-dinamico ha bisogno di una **struttura sua** — ed è lì che entra l'influence map.

---

## 3. Proposta di modello dati per una POSIZIONE (bozza da discutere)

L'utente ha detto *"potrebbero arrivare ad avere anche 15 dati per uno"*. Ecco un elenco concreto —
**non una decisione**, un punto di partenza per sceglierli insieme.

### Autorati (l'INTENTO — pochi, e solo ciò che la macchina non può sapere)
1. `role` — cosa l'autore vuole che quel punto sia (resta, ma diventa suggerimento correggibile)
2. `importance` — quanto conta per il design
3. `destructible` — la copertura si distrugge? (fatto di gameplay, non di geometria)
4. `tags` narrativi — es. "solo difensori", "spawn protetto"

### Derivati STATICI (dalla geometria, al load)
5. `height` / tipo di riparo — peek-over vs peek-around, **dalla forma del box**
6. `protection[dir]` — quanto ripara **per direzione**, non un solo numero
7. `coveredArcs` — quali settori angolari sono protetti
8. `exposure` — da quanta mappa si è battuti *(già esistente)*
9. `covers[]` — quali altre posizioni si battono da qui *(già esistente)*
10. `coveredBy[]` — l'inverso: chi mi copre le spalle *(non esiste)*
11. `overheadCover` — c'è qualcosa sopra *(già esistente)*
12. `elevationAdvantage` — dominanza in quota sulle zone vicine
13. `accessPaths` — da dove ci si arriva, e quante vie ci sono *(il dato che abilita l'aggiramento)*
14. `chokepointScore` — quanto quel punto controlla un passaggio obbligato
15. `roomId` / `spaceType` — interno, corridoio, aperto *(dalla topologia)*
16. `distanceToObjective` — **di percorso**, non euclidea *(la verticalità ci ha appena insegnato quanto conta)*

### Derivati DINAMICI (per-tick / a cadenza)
17. `occupied` *(già esistente: `allyTac.claimed`)*
18. `firesOnEnemyNow` *(già esistente: `canFire`)*
19. `threatLevel` — pressione nemica sulla zona *(→ influence map)*
20. `contested` — quanto è conteso *(esiste a livello settore, non di posizione)*

> **Nota**: già oggi ne abbiamo **cinque** derivati e funzionanti. Non partiamo da zero, e sono
> proprio quelli che si sono rivelati più affidabili — è l'argomento più forte a favore di questa
> direzione.

---

## 4. Ciò che la ricerca dice di NON fare

- **Non mescolare condizione e peso.** Un filtro che scarta e un punteggio che ordina sono cose
  diverse; il TPS le separa, noi no, e le nostre query sono più difficili da leggere per questo.
- **Non usare composizione moltiplicativa.** Killzone somma, Game AI Pro somma. Un fattore a zero
  che azzera tutto produce unità che non fanno nulla — l'abbiamo già visto con KI #81.
- **Non derivare ciò che l'autore deve poter decidere.** L'importanza di un fronte è una
  dichiarazione di design: abbiamo già misurato (changelog 136) che attenuarla peggiora il
  comportamento.
- **Non fidarsi di una seconda verità sul mondo.** Killzone e il TPS partono dalla stessa struttura
  che serve a camminare. Il nostro incidente peggiore (changelog 77, *unità senza bersaglio l'81%
  del tempo*) è nato proprio da una LOS parallela scollegata da quella runtime.

---

## 5. Domande aperte — **TUTTE RISOLTE il 2026-08-04**, il piano è in [doc 46](46_MetadataPlan.md)

1. ~~**Influence map: la introduciamo?**~~ → **SÌ**, decisione dell'utente. Progettata in doc 46 §3.2/§4.
2. ~~**Granularità**: posizioni discrete o poligoni navmesh?~~ → **domanda mal posta**: nessun sistema
   che funziona usa una granularità sola. **Tre livelli** (griglia / poligoni / posizioni) con regola
   di assegnazione secondo la *natura della domanda*. Doc 46 §3.1.
3. ~~**Protezione per direzione?**~~ → **SÌ**: 8 settori da 45°, 1 byte ciascuno, 8 raycast al load.
   Espressività piena al costo di uno scalare. Doc 46 §3.4.
4. ~~**Distanza di percorso vs euclidea?**~~ → **di percorso, e quasi gratis**: un Dijkstra per
   obiettivo sul grafo dei poligoni dà la distanza vera da ogni punto, e il suo gradiente **è** la
   via d'accesso. Doc 46 §3.3.
5. ~~**Dove si ispezionano?**~~ → overlay in gioco (livello A), colorazione nel viewport (B e C),
   `--validate` per i difetti, `--trace-ai` per "fra cosa stavo scegliendo". Doc 46 §8.

---

## Fonti

- [Killzone's AI: Dynamic Procedural Tactics (GDC)](https://www.slideshare.net/slideshow/killzones-ai-dynamic-procedural-tactics-9885496/9885496)
- [Straatman et al., Killzone's AI: dynamic procedural combat tactics (PDF)](http://cse.unl.edu/~choueiry/Documents/straatman_remco_killzone_ai.pdf)
- [The AI of Killzone 2's Multiplayer Bots](https://aarmstrong.org/notes/paris-2009/the-ai-of-killzone-2s-multiplayer-bots)
- [CRYENGINE — Tactical Point System](https://docs.cryengine.com/display/CEPROG/Tactical+Point+System)
- [CRYENGINE — Universal Query System (UQS)](https://docs.cryengine.com/pages/viewpage.action?pageId=29450474)
- [Arma Reforger — Dev Report #20 (generazione cover da navmesh)](https://reforger.armaplatform.com/news/dev-report-20)
- [GDC Vault — The Last of Us: Human Enemy AI](https://www.gdcvault.com/play/1020338/The-Last-of-Us-Human)
- [Game AI Pro — Tactical Position Selection: An Architecture and Query Language](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter26_Tactical_Position_Selection.pdf)
- [Game AI Pro 2 — Modular Tactical Influence Maps](https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter30_Modular_Tactical_Influence_Maps.pdf)
- [Game AI Pro 3 — Guide to Effective Auto-Generated Spatial Queries](http://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter26_Guide_to_Effective_Auto-Generated_Spatial_Queries.pdf)
- [Game AI Pro — Tactical Pathfinding on a NavMesh](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter27_Tactical_Pathfinding_on_a_NavMesh.pdf)
- [Three States and a Plan: The A.I. of F.E.A.R. — Jeff Orkin, GDC 2006 (PDF)](https://www.gamedevs.org/uploads/three-states-plan-ai-of-fear.pdf)
- [Three States and a Plan — versione testuale](https://www.macs.hw.ac.uk/~ruth/ICAPSWorkshop/gdc2006_orkin_jeff_fear/gdc2006_orkin_jeff_fear.doc)
- [F.E.A.R. Designer Diary — A Study of Smart AI (GameSpot)](https://www.gamespot.com/articles/fear-designer-diary-1-a-study-of-smart-ai-part-i/1100-6133519/)
- [Arma 3 — AI Config Reference (Bohemia)](https://community.bistudio.com/wiki/Arma_3:_AI_Config_Reference)
- [LAMBS Danger.fsm — AI che tratta gli edifici come terreno tattico](https://steamcommunity.com/workshop/filedetails/?id=1858075458)
- [Bohemia feedback T62920 — "AI needs building usage routines"](https://feedback.bistudio.com/T62920)
- [The AI of Horizon Zero Dawn — Guerrilla Games](https://www.guerrilla-games.com/read/the-ai-of-horizon-zero-dawn)

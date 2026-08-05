# 46 — Metadata tattici: IL PIANO (Planned Feature)

> **Stato: PIANIFICATO — zero righe di codice.** Documento di scope (CLAUDE.md §5), successore
> operativo della ricerca in [45_MetadataResearch.md](45_MetadataResearch.md).
>
> Richiesto dall'utente il 2026-08-04: *"una pianificazione nel dettaglio della miglior combinazione
> possibile di sistemi, capire come ogni sistema può essere meglio o peggio di un altro, cercando
> una soluzione che vada a usare il meglio di ogni sistema"*.
>
> **Decisione già presa dall'utente**: l'influence map **si fa** (§4).
> **Decisione rimasta aperta**: la granularità — questo documento la risolve in §3, e la risposta è
> *"la domanda sbagliata"*: non esiste una granularità giusta, ne servono tre.

---

## 0. Cosa decide questo documento, e cosa no

**Decide**: quale substrato spaziale usiamo, quali dati ci vivono sopra, come si derivano, quanto
costano, come si interrogano, come si osservano, e in che ordine si costruiscono.

**Non decide**: la geometria della mappa grande — è **[47_MapBuildingPlan.md](47_MapBuildingPlan.md)**
(scritto il 2026-08-04, subito dopo questo) — né i valori di bilanciamento (scelte dell'utente).

**Ordine di lavoro concordato**: questo piano → piano map building/geometrie → **l'utente costruisce
la mappa** → si implementano i metadata su quella mappa.

### Le dimensioni vere di Training Ground — e due mie misure sbagliate prima di arrivarci
Il numero corretto, **verificato contro i bound del navmesh** (che è l'unica verifica che conta,
perché è quello che l'AI usa davvero):

> **Training Ground = 71,3 × 92,4 m = 6.595 m², quota da −0,4 a 15,5 m.**
> 167 box, 169 posizioni tattiche, 23 settori, 5 command post, **1.047 poligoni navmesh**.

Ci sono arrivato sbagliando due volte, e vale la pena scriverlo perché è la stessa trappola:
1. doc 44 e 45 dicevano *"mappa di prova da 50×40"* — quello era **firebase**, un'altra mappa.
2. La mia prima correzione diceva *"154,9 × 91,9 m"* — sbagliata anch'essa, perché avevo calcolato
   l'ingombro dei box **ignorando la loro rotazione `ry`**. Due "Side Bridge" lunghi 90 m e ruotati
   di 90° risultavano estesi lungo X invece che lungo Z, e gonfiavano la mappa di oltre il doppio.

L'errore si è chiuso solo confrontando con i bound che il motore stampa (`bmin`/`bmax` del navmesh):
**il dato calcolato da me era sbagliato, quello del motore no.** È esattamente la regola di
`[[verify-effect-not-data]]` applicata a una misura: leggere il JSON non è misurare la mappa.

Il valore che conta comunque non cambia di segno: **169 posizioni su 6.595 m² = una ogni 39 m²**.
È una densità bassa — il mondo è quasi vuoto di significato, coerente con quello che vediamo: le AI
non sanno cosa farci, con lo spazio.

---

## 1. Il criterio con cui confronto i sistemi

Un sistema di metadata non si giudica per quanto è ricco. Si giudica su cinque assi, e sono gli assi
su cui **noi** ci siamo già fatti male:

| criterio | perché è questo, per noi |
|---|---|
| **C1. Costo per l'autore** | siamo una persona sola. 169 posizioni a mano sono già al limite. |
| **C2. Non può mentire** | il difetto peggiore della nostra storia (changelog 77) è una *seconda verità* sul mondo che divergeva da quella runtime. Un dato che può diventare stantio è peggio di un dato assente. |
| **C3. Ispezionabilità** | ADR-050. Se non lo posso guardare, per me non esiste e non lo posso diagnosticare. |
| **C4. Costo di calcolo** | siamo vertex-bound nel render e abbiamo un budget CPU reale (doc 43). Un'analisi al load è quasi gratis; una per-tick va misurata. |
| **C5. Correggibilità** | un dato derivato che l'autore non può sovrascrivere è magia. Deve poter dire "no, qui ti sbagli". |

Ogni scelta più sotto è argomentata su questi cinque, non su "è più elegante".

---

## 2. I sistemi in campo: cosa prendo da ciascuno, cosa lascio

### 2.1 F.E.A.R. (Monolith, 2005) — *il modello del SENSORE e della validità relativa*

Il pezzo che conta non è GOAP. È questo, dalla talk di Orkin:

> *"the squad behavior does not need to analyze the map and determine where there is available
> cover. This is because **each A.I. already has sensors keeping an up to date list of potentially
> valid cover positions nearby**. All the squad behavior needs to do is select one node that the
> A.I. knows about, and ensure that other A.I. are not ordered to go to the same node."*

Tre cose enormi in una frase:
1. **Il candidato è per-agente e continuo**, non calcolato al momento della decisione. L'agente ha
   *sempre* una lista fresca di posizioni valide vicine.
2. **Il livello squadra non analizza la mappa.** Sceglie fra ciò che l'agente già sa. Questo tiene
   il costo lineare nel numero di agenti invece che quadratico, e tiene la squadra *semplice*.
3. **La rivendicazione** ("non due AI sullo stesso nodo") è tutto ciò che serve al coordinamento.

E l'altra metà: **la validità di una copertura è relativa alla minaccia corrente**, ricontrollata di
continuo. Un nodo non è "buono": è buono *contro quel nemico, da quella direzione, adesso*.

Filosofia di design, esplicita: *"the designer's job is to create interesting spaces for combat,
packed with opportunities for the A.I. to exploit... Designers are not responsible for scripting the
behavior of individuals"*.

| | |
|---|---|
| **Prendo** | il sensore per-agente con lista fresca; la rivendicazione come unico meccanismo di coordinamento; la validità relativa alla minaccia; "l'autore crea opportunità, non comportamenti". |
| **Lascio** | le **decine di migliaia di nodi piazzati a mano**. Funzionava per corridoi su livelli lineari e un team con level designer dedicati. Su C1 è fuori discussione per noi. |
| **Cosa abbiamo già** | `allyTac.claimed` è esattamente la rivendicazione. Il sensore per-agente **non esiste**: oggi ogni query rifà la scansione lineare della mappa. |

### 2.2 Killzone 2/3 (Guerrilla) — *il modello dei PESI ADDITIVI e del grafo di aree*

Punteggio della posizione d'attacco: linea di tiro sulla minaccia primaria peso **40** con copertura
parziale / **20** senza, copertura dalle minacce secondarie peso **20**, distanza dentro il raggio
preferito — **sommati**. Waypoint raggruppati in **aree**, e le aree formano un grafo strategico.

| | |
|---|---|
| **Prendo** | la conferma che la composizione è **additiva** (già nostra, `AiUtility`); il grafo di aree come livello su cui ragiona la strategia (già nostro: settori ADR-034). |
| **Lascio** | anche qui l'annotazione manuale integrale. |
| **Cosa abbiamo già** | quasi tutto. Killzone è l'architettura che **abbiamo già**, e questo è rassicurante: non stiamo per buttare via nulla. |

### 2.3 CryEngine TPS/UQS — *il modello della QUERY in tre sezioni*

`Generation` (da dove nascono i candidati) / `Conditions` (booleani che **scartano**) / `Weights`
(punteggi che **ordinano**). Test componibili: `visible`, `towards`, `reachable`, `canReachBefore`.

| | |
|---|---|
| **Prendo** | la **separazione condizione/peso**, che oggi non abbiamo; i test come primitive riusabili; `canReachBefore` (un test *relativo al nemico*: "ci arrivo prima io?"). |
| **Lascio** | il linguaggio di query testuale e configurabile a dati. È potenza che serve a un team con designer AI dedicati; per noi sarebbe un interprete da scrivere, mantenere e **osservare**, per un guadagno nullo. Le query restano C++, ma **strutturate come il TPS**. |
| **Cosa abbiamo già** | 7 query in `worldintel`, tutte con condizione e peso mescolati nella stessa funzione. È il motivo per cui sono difficili da leggere e da modificare senza rompere qualcosa. |

### 2.4 Arma 3 (Bohemia) — *il modello dell'annotazione sull'ASSET, e il suo fallimento in campo aperto*

Le posizioni utilizzabili dentro un edificio (`buildingPos`) sono **annotate nel modello**, non nella
mappa: piazzi l'edificio e le posizioni vengono con lui. È esattamente la nostra ADR-048 (prefab), e
il fatto che il gioco militare con le mappe più grandi del mercato abbia scelto questa strada è la
conferma più forte che abbiamo preso la direzione giusta.

Ma Arma è anche il **controesempio più istruttivo**. La lamentela storica sulla sua AI non riguarda
il combattimento ravvicinato: riguarda il **ragionamento sul terreno aperto** — unità che attraversano
campi scoperti, che non usano le creste, che non capiscono che una collina domina una valle. Il
motivo strutturale è che in Arma **l'edificio è annotato e il terreno no**: fuori dagli edifici l'AI
non ha alcuna descrizione dello spazio. Gli enormi mod (LAMBS Danger, Vcom, ASR) esistono in gran
parte per costruire a posteriori quella descrizione mancante.

| | |
|---|---|
| **Prendo** | l'annotazione sull'asset (già ADR-048). |
| **Lascio** | l'idea che basti. |
| **Lezione, ed è la più importante del documento** | **Se l'unico livello di metadata è "posizioni su oggetti autorati", lo spazio fra gli oggetti resta muto.** Arma dimostra che questo non è un difetto marginale: è *il* difetto, e non si tappa con più posizioni. Serve un livello che descriva lo **spazio**, non gli **oggetti**. |

> Questa è la ragione tecnica per cui la risposta alla domanda sulla granularità non può essere
> "posizioni discrete". Non perché siano poche: perché descrivono **cose**, e l'AI deve poter
> ragionare sui **luoghi**.

### 2.5 Generazione da bordi navmesh (Arma Reforger e letteratura) — *il modello dello STESSO SUBSTRATO*

I candidati copertura nascono dai **bordi del navmesh** — un bordo è un dislivello, un muro, un
oggetto — e si valida che ciò che sta davanti ripari davvero. Rigenerazione **per tile**.

| | |
|---|---|
| **Prendo** | tutto, ed è la chiave di W3. Il pregio decisivo è su **C2**: la copertura esce dalla stessa struttura che serve a camminare. Nessuna seconda verità. |
| **Lascio** | nulla, ma con una guardia: ADR-026 ci ha già bruciato una generazione automatica. La differenza è in §6. |
| **Cosa abbiamo già** | Recast/Detour, con il navmesh già costruito al load. I bordi sono **già calcolati**: li stiamo buttando via. |

### 2.6 The Last of Us (Naughty Dog) — *il modello dell'analisi CONTINUA*

*"Real-time level analysis"*: il livello si ri-descrive durante la partita.

| | |
|---|---|
| **Prendo** | la distinzione **statico/dinamico** come asse primario. Alcuni fatti (dov'è pericoloso *adesso*) non sono precalcolabili per definizione. |
| **Lascio** | la scala: TLoU ricalcola molto perché ha pochi nemici in spazi piccoli. Noi abbiamo decine di unità su 6.595 m². Il dinamico va in una struttura **aggregata** (la griglia), non in analisi per-agente. |

### 2.7 Influence map (Game AI Pro, Dave Mark; Dishonored 2) — *il modello dello SPAZIO come campo*

Mappe modulari per fazione/tipo, combinate in mappe derivate (tensione, vulnerabilità); l'agente
**chiede alla mappa dove conviene andare** invece di calcolarlo. Dishonored 2 usa il *flooding* per
l'inseguimento.

| | |
|---|---|
| **Prendo** | tutto. È il livello che ci manca e che risolve la lezione di Arma (§2.4). |
| **Attenzione** | è anche il sistema con il rischio più alto su **C3**: una griglia di numeri è opaca. Nasce con il suo visualizzatore o non nasce (§8). |

---

## 3. LA DECISIONE — un substrato a TRE livelli

### 3.1 Perché la domanda sulla granularità era mal posta

L'utente ha detto di non essere sicuro fra posizioni discrete e poligoni navmesh, e ha aggiunto
l'osservazione giusta: *"non è detto che una cosa più autorabile sia la strada giusta considerato
che l'obiettivo è avere moltissimi metadata per mappa"*.

Ha ragione, e la ricerca dice qualcosa di più preciso: **nessuno dei sistemi che funzionano usa una
granularità sola.** Killzone ha waypoint *e* aree. CryEngine ha punti *e* navmesh. Arma ha
`buildingPos` *e* (nei mod) griglie di pericolo. Il motivo è che le domande dell'AI sono di tre
nature diverse, e forzarle su una struttura sola rompe sempre qualcosa:

| la domanda è… | esempio | struttura naturale | forzarla altrove costa |
|---|---|---|---|
| **su un'AREA, adesso** | "dove si sta combattendo? da dove arriva la pressione? dove non devo passare?" | **griglia** | su posizioni: buchi enormi fra un punto e l'altro (il difetto di Arma) |
| **sullo SPAZIO, sempre** | "questo è un corridoio o un'apertura? è al chiuso? quanto dista *di cammino*? da dove ci si entra?" | **poligoni navmesh** | su posizioni: non c'è topologia, non c'è connettività — è il difetto che ci è costato KI #95 |
| **su un PUNTO preciso** | "dove pianto i piedi, da che lato sbircio, che arco batto?" | **posizione tattica** | su poligoni: un poligono non ha un fronte né un'altezza di riparo |

Quindi: **tre livelli, con una regola di assegnazione netta.** Non è complessità gratuita — è meno
complessità, perché ogni livello fa una cosa sola e nessuno dei tre deve fingere di essere un altro.

```
┌─ LIVELLO A — GRIGLIA D'INFLUENZA ──────────────── dinamico, denso, anonimo ─┐
│  celle regolari 2 m · N strati · aggiornata a 3 Hz · nessun authoring       │
│  RISPONDE A: "com'è messa QUEST'AREA, adesso?"                              │
├─ LIVELLO B — POLIGONI NAVMESH ─────────────────── statico, denso, derivato ─┤
│  già prodotti da Recast · annotati al load · nessun authoring               │
│  RISPONDE A: "che tipo di LUOGO è questo, e come ci si arriva?"             │
├─ LIVELLO C — POSIZIONI TATTICHE ──────── statico, rado, semantico, autorabile┤
│  TacticalPositionDef · da prefab, generate, o a mano · l'INTENTO vive qui   │
│  RISPONDE A: "dove mi metto ESATTAMENTE, e cosa ci faccio?"                 │
└─────────────────────────────────────────────────────────────────────────────┘
                    ▲ i tre livelli sono INDICIZZATI fra loro:
                    ogni posizione conosce il suo poligono e la sua cella.
```

**La regola d'oro dei tre livelli**: *un dato vive nel livello più basso che può calcolarlo.* Se lo
sa la griglia, non sta sul poligono. Se lo sa il poligono, non sta sulla posizione. Questo evita la
duplicazione, che è il modo in cui i metadata cominciano a mentire (**C2**).

E la risposta alla preoccupazione dell'utente sui *"moltissimi metadata per mappa"*: arrivano, ma
**non dall'authoring**. Su Training Ground, oggi 169 posizioni autorate. Con i tre livelli: ~3.600
celle × 5 strati + ~2.000 poligoni × 8 campi + le posizioni. Sono **decine di migliaia di dati**
tattici — e l'autore ne scrive **zero in più di adesso**.

### 3.2 Livello A — La griglia d'influenza

**Cos'è**: un insieme di griglie 2D allineate al mondo, celle di **2 m**, una griglia per *strato*.

Dimensionamento reale su Training Ground (155 × 92 m): **78 × 46 = 3.588 celle**. Con 6 strati in
`float`: **86 KB**. Su una mappa grande da 300 × 200 m: 150 × 100 = 15.000 celle, **360 KB**. È
niente. La griglia non è il costo — la **propagazione** lo è, ed è per questo che sta in §7.

**Perché 2 m e non 1**: 2 m è circa il raggio di un'unità con la sua copertura; sotto quella soglia
la griglia descrive dettagli che appartengono al livello C. E quadruplica il costo per nulla.

**Gli strati** (elenco proposto, ognuno con il suo consumatore *già esistente*):

| strato | sorgente | propagazione | cosa risponde | consumatore |
|---|---|---|---|---|
| `presenceAlly` | ogni alleato vivo | flood sul percorribile, decadimento con la distanza | "quanti siamo, qui intorno" | comandante, torre |
| `presenceEnemy` | ogni nemico **noto** (non onnisciente!) | idem | "quanti sono" | idem |
| `threat` | ogni nemico noto, proiettato nel suo **arco di tiro** e gittata, gated dalla LOS | proiezione, non flood | "qui mi sparano addosso" | scelta destinazione, soccorso |
| `recentFire` | ogni colpo sparato/ricevuto | stamp + **decadimento temporale** (τ ≈ 8 s) | "qui si è appena combattuto" | ricerca, allarme, torre |
| `lastKnown` | contatti persi | stamp + decadimento (τ ≈ 20 s) | "l'ultima volta era qui" | ricerca, Hunt |
| `denial` | somma di `threat` avversario | derivato | "non passare di qui" | pathfinding tattico |

**Due strati derivati** (calcolati dagli altri, mai stampati direttamente — è il pattern "mappe
derivate" di Dave Mark):
- `tension = presenceAlly + presenceEnemy` → "quanto si sta combattendo qui" (per la torre, doc 36);
- `control = presenceAlly − presenceEnemy` → "di chi è questa zona" (il fronte è dove è ≈ 0).

**Il punto tecnico che decide se funziona**: la propagazione **non può attraversare i muri**. Una
gaussiana ingenua fa arrivare la minaccia dall'altra parte di un bunker, e produce esattamente il
tipo di dato che mente (**C2**). Quindi:
- `presence*` e `lastKnown` si propagano con un **flood sulle celle percorribili** (BFS a costo
  uniforme, cioè la stessa struttura del navmesh), non con un blur;
- `threat` non si propaga affatto: si **proietta** dentro l'arco/gittata con un test di visibilità
  campionato. È l'unico strato che costa una LOS, ed è quello che conta di più.

**Regola non negoziabile**: la griglia legge **solo ciò che il team sa** (`allyIntel`,
`enemyCommand`), mai lo stato vero del mondo. Altrimenti diventa onniscienza mascherata da
metadata — e sarebbe un modo elegantissimo di distruggere il gioco.

### 3.3 Livello B — I poligoni del navmesh

**Il punto**: Recast produce già i poligoni, i loro vicini e i loro bordi. **Stiamo buttando via il
90% di ciò che ci ha già calcolato.** Ogni campo qui sotto è "gratis o quasi" perché la struttura
esiste già; è il miglior rapporto valore/costo dell'intero piano.

| campo | come si deriva | costo | perché serve |
|---|---|---|---|
| `componentId` | componenti connesse sul grafo dei poligoni | O(P), una volta | **"è raggiungibile?" in O(1)**. È la risposta strutturale a KI #95 e alla verifica di connettività di W1 |
| `elevation`, `slope` | dal poligono stesso | gratis | verticalità |
| `hasOverhead` | 1 raycast verso l'alto per poligono | O(P) sonde | già esiste sulle posizioni, qui diventa denso |
| `width` (larghezza locale) | distanza dal bordo più vicino | O(P·bordi), a bake | **corridoio vs apertura**, senza euristiche |
| `chokeScore` | archi la cui rimozione sconnette due regioni ampie | analisi di grafo | il dato che abilita "presidiare il passaggio" |
| `roomId` | componenti connesse fra poligoni `hasOverhead`, separate dai choke | O(P) | breach/clear room, in futuro |
| `distToObjective[k]` | **un Dijkstra dal poligono dell'obiettivo k** | O(P log P) per obiettivo | **distanza di CAMMINO, non euclidea** |
| `accessCount` | quanti archi entrano nella regione da fuori | dal grafo | "questa piattaforma ha una sola scala" |

> **`distToObjective` merita una nota a parte, perché è il campo che risolve il problema che stiamo
> combattendo da tre giorni.** Un flood di Dijkstra dall'obiettivo produce, in una passata,
> la distanza di cammino da *ogni* punto della mappa. Costa **una volta per obiettivo al load**
> (~2.000 poligoni: microsecondi) e dà per sempre la risposta a *"quanto sono lontano davvero"*.
> Con questo, "la piattaforma Alpha è a 8 m in linea d'aria ma a 40 m di cammino perché si sale
> solo dalla scala" smette di essere una cosa che l'AI **non può sapere** e diventa una lettura di
> un `float`. Il gradiente dello stesso campo dà **la direzione in cui andare**, e i punti dove il
> gradiente converge **sono** le vie d'accesso: `accessPaths` non va calcolato, è già lì.

### 3.4 Livello C — Le posizioni tattiche

Restano `TacticalPositionDef`, **non si riscrivono**. Cambiano tre cose:

1. **Dimagriscono.** Tutto ciò che il livello B sa meglio esce da qui: niente più `hasOverhead`
   autorato o duplicato, la quota viene dal poligono, la distanza dal campo di Dijkstra.
2. **Si moltiplicano**, ma per **generazione** (§6), non per authoring.
3. **Guadagnano la protezione per direzione** — l'unico campo veramente nuovo, e il più importante:
   oggi `protection` è **uno scalare**, cioè afferma che un muretto ripara uguale da tutti i lati.
   È falso, ed è la ragione per cui le AI si mettono "in copertura" e prendono fuoco di fianco.

**Come rappresentare la protezione direzionale, senza esplodere** (risposta alla domanda 3 di doc 45):
**8 settori da 45°, un byte ciascuno** → 8 byte per posizione. Su 2.000 posizioni: **16 KB**. Il
calcolo è 8 raycast corti per posizione **al load**. È l'espressività di un modello direzionale al
costo di uno scalare, e 45° è la risoluzione oltre la quale non cambia nessuna decisione: l'AI non
sceglie fra 40° e 50°, sceglie fra "da lì sono coperto" e "da lì no".

**Cosa resta AUTORATO, e solo questo** (l'intento — è la casella di doc 45 §2):
`role`, `importance`, `destructible`, `tags` narrativi, override di disabilitazione.

---

## 4. L'influence map: risposte alle domande che pone

L'utente ha detto sì. Le decisioni implementative che il sì porta con sé:

**Aggiornamento**: **3 Hz** (0,33 s), la stessa cadenza del quadro tattico della torre — non a caso:
è già la frequenza a cui le decisioni tattiche cambiano nel nostro gioco, e riusare la cadenza
significa riusare il budget e la telemetria che ci sta già attorno.

**Costo, stimato sui numeri veri** (§7 per il dettaglio): il flood di presenza su 3.588 celle
percorribili con ~24 unità è dominato dal numero di **celle**, non di unità — una BFS singola con
sorgenti multiple, ~3.600 visite per strato. Tre strati flood + tre stamp ≈ **15.000 operazioni a
0,33 s**, cioè ~45.000/s. Sotto il rumore.

**Il rischio vero non è il costo, è l'opacità.** Una griglia di numeri è il tipo di dato su cui posso
raccontarmi qualunque storia. Perciò nasce con il visualizzatore (§8) e con questa regola:
**nessuno strato entra in una decisione finché non è visibile a schermo e verificato a occhio su
Training Ground.**

**Cosa NON deve diventare**: un sostituto del ragionamento. L'influence map dice *dov'è caldo*, non
*cosa fare*. La scelta resta additiva in `AiUtility` (§2.2) con l'influenza come **un termine fra
gli altri**. Se un giorno un ramo di comportamento legge solo la griglia, abbiamo sbagliato.

---

## 5. Il livello di QUERY — la riscrittura di `worldintel`

Oggi `worldintel` ha 7 query, ognuna con condizioni e pesi mescolati, ognuna che riscansiona
linearmente tutte le posizioni della mappa. Il piano le riorganizza sul modello TPS (§2.3), **senza**
introdurre un linguaggio di query.

### 5.1 La forma di una query

```
GENERAZIONE  →  CONDIZIONI (scartano)  →  PESI (ordinano)  →  scelta
```

- **Generazione**: da dove nascono i candidati. Sorgenti riusabili: *posizioni entro R*, *posizioni
  che coprono X* (grafo esistente), *poligoni entro R*, *celle entro R*.
- **Condizioni**: predicati booleani riusabili, ognuno una funzione:
  `reachable` (livello B, `componentId` — **O(1)**, oggi non lo verifichiamo affatto),
  `hasLineOfFire`, `withinArc`, `withinRange`, `notClaimed`, `protectedFrom(dir)`,
  `canReachBefore(enemy)` (da CryEngine, usando il campo di Dijkstra), `belowThreat(soglia)`.
- **Pesi**: contributi **additivi**, dichiarati come struct come già fa `AiUtility`.

Il guadagno non è estetico. È che oggi **non so dire perché una query ha scelto quel punto**; con le
tre sezioni separate, il funnel si scrive da solo — *quanti candidati generati → quanti sopravvivono
a ogni condizione → punteggio dei primi tre*. È **esattamente** il funnel con denominatori che
ADR-050 richiede, e verrebbe gratis dalla struttura.

### 5.2 Il sensore per-agente (da F.E.A.R.) — e perché risolve anche il costo

Oggi ogni query è una scansione lineare su tutte le posizioni, rifatta a ogni decisione, da ogni
agente. Con 2.000 posizioni generate e 24 agenti diventa insostenibile — ed è la ragione per cui
**la generazione delle posizioni e il sensore vanno progettati insieme**: senza il sensore, la
generazione ci uccide.

Il modello F.E.A.R.: ogni agente mantiene una **lista corta di candidati vicini** (16-24), aggiornata
a bassa frequenza (~2 Hz, sfalsata fra agenti) via **indice spaziale** sulla griglia del livello A.
Le decisioni interrogano solo quella lista. Il livello squadra sceglie fra ciò che l'agente già sa e
rivendica (`allyTac.claimed`, che **abbiamo già**).

Effetto sul costo: da `O(agenti × posizioni)` a `O(agenti × 20)` per decisione, più un refresh
periodico che usa l'indice. **Il costo smette di dipendere dal numero di posizioni** — che è la
condizione perché la mappa grande e la generazione automatica siano possibili.

### 5.3 La validità relativa alla minaccia

Da F.E.A.R.: una copertura non è buona in assoluto. Con la protezione a 8 settori (§3.4) questo
diventa una lettura di un byte: *`protection[settore(minaccia)]`*. Nessun raycast a runtime.
Oggi usiamo lo scalare, e il risultato è che consideriamo "coperto" chi è esposto di fianco.

---

## 6. La generazione delle posizioni — e come NON ripetere ADR-026

ADR-026 ha già fallito una generazione automatica: euristiche sulle facce dei box, migliaia di
posizioni insensate, rimossa. Non ripetere quell'errore è un requisito, non un auspicio.

**Cosa era sbagliato allora e cosa cambia adesso**:

| ADR-026 (fallito) | questo piano |
|---|---|
| candidati dalle **facce dei box** | candidati dai **bordi del navmesh** — cioè da dove si può stare davvero (§2.5) |
| nessuna verifica | **condizioni** del livello B: raggiungibile, non in un vicolo cieco, larghezza sufficiente |
| nessun contesto | punteggio contro `distToObjective`, `chokeScore`, esposizione, protezione direzionale |
| tutte le posizioni sopravvivono | **soppressione della ridondanza**: due posizioni che coprono lo stesso insieme di bersagli entro X m → ne resta una (doc 41 §5.1) |
| l'autore subiva il risultato | l'autore **corregge**: le generate sono suggerimenti; le sue sopravvivono sempre (`fromPrefab`/hand-placed esiste già, ADR-048) |

**La pipeline**:
```
bordi navmesh → campionamento → condizioni livello B → 8 raycast (protezione dir.)
   → punteggio → soppressione ridondanza → posizioni DERIVATE (mai salvate, ADR-033)
```

**Il gate di accettazione, e va deciso prima di scrivere il codice**: la generazione si adotta solo
se, lanciata su Training Ground, **ritrova le posizioni autorate a mano dall'utente** con
sovrapposizione ≥ 60% (una generata entro 3 m da una autorata, con fronte compatibile entro 45°).
È un test che possiamo fare **davvero**, perché le 169 posizioni a mano sono la verità di riferimento
scritta da chi conosce la mappa. Se la macchina non ritrova ciò che l'autore ha scelto, non ha
capito la mappa, e non se ne parla.

---

## 7. Costi — **MISURATI** su Training Ground, non stimati

Numeri veri, letti dal motore il 2026-08-04 (`--sim-ticks 60` in Release, telemetria di avvio):

| grandezza | valore misurato |
|---|---|
| estensione mappa | **71,3 × 92,4 m**, quota max **15,5 m** |
| box di geometria | **167** → **2.040 triangoli** in input al navmesh |
| **poligoni navmesh** | **1.047** ← metà di quello che avevo stimato |
| posizioni tattiche | **169** |
| **`buildTacticalLinks`** | **2.323 link, 8,1423 ms** |
| **build del navmesh** | **0,113 s** |
| celle di griglia a 2 m | 36 × 46 = **1.656** |

Il livello B è quindi **ancora più economico** del previsto: 1.047 poligoni sono pochissimi, e ogni
analisi O(P) o O(P log P) su di essi è sotto il millisecondo.

### E la mappa da 300 × 200 decisa dall'utente: misurata anche quella

Non stimata. Ho generato una mappa sintetica **300 × 200 m** con la **stessa densità di box** di
Training Ground (1 box ogni 39,5 m² → 1.520 box) e l'ho fatta caricare al motore (Release):

| | Training Ground | sintetica 300 × 200 | rapporto |
|---|---|---|---|
| area | 6.595 m² | 60.000 m² | **9,1×** |
| box | 167 | 1.520 | 9,1× |
| triangoli in input | 2.040 | 18.240 | 8,9× |
| poligoni navmesh | 1.047 | **5.806** | 5,5× |
| **build navmesh** | 0,113 s | **1,385 s** | **12,3×** |
| celle di griglia a 2 m | 1.656 | 15.000 | 9,1× |

**Verdetto: 300 × 200 regge**, con il navmesh a **tile singola** così com'è — nessun cambio
architetturale richiesto, `ok: true`, 5.806 poligoni ben dentro i limiti di Detour. Il prezzo è
**~1,4 s di build al caricamento**, che si paga una volta ed è accettabile.

Due avvertenze che escono da questi numeri, e sono entrambe utili:
- Il costo del navmesh cresce **più che linearmente** con l'area (12,3× per 9,1× di area): il campo
  di altezza è tridimensionale. A 600 × 400 non si scala più a tile singola — **300 × 200 è la
  taglia giusta anche tecnicamente**, non solo di design.
- **`buildTacticalLinks` diventa un problema anche SENZA generazione automatica.** Alla stessa
  densità di oggi, una mappa 9,1× più grande ha ~1.540 posizioni autorate → per la formula di §7
  sono **~676 ms** solo di grafo. M0-bis serve comunque.

| lavoro | quando | costo | note |
|---|---|---|---|
| annotazione poligoni (B, tutti i campi statici) | **load** | O(1.047) + O(P log P) per obiettivo | ordini di grandezza **sotto** gli 8,1 ms che già paghiamo per i link |
| protezione direzionale (8 raycast/posizione) | **load** | 169 × 8 = **1.352** raycast corti | trascurabile al livello di authoring attuale |
| **generazione posizioni** | **load** | ⚠ **vedi sotto — è il problema** | |
| influence map, 6 strati | **3 Hz** | ~15.000 op/aggiornamento | trascurabile |
| refresh sensori per-agente | **2 Hz sfalsato** | 24 × query su indice | sostituisce scansioni lineari: **risparmio netto** |
| query di decisione | per decisione | da O(posizioni) a **O(20)** | il guadagno principale |

### ⚠ Il numero che cambia il piano: `buildTacticalLinks` è O(n²), e l'ho appena misurato

169 posizioni → **8,14 ms**. Sono 28.561 coppie, cioè **~0,29 µs a coppia**. La proiezione è brutale:

| posizioni | coppie | costo di load proiettato |
|---|---|---|
| 169 (oggi) | 28.561 | **8 ms** ✅ |
| 500 | 250.000 | ~71 ms |
| 1.000 | 1.000.000 | ~285 ms |
| **2.000** (generazione plausibile) | 4.000.000 | **~1,14 s** ❌ |

E questo su Training Ground. Su una mappa grande con la stessa densità si moltiplica ancora.

**Conseguenza, ed è una dipendenza dura**: **M7 (generazione) è impossibile finché
`buildTacticalLinks` resta quadratico.** Non è un dettaglio di ottimizzazione da rimandare — è un
prerequisito. La soluzione è nota e non è cara: il grafo "chi copre chi" è **limitato dalla gittata**
(`fireRange`, tipicamente 25 m), quindi con un **indice spaziale a griglia** — la stessa griglia del
livello A, riusata — le coppie da esaminare passano da tutte a quelle entro il raggio. Su 155 × 92 m
con raggio 25 m, il fattore di riduzione è circa **10×**, e cresce con la mappa.

> Questo è il motivo per cui misurare prima di pianificare vale più di pianificare bene: senza questo
> numero avrei messo M7 in fondo alla lista come "nice to have", e l'avrei trovato irrealizzabile
> **dopo** aver costruito tutto il resto. Aggiunto come **M0-bis** in §10.

**Regola di budget**, coerente con doc 43: il **load** può costare secondi (si paga una volta), il
**tick** no. Tutto ciò che è statico va al load; l'unico costo per-tick accettato in questo piano è
l'influence map a 3 Hz e i sensori a 2 Hz — entrambi **aggregati e a cadenza**, mai per-agente
per-tick.

**E va misurato, non affermato**: il profiler (doc 42) ha già le zone annidate. Ogni fase qui sotto
entra con la sua zona, e l'inventario di avvio riporta i numeri veri. Nessuna fase si dichiara
finita su una stima.

---

## 8. Osservabilità — cosa nasce insieme a cosa (ADR-050)

Non un capitolo finale: è **parte della definizione di finito** di ogni livello. Per ciascuno servono
i tre pezzi che ADR-050 richiede — sintomo, funnel, singola entità.

**Livello A (griglia)** — il più opaco, quindi il più strumentato:
- *sintomo*: `celle_calde_senza_unità` (l'influenza dice "caldo" dove non c'è nessuno → la
  propagazione perde muri) e `unità_in_denial_alto` (quanto tempo passiamo dove non dovremmo);
- *funnel*: sorgenti → celle stampate → celle raggiunte dal flood → celle sopra soglia;
- *singola entità*: **overlay a schermo**, uno strato per volta, con la scala di colore. Senza
  questo, la griglia non entra in nessuna decisione (§4).

**Livello B (poligoni)**:
- *sintomo*: `poligoni_irraggiungibili_dallo_spawn` — che è KI #95 espressa come **numero permanente**
  invece che come indagine da mezza giornata;
- *funnel*: poligoni totali → per componente connessa → con overhead → choke → per stanza;
- *singola entità*: `--dump-poly <id>` con tutti i campi, e la colorazione nel viewport dell'editor
  (per componente / per quota / per `distToObjective`, che si legge a colpo d'occhio come una mappa
  di calore delle distanze reali).

**Livello C (posizioni)**:
- *sintomo*: `posizioni_mai_usate` in una run (una posizione che nessuno sceglie mai è o inutile o
  irraggiungibile — in entrambi i casi è un difetto che oggi non vediamo);
- *funnel*: **quello delle query** (§5.1), gratis dalla separazione condizioni/pesi;
- *singola entità*: estensione di `--trace-ai <id>` con *"perché ho scelto questo punto"* — i primi
  tre candidati con il loro punteggio scomposto per termine. Questo è il pezzo che oggi manca di più:
  vedo *cosa* fa un'unità, non *fra cosa stava scegliendo*.

**Il gate `--validate`** guadagna i difetti nuovi che i livelli rendono calcolabili: settore senza
posizioni che ne battono gli accessi (*buco tattico*, doc 41 §5.1), obiettivo irraggiungibile da uno
spawn (`componentId`), posizione con protezione nulla in ogni direzione, stanza senza ingressi.

---

## 9. Cosa NON facciamo, e perché (deciso qui, per non ridiscuterlo)

- **Nessun linguaggio di query a dati.** Struttura del TPS sì, interprete no (§2.3): sarebbe un
  sistema in più da osservare per un guadagno che una persona sola non incassa.
- **Nessun metadata salvato su file oltre l'intento.** ADR-033 vale per tutto il livello A e B e per
  le posizioni generate. Ciò che si salva è solo ciò che l'autore ha scritto.
- **Nessuna onniscienza.** L'influence map legge la conoscenza del team, non il mondo (§3.2).
- **Nessuna derivazione dell'intento.** `importance` resta autorata: attenuarla ha già **peggiorato**
  il comportamento in modo misurato (changelog 136).
- **Nessun cambio ad ADR-047** (box = verità tattica): tutto questo piano ci si appoggia sopra e lo
  rafforza.
- **Niente pathfinding tattico completo** (costo del percorso pesato sul `denial`) in questa fase: è
  il naturale passo successivo, ma raddoppia il costo del pathfinding e va affrontato quando i tre
  livelli esistono e sono misurati.

---

## 10. Fasi, con criteri di accettazione

Ogni fase ha un criterio **verificabile**, non "sembra migliore". L'ordine non è negoziabile: ogni
fase usa quella prima.

| # | fase | criterio di accettazione |
|---|---|---|
| **M0** | **Prerequisito**: la mappa grande esiste (doc 47 + lavoro dell'utente) | il navmesh è connesso: 1 sola componente principale che copre ≥ 95% del percorribile |
| **M0-bis** | **`buildTacticalLinks` non più quadratico** (indice spaziale a griglia, limite di gittata) | stessi 2.323 link su Training Ground (**invarianza**, non "circa"), tempo ≤ 2 ms; e il tempo cresce **linearmente** su un test sintetico a 500/1000 posizioni. **Senza questo, M7 non esiste** (§7) |
| **M1** | **Livello B — annotazione poligoni**: componenti, quota, larghezza, overhead | `--validate` riporta "poligoni irraggiungibili dallo spawn"; su Training Ground il numero è **spiegabile box per box** |
| **M2** | **Livello B — campi di distanza** (Dijkstra per obiettivo) + vie d'accesso | l'AI usa la distanza di cammino: su Training Ground nessuna unità punta più a un ripiano scegliendo la faccia sbagliata; visualizzabile come mappa di calore nell'editor |
| **M3** | **Livello C — protezione direzionale** (8 settori) | *sintomo*: colpi presi da un settore dichiarato protetto **scendono**, misurati con `--sim-ticks` nella stessa run |
| **M4** | **Query in tre sezioni + sensore per-agente** | invarianza di comportamento (come A5: stesso numero di eventi ±5%) **e** il funnel per query esiste; il costo per decisione non dipende più dal numero di posizioni |
| **M5** | **Livello A — influence map**, strati base + overlay | overlay leggibile a schermo; `celle_calde_senza_unità` ≈ 0 (cioè la propagazione rispetta i muri) |
| **M6** | **Consumo dell'influenza** nelle decisioni (additivo) | *sintomo*: tempo passato in `denial` alto **scende**; eventi di combattimento non peggiorano |
| **M7** | **Generazione posizioni** (§6) | ritrova ≥ 60% delle 169 posizioni autorate su Training Ground; il costo di load resta sotto budget |

Fra M4 e M5 c'è un punto di uscita naturale: se a quel punto il comportamento è già buono, l'influence
map può aspettare. **Nessuna fase è un impegno a fare la successiva.**

---

## 11. Rischi, e cosa li disinnesca

| rischio | perché è concreto (non teorico) | disinnesco |
|---|---|---|
| **Sovra-derivazione**: dati che l'autore non capisce | ci è già successo con `facing_deg` sbagliato nei prefab | ogni campo derivato è **visibile** nell'editor e **sovrascrivibile** (C5) |
| **La generazione produce spazzatura** | è già successo, ADR-026 | il gate del 60% (§6): se non ritrova il lavoro dell'autore, non si adotta |
| **Costo di load fuori budget** | **non è più un rischio, è un fatto misurato**: `buildTacticalLinks` = 8,14 ms a 169 posizioni, ~1,14 s proiettato a 2.000 | **M0-bis** è un prerequisito duro di M7, non un'ottimizzazione (§7) |
| **L'influence map diventa il cervello** | tentazione reale: è comoda e dà sempre una risposta | resta **un termine additivo**; nessun ramo la legge da sola (§4, e §5-bis di CLAUDE.md) |
| **Tre livelli = tre verità divergenti** | è il difetto C2, il nostro peggiore | **regola d'oro** (§3.1): un dato vive nel livello più basso che può calcolarlo, e in nessun altro |
| **Doppio lavoro con la mappa grande** | se i metadata si progettano su Training Ground e la mappa grande ha casi diversi | è precisamente il motivo dell'ordine deciso dall'utente: **prima la mappa, poi i metadata** |

---

## 12. Cosa resta da decidere all'utente

Le cinque domande di doc 45 §5 sono risolte qui, quattro su decisione tecnica e una su decisione
dell'utente già data:

1. ~~Influence map?~~ → **SÌ** (utente, 2026-08-04). Progettata in §3.2 e §4.
2. ~~Granularità?~~ → **tutte e tre**, con regola di assegnazione (§3.1). La domanda era mal posta.
3. ~~Protezione per direzione?~~ → **SÌ, 8 settori da 45°, 8 byte** (§3.4). Costo trascurabile,
   espressività piena.
4. ~~Distanza di percorso vs euclidea?~~ → **di percorso, e gratis**: campo di Dijkstra per
   obiettivo (§3.3). È il campo che risolve la verticalità.
5. ~~Dove si ispezionano?~~ → §8: overlay in gioco per il livello A, colorazione nel viewport
   dell'editor per B e C, `--validate` per i difetti, `--trace-ai` per la scelta.

- ~~**A. Quanto grande la mappa grande.**~~ → **300 × 200 m, deciso dall'utente il 2026-08-04**
  (*"deve essere semplicemente grande abbastanza per testare... tutti i sistemi"*, con mappe più
  grandi eventualmente in futuro). **Verificato per misura**, non per stima (§7): regge a tile
  singola, 1,4 s di build navmesh, nessun cambio architetturale. È anche il limite naturale della
  tile singola — oltre serve il tiling, quindi è la taglia giusta anche tecnicamente.

**Restano due cose che decidi tu**, entrambe di scala e non tecniche:

- **B. Fin dove arriviamo in questo giro.** Le fasi M1-M4 sono il valore certo e a basso rischio;
  M5-M7 sono il salto. Il punto di uscita naturale è dopo M4.
- **C. Se le posizioni generate ti servono davvero.** Se preferisci restare sull'authoring per prefab
  (ADR-048, che funziona), M7 si può togliere: gli altri sei passi valgono da soli, e questa è la
  decisione più reversibile del piano.

---

## Fonti

Oltre a quelle di [45_MetadataResearch.md](45_MetadataResearch.md):

- [Three States and a Plan: The A.I. of F.E.A.R. — Jeff Orkin, GDC 2006 (PDF)](https://www.gamedevs.org/uploads/three-states-plan-ai-of-fear.pdf)
- [Three States and a Plan — versione testuale](https://www.macs.hw.ac.uk/~ruth/ICAPSWorkshop/gdc2006_orkin_jeff_fear/gdc2006_orkin_jeff_fear.doc)
- [GDC Vault — Three States and a Plan](https://gdcvault.com/play/1013282/Three-States-and-a-Plan)
- [F.E.A.R. Designer Diary — A Study of Smart AI (GameSpot)](https://www.gamespot.com/articles/fear-designer-diary-1-a-study-of-smart-ai-part-i/1100-6133519/)
- [The Technology of F.E.A.R. 2: Engine and AI (Game Developer)](https://www.gamedeveloper.com/design/the-technology-of-f-e-a-r-2-an-interview-on-engine-and-ai-development)
- [Arma 3 — AI Config Reference (Bohemia)](https://community.bistudio.com/wiki/Arma_3:_AI_Config_Reference)
- [Arma 3 — findCover (deprecato dal 2: nota storica)](https://community.bistudio.com/wiki/findCover)
- [LAMBS Danger.fsm — AI che tratta gli edifici come terreno tattico](https://steamcommunity.com/workshop/filedetails/?id=1858075458)
- [Bohemia feedback T62920 — "AI needs building usage routines"](https://feedback.bistudio.com/T62920)

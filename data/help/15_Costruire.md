# Costruire velocemente

Questo capitolo e' il piu' importante se stai facendo una mappa grande. Gli strumenti qui
descritti servono tutti alla stessa cosa: **ridurre i gesti necessari a mettere giu' geometria**.

Prima c'era un solo modo di fare un muro: `+ Box` (nasce 2x2x2 al centro di dove guardi), poi tre
misure nei campi a destra, poi trascinarlo in posizione. Sei gesti. Con gli strumenti di questo
capitolo ne bastano due, e le misure sono esatte per costruzione invece che per attenzione.

---

## Il passo di griglia: il parametro che cambi di piu'

**Ctrl + rotella** dentro il viewport cambia il passo di aggancio, senza andare a cercare il
combo nella barra. I passi disponibili sono `0.10 · 0.25 · 0.50 · 1.0 · 2.0 · 4.0 · 8.0` metri,
piu' `Off`.

**Come si usa davvero** (e' la pratica standard del level design, non una preferenza):

- **passo grande (2–4 m)** per posare stanze, corridoi, muri perimetrali: le cose grosse devono
  cadere su una griglia grossa o non combaciano mai;
- **passo piccolo (0.10–0.25 m)** solo per la rifinitura: spessori, sporgenze, gradini.

Il passo governa **tutto** cio' che si aggancia: il righello, il disegno di un box, il tiro di una
faccia, e il pulsante "Un passo" della finestra Precisione. Sono un numero solo apposta: due
agganci diversi nella stessa scena producono geometria che sembra allineata e non lo e'.

> **Off non e' una scorciatoia, e' un rischio.** Senza aggancio, due muri accostati a mano
> finiscono a 3 cm di distanza. Quella fessura non si vede a schermo, ma sta sotto la soglia di
> erosione del navmesh: l'AI non ci passa e non capirai perche'.

---

## Disegna: un box in un gesto

Menu **Crea → Disegna sulla griglia**. Lo strumento **resta acceso** finche' non lo spegni, cosi'
si costruisce di seguito senza tornare al menu ogni volta (la voce compare con la spunta).

**Il gesto**: premi il tasto sinistro sul piano di lavoro e trascina. Mentre trascini vedi il
rettangolo giallo e, accanto, **le sue misure in metri**. Rilasci, e il box c'e'.

Riaprendo **Crea** con lo strumento acceso compaiono due campi:

- **altezza** — l'altezza dei box che disegni. Un muro standard e' 3,00 m. La decidi una volta e
  vale per tutti quelli che tracci di seguito.
- **quota** — il piano su cui stai disegnando, cioe' la **BASE** dei box nuovi. A 0 costruisci a
  terra; portalo a 3 e stai costruendo il primo piano.

> **Attenzione alla convenzione della Y**: un box ha la Y al **centro**, ma il piano di lavoro e'
> la sua **base**. L'editor ci pensa lui — un box disegnato a quota 4 con altezza 3 va da 4 a 7 —
> ma quando poi leggi la Y nel pannello di destra la vedi a 5,50, ed e' giusto cosi'.

**Cosa NON fa**: mentre Disegna e' acceso il clic sinistro appartiene allo strumento, quindi **non
seleziona**. E' voluto: altrimenti ogni rettangolo tracciato cambierebbe la selezione sotto e a
fine gesto ti ritroveresti il pannello di destra su un oggetto a caso. Per selezionare, spegni
Disegna.

**Un clic senza trascinare non crea niente.** Un box di lato zero sparirebbe dal navmesh senza dire
niente: meglio non farlo nascere.

---

## Tira una faccia: il gesto con cui si costruisce davvero

Seleziona un box e premi **Faccia** nella barra del gizmo (in alto a sinistra nel viewport,
accanto a Sposta / Ruota / Scala). Compaiono **sei maniglie quadrate**, una al centro di ogni
faccia dell'ingombro della selezione: rosse per X, verdi per Y, blu per Z.

Afferri una maniglia e la tiri: **quella faccia si sposta, la faccia opposta resta ferma.**

### Perche' non e' la stessa cosa della Scala
La Scala muove **entrambe** le facce: allunghi un muro di 2 m e il muro cresce di 1 m per parte,
quindi devi anche rispostarlo per rimetterlo a filo. Due gesti, e mezzo metro di errore ogni volta
che ti distrai.

Tirando la faccia il capo opposto non si muove di un millimetro. Un gesto, ed e' esatto.
E' il motivo per cui gli editor di livelli (Hammer, TrenchBroom, CubeGrid di Unreal) hanno tutti
questo gesto come strumento primario.

### E e Q: costruire da tastiera
Con la modalita' Faccia attiva e il mouse sul viewport:

- **E** tira fuori la faccia attiva di **un passo di griglia**;
- **Q** la spinge dentro di un passo.

La faccia attiva e' quella con il **contorno bianco**. E' l'ultima che hai toccato; se non ne hai
ancora toccata nessuna e' quella superiore (+Y), perche' "alza il muro" e' il caso piu' frequente.

Tre pressioni di E = tre passi esatti. Col mouse la stessa cosa richiede di mirare una maniglia e
di fidarsi dell'aggancio.

> **E e Q funzionano solo quando NON stai volando.** Tenendo premuto il tasto destro (o in cattura
> con Tab) restano salita e discesa della telecamera, come sono sempre stati. Un tasto che cambia
> significato senza dirlo e' peggio di due tasti diversi.

### Su piu' elementi, e sulle strutture
- Con **piu' elementi selezionati** le maniglie stanno sull'ingombro di tutto il gruppo, e ognuno
  cresce della sua parte.
- Su una **struttura parametrica** (scala, rampa, piattaforma) il tiro agisce sulla **misura
  giusta della ricetta**, non sui box: una scala allungata resta una scala a norma, con le sue
  alzate. E' la stessa regola del gizmo di scala — non ce ne sono due.

**Non si puo' schiacciare a zero**: sotto 5 cm il tiro si ferma. Un box a spessore nullo sparisce
dal navmesh senza errori, ed e' il tipo di difetto che si scopre giorni dopo.

---

## I campi numerici del pannello

Ogni misura nel pannello di destra e' un **campo di trascinamento**: lo trascini per regolare, e
con **Ctrl+clic** ci scrivi dentro il valore esatto. Gli slider sono stati tolti: erano nati per
vedere l'effetto in tempo reale, mestiere che ora e' del gizmo, e su intervalli larghi (0,1 →
120 m) un pixel valeva piu' di un metro — da cui gli scatti grossi e irregolari.

> **Il passo di aggancio NON tocca i numeri che scrivi.** Governa i gesti nel viewport — disegnare,
> trascinare il gizmo, tirare una faccia — dove serve a far combaciare le cose. Se scrivi 2,40
> resta 2,40, anche col passo a 0,5. (Prima veniva agganciato anche il valore digitato, ed era il
> motivo per cui certe misure erano impossibili da mettere.)

L'unico limite che resta sulle dimensioni e' **5 cm**: sotto, un box sparisce dal navmesh senza
dire niente.

## Precisione: quando l'occhio non basta

Menu **Modifica → Precisione...**: una finestra con quattro gruppi. Agisce sulla selezione corrente,
che sia di uno o di venti elementi.

### Sposta di una misura esatta
Tre campi (X, Y, Z) e **Sposta**. Serve per "questo muro va spostato di 4 m in X", che a mano
significa leggere la coordinata, sommare a mente e riscriverla.

**Un passo** riempie X con il passo di griglia corrente: e' la scorciatoia per lo spostamento piu'
frequente.

### Allinea — e perche' e' "a filo" e non "al centro"
Per ogni asse ci sono tre pulsanti: **min · centro · max**. Portano tutti gli elementi selezionati
allo stesso bordo su quell'asse.

L'allineamento e' calcolato sui **bordi**, non sui centri. E' la differenza che conta: due muri di
spessore diverso allineati al centro restano **sfalsati**, e la fessura che ne nasce e' proprio
quella che il navmesh non attraversa. Allineati a filo, sono complanari.

- **min** — tutti al bordo piu' basso/sinistro/vicino esistente nella selezione;
- **max** — tutti al bordo piu' alto/destro/lontano;
- **centro** — tutti al centro medio della selezione.

### Distribuisci
Un pulsante per asse. Prende gli elementi selezionati, **tiene fermi i due estremi** e mette gli
altri a spazio uguale fra loro. Serve da tre elementi in su: pilastri, finestre, barricate in fila.

A mano la stessa cosa e' una divisione e N posizionamenti, e l'errore si accumula.

### Appoggia — fino a toccare, senza fessura
Dentro **Modifica → Precisione...**, sei pulsanti: **Giu' · Su** per la verticale, **-X +X -Z +Z** per i lati. Spostano la selezione
lungo quell'asse finche' **tocca** la geometria che le sta davanti: non ci entra dentro e non
lascia spazio.

- **Giu'** e' quello che si usa sempre: posa la selezione sulla superficie sottostante, o a terra
  se sotto non c'e' niente.
- I quattro laterali **accostano** a un muro o a un altro box.

La selezione si muove come **un corpo unico**: se hai selezionato cinque box, la loro forma
reciproca non cambia — si sposta tutto insieme finche' il primo tocca.

Se davanti non c'e' niente, **non succede nulla**. E' voluto: un comando che sposta "verso il
nulla" farebbe sparire la geometria dalla vista senza dirti dove e' finita.

> **Perche' non basta l'occhio.** Un box a 3 cm da terra sembra appoggiato: a schermo non si
> distingue. Ma 3 cm stanno **sotto la soglia di erosione del navmesh**: quella superficie non
> viene generata, l'AI non ci sale, e la causa non e' visibile da nessuna parte. L'aggancio alla
> griglia riduce il problema ma non lo elimina — due box costruiti con passi diversi, o uno
> ruotato, restano sfalsati lo stesso.

---

## Prova da qui: l'unico modo di accorgersi della scala

Il pulsante **Prova da qui** nella barra in alto **salva la mappa** e ti mette a camminare **dove
sei con la telecamera**. Nessun menu, nessun pre-partita: si apre direttamente la mappa.

**Conta anche la quota.** Se ti porti sopra una passerella rialzata e premi il pulsante, nasci
**sulla passerella**, non sul pavimento sotto: il gioco prende la superficie piu' alta *al di
sotto* di dove sei. E' il motivo per cui la posizione usata e' quella della **telecamera** e non
il punto che stai guardando — su una mappa a piu' livelli "dove guardo" ha troppi casi speciali,
"dove sono" nessuno.

> Regolati cosi': **portati con la telecamera dove vuoi comparire**, poco sopra il piano, e premi.

**Sei da solo.** Niente manichini, niente nemici, niente simulazione: serve a sentire com'e'
percorrere lo spazio — se una stanza e' troppo grande, un corridoio troppo stretto, un muro
troppo basso. Se poi vuoi anche i bersagli, quella e' la **Sandbox**, ed e' un'altra cosa
(si apre dal menu principale e ha il suo pannello per scegliere mappa, conteggi e simulazione).

> Sotto sotto **e' la sandbox con zero manichini**: stesso codice, stessa geometria, stesse
> strutture e veicoli. Il pannello della sandbox resta disponibile in partita, quindi se a meta'
> prova ti serve un bersaglio puoi farlo comparire da li'.

Non e' una comodita': e' la parte piu' importante del ciclo. L'errore piu' frequente in assoluto
nel level design e' la **scala** — stanze troppo grandi, corridoi troppo stretti, muri troppo
bassi — e non si vede dall'alto ne' volando. Si vede **camminando**, con la gravita', la
collisione e la velocita' vere.

**Come si usa**: mentre costruisci, ogni volta che finisci una stanza o un passaggio, portati con
la telecamera dove vuoi comparire e premi Prova da qui. Cammina, torna, correggi.

Accanto al pulsante compare cosa sta facendo (`Avvio a 12.5, -8.2...`) oppure il motivo per cui
non puo': se la mappa non ha ancora un nome, salvala prima.

> Sotto sotto lancia `GFEngine.exe --walk --map "<la tua mappa>" --at x,z`. Gli stessi argomenti
> si possono usare a mano dalla riga di comando (capitolo *Riga di comando*).

---

## L'ordine di lavoro consigliato

Non e' una regola dell'editor, e' la pratica che la letteratura di level design ripete piu' spesso:

1. **Passo grande.** Posa l'impronta: pavimenti, muri perimetrali, i volumi delle stanze. Disegna
   in vista **Alto**, dove una lunghezza sullo schermo e' una lunghezza vera.
2. **Alza.** Passa alla modalita' Faccia e tira su i muri con E, un passo alla volta.
3. **Cammina.** Non giudicare uno spazio guardandolo dall'alto: l'errore piu' frequente in
   assoluto e' la scala, e si vede solo attraversando lo spazio alla velocita' del giocatore.
   Usa la **figura di scala** (menu Vista) mentre costruisci.
4. **Passo piccolo, e rifinisci.** Spessori, sporgenze, coperture. Qui servono Precisione e il
   righello.
5. **Verifica il navmesh** prima di considerare finita una zona (capitolo *Navmesh e metriche*).
   Una superficie dichiarata calpestabile che il navmesh non produce non e' un dettaglio: e' una
   stanza in cui nessuno entrera' mai.

---

## Scorciatoie di questo capitolo

| Gesto | Effetto |
|---|---|
| **Ctrl + rotella** | passo di griglia piu' grande / piu' piccolo |
| **Disegna** + trascina | crea un box con l'impronta tracciata |
| **Faccia** + trascina una maniglia | sposta quella faccia, la opposta resta ferma |
| **E** / **Q** | tira fuori / spingi dentro la faccia attiva di un passo |
| **Ctrl+S** | salva (mappa, tipo o singola struttura, secondo il tab) |
| **Prova da qui** | salva e avvia il gioco dove stai guardando |
| **Precisione... → Appoggia → Giu'** | posa la selezione sulla superficie sotto |
| **F** | inquadra tutto |
| **Ctrl+Z / Ctrl+Y** | annulla / ripeti (un trascinamento intero conta come uno) |
| **Ctrl+clic** | aggiungi o togli dalla selezione |
| **Ctrl+A** | seleziona tutto, e di nuovo per deselezionare |

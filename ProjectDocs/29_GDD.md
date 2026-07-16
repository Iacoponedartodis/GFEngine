# GDD — Galactic Front (fonte: Galactic_Front_GDD.docx)

> **Questo file è generato automaticamente dal .docx nella root del repo — NON modificarlo qui.**
> Autoring: modifica `Galactic_Front_GDD.docx` in Word/LibreOffice, poi rigenera questo file
> (vedi 23_GameDesignBridge.md, sezione "Dove vive il GDD", per il comando). Questo .md esiste
> perché è il formato che il resto di ProjectDocs usa: diffabile, grep-abile, citabile per
> sezione — senza dover riestrarre XML da uno zip a ogni sessione.
>
> **Autorità**: sull'intento di design, questo documento VINCE sugli altri ProjectDocs
> (23_GameDesignBridge). Un conflitto fra questo file e un doc 00-28 apre un ADR (13_ADR.md).
>
> Rigenerato: 2026-07-16.

---
**GRANDE ARMATA DELLA REPUBBLICA · GUERRE DEI CLONI**
**GALACTIC FRONT**
Game Design Document — Edizione Definitiva
*FPS/TPS tattico · progressione RPG · guerra dinamica su larga scala*
*Prospettiva del soldato clone della Repubblica Galattica*
Documento di riferimento per la progettazione dei sistemi di gioco
e base per la successiva definizione dell'architettura tecnica
**Indice**
Nota sul documento
**Parte I — Visione e Filosofia**
1. Visione del Progetto
2. Pilastri di Design
3. Esperienza del Giocatore e Player Fantasy
**Parte II — Struttura dell'Esperienza**
4. Architettura delle Modalità di Gioco
5. Gameplay Loop
**Parte III — Sistemi Core**
6. Sistema di Combattimento FPS/Tattico
7. Sistema di Squadra e Comando Tattico
8. Intelligenza Artificiale e Comportamento delle Unità
9. Mission Design e Struttura degli Obiettivi
10. Mappe e Level Design
**Parte IV — Progressione e Personalizzazione**
11. Progressione Militare, Gradi e Carriera
12. Classi, Ruoli e Specializzazioni
13. Equipaggiamento, Armi e Personalizzazione
14. Veicoli e Combattimento su Larga Scala
**Parte V — Mondo e Meta-gioco**
15. Fazioni e Universo
16. Galactic Conquest: Guerra Dinamica e Campagna Strategica
17. Persistenza, Salvataggio e Carriera
**Parte VI — Presentazione**
18. Interfaccia, HUD e User Experience
19. Audio e Immersione
20. Animazioni, Movimento e Feel
**Parte VII — Produzione e Architettura**
21. Filosofia di Sviluppo, Modularità e Priorità
22. Mappa dei Sistemi e Interconnessioni
23. Roadmap Concettuale e Criteri di Successo
**Appendici**
Appendice A — Arsenale della Repubblica e Sistema d'Armatura
Appendice B — Bestiario Tattico: l'Esercito Separatista (CSI)
Appendice C — Veicoli e Impianti sul Campo
Appendice D — Struttura Militare Canonica (GAR)
Appendice E — Glossario dei Sistemi e Termini

### **Nota sul documento**
**Genere:** FPS/TPS tattico militare con progressione RPG e guerra dinamica su larga scala.
**Ambientazione:** Guerre dei Cloni (22–19 BBY). Il giocatore è un soldato clone della Grande Armata della Repubblica.
**Documento:** questo è il documento di game design consolidato del progetto. Definisce la visione, i sistemi di gioco, le loro regole e — soprattutto — il modo in cui i sistemi si collegano tra loro. È pensato per fornire una visione d'insieme completa e coerente, così da poter fungere da base per la successiva progettazione tecnica: architettura del codice, scelta delle librerie, definizione dei moduli e delle loro dipendenze.
**Come leggere questo documento:** ogni sistema è descritto secondo lo stesso schema — *scopo* (quale esperienza produce), *meccaniche* (come funziona), *parametri e dati* (cosa deve essere modellato), *interconnessioni* (con quali altri sistemi comunica). La Parte VII raccoglie la mappa completa delle dipendenze tra sistemi e le priorità di produzione.

## **Parte I — Visione e Filosofia**

### **1. Visione del Progetto**

#### **1.1 Concept in una frase**
**Galactic Front** è uno sparatutto tattico in cui non interpreti l'eroe che salva la galassia, ma un soldato clone che costruisce la propria carriera militare all'interno di una guerra molto più grande di lui. Il combattimento in prima e terza persona è il cuore dell'esperienza, ma la vittoria nasce dalle decisioni tattiche, dalla gestione della squadra e dagli obiettivi raggiunti, non dalla sola mira.

#### **1.2 La fantasia del giocatore**
La fantasia centrale non è quella del Jedi più potente né del super-soldato invincibile. È la fantasia di **appartenere a un esercito reale**: combattere fianco a fianco con i propri commilitoni, sviluppare competenze, guadagnare responsabilità, farsi un nome. La progressione non è un accumulo di numeri, ma il racconto di una carriera. Alla fine, il giocatore deve poter guardare indietro e ricordare non "quante uccisioni ho fatto", ma la squadra con cui ha combattuto, le decisioni difficili, le battaglie vinte e perse, e il percorso da recluta a veterano.

#### **1.3 I cinque punti che rendono Galactic Front diverso**
- **Sei un clone qualunque, e questo è il punto.** L'identità del progetto è la prospettiva del soldato semplice che cresce, non del protagonista predestinato.
- **Le decisioni tattiche contano più dei riflessi.** La posizione, l'ordine dato al momento giusto, la scelta dell'obiettivo prioritario cambiano l'esito più della precisione pura.
- **La guerra è viva e continua senza di te.** Le linee del fronte si muovono, le unità alleate avanzano o arretrano, gli obiettivi vengono conquistati o persi anche quando non sei tu a farlo.
- **La squadra è una risorsa, non un accessorio.** Gli alleati combattono davvero, hanno ruoli e limiti, e una buona gestione produce risultati migliori del gioco in solitaria.
- **Le meccaniche generano storie.** I momenti memorabili nascono dall'interazione tra i sistemi — una difesa disperata, una squadra isolata, un diversivo riuscito — non solo da eventi scriptati.

#### **1.4 Posizionamento**
Galactic Front non compete frontalmente con *Battlefront* (arena multigiocatore spettacolare) né con *Arma* (simulazione militare pura). Occupa lo spazio intermedio esplorato da **Star Wars: Republic Commando**: uno sparatutto in cui comandi una piccola unità, con HUD diegetico da elmetto clone e missioni militari concrete — ma esteso con una progressione di carriera profonda e una guerra galattica dinamica che dà contesto a ogni missione.
*La domanda che guida ogni scelta di design: "Questa decisione rende Galactic Front più vicino all'esperienza di essere un clone della Grande Armata della Repubblica?"*

### **2. Pilastri di Design**
Le meccaniche non si aggiungono perché "interessanti", ma solo se rafforzano la visione. Prima di introdurre un sistema ci si chiede: quale problema risolve? Migliora combattimento, tattica o immersione? Interagisce con gli altri sistemi? Vale la complessità che introduce? Se la risposta è negativa, il sistema va rimandato o eliminato. I sette pilastri seguenti sono i criteri di valutazione permanenti del progetto.

#### **Pilastro 1 — Decidere, non solo mirare**
Durante una missione il giocatore valuta di continuo: quali obiettivi hanno priorità, dove concentrare gli sforzi, quando avanzare e quando ritirarsi, come impiegare la squadra, quali rischi accettare. L'abilità di combattimento resta importante ma è solo una delle leve. Un ottimo tiratore con una cattiva strategia deve poter fallire; un buon comandante con mira mediocre deve poter comunque contribuire.

#### **Pilastro 2 — La guerra è più grande del giocatore**
Il giocatore modifica il corso degli eventi senza controllarli del tutto. Questo rende ogni vittoria significativa e giustifica meccanicamente sistemi come la guerra dinamica (cap. 16) e l'IA operativa (cap. 8), che fanno evolvere il campo di battaglia indipendentemente dalle azioni dirette del giocatore.

#### **Pilastro 3 — Gli obiettivi valgono più delle uccisioni**
Il numero di nemici eliminati non misura il successo. Sabotare una struttura, tenere una posizione, proteggere un convoglio, recuperare informazioni o rallentare un'avanzata possono valere più di un combattimento vinto. Questo pilastro ha una conseguenza sistemica precisa: **l'economia tattica e l'esperienza premiano il completamento degli obiettivi**, non il volume di fuoco (cap. 5, 7, 11).

#### **Pilastro 4 — Ogni scelta ha conseguenze osservabili**
Le decisioni producono effetti sulla missione corrente, sulle missioni successive, sulle risorse disponibili, sulla situazione strategica e sulla crescita del personaggio. Le conseguenze non devono creare enormi ramificazioni narrative, ma devono modificare in modo percepibile il contesto (cap. 16, 17).

#### **Pilastro 5 — La progressione racconta una carriera**
Il sistema RPG non serve solo a sbloccare equipaggiamento: rappresenta esperienza, competenza, responsabilità, specializzazione e reputazione. Il giocatore non diventa potente perché ha statistiche più alte, ma perché sale nella gerarchia e acquisisce nuove capacità di gameplay (cap. 11).

#### **Pilastro 6 — Complessità accessibile**
Sistemi facili da comprendere e difficili da padroneggiare. La profondità nasce dall'interazione tra sistemi semplici e ben collegati, non dalla quantità di regole. Principio guida della produzione: *profondità attraverso sistemi collegati, non attraverso quantità inutile di sistemi.*

#### **Pilastro 7 — Immersione tramite coerenza; realismo funzionale**
Interfaccia, animazioni, comportamento delle unità, audio ed eventi devono convergere sulla sensazione di partecipare alla Guerra dei Cloni. Il realismo è al servizio del gameplay: quando necessario può essere sacrificato, purché il risultato resti credibile dentro l'universo di Star Wars. L'obiettivo è un'esperienza autentica, non una simulazione militare.

### **3. Esperienza del Giocatore e Player Fantasy**
Questo capitolo definisce cosa il giocatore deve *provare*. È il riferimento contro cui misurare tutti i sistemi presentati nel resto del documento.

#### **3.1 Le tre sensazioni fondamentali**
**Appartenenza. **Il clone non è un individuo isolato: fa parte di un fireteam, di una squadra, di una compagnia, di una campagna. Commilitoni e unità alleate hanno peso sia narrativo sia di gameplay.
**Vulnerabilità. **Il giocatore è competente ma non invincibile. Anche un veterano può trovarsi in difficoltà davanti a superiorità numerica, posizioni difensive o mezzi corazzati. La sopravvivenza dipende dalle decisioni, non solo dai riflessi.
**Crescita. **Ogni missione contribuisce alla storia del personaggio: il primo equipaggiamento, le prime battaglie difficili, i compagni incontrati, il ruolo raggiunto. La carriera è la spina dorsale emotiva del gioco.

#### **3.2 Il ciclo emotivo di una missione**
Ogni missione è strutturata come una situazione militare, non come una sequenza di combattimenti. Il giocatore attraversa cinque fasi: **preparazione** (valuta obiettivo, equipaggiamento, squadra, informazioni), **inserimento** (percepisce la scala dello scontro e la situazione degli alleati), **analisi** (identifica minacce prioritarie e decide dove muoversi), **azione** (mette in pratica la strategia tramite combattimento, movimento e coordinazione) e **conseguenza** (comprende cosa è andato bene o male e quali decisioni hanno pesato). Questo ciclo è formalizzato nel Gameplay Loop (cap. 5).

#### **3.3 Momenti memorabili (design per emergenza)**
Il gioco punta a momenti che il giocatore ricorda: difendere una posizione mentre arrivano i rinforzi, salvare una squadra isolata, completare un obiettivo nonostante pesanti perdite, ribaltare una battaglia con una scelta tattica. Questi momenti devono nascere dall'interazione dei sistemi (IA + obiettivi + eventi dinamici + squadra), non da script rigidi. Progettare per l'emergenza è un obiettivo trasversale che vincola in particolare IA (cap. 8), Mission Design (cap. 9) e Mappe (cap. 10).

#### **3.4 Stili di gioco supportati**
La progressione deve permettere a giocatori diversi di vivere esperienze diverse restando sempre "un clone". Quattro archetipi di riferimento, che il sistema di classi (cap. 12) deve rendere tutti validi e complementari:
- **Assalto** — combattimento diretto, avanzata aggressiva, eliminazione delle minacce.
- **Supporto tattico / Comando** — coordinazione della squadra, gestione degli alleati, controllo del campo.
- **Ricognizione** — informazioni, posizionamento, eliminazione mirata a distanza.
- **Specialista** — gadget, problem solving, supporto tecnico e sabotaggio.

#### **3.5 Atmosfera**
Non tutte le missioni sono grandi battaglie. Il gioco alterna tensione, preparazione, esplorazione, coordinazione, caos dello scontro, vittoria e perdita. La Guerra dei Cloni va percepita come un conflitto complesso, non come una serie di scontri spettacolari slegati.

## **Parte II — Struttura dell'Esperienza**

### **4. Architettura delle Modalità di Gioco**
Tutte le modalità condividono lo stesso motore di gioco: combattimento FPS/tattico, classi, armi, IA, mappe, sistema di squadra e progressione. Cambiano soltanto il **contesto**, la **scala temporale** e il **livello di persistenza**. Le modalità non sono giochi diversi con regole diverse: sono quattro modi di vivere la stessa guerra. Il lavoro sul core serve l'intero progetto.

#### **4.1 Le quattro modalità definitive**
Il progetto definisce quattro modalità. Ciascuna risponde a una domanda diversa del giocatore.

| **Modalità** | **Domanda del giocatore** | **Focus** | **Persistenza** |
| --- | --- | --- | --- |
| Campagna Clone | "Come cresce un soldato durante la guerra?" | Narrazione lineare, carriera personale, rapporto con la squadra | Massima (carriera + squadra + eventi) |
| Galactic Conquest | "Come sarebbe vivere una guerra galattica persistente?" | Sandbox strategico, guerra dinamica, carriera aperta | Strategica (mappa galattica + carriera) |
| Chronicles | "Come rivivo le battaglie leggendarie della guerra?" | Scenari storici curati (Geonosis, Kashyyyk, Christophsis…) | Media (progressione condivisa) |
| Schermaglia | "Come entro subito in battaglia?" | Battaglie personalizzate istantanee, test di build e strategie | Nulla o ridotta |

#### **4.2 Campagna Clone**
Esperienza narrativa principale. Il giocatore segue la carriera di un clone dall'assegnazione fino ai ruoli d'élite, attraverso operazioni militari collegate. La struttura tipica è: **Addestramento → Prime operazioni → Missioni di linea → Specializzazione → Operazioni avanzate → Missioni critiche.** La narrazione emerge non solo da filmati e dialoghi ma anche dai risultati delle missioni, dai compagni incontrati, dalle perdite subite e dalle decisioni prese. La squadra ha importanza narrativa: gli alleati accompagnano il giocatore, sviluppano un rapporto, possono sopravvivere o morire e influenzano la storia.

#### **4.3 Galactic Conquest**
Modalità sandbox strategica e cuore del meta-gioco (dettagliata al cap. 16). Il giocatore crea il proprio clone (aspetto, nome, divisione, specializzazione iniziale) e vive una guerra aperta dove i fronti cambiano, le battaglie nascono dalla situazione strategica e la carriera può svilupparsi in direzioni diverse: specialista ARC, comandante tattico, soldato di prima linea, esperto di operazioni speciali. A differenza della Campagna, il percorso non è predeterminato: è una simulazione della Guerra dei Cloni.

#### **4.4 Chronicles**
Modalità dedicata alla rappresentazione di eventi specifici e battaglie celebri della guerra. Le missioni sono più curate e narrative, focalizzate su momenti storici (difesa di un pianeta, grandi battaglie campali, operazioni dietro le linee). Riusa interamente il core ma con scenari e vincoli progettati a mano, offrendo un'esperienza più diretta e ad alta intensità rispetto alla sandbox.

#### **4.5 Schermaglia (Battaglie Rapide)**
Modalità libera per entrare subito in azione. Il giocatore configura mappa, fazioni, numero di unità, obiettivo, difficoltà, classe ed equipaggiamento. Serve a imparare i sistemi, provare classi e build, testare strategie e ricreare grandi scontri ("100 Clone Trooper contro 300 droidi su Geonosis", "una squadra ARC infiltrata in una base separatista").

#### **4.6 Progressione condivisa tra modalità**
La gestione della progressione richiede attenzione per non rompere il bilanciamento. Approccio consigliato: **separare la progressione narrativa dalla progressione degli sblocchi.** Gli sblocchi di equipaggiamento e specializzazioni sono condivisi a livello di account/profilo; lo stato di carriera (grado, storia, squadra) è persistente e per-partita nelle modalità che lo richiedono. Questo mantiene il senso di crescita continuo senza costringere a ricominciare da zero in ogni modalità e senza permettere di importare un veterano già maxato in uno scenario Chronicles bilanciato.

### **5. Gameplay Loop**
Galactic Front ha tre loop annidati che operano su scale temporali diverse ma condividono gli stessi sistemi. La progettazione tecnica deve trattarli come tre cicli che leggono e scrivono sullo stesso stato persistente (profilo del clone, stato della guerra).

#### **5.1 Loop di missione (minuti)**
Il ciclo fondamentale, vissuto in ogni singola operazione:
- **Preparazione** — analisi del tipo di missione, scelta di equipaggiamento, ruolo e composizione della squadra.
- **Briefing** — comunica situazione, obiettivo principale, obiettivi secondari, informazioni sul nemico e risorse. Deve chiarire *"qual è il problema da risolvere?"*, non *"qual è la sequenza esatta di azioni?"*.
- **Inserimento** — il giocatore entra in un conflitto già in corso e percepisce scala, posizione, situazione degli alleati e urgenza.
- **Combattimento e decisioni tattiche** — la parte centrale: abilità individuale, gestione della squadra, scelta degli obiettivi, adattamento agli eventi dinamici.
- **Completamento degli obiettivi** — il successo si misura sugli obiettivi (primari e secondari), sulle perdite subite e sulle risorse conservate, non sul solo combattimento.
- **Valutazione (debrief)** — successo militare + prestazione personale + costi. Il risultato deve raccontare una storia, non essere solo un punteggio.
- **Ricompense e progressione** — esperienza, sblocchi, promozioni, riconoscimenti alimentano il loop di carriera.
**Campo di battaglia dinamico. **Durante lo svolgimento la situazione evolve: una posizione alleata cade, arrivano rinforzi, un obiettivo secondario diventa cruciale, il nemico cambia strategia. Il piano iniziale può e deve cambiare.

#### **5.2 Loop di carriera (ore/sessioni)**
Oltre alla singola missione esiste il ciclo della carriera: **missione → esperienza acquisita → crescita del soldato → nuove responsabilità → missioni più complesse → maggiore impatto sulla guerra.** Il giocatore passa progressivamente da soldato semplice, a membro esperto della squadra, a specialista, a soldato d'élite, fino a figura di comando. La progressione deve essere *diegetica*: si migliora perché si sono vissute esperienze, non solo perché si sono accumulati punti.

#### **5.3 Loop di guerra (campagna intera)**
Nelle modalità strategiche (soprattutto Galactic Conquest) esiste un terzo livello: **situazione galattica → scelta dell'operazione → battaglie sul campo → risultato militare → modifica della situazione strategica → nuove operazioni.** Il giocatore non affronta missioni isolate ma contribuisce all'evoluzione di un conflitto più grande. Questo loop collega il livello tattico (le missioni FPS) al livello strategico (la mappa galattica), dettagliato al cap. 16.

#### **5.4 L'economia tattica come collante dei loop**
Un singolo sistema attraversa e collega i tre loop: l'**economia tattica** (dettagliata al cap. 7). Completando obiettivi e agendo da buon soldato il giocatore e la squadra accumulano una risorsa di richiesta (Punti Comando / Requisizione) spendibile in-missione per chiamare rinforzi, veicoli o supporto orbitale. Il modello è ispirato ai *Battle Points* di Battlefront II, ma con una differenza filosofica coerente con il Pilastro 3: **la risorsa si guadagna soprattutto completando obiettivi e supportando la squadra, non accumulando uccisioni.** Questo trasforma un principio filosofico in una meccanica concreta e misurabile.

## **Parte III — Sistemi Core**
Da qui in avanti ogni sistema è descritto secondo lo schema *scopo → meccaniche → parametri e dati → interconnessioni*, così da alimentare direttamente la progettazione tecnica.

### **6. Sistema di Combattimento FPS/Tattico**
**Scopo. **È il nucleo dell'esperienza. Il combattimento unisce l'immediatezza di uno sparatutto alla profondità decisionale di un gioco tattico. Non deve basarsi solo sulla velocità di reazione, ma sulla capacità di leggere la situazione e scegliere il comportamento corretto.

#### **6.1 Prospettiva**
Il gioco supporta **prima e terza persona** commutabili. La prima persona serve immersione e precisione con l'HUD diegetico da elmetto (cap. 18); la terza persona dà consapevolezza spaziale e valorizza l'armatura e l'identità del clone. Le meccaniche di mira e copertura devono restare coerenti tra le due visuali.

#### **6.2 Modello di sopravvivenza**
Modello ibrido a due strati, ispirato agli standard consolidati del genere e in particolare a *Republic Commando* (scudo rigenerante + salute):
- **Scudo/armatura rigenerante:** assorbe danno e si ricarica dopo alcuni secondi fuori dal fuoco. Premia il disimpegno e l'uso delle coperture.
- **Salute non (o lentamente) rigenerante:** si recupera tramite il Medic della squadra o stazioni bacta. Rende le ferite gravi una risorsa da gestire.
- **Stato "a terra" (downed) e rianimazione:** il giocatore e gli alleati, esauriti i punti, non muoiono subito ma vengono messi fuori combattimento e possono essere rianimati da un compagno. La partita/missione fallisce solo se l'intera squadra è a terra o se il giocatore isolato viene abbattuto. Questo sistema è il fondamento meccanico del pilastro "la squadra è una risorsa".

#### **6.3 Feel delle armi e gestione del fuoco**
Ogni arma ha una forte identità comunicata da peso, potenza, tecnologia e ruolo — non solo da numeri. La differenza tra armi deve cambiare *come* si affronta uno scontro. Il sistema valorizza il comportamento militare: controllo della mira, gestione delle raffiche, copertura e distanza. Sparare senza criterio è meno efficace di un uso controllato. Elemento tematico di Star Wars da modellare: il **surriscaldamento** dei blaster (invece del semplice munizionamento) per le armi ad alto rateo, che introduce una gestione del ritmo di fuoco.

#### **6.4 Ruoli delle armi (matrice)**

| **Categoria** | **Ruolo** | **Punti di forza** | **Limiti** |
| --- | --- | --- | --- |
| Fucile standard (es. DC-15A/S) | Fanteria versatile | Equilibrio, adattabilità | Non eccelle nei casi estremi |
| Arma pesante (es. Z-6) | Supporto, controllo area | Volume di fuoco, anti-gruppo | Mobilità ridotta, spin-up |
| Precisione (es. DC-15x) | Ricognizione, ingaggio a distanza | Letalità mirata, controllo territorio | Vulnerabile da vicino, richiede posizione |
| Specialistica (es. PLX-1) | Anti-corazza, situazioni specifiche | Neutralizza minacce che le altre non toccano | Munizioni scarsissime, ingombro |

Il dettaglio completo dell'arsenale è in Appendice A. Il principio di bilanciamento (cap. 13) è che non esistano scelte universalmente superiori: ogni arma ha un contesto ideale.

#### **6.5 Combattimento su larga scala e differenze tra unità**
La Guerra dei Cloni è fatta di grandi battaglie: il sistema deve reggere molti combattenti senza perdere profondità, generando pressione e caos controllato. Le unità nemiche non sono intercambiabili — un droide B1 non è un B2 né un Droideka — e ciascuna richiede una risposta tattica diversa (dettaglio in Appendice B). La difficoltà crescente non deve limitarsi ad aumentare i punti vita: deve rendere i nemici più intelligenti, ridurre le risorse e complicare le situazioni.
**Parametri e dati. **Per ogni arma: rateo di fuoco, danno per colpo, calore generato/soglia di surriscaldamento, spread base e in movimento/mira, rinculo, tempo di equipaggiamento, profilo balistico, categoria di danno (fanteria/anti-scudo/anti-corazza). Per ogni entità combattente: pool scudo + salute, resistenze per tipo di danno, zone di danno (headshot/punti deboli), stato (integro/a terra/eliminato).
**Interconnessioni. **Legge da Equipaggiamento (loadout, cap. 13) e Classi (cap. 12); scrive su Progressione (esperienza per uso coerente, cap. 11) ed Economia tattica (obiettivi, cap. 7); è pilotato per gli NPC dall'IA (cap. 8); consuma dati da Mappe (coperture, verticalità, cap. 10).

### **7. Sistema di Squadra e Comando Tattico**
**Scopo. **Trasformare uno sparatutto individuale in un'esperienza di comando: il giocatore è sempre parte di una struttura organizzata e, salendo di grado, ne dirige porzioni crescenti. La forza della Repubblica nasce dalla combinazione dei ruoli, non dal singolo.

#### **7.1 Struttura gerarchica e controllo scalabile col grado**
Il controllo disponibile dipende dal grado del giocatore (cap. 11). Un soldato semplice esegue e collabora; un caporale guida un fireteam; un sergente una squadra; un ufficiale coordina più squadre. La gerarchia segue la struttura canonica della GAR (Appendice D), con il **fireteam** come astrazione di gameplay per la più piccola unità manovrabile:

| **Unità** | **Composizione** | **Comandante (grado giocatore)** | **Cosa controlla il giocatore** |
| --- | --- | --- | --- |
| Fireteam | ~4 cloni | Caporale | Posizioni, movimento tattico, ordini base |
| Squadra | 9 cloni (2 fireteam) | Sergente | Più fireteam, scelta dell'approccio, coordinamento |
| Plotone | 36 cloni (4 squadre) | Tenente | Assegnazione obiettivi a più squadre |
| Compagnia e oltre | 144+ cloni | Capitano+ | Priorità operative, gestione risorse (astratta) |

Ai gradi elevati il controllo diventa più astratto (ordini a gruppi, priorità, richieste di supporto) per non trasformare il gioco in un RTS: il giocatore resta un soldato sul campo che *dirige*, non un'interfaccia che micro-gestisce centinaia di unità.

#### **7.2 Sistema di ordini a due livelli**
Il comando deve essere rapido e usabile *durante* il combattimento, senza menu lenti. Si adotta un modello a due livelli, con riferimento diretto al sistema contestuale di *Republic Commando*.
**Livello 1 — Ordine contestuale (un tasto). **Il giocatore punta il reticolo su un elemento del mondo e un tasto emette l'ordine appropriato al contesto. Esempi: puntare una **copertura/posizione elevata** → "prendi posizione"; una **porta/breccia** → "sfonda e bonifica"; una **console** → "hackera/slice"; un **nemico** → "concentra il fuoco"; un **alleato a terra** → "rianima"; un **punto** → "raggiungi e mantieni". Il mondo evidenzia gli elementi interagibili quando il reticolo li sorvola.
**Livello 2 — Ruota di comando. **Per ordini non contestuali o comportamenti generali si apre una ruota rapida (radiale) con categorie: **Movimento** (vai, seguimi, mantieni, avanza), **Combattimento** (attacca, difendi, concentra il fuoco, elimina minaccia), **Comportamento** (aggressivo, difensivo, prudente, autonomo), **Supporto** (cura, copertura, gadget, anti-veicolo).
Il sistema contestuale rende la tattica parte del flusso dell'azione invece di un livello separato: è la chiave per "complessità accessibile".

#### **7.3 Autonomia dell'IA di squadra**
Gli alleati non aspettano un ordine per ogni gesto: il giocatore stabilisce obiettivo, priorità e comportamento generale, mentre l'IA gestisce esecuzione, micro-movimenti e reazioni immediate. Esempio: un Medic che vede un compagno ferito valuta, cerca una posizione sicura e interviene autonomamente. Gli alleati cercano coperture, reagiscono agli attacchi, chiedono supporto e sfruttano le occasioni. Il dettaglio comportamentale è nel cap. 8.

#### **7.4 Peso delle perdite**
La perdita di un alleato non è la sostituzione di un NPC generico: riduce le capacità della squadra, modifica gli approcci possibili e incide sulla valutazione della missione. Nelle modalità persistenti (cap. 17) i membri veterani della squadra hanno identità e progressione proprie, così che perderli sia una perdita reale.
**Parametri e dati. **Per la squadra: roster (id unità, classe, ruolo, esperienza, stato), formazione corrente, ordine attivo per unità/gruppo, postura di comportamento. Per il comando: mappa contesto→ordine, livello di comando abilitato dal grado, coda ordini. Comunicazione tattica: eventi radio (nemico avvistato, unità ferita, ordine completato).
**Interconnessioni. **Dipende da Progressione/Gradi (livello di comando, cap. 11) e Classi (ruoli, cap. 12); pilota l'IdS di squadra (cap. 8); alimenta HUD/comandi (cap. 18); genera esperienza e valutazione verso Progressione e Persistenza (cap. 11, 17); consuma e produce eventi dell'Economia tattica (richieste di supporto).

### **8. Intelligenza Artificiale e Comportamento delle Unità**
**Scopo. **Creare unità che sembrino soldati dentro una guerra organizzata, non bersagli automatici. L'IA non è progettata solo per "battere il giocatore", ma per generare pressione, opportunità tattiche e momenti imprevedibili. Test mentale di riferimento: *"se il giocatore non fosse presente, questa battaglia sembrerebbe comunque una vera battaglia?"*

#### **8.1 Tre livelli di IA**
L'IA è organizzata in tre strati che devono comunicare tra loro:
- **Individuale** — la singola unità: movimento, mira, uso dell'arma, ricerca copertura, reazione alle minacce. Implementabile con behavior tree / state machine per unità.
- **Squadra** — il gruppo: formazioni, coordinazione, fiancheggiamenti, difesa e supporto reciproco. Un livello di IA che assegna ruoli e posizioni ai membri.
- **Operativo (Commander AI / "Director")** — l'intera battaglia: priorità degli obiettivi, distribuzione delle unità, ondate di rinforzi, innesco di eventi dinamici in base al ritmo. È questo strato a far sì che la guerra evolva senza il giocatore ed è responsabile del pacing (ispirazione: director di gioco che modula pressione e rinforzi).

#### **8.2 IA alleata**
Deve essere utile senza sostituire il giocatore. Esegue gli ordini (cap. 7), ma con autonomia: sotto ordine di "difendi", cerca coperture, mantiene il punto, reagisce e chiede supporto quando serve. Il giocatore guida la squadra, non ne controlla ogni azione.

#### **8.3 IA nemica**
Il nemico deve essere una minaccia credibile: non avanza frontalmente sparando a caso in attesa di essere eliminato. Comportamenti offensivi (avanzare, aggirare, usare coperture, concentrare il fuoco, sfruttare la superiorità numerica) e difensivi (mantenere posizioni, creare punti forti, proteggere obiettivi, ritirarsi quando serve). Il comportamento deriva dall'obiettivo: un nemico che difende una base non agisce come uno che deve ritirarsi.

#### **8.4 IA per fazione (identità asimmetrica)**
L'IA riflette l'identità delle fazioni (cap. 15):
- **Repubblica** — coordinata, disciplinata, basata sulla squadra: formazioni, supporto reciproco, ordini gerarchici. Qualità e adattabilità.
- **CSI (Separatisti)** — numerica e automatizzata: pressione costante, aggressività, unità specializzate ma meno flessibili. Quantità e produzione. L'asimmetria comportamentale è un pilastro dell'esperienza di combattimento (cap. 6, App. B).

#### **8.5 Percezione e informazione**
Un'unità non conosce automaticamente tutto: la sua conoscenza dipende da visibilità, rumore, distanza e informazioni ricevute dagli alleati. Le unità rilevano minacce, le comunicano e condividono obiettivi. Questo sistema di percezione/comunicazione è ciò che rende sensate le tattiche del giocatore (diversivi, aggiramenti, ricognizione): una tattica funziona solo se il nemico *reagisce*.

#### **8.6 Unità speciali e HVT**
Alcune unità (comandanti, ARC Trooper, Jedi, droidi tattici, droidi specializzati) hanno capacità superiori e modificano il comportamento della battaglia. Il **Droide Tattico serie T** è l'esempio canonico di *High Value Target*: non combatte direttamente ma migliora l'IA di tutti i droidi vicini; eliminarlo "rompe" l'organizzazione nemica nell'area. Le HVT creano obiettivi tattici emergenti (App. B).

#### **8.7 Prestazioni e scala**
Le grandi battaglie richiedono di gestire molti combattenti mantenendo credibilità e frame rate: non tutte le unità hanno lo stesso livello di complessità. Si adotta un modello a **Level of Detail comportamentale** — logica completa vicino al giocatore, logica semplificata/aggregata per le unità lontane — così da simulare eserciti senza costo lineare.
**Parametri e dati. **Per unità: profilo percezione (raggio vista, FOV, udito), memoria minacce, blackboard, stato behavior tree. Per squadra: ruoli, punti tattici disponibili (coperture, punti elevati) forniti dalla mappa. Per il director: budget rinforzi, tabella eventi, curva di intensità/pacing, priorità obiettivi.
**Interconnessioni. **Guida il Combattimento per gli NPC (cap. 6); riceve ordini dalla Squadra (cap. 7); legge la geometria tattica dalle Mappe (cap. 10); è vincolata dagli obiettivi del Mission Design (cap. 9); il director scrive sull'Economia tattica (rinforzi) e legge lo stato della Guerra dinamica (cap. 16).

### **9. Mission Design e Struttura degli Obiettivi**
**Scopo. **Costruire missioni che siano situazioni militari complete, non sequenze di combattimenti. Il giocatore non deve chiedersi "quanti nemici devo eliminare?" ma "qual è il problema militare da risolvere, con le risorse che ho?".

#### **9.1 Struttura di una missione**
Ogni missione segue lo scheletro: **Briefing → Preparazione → Inserimento → Svolgimento → Eventi dinamici → Obiettivo finale → Risultato e conseguenze.** Il briefing informa senza eliminare la libertà: comunica situazione, obiettivo, informazioni note e rischi, ma non l'esatta sequenza di azioni.

#### **9.2 Tipologie di obiettivo**

| **Tipo** | **Obiettivo** | **Tensione principale** |
| --- | --- | --- |
| Eliminazione | Neutralizzare una minaccia (comandante, unità speciale, sistema difensivo) | La difficoltà nasce dalla situazione, non dal bersaglio |
| Difesa | Proteggere una posizione o un'unità | Gestione risorse, scelta posizioni, resistenza alle ondate |
| Conquista | Prendere e mantenere il controllo di un'area | Avanzata + coordinazione + tenuta |
| Recupero | Ottenere informazioni, oggetti o persone | Esplorazione, pressione temporale, protezione dell'obiettivo |
| Sabotaggio | Danneggiare capacità nemiche (strutture, sistemi, comunicazioni) | Pianificazione, infiltrazione, fuga |
| Evacuazione | Portare in salvo unità o informazioni | Sopravvivenza, ritirata gestita, adattamento |

#### **9.3 Obiettivi secondari e approcci multipli**
Gli obiettivi secondari non sono obbligatori ma offrono vantaggi, ponendo al giocatore la scelta *"vale la pena rischiare per questo vantaggio?"*. Ogni missione deve permettere strategie diverse — es. distruggere una base separatista via assalto diretto (rapido, rischioso), infiltrazione (pianificato, poco combattimento) o attacco tattico (neutralizzare le difese, preparare l'area) — senza che un approccio sia sempre superiore. Questo requisito vincola direttamente il level design (cap. 10).

#### **9.4 Eventi dinamici**
Gli eventi rendono le missioni imprevedibili: arrivo di rinforzi, perdita di una posizione, cambio degli obiettivi, nuova minaccia, opportunità improvvisa. Devono essere **conseguenza della situazione** (generati dal director in base allo stato della battaglia), non script casuali. È qui che nascono le storie emergenti.

#### **9.5 Fallimento come parte della storia**
Il fallimento non è sempre "missione fallita, ricarica": può significare perdita di risorse, peggioramento della situazione strategica, cambiamento degli obiettivi o necessità di una nuova strategia. Nelle modalità persistenti il fallimento alimenta la guerra dinamica invece di interromperla.

#### **9.6 Scala e valutazione**
Il gioco alterna scontri piccoli (tattica, precisione, squadra), operazioni medie (coordinazione, più obiettivi, campo mutevole) e battaglie grandi (scala, caos controllato, ruolo dentro un esercito). La valutazione finale pesa obiettivi (primari/secondari/falliti), prestazione tattica (gestione squadra, uso risorse, efficacia decisioni) e costi (perdite, tempo, risorse). Il risultato è narrativo, non un semplice voto.
**Parametri e dati. **Definizione missione: tipo, obiettivi (primari/secondari con condizioni), mappa, fazioni e composizioni, budget director, tabella eventi condizionati, condizioni di vittoria/fallimento parziale. Runtime: stato obiettivi, timeline eventi, log decisioni per il debrief.
**Interconnessioni. **Orchestrata dal director (cap. 8); legge Mappe (cap. 10); definisce il contesto per Combattimento e Squadra; produce risultati verso Progressione (cap. 11), Persistenza (cap. 17) e Guerra dinamica (cap. 16). In Galactic Conquest le missioni sono *generate* dallo stato strategico invece che scritte a mano.

### **10. Mappe e Level Design**
**Scopo. **Le mappe sono strumenti tattici, non semplici arene. Devono creare decisioni: non chiedono "qual è la strada giusta?" ma "qual è il modo migliore di affrontare questa situazione?".

#### **10.1 Tre scale di lettura**
Ogni mappa si legge su tre livelli sovrapposti: **tattico** (lo spazio immediato: coperture, posizioni, percorsi brevi), **operativo** (la zona controllata dalla squadra: obiettivi, aree strategiche, punti di supporto), **strategico** (il fronte: movimenti delle unità, controllo territoriale). Le mappe devono supportare dimensioni diverse — piccole (basi, edifici: CQB e infiltrazione), medie (avamposti, zone industriali: tattica di squadra), grandi (pianure, città: veicoli e guerra aperta).

#### **10.2 Ambientazioni planetarie**
Ogni pianeta ha caratteristiche che cambiano il gameplay: **desertici** (spazi aperti, poca copertura, veicoli e controllo delle distanze — Geonosis, Tatooine), **urbani** (edifici, imboscate, controllo verticale — Coruscant, Christophsis), **forestali** (visibilità ridotta, occultamento, agguati — Kashyyyk, Felucia), **industriali** (strutture separatiste, sabotaggio, combattimento verticale), **navi e stazioni** (corridoi, sezioni chiuse, operazioni speciali).

#### **10.3 Elementi che generano tattica**
- **Punti strategici** con vantaggi reali: alture, torrette, depositi, centri comunicazione, posizioni difensive.
- **Verticalità:** piani superiori, strutture sopraelevate, tunnel, livelli sotterranei che influenzano visibilità, sicurezza e capacità di supporto.
- **Percorsi alternativi:** via principale, percorsi secondari, aggiramenti (una base affrontabile da ingresso principale, tunnel di servizio, punto sopraelevato o sabotaggio interno).
- **Elementi interattivi con scopo tattico:** porte, sistemi difensivi, generatori (di scudo), comunicazioni, strutture distruttibili.

#### **10.4 Mappe polivalenti e rigiocabilità**
Una mappa non deve essere legata a un solo tipo di missione: la stessa ambientazione supporta assalto, difesa, ricognizione, evacuazione e sabotaggio. Il design deve supportare l'IA (coperture, aggiramenti, difese, ritirate credibili) e le situazioni emergenti: nelle mappe sandbox più ampie la battaglia deve poter generare eventi non previsti (una squadra alleata isolata, un veicolo nemico che entra nell'area, il giocatore che sceglie dove intervenire). La grande scala non deve eliminare il dettaglio: dentro una grande battaglia devono restare piccoli scontri, decisioni locali e momenti personali.
**Parametri e dati. **Per mappa: navmesh e volumi di navigazione (fanteria + veicoli), grafo di punti tattici (coperture con direzione, punti elevati, chokepoint), zone/settori catturabili con stato di controllo, spawn/insertion point, elementi interattivi con id e stato, metadati di scala e ambiente per il pacing.
**Interconnessioni. **Fornisce dati a IA (punti tattici, navmesh, cap. 8), Mission Design (zone obiettivo, cap. 9), Squadra (posizioni ordinabili, cap. 7), Veicoli (percorribilità, cap. 14). In Galactic Conquest ogni mappa è associata a uno o più pianeti/settori della mappa galattica (cap. 16).

## **Parte IV — Progressione e Personalizzazione**

### **11. Progressione Militare, Gradi e Carriera**
**Scopo. **Simulare una vera carriera militare, non un semplice sistema di livelli. Il clone evolve da recluta a veterano fino a figura di comando attraverso esperienza, risultati, specializzazione e responsabilità crescente. La domanda centrale del sistema è: *"che tipo di soldato della Repubblica sta diventando il mio clone?"*

#### **11.1 I tre assi della crescita**
La progressione è composta da tre assi indipendenti ma collegati, che vanno modellati separatamente:

| **Asse** | **Rappresenta** | **Determina** |
| --- | --- | --- |
| Grado (carriera) | Posizione nella gerarchia militare | Livello di comando, responsabilità, accesso a missioni e influenza |
| Classe (ruolo operativo) | Funzione sul campo | Loadout, comportamento, contributo alla squadra |
| Specializzazione (élite) | Percorsi militari avanzati | Capacità uniche, equipaggiamenti avanzati, accesso a unità d'élite |

Separare i tre assi permette al giocatore di essere, per esempio, un Sergente (grado) con classe Heavy e specializzazione ARC in via di sblocco — tre dimensioni ortogonali invece di un'unica barra di livello.

#### **11.2 Sistema dei gradi (allineato alla GAR canonica)**
La progressione dei gradi rispetta la struttura della Grande Armata della Repubblica (Appendice D), collegando ogni grado a un livello di comando concreto (cap. 7). Il giocatore parte come Trooper e sale mano a mano che dimostra competenza. I gradi di comando più alti diventano progressivamente astratti sul piano del gameplay, per non snaturare l'esperienza di soldato.

| **Grado (giocatore)** | **Unità comandata** | **Effettivi** | **Focus di gameplay** |
| --- | --- | --- | --- |
| Clone Trooper | — | 1 | Combattimento diretto, esecuzione, sopravvivenza |
| Caporale | Fireteam | ~4 | Primi ordini, movimento tattico |
| Sergente | Squadra | 9 | Gestione di più fireteam, scelta dell'approccio |
| Tenente | Plotone | 36 | Coordinamento di più squadre, obiettivi locali |
| Capitano | Compagnia | 144 | Operazioni più grandi, gestione risorse |
| Maggiore / Comandante | Battaglione / Reggimento | 576 / 2.304 | Coordinamento di operazioni importanti |
| Marshal Commander | Corpo | ~36.864 | Comando strategico regionale (astratto) |
| Alto Generale | Grande Armata | ~milioni | Comando supremo (livello narrativo/end-game) |

Nota di canone: nella GAR le formazioni superiori al reggimento sono guidate anche da Jedi Generali. Poiché il giocatore è un clone, i gradi elevati rappresentano il ruolo di *Clone Commander* al fianco della leadership Jedi (cap. 15), non la sostituzione del Jedi.

#### **11.3 Progressione diegetica: si diventa ciò che si gioca**
Principio fondamentale: la classe non è una scelta rigida all'inizio, ma un'identità che emerge dal comportamento. Un clone che usa spesso armi pesanti sviluppa capacità da Heavy; uno che completa missioni di ricognizione sviluppa tratti da Recon. Ogni classe ha una propria progressione, con esperienza ottenuta tramite azioni coerenti:
- **Heavy** — uso di armi pesanti, distruzione di mezzi, difesa di posizioni.
- **Medic** — cura degli alleati, rianimazioni, supporto alla squadra.
- **Recon** — ricognizione, eliminazioni precise, raccolta informazioni.
- **Engineer** — hackeraggio, riparazioni, sabotaggio, gestione di dispositivi.
La domanda non è "quale classe scelgo?" ma "che soldato sto diventando attraverso le mie azioni?". Questo lega la progressione al gameplay reale invece che a un menu iniziale.

#### **11.4 Fonti di esperienza e valutazione delle prestazioni**
L'esperienza non dipende dal solo numero di uccisioni (Pilastro 3). Fonti principali: completamento degli obiettivi, successo della missione, supporto alla squadra, sopravvivenza degli alleati, decisioni tattiche efficaci, compiti speciali. Ogni missione valuta il giocatore su tre dimensioni — **efficienza militare** (obiettivi, tempo, risorse), **prestazione tattica** (uso della squadra, scelta degli approcci, gestione delle crisi), **capacità individuale** (combattimento, sopravvivenza, uso dell'equipaggiamento). Queste valutazioni alimentano le promozioni.

#### **11.5 Promozioni**
Le promozioni sono legate alle prestazioni, non al completamento automatico delle missioni. Esempio di design coerente con la filosofia: un soldato che completa missioni difficili mantenendo basse le perdite alleate deve poter avanzare più rapidamente di uno che vince con tattiche puramente aggressive e costose. Il grado deve cambiare il *modo di giocare* (più comando, nuove missioni), non solo aumentare un numero.

#### **11.6 Specializzazioni d'élite**
Le specializzazioni sono percorsi avanzati ("élite militare") sbloccati da requisiti coerenti: esperienza elevata, missioni difficili completate, uso efficace di certi equipaggiamenti, risultati sul campo. Le abilità che concedono rappresentano *addestramento militare*, non poteri arbitrari.
- **ARC Trooper** — autonomia operativa, combattimento avanzato, capacità tattiche superiori, accesso a equipaggiamenti avanzati (es. DC-17 in dual-wield).
- **Clone Commando** — operazioni speciali, missioni ad alto rischio, equipaggiamento specializzato, indipendenza.
- **Special Operations** — infiltrazione, sabotaggio, missioni non convenzionali.
**Parametri e dati. **Profilo clone: grado + XP di grado, classe attiva + XP per-classe, specializzazioni sbloccate, abilità/perk, statistiche di carriera. Regole di promozione (soglie XP + criteri di prestazione). Tabelle di sblocco (grado/esperienza/missioni → equipaggiamento e specializzazioni). Il profilo è l'entità persistente centrale del gioco (cap. 17).
**Interconnessioni. **Riceve segnali da Combattimento, Squadra e Mission Design (cap. 6, 7, 9); determina il livello di comando (cap. 7) e gli sblocchi di Equipaggiamento (cap. 13); è serializzata dalla Persistenza (cap. 17); in Galactic Conquest cresce insieme allo stato della guerra (cap. 16).

### **12. Classi, Ruoli e Specializzazioni**
**Scopo. **Rappresentare professioni militari, non semplici categorie di armi. Ogni classe cambia comportamento sul campo, contributo alla squadra, approccio tattico e priorità. Nessuna classe è superiore alle altre: ognuna ha vantaggi, limiti e situazioni in cui eccelle o necessita di supporto. La forza nasce dalla combinazione.

#### **12.1 Classi principali**

| **Classe** | **Ruolo** | **Punti di forza** | **Limiti** |
| --- | --- | --- | --- |
| Clone Trooper (base) | Fanteria versatile | Equilibrio, adattabilità, facilità d'uso | Non eccelle nei ruoli dei specialisti |
| Heavy Trooper | Supporto offensivo, controllo area | Anti-gruppo e anti-corazza, tenuta delle posizioni | Mobilità ridotta, dipende dal posizionamento |
| Engineer / Specialist | Supporto tecnico, problem solving | Versatilità, hacking, torrette, sabotaggio | Meno efficace negli scontri diretti |
| Recon / Sniper | Ricognizione, ingaggio a distanza | Informazioni, precisione, controllo del territorio | Vulnerabile da vicino, richiede posizioni |
| Medic / Support | Sopravvivenza della squadra | Cura, rianimazione, tenuta nelle battaglie lunghe | Bassa potenza offensiva individuale |

#### **12.2 Classi d'élite (obiettivo di progressione)**
Le classi d'élite non sono selezionabili all'inizio ma sono il traguardo del percorso di carriera:
- **ARC Trooper** — clone d'élite altamente addestrato, grande autonomia, missioni ad alto rischio, comando sul campo.
- **Clone Commando** — squadra specializzata per infiltrazione, sabotaggio ed eliminazione dietro le linee nemiche; meno adatta agli scontri frontali su larga scala.
- **Ruoli di comando** — evoluzione oltre il combattente: coordinare squadre, assegnare obiettivi, gestire risorse, prendere decisioni tattiche (collega Progressione e Sistema di Squadra).

#### **12.3 Le classi come progettazione di squadra (NPC)**
Le classi servono anche a definire le composizioni degli NPC, il loro comportamento IA e loadout, per creare formazioni credibili. Una squadra bilanciata — Trooper (flessibilità) + Heavy (potenza) + Recon (informazioni) + Engineer (supporto) + Leader (coordinazione) — deve essere più efficace di una monoclasse, e deve *comportarsi* diversamente. Questo lega il sistema di classi direttamente all'IA (cap. 8).

#### **12.4 Percorsi evolutivi**
Le classi non sono compartimenti stagni: la progressione permette percorsi ramificati, es. *Clone Trooper → specializzazione offensiva → Heavy avanzato → ARC Heavy*, oppure *Clone Trooper → ricognizione → Recon avanzato → ARC Recon*. La crescita deve essere naturale e leggibile.
**Parametri e dati. **Definizione classe: loadout base, abilità/perk sbloccabili, curva XP di classe, comportamento IA associato (per gli NPC), affinità con equipaggiamenti. Requisiti di sblocco delle classi d'élite.
**Interconnessioni. **Definisce il ruolo per Combattimento (cap. 6) e IA (cap. 8); vincola l'accesso all'Equipaggiamento (cap. 13); è un asse della Progressione (cap. 11); determina il ruolo tattico nel Sistema di Squadra (cap. 7).

### **13. Equipaggiamento, Armi e Personalizzazione**
**Scopo. **Permettere al giocatore di trasformare un clone standard in un soldato riconoscibile, con un ruolo preciso e uno stile di combattimento personale. L'equipaggiamento è una scelta strategica ("cosa porto per questa operazione?"), non un semplice aumento di potenza. La domanda centrale: *"come combatte il mio clone e quale ruolo ricopre?"*

#### **13.1 Filosofia: nessuna scelta universalmente migliore**
Un oggetto migliore non è automaticamente la scelta migliore. Ogni scelta comporta vantaggi, svantaggi e un contesto ideale. Un'arma avanzata può essere più potente ma più difficile da controllare; una modifica può aumentare la precisione riducendo la mobilità. L'ultimo sblocco non deve essere sempre superiore: il sistema evita la progressione lineare di potenza a favore della differenziazione (Pilastro 1 e 6).

#### **13.2 Slot operativi (loadout)**
Il clone dispone di un set di slot che il technical design tratterà come struttura dati del loadout: **arma primaria, arma secondaria, equipaggiamento tattico, gadget, corazza/moduli, accessori, modifiche.** Le armi primarie coprono i quattro ruoli della matrice del cap. 6 (fucili, pesanti, precisione, specialistiche); le secondarie (pistole blaster, armi compatte, strumenti d'emergenza) coprono i casi in cui la primaria non è adatta.

#### **13.3 Modifiche e gadget**
Le armi si personalizzano con modifiche (ottiche, sistemi di mira, caricamenti, stabilizzatori, componenti specializzati) che devono creare scelte, non trasformarsi in un'unica configurazione obbligatoria. I gadget offrono nuove possibilità tattiche in tre famiglie: **offensivi** (esplosivi, anti-veicolo, dispositivi tattici), **difensivi** (protezione, copertura, sopravvivenza), **tecnici** (hackeraggio, riparazione, interazione con i sistemi). Il dettaglio dell'arsenale, dei gadget e delle granate è in Appendice A.

#### **13.4 Personalizzazione: identità del clone**
Tra migliaia di soldati identici, il proprio clone deve essere riconoscibile. La personalizzazione è sia **estetica** (colori dell'armatura, livree di battaglione, simboli, usura, elmetto, accessori) sia **funzionale** (moduli d'armatura che alterano capacità). Un elemento chiave: i **colori non definiscono il grado** (gestito dall'esperienza) ma esprimono lo stile e l'appartenenza a battaglioni della lore — 501ª (blu, assalto), 212° (arancione, assedio/supporto pesante), 41° (verde/camo, ricognizione), 104° "Wolfpack" (grigio, recupero e supporto). Vedi Appendice A per il sistema modulare d'armatura completo.

#### **13.5 Sblocco come riconoscimento**
L'equipaggiamento si ottiene tramite la progressione militare (grado, esperienza, missioni, specializzazione). Una ricompensa deve raccontare una storia — un riconoscimento, una nuova responsabilità, un miglioramento guadagnato sul campo — non sembrare un premio casuale. La differenza tra equipaggiamento standard e specializzato marca la differenza tra soldato normale, veterano e unità d'élite.
**Parametri e dati. **Catalogo item (id, categoria, statistiche, ruolo, requisiti di sblocco, affinità di classe). Struttura loadout per profilo. Regole di modifica (compatibilità arma↔mod, trade-off). Dati cosmetici (livree, simboli, usura) separati dai funzionali. Loadout NPC per composizione delle squadre.
**Interconnessioni. **Consumato dal Combattimento (statistiche effettive, cap. 6); vincolato da Classi (affinità, cap. 12) e Progressione (sblocchi, cap. 11); determina i loadout NPC usati dall'IA (cap. 8); serializzato dalla Persistenza (cap. 17); presentato in fase di Preparazione dall'UI (cap. 18).

### **14. Veicoli e Combattimento su Larga Scala**
**Scopo. **Ampliare le possibilità tattiche e dare la scala della Guerra dei Cloni. Un veicolo non è "un'arma più potente": cambia il modo di affrontare una situazione. L'obiettivo non è un simulatore di veicoli complesso, ma mezzi ben integrati nel sistema generale della battaglia.

#### **14.1 Categorie e ruoli**

| **Categoria** | **Funzione** | **Esempi (App. C)** |
| --- | --- | --- |
| Trasporto | Movimento truppe, supporto logistico | Gunship LAAT, mezzi da sbarco |
| Corazzati / Walker | Combattimento diretto, supporto alla fanteria | AT-RT (leggero), AT-TE (pesante), AAT (CSI) |
| Aerei | Superiorità aerea, supporto tattico, ricognizione | Caccia e supporto orbitale |
| Specializzati | Missioni e operazioni specifiche | Mezzi da ricognizione/artiglieria |

#### **14.2 Interazione del giocatore e ruolo della classe**
Il livello di controllo dipende dal ruolo: pilotare, usare una postazione/torretta, richiedere supporto, o coordinare i mezzi. Un soldato semplice usa una torretta; un comandante coordina l'impiego dei veicoli. Le classi influenzano il rapporto con i mezzi: l'Heavy è più efficace con armi anti-veicolo, l'Engineer gestisce sistemi e riparazioni, il comandante assegna gli obiettivi ai mezzi.

#### **14.3 Fanteria contro veicoli (rapporto tattico)**
Un soldato singolo non deve poter affrontare facilmente un mezzo pesante senza strumenti adeguati. Per contrastarlo servono armi specializzate (PLX-1), supporto della squadra, attacchi coordinati o sfruttamento dell'ambiente e dei punti deboli (es. il motore posteriore dell'AAT). Questo mantiene i veicoli minacciosi e valorizza la coordinazione.

#### **14.4 Veicoli come nodi tattici**
I mezzi non sono solo strumenti offensivi ma punti di ancoraggio della battaglia. Meccanica di riferimento (App. C): l'**AT-TE come Centro di Comando Mobile** — se presente nell'area consente respawn e rifornimento della squadra, diventando un obiettivo cruciale da difendere. I veicoli si collegano direttamente agli obiettivi (proteggere un convoglio, distruggere un mezzo nemico, conquistare un punto con supporto corazzato, evacuare via trasporto).

#### **14.5 IA, distruzione e scala**
Non tutti i veicoli sono guidati dal giocatore: molti sono gestiti dall'IA (colonne militari, supporto alleato, mezzi nemici, trasporti), il che permette grandi battaglie senza dipendere dal giocatore. Il sistema di danno è a **punti deboli e perdita progressiva di funzionalità** (parti danneggiabili) più che a semplice barra di salute: la distruzione totale non è sempre necessaria, un mezzo danneggiato cambia comunque la battaglia. Come per l'IA, la scala richiede attenzione alle prestazioni: pochi veicoli significativi, comportamenti credibili, buona integrazione.
**Parametri e dati. **Per veicolo: pool corazza + componenti danneggiabili (con effetti sulla funzionalità), punti deboli, postazioni (pilota/artigliere/passeggeri), profilo di mobilità, ruolo IA, eventuale funzione speciale (respawn/rifornimento). Percorribilità legata al navmesh veicoli della mappa.
**Interconnessioni. **Estende Combattimento (cap. 6) e IA (cap. 8); usa la percorribilità delle Mappe (cap. 10); si lega a Mission Design (obiettivi con veicoli, cap. 9) e all'Economia tattica (richiesta di mezzi, cap. 7); contribuisce alla scala percepita della Guerra dinamica (cap. 16).

## **Parte V — Mondo e Meta-gioco**

### **15. Fazioni e Universo**
**Scopo. **Raccontare la Guerra dei Cloni dal punto di vista della Repubblica. A differenza di altri giochi, il focus non è rendere entrambe le fazioni giocabili, ma far vivere il conflitto attraverso i cloni. La Repubblica non è un'estetica: è una struttura militare con gerarchia, disciplina, specializzazione e organizzazione.

#### **15.1 Identità della Repubblica**
L'esercito repubblicano è caratterizzato da cloni altamente addestrati, tecnologia avanzata, organizzazione militare e cooperazione tra unità. La sua forza non deriva dal numero ma dalla **qualità e adattabilità** delle truppe. Il giocatore deve sentirsi parte di un esercito reale (Appendice D per la struttura).

#### **15.2 I Jedi e la leadership**
I Jedi fanno parte della struttura militare come Generali e Comandanti, e possono comparire come figure di comando, presenza narrativa o supporto in alcune operazioni. Ma il focus resta il soldato clone: il giocatore appartiene all'esercito, non si trasforma automaticamente nel protagonista Jedi della battaglia. Questa scelta di design mantiene la fantasia coerente col concept.

#### **15.3 La CSI come nemico con identità**
Anche se non giocabile, la Confederazione dei Sistemi Indipendenti ha una propria identità: grandi quantità di droidi, produzione industriale, unità specializzate, strategie basate su quantità e automazione. Non è "la fazione avversaria" generica, ma un avversario con un carattere tattico riconoscibile (Appendice B per il bestiario).

#### **15.4 L'asimmetria come pilastro di gameplay**

|   | **Repubblica** | **CSI (Separatisti)** |
| --- | --- | --- |
| Principio | Qualità e adattabilità | Quantità e produzione |
| Basata su | Soldati esperti, coordinazione, flessibilità | Numeri elevati, pressione costante, unità specializzate |
| Esperienza di combattimento | Poche unità capaci, tattica di squadra | Ondate, superiorità numerica, minacce specializzate |

Questa asimmetria influenza direttamente l'IA (cap. 8), il combattimento (cap. 6) e il bilanciamento: contro i Separatisti il giocatore affronta grandi numeri e deve compensare con tattica e qualità.

#### **15.5 Fedeltà all'universo al servizio del gameplay**
Il gioco rispetta l'identità della Guerra dei Cloni (estetica militare clone, tecnologia coerente, struttura dell'esercito, atmosfera del conflitto) ma adatta questi elementi alle necessità del gameplay. La fedeltà rafforza l'esperienza; la priorità resta un'esperienza coerente e divertente.
**Interconnessioni. **Definisce le identità comportamentali per l'IA (cap. 8) e le unità del Combattimento (cap. 6); popola il roster di Classi/unità (cap. 12); è la cornice della Guerra dinamica (cap. 16) e dei contenuti di Appendice B–C.

### **16. Galactic Conquest: Guerra Dinamica e Campagna Strategica**
**Scopo. **Trasformare Galactic Front da una raccolta di battaglie in una simulazione della Guerra dei Cloni. Ogni missione è parte di un conflitto più grande: il giocatore non combatte per completare livelli, ma per modificare l'andamento della guerra. La domanda centrale: *"quale impatto avrà il mio soldato sull'esito della guerra?"*

#### **16.1 Due livelli simultanei**
Il giocatore vive contemporaneamente il **livello personale** (la carriera del proprio clone) e il **livello galattico** (il destino di territori e operazioni). Il collante è il collegamento strategia↔FPS: il livello strategico determina il *contesto*, il livello FPS ne è l'*esecuzione*.

#### **16.2 Struttura della mappa galattica**
La guerra è rappresentata come un grafo gerarchico: **Galassia → Settori → Sistemi stellari → Pianeti → Operazioni → Missioni FPS.** Ogni livello influenza il successivo. Modello dati consigliato per la progettazione tecnica: un **grafo di nodi** (pianeti/sistemi) con stato di controllo (Repubblica / conteso / CSI), collegamenti (rotte/linee di rifornimento), valore strategico e risorse associate.

| **Elemento** | **Descrizione** | **Effetto sul gameplay** |
| --- | --- | --- |
| Pianeta (nodo) | Un fronte di guerra con valore, risorse, infrastrutture, valore narrativo | Un pianeta industriale fornisce produzione/tecnologia; uno di confine controlla una rotta |
| Controllo | Stato: Repubblica / conteso / CSI | Determina quali operazioni sono disponibili e chi ha l'iniziativa |
| Linee di rifornimento | Collegamenti tra nodi | La loro perdita isola i territori e cambia le priorità |
| Risorse strategiche | Truppe, mezzi, informazioni, logistica | Capacità militare; il giocatore le influenza tramite le missioni |

#### **16.3 Dalle operazioni alle missioni FPS**
La situazione strategica genera **operazioni** (difendere un pianeta, conquistare una posizione, distruggere infrastrutture, supportare un'offensiva). Ogni operazione si concretizza in una o più missioni FPS. Esempio del collegamento: situazione strategica *"la Repubblica deve conquistare un avamposto separatista"* → missione che può diventare assalto alla base, sabotaggio delle difese, eliminazione di un comandante o protezione delle truppe d'assalto. In questa modalità le missioni sono **generate** dallo stato della guerra (parametrizzando i template di Mission Design del cap. 9), non scritte a mano.

#### **16.4 Conseguenze e guerra dinamica**
Le missioni hanno conseguenze strategiche: un successo espande il controllo, ottiene risorse, migliora la posizione; un fallimento cede terreno, aumenta la pressione nemica, cambia gli obiettivi futuri. La guerra non è predeterminata — eventi possono modificarla (un pianeta viene attaccato, una rotta viene persa, appare una nuova minaccia). Il livello operativo dell'IA (il "director strategico", cap. 8) muove le forze CSI in modo autonomo, così che il fronte viva anche senza il giocatore (Pilastro 2).

#### **16.5 Ruolo crescente del giocatore**
Il ruolo nella guerra cambia con la progressione: a inizio carriera il giocatore influenza la propria squadra e singole operazioni; da veterano/comandante influenza più squadre, operazioni e decisioni tattiche. La carriera personale è così cucita allo stato galattico: un soldato che sopravvive a molte campagne diventa veterano, leader o specialista avanzato, e le esperienze vissute hanno un significato persistente.
**Parametri e dati. **Stato guerra: grafo pianeti (controllo, valore, risorse, collegamenti), fronti attivi, pool di forze per fazione, coda di operazioni, tabella eventi strategici. Regole di generazione operazioni (stato nodo → tipo missione + parametri). Turn/tick strategico e IA operativa CSI. È il secondo grande stato persistente del gioco insieme al profilo clone (cap. 17).
**Interconnessioni. **Consuma i template del Mission Design (cap. 9) e le Mappe associate ai pianeti (cap. 10); è mossa dall'IA operativa (cap. 8); alimenta e legge la Progressione (cap. 11); è serializzata dalla Persistenza (cap. 17). È il meta-loop descritto al cap. 5.3.

### **17. Persistenza, Salvataggio e Carriera**
**Scopo. **Mantenere la continuità dell'esperienza: il clone, le sue decisioni e i risultati devono avere conseguenze nel tempo. La domanda centrale: *"alla fine della guerra, quale storia racconterà il mio clone?"*

#### **17.1 Livelli di dati persistenti**
Il sistema distingue quattro categorie di dati, che la progettazione tecnica tratterà come modelli serializzabili distinti:

| **Categoria** | **Contenuto** | **Livello di dettaglio** |
| --- | --- | --- |
| Dati del personaggio | Nome/ID, grado, XP, classe, specializzazioni, abilità, equipaggiamento, estetica | Completo (entità centrale) |
| Dati di carriera | Missioni completate, risultati, promozioni, riconoscimenti, statistiche | Completo |
| Dati della guerra | Pianeti, territori controllati, campagne, situazione strategica, eventi | Strategico (grafo, cap. 16) |
| Dati delle unità | Roster squadra, sopravvissuti, perdite, esperienza, ruoli | Variabile (vedi 17.2) |

#### **17.2 Persistenza a livelli della squadra**
Non tutti gli alleati necessitano di persistenza individuale completa: il livello di dettaglio varia per contenere il costo. **Personaggi importanti** → persistenza completa (nome, storia, progressione, rapporto col giocatore); **soldati standard** → persistenza semplificata (ruolo, esperienza, stato operativo); **unità generiche** → persistenza tramite statistiche aggregate. Un clone veterano perso in missione rappresenta una perdita reale per l'unità.

#### **17.3 Persistenza per modalità**
- **Campagna Clone** — persistenza massima (carriera + squadra + eventi narrativi).
- **Galactic Conquest** — persistenza strategica (stato della guerra + carriera aperta).
- **Chronicles** — persistenza media (sblocchi condivisi, stato per-scenario).
- **Schermaglia** — persistenza ridotta o assente (focus sull'esperienza immediata).

#### **17.4 Scelte, conseguenze e gestione dei salvataggi**
Le decisioni modificano il contesto futuro senza richiedere enormi ramificazioni: missione riuscita → una base resta della Repubblica; missione fallita → il nemico ottiene un vantaggio strategico. Il sistema supporta salvataggi manuali, automatici e caricamento della carriera, con punti di salvataggio sicuri (fine missione, fase strategica); durante il combattimento va evitato qualsiasi salvataggio che comprometta la progressione.

#### **17.5 Compatibilità con lo sviluppo futuro**
La struttura dati deve essere modulare e versionata: aggiungere nuove campagne, specializzazioni, unità o modalità non deve richiedere una ricostruzione dei salvataggi esistenti. Questo requisito di **schema estensibile e retro-compatibile** è un vincolo diretto per la progettazione tecnica del sistema di persistenza.
**Parametri e dati. **Save game = { profilo clone (cap. 11/13), stato carriera, stato guerra (cap. 16), roster squadra a livelli }. Versioning dello schema, migrazione, slot di salvataggio, autosave su eventi chiave (fine missione, promozione, cambio stato strategico).
**Interconnessioni. **Serializza Progressione (cap. 11), Equipaggiamento (cap. 13), Squadra (cap. 7) e Guerra dinamica (cap. 16); è letto/scritto ai confini del Gameplay Loop (cap. 5). È, con lo stato guerra, uno dei due grandi contenitori di stato del gioco.

## **Parte VI — Presentazione**

### **18. Interfaccia, HUD e User Experience**
**Scopo. **Dare al giocatore le informazioni necessarie senza interrompere l'immersione. L'HUD deve sembrare parte dei sistemi integrati nell'armatura del clone, non un'interfaccia generica da videogioco. La domanda centrale: *"quali informazioni servono davvero al soldato per prendere decisioni migliori?"*

#### **18.1 Tre principi dell'HUD**
- **Informazioni essenziali prima di tutto** — munizioni/calore, stato del personaggio, ordini della squadra, obiettivo corrente devono essere leggibili a colpo d'occhio in combattimento.
- **Immersione (HUD diegetico)** — l'estetica richiama la tecnologia della Repubblica e la visiera dell'elmetto clone (riferimento: l'HUD-visore di Republic Commando, con effetti come lo sporco sulla visiera che si pulisce). L'estetica resta però subordinata alla leggibilità.
- **Semplicità** — troppe informazioni riducono leggibilità, immersione e attenzione. Ogni elemento deve avere uno scopo.

#### **18.2 HUD di combattimento e informazioni tattiche**
L'HUD principale mostra salute/scudo, munizioni/calore, arma e gadget equipaggiati, obiettivo corrente e stato della squadra. Le informazioni tattiche (posizione degli alleati, ordini attivi, minacce note, punti d'interesse) devono essere accessibili rapidamente senza trasformare lo schermo in una mappa strategica permanente.

#### **18.3 Comando della squadra nell'interfaccia**
L'interfaccia deve permettere di impartire ordini rapidamente (selezione squadra, assegnazione obiettivo, scelta comportamento, indicazione posizione), progettata per funzionare *durante* gli scontri. Questo è il front-end del sistema a due livelli del cap. 7: reticolo contestuale + ruota di comando. La **mappa tattica** offre una visione più ampia (obiettivi, unità note, informazioni strategiche) come strumento decisionale, non come semplice navigatore.

#### **18.4 Menu, personalizzazione e schermata di missione**
I menu supportano la complessità del gioco con aree chiare: campagna, personalizzazione clone, equipaggiamento, progressione, gestione squadra, impostazioni. La schermata di progressione mostra chiaramente grado, esperienza, specializzazione, abilità ed equipaggiamenti disponibili — cosa ho ottenuto, cosa posso sbloccare, quale direzione sto seguendo. La schermata di missione (Preparazione, cap. 5) presenta briefing, obiettivi, ambiente, minacce note ed equipaggiamento consigliato.

#### **18.5 Feedback, stile e accessibilità**
Il gioco comunica gli eventi importanti in modo immediato ma non invasivo (obiettivo completato, nuovo ordine, alleato in difficoltà, cambio della situazione tattica). Sono previste opzioni di adattamento: ridimensionamento HUD, modifica degli elementi visibili, impostazioni di leggibilità. L'accessibilità è parte del design, non un'aggiunta finale.
**Interconnessioni. **Visualizza dati da Combattimento (cap. 6), Squadra (cap. 7), Mission Design (cap. 9), Progressione (cap. 11), Equipaggiamento (cap. 13), Guerra dinamica (cap. 16). È il front-end del sistema di comando (cap. 7) e della fase di Preparazione del loop (cap. 5).

### **19. Audio e Immersione**
**Scopo. **Aumentare immersione e leggibilità del combattimento con un'identità sonora coerente con Star Wars. L'obiettivo non è un sistema audio estremamente complesso, ma un sistema semplice, scalabile e utile al gameplay. La domanda centrale: *"l'audio rende il combattimento più chiaro e immersivo senza aumentare inutilmente la complessità?"*

#### **19.1 Elementi fondamentali**
- **Armi** — ogni arma ha un suono riconoscibile che comunica categoria, potenza e ruolo, così da distinguere rapidamente le minacce a orecchio. Identità efficace più che simulazione realistica.
- **Ambiente** — livello base di atmosfera (battaglia, esplosioni, veicoli, interni/esterni) per evitare mappe "vuote".
- **Comunicazioni** — le comunicazioni militari (ordini, aggiornamenti, informazioni sugli obiettivi) sono soprattutto uno strumento di gameplay: chiare e funzionali.
- **Musica** — usata per i momenti importanti, i combattimenti principali e gli eventi narrativi; un sistema musicale dinamico complesso non è una priorità.

#### **19.2 Priorità di sviluppo audio**

| **Priorità** | **Elementi** |
| --- | --- |
| Alta | Effetti armi, suoni di movimento, feedback del giocatore, comunicazioni base |
| Media | Atmosfera ambientale, effetti veicoli, variazioni sonore |
| Bassa | Sistemi musicali avanzati, simulazione audio complessa, dettagli molto specifici |

Principi: l'audio serve prima di tutto il gameplay; la semplicità è preferibile alla complessità; l'identità (suonare "Star Wars") è più importante del realismo.

### **20. Animazioni, Movimento e Feel**
**Scopo. **Rendere il giocatore credibile come soldato clone: disciplina militare, peso dell'equipaggiamento, differenze tra ruoli, fluidità del combattimento. Non un sistema con centinaia di animazioni uniche, ma una base solida e modulare. La domanda centrale: *"il giocatore sente di essere un soldato clone quando si muove e combatte?"*

#### **20.1 Filosofia del movimento**
Il clone non si muove come un personaggio arcade né come un supereroe: ha controllo preciso, movimenti militari e reattività. Tre principi: **reattività** (controllo immediato, niente movimenti così pesanti da frustrare), **credibilità** (coerenza con armatura, arma e situazione), **differenziazione** (ruoli diversi si muovono e si percepiscono in modo diverso).

#### **20.2 Base modulare e movimento tattico**
Il sistema include le azioni fondamentali (camminata, corsa, sprint, accovacciamento, salto, cambio direzione, movimento con arma) come base riutilizzabile per tutte le classi, più il movimento tattico dell'FPS tattico (uso delle coperture, avanzamento controllato, posizionamento, movimento mentre si mira, transizioni tra posizioni). Le animazioni delle armi (impugnatura, mira, sparo, ricarica, cambio, rinculo) danno a ogni categoria una propria identità.

#### **20.3 Differenze tra ruoli e NPC**
Le animazioni comunicano il ruolo: il Trooper è equilibrato, l'Heavy ha peso e movimenti controllati (armi grandi), il Recon è agile e discreto, l'ARC/Commando trasmette sicurezza ed esperienza. Gli NPC (alleati e nemici) hanno animazioni coerenti col comportamento: muoversi in formazione, reagire agli ordini, usare correttamente le armi, interagire con l'ambiente. Il sistema è **modulare**: locomozione base e interazioni comuni condivise, elementi specializzati (ARC, unità speciali, equipaggiamenti particolari) aggiunti sopra.

#### **20.4 Animazioni al servizio del gameplay**
Le animazioni migliorano sempre l'esperienza e comunicano informazione: una ricarica più lunga rappresenta il peso di un'arma pesante; un movimento più rapido, un soldato specializzato. Le differenze devono essere percepibili ma non compromettere il controllo. Principio: la fluidità conta più della quantità; il gameplay viene prima del realismo.

## **Parte VII — Produzione e Architettura**

### **21. Filosofia di Sviluppo, Modularità e Priorità**
Galactic Front è un progetto ambizioso (un gioco completo e, potenzialmente, un engine dedicato) che deve mantenere un equilibrio tra profondità e sostenibilità. Questo capitolo fissa i principi che guidano ogni decisione tecnica e di produzione.

#### **21.1 Principi fondamentali**
- **Il gameplay viene prima della tecnologia.** La tecnologia esiste per abilitare le esperienze desiderate, non per creare sistemi complessi senza beneficio concreto. Ogni funzionalità deve rispondere: migliora combattimento, tattica o immersione? Rende la guerra più credibile?
- **Prima le fondamenta, poi l'espansione.** Meglio pochi sistemi solidi che molti incompleti. Ogni nuova funzionalità poggia su fondamenta già stabili.
- **Profondità tramite sistemi collegati, non tramite quantità.** Un sistema di comando semplice ma ben integrato con IA, missioni e progressione vale più di un sistema complessissimo usato raramente.
- **Modularità e schema estensibile.** Ogni sistema si sviluppa in modo indipendente ma si integra tramite interfacce chiare; aggiungere contenuti non deve richiedere riscritture (cap. 17, 22).
- **Scala intelligente.** Non serve simulare ogni elemento della galassia: l'impressione di una guerra enorme nasce da sistemi coerenti, IA credibile con LOD comportamentale, eventi dinamici e ambienti vivi.
- **Immersione dalla coerenza.** Un mondo credibile nasce da sistemi collegati e da un'estetica coerente, non dall'accumulo di dettagli isolati.

#### **21.2 Evitare i sistemi isolati**
I sistemi principali devono comunicare. La progressione influenza equipaggiamento, comando e specializzazioni; le missioni influenzano carriera, stato della guerra e risorse; le classi influenzano IA, tattiche e composizione delle squadre. La mappa completa di queste dipendenze è il cap. 22 — il riferimento primario per definire i moduli del codice e i loro confini.

#### **21.3 Filosofia dell'engine e dell'ottimizzazione**
L'engine (o il set di sistemi sopra un motore esistente) va progettato attorno alle esigenze specifiche di Galactic Front, non come motore universale con funzionalità inutilizzate. Priorità: strumenti utili allo sviluppo, sistemi modulari, facilità di iterazione, capacità di creare grandi ambienti e battaglie. L'ottimizzazione non punta al massimo dettaglio ma al miglior rapporto tra scala, qualità visiva e prestazioni, concentrando le risorse su combattimento, IA, ambienti e sistemi di gioco.

#### **21.4 Nota di processo: due ruoli dell'IA nello sviluppo**
Durante lo sviluppo si distinguono due ruoli. L'**IA di progettazione** analizza il game design, individua problemi, propone miglioramenti, effettua ricerche e aggiorna la documentazione. L'**IA di sviluppo** implementa i sistemi definiti dalla documentazione, scrive codice, corregge bug e mantiene la coerenza con l'architettura. Obiettivo: ridurre al minimo le decisioni progettuali prese durante la scrittura del codice — questo documento serve proprio a fornire quella base decisionale.

### **22. Mappa dei Sistemi e Interconnessioni**
Questo capitolo è il ponte tra design e implementazione. Riassume i sistemi come **moduli** con confini definiti, ne indica le dipendenze e individua i due grandi contenitori di stato. È il punto di partenza consigliato per l'architettura del codice.

#### **22.1 I moduli e le loro responsabilità**

| **Modulo** | **Responsabilità** | **Cap.** |
| --- | --- | --- |
| Combat | Danno, armi, scudo/salute, stato downed, feel | 6 |
| Squad & Command | Ordini contestuali + ruota, formazioni, comando scalabile | 7 |
| AI | IA individuale, di squadra, director operativo/strategico, percezione | 8, 16 |
| Mission | Definizione obiettivi, eventi dinamici, valutazione/debrief | 9 |
| Level/Map | Navmesh, punti tattici, settori catturabili, interattivi | 10 |
| Progression | Grado, classe, specializzazione, XP, promozioni | 11, 12 |
| Equipment | Loadout, armi/mod/gadget, cosmetici, sblocchi | 13 |
| Vehicles | Postazioni, danno a componenti, funzioni speciali | 14 |
| Factions | Identità Repubblica/CSI, roster unità | 15 |
| Strategic War | Grafo galattico, controllo, operazioni, IA strategica | 16 |
| Persistence | Serializzazione profilo + guerra + squadra, versioning | 17 |
| UI/HUD | HUD diegetico, comando, mappa tattica, menu | 18 |
| Audio | Effetti armi, ambiente, comunicazioni, musica | 19 |
| Animation | Locomozione modulare, armi, differenze di ruolo | 20 |

#### **22.2 Grafo delle dipendenze principali**
Lettura: "A → B" significa "A dipende da / consuma B". Le dipendenze sono volutamente unidirezionali dove possibile, per ridurre l'accoppiamento.
- **Combat → Equipment, Classes, Map** (statistiche del loadout, ruolo, coperture).
- **AI → Combat, Map, Mission, Factions** (esegue combattimento, legge geometria e obiettivi, applica identità di fazione).
- **Squad & Command → Progression, Classes, AI** (livello di comando dal grado, ruoli, pilota l'IA alleata).
- **Mission → Map, AI(director)** (zone obiettivo, orchestrazione eventi).
- **Progression → Mission, Combat, Squad** (riceve segnali di XP/valutazione).
- **Equipment → Progression, Classes** (sblocchi e affinità).
- **Strategic War → Mission, Map, AI(strategica), Progression** (genera operazioni, muove le forze, fa crescere la carriera).
- **Persistence ← Progression, Equipment, Squad, Strategic War** (serializza gli stati; nessun sistema dipende da Persistence a runtime).
- **UI/HUD ← quasi tutti** (visualizzazione e input di comando); non contiene logica di gioco.
- **Audio/Animation ← Combat, Squad, Vehicles** (reagiscono agli eventi; non producono stato di gioco).

#### **22.3 I due grandi stati e il flusso a eventi**
Tutto lo stato persistente converge in due contenitori: il **Profilo del Clone** (grado, classe, specializzazioni, equipaggiamento, statistiche, squadra) e lo **Stato della Guerra** (grafo galattico, controllo, operazioni). Il Gameplay Loop (cap. 5) legge questi stati all'ingresso di una missione e vi scrive all'uscita. La comunicazione tra moduli a runtime è consigliata **a eventi** (es. "obiettivo completato", "unità a terra", "HVT eliminato", "settore perso"): riduce l'accoppiamento e permette a più sistemi (Progression, Audio, UI, director) di reagire allo stesso evento in modo indipendente.

#### **22.4 Ordine di costruzione suggerito**
Coerentemente con "prima le fondamenta", i moduli vanno costruiti in strati: prima il **verticale giocabile minimo** (Combat + Map + una classe + Animation/Audio essenziali + UI di combattimento), poi il **livello tattico** (Squad & Command + AI alleata/nemica + Mission), poi la **carriera** (Progression + Equipment + Persistence), infine il **meta-gioco e la scala** (Vehicles, Strategic War, IA operativa/strategica, grandi battaglie). Ogni strato deve essere solido prima del successivo.

### **23. Roadmap Concettuale e Criteri di Successo**
Questa non è una roadmap tecnica dettagliata ma l'ordine di priorità che costruisce il progetto senza gonfiarne la complessità. Ogni funzionalità va valutata prima di essere aggiunta: migliora l'esperienza? Rafforza l'identità? È coerente con la fantasia del clone? Giustifica il tempo? È implementabile in modo modulare?

#### **23.1 Priorità del progetto**

| **Livello** | **Sistemi** |
| --- | --- |
| Core (identità del gioco) | FPS/TPS tattico, IA di cloni e droidi, sistema di squadra, progressione militare, sistema di missioni, equipaggiamento base |
| Importante | Veicoli, guerra dinamica, personalizzazione, grandi battaglie, gestione avanzata delle unità |
| Espansione | Nuove modalità, nuove specializzazioni, nuove campagne, nuovi pianeti, eventuali nuove fazioni oltre la Repubblica |
| Secondario | Dettagli estetici avanzati, sistemi molto complessi, funzionalità non essenziali |

#### **23.2 Criteri di successo**
Il progetto è coerente quando: ogni sistema ha uno scopo preciso; le funzionalità sono integrate tra loro; la complessità resta controllata; il gameplay resta al centro; e il giocatore percepisce realmente di essere un clone della Grande Armata della Repubblica. La visione a lungo termine è un insieme di sistemi coerenti che lavorano insieme per un'unica esperienza — vivere la carriera di un clone durante la Guerra dei Cloni — con ogni sistema pensato per ricevere nuovi contenuti, essere esteso senza riscritture e mantenere la compatibilità nel tempo.
*Domanda finale che chiude il documento: "Questa scelta rende Galactic Front un'esperienza più credibile, più coerente e più divertente come simulazione della vita di un clone della Repubblica?"*

## **Appendici**
Le appendici traducono l'universo delle Guerre dei Cloni in elementi di gameplay meccanicamente rilevanti. Ogni arma, nemico o veicolo elencato non è solo un elemento narrativo, ma uno strumento di game design che supporta il sistema di classi, la personalizzazione e il loop tattico definiti nel documento.

### **Appendice A — Arsenale della Repubblica e Sistema d'Armatura**

#### **A.1 Armatura modulare (Attachments)**
L'armatura clone non è divisa in tier bloccanti. Il giocatore indossa l'Armatura Clone Standard (ispirata alla versatilità della Fase II) come "piattaforma" modulare; la progressione e la classe permettono di agganciare moduli tattici che alterano capacità visive e meccaniche.

| **Modulo** | **Effetto di gameplay** | **Classe tipica** |
| --- | --- | --- |
| Macrobinocolo | Visore abbassabile: zoom variabile e "spotting" dei bersagli a lungo raggio per la squadra | Recon / Sniper |
| Zaino Radio (Comlink) | Riduce il cooldown per chiamare bombardamenti orbitali, rinforzi o scansioni della minimappa | Leader / Officer |
| Bacta Pack / Dispensatore | Posiziona stazioni di cura o cura istantaneamente gli alleati vicini | Medic |
| Kama e Pauldron | Elementi di grado: leggera riduzione danni da esplosivi/schegge (flak); distingue i veterani/ARC | Veterano / ARC |
| Visore Anti-Abbagliamento | Riduce o annulla l'accecamento da granate flash e fuoco soppressivo | Tutte |

#### **A.2 Livree e identità visiva**
I colori esprimono stile e appartenenza (non il grado): 501ª Legione (blu — fanteria d'assalto), 212° Battaglione d'Attacco (arancione — supporto pesante e assedio), 41° Corpo d'Elite (verde/camo — ricognizione e infiltrazione), 104° "Wolfpack" (grigio — recupero e supporto logistico).

#### **A.3 Armi primarie (BlasTech)**

| **Arma** | **Ruolo** | **Caratteristiche e trade-off** |
| --- | --- | --- |
| Fucile DC-15A (lungo) | Ingaggio a media/lunga distanza | Altissima precisione e letalità; lento da manovrare negli spazi stretti |
| Carabina DC-15S (corta) | Truppe d'assalto, CQB | Alta cadenza e maneggevolezza; minor precisione a lungo raggio; ideale per catturare i punti |
| DC-15x (precisione) | Tiratore scelto | Elimina minacce ad alto valore in un colpo; basso rateo, surriscaldamento elevato |
| Z-6 (blaster rotante) | Soppressione d'area (Heavy) | Volume di fuoco devastante; rallenta il movimento e ha "spin-up" prima di sparare |
| PLX-1 (lanciamissili) | Anti-corazza / anti-aerea | Unica arma capace di neutralizzare rapidamente veicoli (AAT); munizioni limitatissime |

#### **A.4 Armi secondarie, gadget e granate**
- **Pistola DC-17** — arma da fianco ad estrazione rapida; dual-wield (akimbo) per gli ARC Trooper.
- **Detonatore Termico V-1** — granata a frammentazione standard: raggio contenuto, letale sulla fanteria ammassata.
- **Granata EMP** — danno quasi nullo agli esseri viventi, ma paralizza i Super Droidi e fa collassare lo scudo dei Droideka (strumento tattico chiave).
- **Auto-Turret pieghevole** — piccola torretta anti-fanteria schierabile: essenziale per l'Engineer per difendere strettoie o presidiare un obiettivo appena conquistato.

### **Appendice B — Bestiario Tattico: l'Esercito Separatista (CSI)**
L'IA nemica (Baktoid Automata) è diversificata per costringere il giocatore e la squadra a cambiare continuamente strategia. Ogni unità impone una risposta tattica diversa.

| **Unità** | **Comportamento** | **Ruolo tattico / risposta richiesta** |
| --- | --- | --- |
| B1 (fanteria base) | Lenti, in formazione ravvicinata, scarsa precisione | Carne da cannone: pericolosi solo in grande numero o se ignorati |
| B2 Super Droide (fanteria pesante) | Avanza frontalmente senza copertura, armi da polso ad alta cadenza | "Spugna" tattica: assorbe fuoco leggero; richiede fuoco concentrato o l'Heavy |
| BX Commando (élite) | Agile, salta/rotola, usa coperture, fiancheggia, corpo a corpo | Anti-camper: impedisce di restare statici; obbliga a muoversi e reagire |
| Droideka (distruttore) | Si dispiega in modalità ruota, scudo deflettore, doppi cannoni ad alta cadenza | Area denial: non affrontabile frontalmente col DC-15; usare EMP, aggiramenti o sopressione |
| Droide Tattico serie T (comandante IA) | Evita il combattimento; migliora l'IA di tutti i droidi vicini | HVT: richiede uno Sniper o un assalto chirurgico; eliminarlo rompe l'organizzazione nemica |

### **Appendice C — Veicoli e Impianti sul Campo**
- **AT-RT** — camminatore bipede leggero, veloce, monoposto: ricognizione rapida, attraversamento di mappe ampie, supporto di fuoco leggero; lascia il pilota esposto.
- **AT-TE** — il "carro" a sei zampe della Repubblica, pesantemente corazzato con cannone di massa. **Meccanica chiave: Centro di Comando Mobile** — se presente nella zona abilita respawn e rifornimento pesante della squadra, diventando un obiettivo cruciale da difendere.
- **AAT (CSI)** — veicolo d'assalto separatista con cannoni pesanti frontali; presidia spesso i punti importanti. Richiede attacchi combinati, armi anticarro (PLX-1) o aggiramenti per colpire il motore posteriore, suo punto debole critico.

### **Appendice D — Struttura Militare Canonica (GAR)**
Struttura di riferimento della Grande Armata della Repubblica, usata per allineare gradi (cap. 11) e comando (cap. 7). Il fireteam (~4) è un'astrazione di gameplay sotto la squadra; il resto segue il canone.

| **Unità** | **Composizione** | **Effettivi** | **Comando** |
| --- | --- | --- | --- |
| Squadra | Base (2 fireteam) | 9 | Sergente + Caporale |
| Plotone | 4 squadre | 36 | Tenente |
| Compagnia | 4 plotoni | 144 | Capitano |
| Battaglione | 4 compagnie | 576 | Maggiore / Battalion Commander (+ Jedi Commander) |
| Reggimento | 4 battaglioni | 2.304 | Regimental Commander |
| Legione / Brigata | 4 reggimenti | 9.216 | Senior Commander + Jedi General |
| Corpo | 4 legioni | 36.864 | Marshal Commander + Jedi General |
| Armata di Settore | 4 corpi | 147.456 | Senior Jedi General |
| Armata di Sistema | 2 armate di settore | 294.912 | High Jedi General |
| Grande Armata | 10 armate di sistema | ~3.000.000 | Comando supremo |

### **Appendice E — Glossario dei Sistemi e Termini**
- **Gameplay Loop** — i tre cicli annidati (missione, carriera, guerra) che condividono lo stato persistente (cap. 5).
- **Economia tattica (Punti Comando / Requisizione)** — risorsa in-missione, guadagnata soprattutto completando obiettivi, spendibile per rinforzi, veicoli e supporto (cap. 5.4, 7).
- **Ordine contestuale** — ordine emesso con un tasto puntando un elemento del mondo (copertura, porta, console, nemico, alleato a terra) (cap. 7.2).
- **Director (IA operativa/strategica)** — livello di IA che gestisce ondate, rinforzi, eventi e pacing a livello di battaglia, e movimenti delle forze a livello galattico (cap. 8.1, 16.4).
- **HVT (High Value Target)** — bersaglio ad alto valore (es. Droide Tattico) la cui eliminazione degrada l'IA nemica dell'area (cap. 8.6, App. B).
- **Progressione diegetica** — si diventa la classe/specializzazione che si gioca, tramite azioni coerenti (cap. 11.3).
- **LOD comportamentale** — livello di dettaglio dell'IA variabile con la distanza dal giocatore, per reggere le grandi battaglie (cap. 8.7).
- **Profilo del Clone / Stato della Guerra** — i due grandi contenitori di stato persistente del gioco (cap. 22.3).
- **Stato downed / rianimazione** — messa fuori combattimento non letale con possibilità di rianimazione da parte della squadra (cap. 6.2).
**Fine del documento.**

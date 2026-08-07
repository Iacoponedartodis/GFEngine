# 49 — Analisi di stabilità del GFEditor (2026-08-05)

> Nasce da una segnalazione dell'utente: *"ogni tanto cambiare modulo funziona un po' male … la
> funzione serie non funziona più … penso che abbiamo toccato qualcosa che ha rotto la stabilità
> dell'editor"*. Aveva ragione su due difetti su tre, e la terza intuizione — che il problema fosse
> strutturale e non un singolo bug — è quella giusta.

## 0. Riepilogo in una pagina

**Tre difetti trovati e riparati oggi.** Due erano miei di ieri/oggi, uno era lì da settimane.

**Ma il difetto sotto i difetti è uno solo**: nell'editor **l'identità di un elemento è la sua
posizione in un array**, codificata in un `int`. Da lì discendono, come conseguenze e non come
coincidenze, sia il bug di "serie", sia un **tetto silenzioso al numero di metadati per mappa** che
la prossima mappa 300 × 200 rischia concretamente di superare.

Priorità consigliata: **R1 (tetti dei codici) prima di costruire la mappa grande**, il resto dopo.

---

## 1. I tre difetti trovati (chiusi)

### 1.1 "Serie" spostava l'elemento sbagliato — GRAVE, preesistente (commit `bc9a281`)
`duplicateBox` inserisce la copia **accanto** all'originale (`insert(begin()+idx+1)`), ma `makeArray`
deduceva il codice della copia con *"sarà l'ultima del vettore"*. La deduzione puntava a **un'altra
box**, che veniva spostata e ruotata al posto della copia.

Non era una funzione inerte: **danneggiava un elemento sano**, in silenzio, e l'undo lo copriva solo
se te ne accorgevi subito. Lo stesso errore rendeva sbagliato anche `duplicateSelected` su selezione
multipla — con un commento nel codice che affermava il contrario (*"le copie si accodano"*).

**Riparato**: `duplicateOne` ora **restituisce** il codice della copia; chi duplica non lo deduce più.
E i codici raccolti prima del ciclo vengono aggiornati dopo ogni inserimento, perché inserire in
mezzo a `m_boxes` fa scalare tutti gli indici superiori.

### 1.2 Modale invisibile che blocca i clic — mio, di oggi
`ImGui::OpenPopup("##structdiscard")` era chiamata **dentro** `BeginTabBar`, che spinge un proprio
livello di ID (`PushOverrideID`, `imgui_widgets.cpp:9849`); `BeginPopupModal` era chiamata **fuori**.
Due identificatori diversi → finestra registrata come aperta e **mai disegnata**.

È un tranello noto di Dear ImGui (issue #331, *"Can not open modal popup from menu / Issue with ID
stack"*): stesso nome, contesti di ID diversi. Nella stessa riparazione è emerso un **secondo**
errore: la modale agiva su `m_activeTab`, cioè sul tab **attivo**, non su quello che stavi chiudendo —
la ✕ si può premere su un tab in secondo piano, e avrebbe scartato il lavoro sbagliato.

### 1.3 Viewport della mappa che poteva smettere di avanzare — mio, di oggi
`m_activeTab` lo scrivono i `BeginTabItem`, che in certi frame **non vengono eseguiti**. Se restava
puntato a un tab struttura non più esistente, la viewport della mappa non avanzava e **non rilasciava
il mouse**: è il sintomo *"cambiando modulo aveva smesso di funzionare e sono dovuto tornare alla
home"*. Ora la verità è quante schede esistono davvero, e il rilascio del mouse copre **entrambe** le
viewport.

---

## 2. La causa comune: **l'identità è posizionale**

Un elemento dell'editor si identifica con un `int code` il cui **significato dipende dall'intervallo**
e il cui **valore è l'indice nell'array**:

| intervallo | contenitore | tetto implicito |
|---|---|---|
| `>= 0` | `m_boxes` | — |
| `-10 … -100` | command post | **90** |
| `-200 … -300` | danger zone | **100** |
| `-300 … -400` | patrol route | **100** |
| `-400 … -500` | spawn veicoli | **100** |
| `-500 … -1000` | bersagli strategici | **500** |
| `-1000 … -2000` | **posizioni tattiche** | **1000** |
| `-2000 … -3000` | settori | **1000** |
| `-3000 / -3100` | punti multi-spawn T1 / T2 | **100** ciascuno |
| `-4000 … -5000` | istanze prefab | **1000** |
| `-6000 …` | strutture | — |

Ne derivano tre classi di fragilità, tutte già manifestate:

1. **Gli indici scalano.** Inserire o cancellare invalida i codici raccolti altrove. `deleteSelection`
   lo sapeva (cancella in ordine decrescente); `makeArray` e `duplicateSelected` no. Chiunque scriva
   la prossima operazione dovrà ricordarsene da capo.
2. **Le operazioni comunicano per deduzione.** Un'operazione che crea qualcosa non diceva *cosa* ha
   creato. Ora `duplicateOne` lo dice; **le altre no** (`addBox`, `addStructure`, incolla, prefab).
3. **I tetti sono silenziosi.** Superarli non dà errore: il codice **cambia significato** e si
   seleziona o si muove un elemento di un altro tipo.

### 2.1 ⚠ Il tetto che ti riguarda adesso
Training Ground ha **169 posizioni tattiche** su una mappa di ~71 × 92 m. La prossima è **300 × 200**,
circa **nove volte** l'area, e l'obiettivo dichiarato è *"moltissimi metadata per mappa"*. A parità di
densità sono ~1.500 posizioni: **oltre il tetto di 1.000**, dove i codici delle posizioni sconfinano
in quelli dei settori.

Anche i **percorsi di pattuglia** meritano un'occhiata: 22 su Training Ground, tetto 100.

Non c'è oggi nessun controllo che lo segnali.

---

## 3. Cosa manca strutturalmente (con i riferimenti)

### 3.1 Undo a fotografie invece che a comandi
Oggi l'undo salva **snapshot interi** dello stato. Funziona, ed è semplice — ma è il motivo per cui un
difetto come 1.1 resta invisibile: nessuna operazione **dichiara** cosa tocca, quindi non c'è nulla da
verificare. Il pattern di riferimento (Unity, Godot, la letteratura sul Command pattern) incapsula
ogni operazione con il suo inverso, e raggruppa le operazioni correlate in **transazioni** così che
uno stato parziale non sia rappresentabile.

Non propongo di riscrivere l'undo adesso: costa molto e quello attuale non ha mai perso dati. Lo
segnalo come **la direzione** quando le operazioni cresceranno ancora.

### 3.2 Nessun collaudo delle operazioni — **parzialmente chiuso oggi**
Unity ha gli *Edit Mode tests* (girano nell'editor, con accesso al codice dell'editor); Unreal ha
l'*Automation Framework* con test specifici per l'editor. Qui non c'era nulla: **ogni operazione
dell'editor era verificabile solo a mano, col mouse, dall'utente.**

Aggiunto `GFEditor.exe --editor-selftest`: esercita duplica / serie / annulla su uno stato sintetico
e verifica gli invarianti, senza finestra e senza frame. Sette controlli, fra cui *"serie non sposta
elementi fuori dalla selezione"* — cioè esattamente il difetto 1.1.

Il criterio per aggiungerne uno è deliberatamente stretto: **"questo si è già rotto una volta"**. Non
è un framework, e non deve diventarlo: deve restare abbastanza piccolo da girare a ogni build.

### 3.3 Regole ImGui non scritte
Il difetto 1.2 è la terza volta che un problema nasce dal modello a ID di ImGui (prima: liste con
etichette duplicate, poi il grip di ridimensionamento sul bordo sbagliato). Serve una regola scritta,
perché è controintuitivo e si ripresenta: **apri e disegna un popup nello STESSO livello di ID**, e
non fidarti del fatto che il nome coincida.

---

## 4. Interventi proposti, in ordine di rischio

| # | Intervento | Perché ora |
|---|---|---|
| **R1** | **Guardia sui tetti dei codici**: un controllo nel gate (`--validate`) e un avviso nell'editor quando un contenitore si avvicina al suo tetto. | È l'unico che può **corrompere dati** in silenzio, ed è quello che la mappa 300 × 200 rischia di incontrare per primo. Costa poco. |
| **R2** | **Ogni operazione che crea restituisce l'identità** di ciò che ha creato (`addBox`, `addStructure`, incolla, prefab), come già fa `duplicateOne`. | Chiude la classe di difetti 1.1 alla radice invece che caso per caso. |
| **R3** | **Ampliare il self-test** man mano: elimina-selezione, incolla, il salvataggio RMW, il ciclo salva→ricarica→confronta. | Il salva/ricarica è l'invariante più prezioso e oggi non è collaudato da nulla. |
| **R4** | **Regola ImGui scritta** in CLAUDE.md o in un documento di convenzioni dell'editor. | Difetto ricorrente, costo zero. |
| **R5** | Identità **stabile** (handle/GUID) al posto dell'indice, con una mappa handle→indice. | È la riparazione vera di §2, ma è invasiva: tocca selezione, undo, gizmo, viewport, salvataggio. **Non ora** — dopo la mappa grande, e solo se R1/R2 non bastano. |

## 5. Cosa NON è emerso

Per onestà: non ho trovato prove che il cambio modulo abbia un difetto **oltre** 1.2 e 1.3. Un
percorso automatico di **96 transizioni** fra sei moduli, con un tab struttura aperto, gira pulito.
Se ti ricapita, serve sapere **da quale modulo a quale**: la modale invisibile era per definizione
invisibile, e un secondo caso del genere si riconosce solo così.

Resta aperto **KI #98** (crash entrando in Entity Editor), che è indipendente da tutto questo.

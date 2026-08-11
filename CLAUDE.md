# Galactic Front — Claude Code Operating Rules

Questo file è la guida operativa permanente per Claude Code su questo repository. Non
duplica ProjectDocs: lo referenzia. Se questo file e ProjectDocs sembrano in conflitto,
ProjectDocs vince — segnala il conflitto e proponi la correzione a questo file.

## 1. Prima di ogni task non trivial

Segui esattamente 09_AI_Workflow.md (`ProjectDocs/09_AI_Workflow-10.md`). In sintesi:

1. Leggi prima 05_CurrentState, 06_Todo, 08_KnownIssues, 10_ProjectMemory, 13_ADR. Per task
   architetturali leggi anche 00/01/03/11/12.
2. Verifica sempre contro il codice live. Se il codice contraddice la documentazione, il
   codice vince — e la documentazione va corretta nello stesso change set.
3. Nomina esplicitamente i sottosistemi impattati.
4. Traccia l'impatto globale di ogni cambio di id/schema/naming: loader in
   `DefinitionRegistry`, ogni UI editor che legge/scrive quel campo, ogni game mode che lo
   legge, ogni cross-reference in altri JSON.
5. Cambio minimo e sicuro. Se stai migrando via da un pattern hardcoded, mantieni un fallback
   documentato durante la transizione.
6. Implementa in place, poi builda (`cmake --build build/windows-debug --config Debug`) con
   `GFEditor.exe`/`GFEngine.exe` chiusi prima (altrimenti LNK1168 per file lock).
7. Aggiorna ProjectDocs (05/06/07/08 sempre se applicabile; 13_ADR per decisioni
   architetturali; 10 per nuovi vincoli confermati).
8. Dichiara sempre cosa è build-verified e cosa richiede uno smoke test manuale — non
   affermare "funziona" per ciò che non hai eseguito.

## 2. Regole non negoziabili (violarle è un errore di implementazione, non una scelta di stile)

- **id = filename stem** per ogni tipo di definizione (ADR-001). Non scrivere mai `id`/
  `profile_id` come autoritativo dentro il JSON.
- **Due binari, contratto solo-file** (ADR-002). GFEngine non deve mai linkare codice
  dell'editor. Nessuna dipendenza circolare `src/` ↔ `editor/`.
- **Non toccare il rendering client-side-array / OpenGL 3.3 Compatibility** senza una
  giustificazione concreta legata al driver Intel (ADR-003). Non è debito tecnico, è un
  workaround intenzionale.
- **READ-MODIFY-WRITE obbligatorio per ogni save JSON.** Leggi il file esistente, modifica
  solo i tuoi campi, scrivi. Costruire un `json` nuovo da zero e sovrascrivere ha già causato
  un incidente reale di perdita dati (2026-07-08, vedi 10_ProjectMemory). Se esiste già
  l'helper `saveJsonRMW` (ADR-010), usalo sempre; se non esiste ancora, implementa RMW a mano
  e segnalalo esplicitamente nel changelog.
- **Dropdown dal registry, mai id in testo libero.** Qualsiasi campo UI che assegna una
  definizione a un'altra (arma, ai_profile, hitbox_profile, nemico, mappa, classe, ability)
  deve essere un combo popolato da `DefinitionRegistry`. Un `ImGui::InputText` su un campo id
  è un errore da correggere, non uno stile alternativo.
- **Rinominare una definizione = usare il comando di rename dell'editor** (ADR-010), non
  creare un nuovo file con nuovo id abbandonando il vecchio. Se il comando di rename non
  esiste ancora nel modulo su cui stai lavorando, non procedere con una rinomina manuale:
  implementa prima il comando (è P0, 06_Todo #1) o chiedi conferma esplicita prima di un
  workaround manuale, documentandolo in 08_KnownIssues.
- **Nessuna nuova costante di gameplay hardcoded nei game mode.** Va in `data/` o in
  `core/GameConfig.hpp` se è veramente globale.
- **Nessun nuovo id/archetipo hardcoded nei game mode.** Risolvi sempre via registry + MapDef.

## 3. Gerarchia di autorità in caso di conflitto tra documenti

1. Architecture Decision Records (`13_ADR.md`) — decisioni tecniche ufficiali.
2. Documenti di architettura (`00_Vision`, `01_Architecture`, `03_SystemReference`).
3. Roadmap (`06_Todo`).
4. Documentazione di feature (`14_ClassSystem`, `15_MapMetadata`, ecc.).
5. Note ed esplorazioni non ancora promosse a decisione.

Non promuovere mai automaticamente un'idea esplorativa a requisito. Se un documento di
livello inferiore contraddice un ADR "in force", il documento di livello inferiore è
sbagliato e va corretto — non l'ADR.

## 4. Versioning delle informazioni — non confondere questi quattro stati

- **Confirmed Decision** — ADR con Status: Accepted / in force.
- **Current Implementation** — descritto in `05_CurrentState`, verificato contro il codice
  live.
- **Planned Feature** — documento come `14_ClassSystem.md`/`15_MapMetadata.md`: schema e
  scope definiti, zero righe di codice esistenti. Implementalo solo quando esplicitamente
  richiesto, seguendo lo scope documentato — non anticipare funzionalità elencate come
  "Out of Scope" in quel documento.
- **Exploration** — ADR con Status: Proposed (es. ADR-011 sullo split-screen). Non è ancora
  una decisione: se implementi qualcosa in quest'area, il primo passo è eseguire la verifica
  che l'ADR richiede, non costruire la soluzione finale.

## 5. Quando introduci un nuovo sistema

Prima di scrivere codice per un sistema nuovo (engine, editor, gameplay, AI, pipeline):
1. Verifica se esiste già un documento Planned Feature in ProjectDocs per quel sistema. Se
   sì, implementa quello scope esatto — non reinventare lo schema.
2. Se non esiste, non procedere silenziosamente: proponi prima uno scope minimo (Overview /
   Goal / Problem Solved / Scope / Out of Scope / Dependencies) coerente con il template già
   in uso in 14_ClassSystem.md e 15_MapMetadata.md, poi implementa.
3. Ogni sistema ha una responsabilità chiara. Se stai per far fare a un sistema esistente
   qualcosa che non è la sua responsabilità dichiarata (es. far decidere comportamento AI a
   `MapDef.dangerZones` invece che a un futuro sistema AI dedicato), fermati: è quasi
   certamente un segnale che serve un nuovo sistema o un ADR, non un'estensione ad hoc.

## 5-bis. Ogni sistema nasce con la sua OSSERVABILITÀ (non solo con l'authoring)

Finora la regola era: chi costruisce un sistema costruisce anche gli strumenti per **autorarlo**.
Manca la metà più importante per il lavoro dell'AI: gli strumenti per **osservare cosa sta
realmente facendo**. Un sistema che non si può guardare in funzione non si può nemmeno
diagnosticare, e ogni indagine ricomincia da ipotesi.

**Un sistema non è finito finché non si può rispondere a "cosa sta facendo, adesso, questa
singola entità, e perché".** Non serve solo all'utente: serve soprattutto a me. Senza, io
ragiono su aggregati — e gli aggregati hanno già fuorviato tre diagnosi consecutive su KI #86
(gli eventi di combattimento fra run divergenti, la classificazione dei bloccanti con soglia
fissa, il "57% di geometria muta"). Ogni volta la risposta è arrivata solo quando ho potuto
guardare **una** unità.

Per ogni sistema nuovo, e prima di dichiararlo completo, servono tutti e tre:

1. **Un contatore del SINTOMO, non dell'esito.** Misura direttamente il comportamento che
   potrebbe rompersi (`evasivo_durata_max_s`, `stalli per causa`), non una conseguenza
   lontana come "quanti colpi a segno". L'esito varia per divergenza fra run e non è
   attribuibile a un cambio.
2. **Un funnel con i suoi denominatori.** Dove muore il processo, e su quale base
   (`occ_in_raggio → occ_nel_cono → occ_acquisito`, `gate_*`). Un numeratore senza
   denominatore è un aneddoto.
3. **Una via per scendere alla SINGOLA entità.** Un rilevatore che dice *quale* entità
   guardare + una traccia che dice *cosa ha fatto tick per tick* (`--trace-ai <id>`,
   `AiTrace.cpp`). Questo è il pezzo che manca quasi sempre ed è quello che chiude le indagini.

Regole di igiene per questi strumenti:
- **Non decidono nulla.** Nessun ramo di comportamento legge i dati di osservazione: se
  succede, non è più un osservatore ma un sistema, e vale la §5.
- **Lo stato di osservazione vive sul COMPONENTE**, non nel sistema — dentro un sistema
  sopravvive a `initialize()` e va azzerato a mano (trappola già costata una regressione).
- **Le guardie restano, le sonde no.** Un contatore permanente costa un incremento; una sonda
  che costa una LOS o un'allocazione si toglie appena ha risposto, e si annota nel codice
  quale risposta ha dato (così non la si rifà).
- **Il gate `--validate` è osservabilità di authoring.** Se un dato può essere sbagliato in
  silenzio, il gate deve dirlo — con l'azione concreta per correggerlo, non solo la diagnosi.

## 6. Testing e verifica

Non esistono test automatici che bloccano le modifiche (12_TestingStrategy). Ogni modifica
significativa richiede almeno:
- Build pulita (`cmake --build build/windows-debug --config Debug`, 0 errori).
- Lo smoke test manuale rilevante da 12_TestingStrategy.md (liste editor senza duplicati,
  cross-reference dei dropdown risolte, partita/sandbox avviata senza crash, viewport
  corretto).
Dichiara sempre esplicitamente quali di questi hai eseguito e quali restano da verificare
manualmente dallo sviluppatore.

## 6-bis. Una funzione che l'utente non trova NON ESISTE

Gli assemblaggi (ADR-056) erano implementati, collaudati con 5 controlli e documentati in
ProjectDocs. L'utente non è riuscito a usarli: *"non ho trovato il modo per fare un
assemblaggio… nell'editor strutture non è cambiato nulla"*. La capacità era dietro
un'intestazione **chiusa** che diceva "Parti (0)".

È la stessa lezione di ADR-023 (un dropdown incompleto rende la capacità inesistente), e vale
come criterio di completamento:

- **Il percorso per invocare una funzione nuova va dichiarato** insieme alla funzione: da quale
  menu, con quale etichetta, e cosa si vede quando la funzione non è ancora stata usata (lo
  stato vuoto è ciò che l'utente incontra per primo).
- **Niente capacità dietro sezioni chiuse per difetto.** Se una cosa è nuova, si vede.
- **Ogni cambiamento che aggiunge o modifica un comando aggiorna `data/help/*.md`**, nello
  stesso change set. Sono due documentazioni diverse e non si sostituiscono:
  **ProjectDocs spiega PERCHÉ** (è per me, per non ripetere gli errori);
  **`data/help/` spiega COME** (è per chi costruisce, dentro l'editor, con F1).
- Nel dichiarare cosa resta da verificare a mano, dire **dove si clicca**, non solo cosa
  provare.

## 6-ter. Non posso vedere lo schermo: le regole visive vanno rese STRUTTURALI

"Mai far tagliare i comandi" era una regola dal 2026-07. **È stata violata tre volte**, sempre
allo stesso modo: si aggiunge un comando utile, la barra supera la larghezza del pannello, e
l'ultimo comando smette di esistere per chi lo usa. L'ultima volta è toccato a "Prova da qui",
consegnato e dichiarato fatto — l'utente non l'ha trovato.

**La causa non è la distrazione: è che io non vedo lo schermo.** Una regola la cui verifica
richiede di guardare il risultato non posso rispettarla, per quante volte la si riscriva. La
conseguenza generale, valida oltre le barre:

> **Una regola di layout che non ha un controllo eseguibile non è una regola: è una speranza.**
> Ogni volta che una regola dell'interfaccia dipende da "quanto spazio c'è", vanno prodotti
> ENTRAMBI: (a) un meccanismo che rende la violazione **inesprimibile**, e (b) un **controllo
> headless** che io possa eseguire.

Come si applica, in concreto:

- **Le barre di comandi si DICHIARANO, non si disegnano a mano.** Elenco di voci →
  `editor::toolbar::draw` (`editor/include/framework/Toolbar.hpp`), che misura e manda
  l'eccedenza in un menu «...». L'ordine dell'elenco è la priorità: la prima voce è l'ultima a
  finire nel menu. Una fila di `ImGui::Button` + `SameLine()` in un pannello di larghezza
  variabile è un difetto da correggere, non uno stile alternativo.
- **La decisione di layout sta in una funzione PURA** (`toolbar::fitCount`), separata dal
  disegno, così si collauda in `--editor-selftest` senza finestre. È l'unico modo in cui io possa
  verificarla.
- **Raggruppare prima di aggiungere.** Prima di mettere un pulsante nuovo in una barra, chiedersi
  in quale menu esistente sta (`Mappa`, `Crea`, `Modifica`, `Vista`). Riferimento professionale
  (PatternFly, priority+): **al massimo 1-2 azioni visibili per gruppo, il resto in un menu**.
  Restano sempre visibili solo il contesto (quale mappa, se ci sono modifiche non salvate) e ciò
  che si usa di continuo (il passo di aggancio).
- **Ogni comando frequente ha una scorciatoia**, e la scorciatoia fa la cosa giusta per il
  contesto attivo (Ctrl+S salva la mappa, il tipo o l'istanza a seconda del tab).
- **Quando consegno una funzione dichiaro DOVE si clicca** (§6-bis) — e adesso anche **in quale
  menu**, perché "è nella barra" ha già smesso di essere un indirizzo sufficiente.

Lo stesso ragionamento vale per ogni altra proprietà che si vede e non si misura: testi troncati,
finestre più piccole del loro contenuto, elementi sovrapposti. Se non c'è un modo di chiederlo al
programma, il difetto tornerà.

**Trappole di layout già pagate — non ripeterle:**

- **Mai `TextWrapped` dopo `SameLine`.** Un `Selectable`/`Button` con larghezza 0 occupa TUTTA la
  riga: `SameLine` porta il cursore al bordo destro e al testo resta una larghezza di wrap ≈ 0 →
  una lettera per riga → migliaia di righe per voce → **l'editor lampeggia e si blocca**
  (2026-08-11, l'utente ha dovuto chiuderlo da Gestione attività). Se serve testo lungo in una
  riga cliccabile, va DENTRO l'etichetta del `Selectable` (ImGui la ritaglia, non la manda a capo)
  e per esteso nel suggerimento, dove il wrap si dichiara con `PushTextWrapPos(<larghezza>)`.
- **`PushTextWrapPos` sempre con una larghezza esplicita**, mai con il valore di riporto dentro un
  contenitore la cui larghezza dipende dal contenuto: è la stessa spirale.
- **La misura di un layout si prende IMMEDIATAMENTE dopo ciò che si misura.**
  `ImGui::GetItemRectSize()` ritorna l'ultimo elemento **di chiunque**: basta disegnare qualcosa
  in mezzo (anche un'altra finestra) e si misura quello. Con una finestra a schermo intero fra la
  barra e la sua misura, l'altezza calcolata esplode, lo spazio residuo diventa negativo e il
  pannello oscilla di frame in frame — **secondo blocco dell'editor in un giorno**, 2026-08-11.
- **Le finestre a sé (`ImGui::Begin` top-level) si disegnano per ULTIME**, dopo il layout di tutto
  il resto. Mai in mezzo.
- Un difetto di layout che **blocca** l'editor è un difetto di perdita dati, non di estetica:
  l'utente chiude il processo e perde ciò che non è nell'autosalvataggio.

**E la regola generale che li racchiude tutti** (dall'utente, 2026-08-11): *"meno vedi più fai
cose su basi sbagliate e quindi commetti errori"*. Quando un sottosistema si può osservare solo
aprendo una finestra, **io non lo posso osservare** — e finisco a ragionare su descrizioni di
seconda mano. Il rimedio non è chiedere all'utente di descrivere meglio: è **portare l'analisi
fuori dall'interfaccia**, headless, sullo stesso codice (`--validate`, `--navcheck`,
`--editor-selftest`). Ogni volta che mi trovo a dedurre lo stato di un sistema da ciò che mi viene
riferito, quello è il segnale che manca uno strumento, e lo strumento viene prima della correzione.

## 7. Aggiornamento della documentazione

Ogni change set che modifica comportamento, schema, o convenzione deve aggiornare
ProjectDocs nello stesso change set:
- Bug risolto → aggiorna 08_KnownIssues (marca risolto con data) e 07_Changelog.
- Task completato → aggiorna 06_Todo (sposta a "Done" con data).
- Decisione strutturale nuova → nuovo ADR in 13_ADR, Status: Proposed finché non è
  implementata e verificata, poi Accepted.
- Nuovo vincolo confermato sul codice reale → 10_ProjectMemory.
- Non lasciare mai un documento in uno stato che contraddice il codice che hai appena
  scritto: è la causa numero uno di deriva tra documentazione e realtà in questo progetto.
# Riga di comando e diagnostica

## Sto guardando la versione giusta?

In fondo alla barra dei menu c'e' scritto **`build <data> <ora>`**: e' quando questo eseguibile e'
stato compilato. Se dopo una modifica "non e' cambiato niente", **guarda prima quel timbro**: se
non corrisponde all'ultima compilazione, stai lanciando un eseguibile vecchio oppure l'editor non
e' stato riavviato. Sono i due casi che da fuori sembrano identici a "il codice non funziona".

## GFEditor
- `--module <nome>` apre subito un modulo: `map`, `entity`, `weapon`, `vehicle`, `balance`,
  `mission`, `class`, `validate`, `viewport`, `home`.
- `--module a,b,c` li ATTRAVERSA in sequenza (`--module-frames N` per la durata di ogni passo).
  Serve a riprodurre i difetti che si presentano nel PASSAGGIO fra moduli.
- `--entity <id|indice>` apre l'Entity Editor con un'entita' gia' selezionata, quindi col
  modello caricato.
- `--struct-tab <id>` apre il tab di un tipo di struttura ed esegue subito la verifica,
  stampandone l'esito.
- `--editor-selftest` esegue i collaudi delle operazioni (duplica, serie, annulla, tetti,
  camera, inquadratura, assemblaggi) senza aprire finestre, e ritorna il numero di fallimenti.
- `--crash-test` provoca un crash volontario per verificare che il rapporto di crash funzioni.

## GFEngine
- `--validate` controlla tutti i contenuti e riporta problemi e avvisi per mappa.
- `--map "Nome Mappa"` sceglie la mappa. **Le virgolette servono** se il nome ha spazi.
- `--sim-ticks N` simula N tick e esce: e' deterministico, quindi e' la forma giusta per
  CONFRONTARE due versioni.
- `--at x,z` fa nascere il giocatore in quel punto invece che allo spawn della mappa. Il valore e'
  in **metri**, con la virgola a separare (non lo spazio): `--at 12.5,-8.25`. Se il formato e'
  sbagliato o la mappa non esiste, il gioco parte lo stesso e **te lo dice** invece di far nascere
  il giocatore altrove in silenzio.
- `--at x,y,z` aggiunge la **quota**: si nasce sulla superficie piu' alta *al di sotto* di quella
  quota. Serve nelle mappe a piu' livelli — con `--at 0,12,0` nasci sulla passerella a 3 m,
  con `--at 0,1.5,0` nello stesso punto in pianta ma **a terra**. Senza la quota (forma a due
  valori) vale la regola di prima: la superficie piu' alta a quelle coordinate, qualunque sia.
- `--navcheck [--map "Nome"]` costruisce il **navmesh vero** (stesso codice del gioco) senza
  aprire una finestra e stampa, per mappa: poligoni, m² navigabili, componenti, e **ogni isola**
  con area, posizione e quanta della sua superficie sta **sotto un ostacolo** — che e' la
  differenza fra "terreno chiuso sotto un cubo" (normale) e "zona scollegata" (da correggere).
  Piu' le posizioni tattiche e i command post irraggiungibili. Esce con codice diverso da zero se
  ci sono isole vere o post incatturabili, quindi vale anche come controllo automatico.
- `--walk` entra **direttamente nella mappa, da solo**: niente menu, niente manichini, niente
  simulazione. Serve a provare com'e' percorrere uno spazio. Tecnicamente e' la **sandbox con zero
  manichini**, non una modalita' a parte: stessa geometria, stesse strutture, stessi veicoli, e il
  pannello della sandbox resta disponibile se a meta' prova ti serve un bersaglio.
  **Prova da qui** del Map Editor lancia esattamente `--walk --map "..." --at x,z`.
- `--sandbox` e' l'altra cosa: la sandbox completa, con i manichini di **ogni** unita' registrata
  (almeno uno per definizione, cosi' ogni unita' autorata e' subito provabile) e gli strumenti di
  simulazione.

## Quando qualcosa va storto
In `_telemetry_data/` trovi:
- `crash_report.txt` — motivo, **fase** (cosa stava facendo il programma), e lo stack con nomi
  di funzione, file e riga;
- `editor_run.log` / i log del gioco.

Se l'editor crasha, quel file e' l'unica cosa che serve per diagnosticarlo: allegalo.

## Aspetto dell'interfaccia
Menu **Aspetto → Interfaccia**: dimensione del testo e densita' dei comandi, con effetto
immediato; si conservano alla chiusura. A densita' bassa entra molto piu' contenuto nei
pannelli senza tagliare nulla.

Menu **Aspetto → Diagnostica ImGui**: strumenti interni della libreria di interfaccia, utili
per segnalare un difetto grafico con precisione.

## Annulla / Ripeti — dove funziona
**Ctrl+Z** annulla, **Ctrl+Y** (o **Ctrl+Shift+Z**) ripete. Un trascinamento intero conta come UNA
operazione, non come un passo per fotogramma.

Funziona in:
- **Map Editor**, tab Mappa — con la sua cronologia;
- **Map Editor**, tab di un tipo di struttura — cronologia separata, per tab;
- **Entity Editor** — attach point, zone hitbox, rotazione e scala del modello.

La cronologia si azzera quando si cambia entita' o si carica un'altra mappa: applicare a una cosa
lo stato di un'altra sarebbe un annullamento che rompe invece di riparare.

Negli altri moduli non c'e' ancora: e' in corso l'adozione del componente condiviso (doc 52).

## Modifiche non salvate — dove sei protetto
Chiudendo GFEditor, l'avviso ora interroga TUTTI i moduli e dice in chiaro cosa non e' salvato
("la mappa Training Ground, l'entita' Clone Trooper e un'arma").

- **Salva ed esci** compare solo se tutto cio' che e' in sospeso sa salvarsi da solo. Il Balance
  Editor scrive per singola definizione e non ha un "salva tutto": quando ha modifiche aperte,
  l'avviso non offre il salvataggio automatico — meglio tornare indietro e salvare a mano che
  credere di aver salvato tutto.
- Il **Map Editor** ha in piu' una copia di recupero automatica ogni 2 minuti in `_autosave/`.

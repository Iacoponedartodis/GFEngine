# 39 — Editor UX & Accessibilità (Planned Feature → in lavorazione)

> **Stato**: aperto 2026-07-22, su richiesta esplicita dell'utente. Non è un sistema di gameplay: è
> rendere il **Map Editor** uno strumento con cui si può davvero costruire una mappa complessa senza
> combattere contro l'interfaccia. Prerequisito dichiarato al "salto di complessità" su Training Ground.

## Overview
Il Map Editor funziona ma è **rigido**: diverse operazioni di authoring sono scomode o impossibili, e la
UI ha iniziato a tagliare comandi fuori dallo schermo. Prima di caricare Training Ground di contenuti
serve che lo strumento sappia posizionare, alzare, vedere e annotare gli oggetti correttamente.

## Goal
Un editor in cui: nessun comando è tagliato o irraggiungibile; ogni oggetto si posiziona dove serve (non
solo al centro) e si alza in Y; le superfici si vedono (non solo wireframe); i metadata si inseriscono
senza attrito. Poi lo si usa per fare di Training Ground la mappa-banco-di-prova di tutti i sistemi.

## Problem solved
Oggi: comandi tagliati dalla toolbar satura; oggetti creati solo al centro e spostati a mano; comandante
strategico / torri / bersagli **non alzabili** (manca il campo Y nel dato, non solo lo slider); viewport
a sole linee (superfici invisibili → difficile capire volumi e coperture); inserire i metadata è
laborioso e poco guidato.

## Scope — fasi (l'ordine è una proposta, si conferma con l'utente)

### F1 — Igiene della UI (toolbar & overlay) · ✅ **FATTA 2026-07-22** (changelog 55)
- **La toolbar non deve tagliare comandi.** Principio permanente ([[ui-no-clipping-use-dropdowns]]):
  quando è satura, raccogliere in **menù dropdown** (come il menu Moduli), non accodare `SameLine`.
- **Rimuovere i pulsanti Sposta/Ruota/Scala dalla toolbar** (`gizmoModeBar`): sono duplicati
  dell'overlay che compare in alto a sinistra sulla viewport quando selezioni un oggetto. Si tiene solo
  l'overlay.
- **"Nuova mappa" va in coda al dropdown delle mappe**: si clicca "＋ Nuova mappa…", esce un popup di
  conferma (annulla / nome + conferma), non un `InputText` sciolto sulla barra.
- **Bug click-through dell'overlay**: cliccando i pulsanti Sposta/Ruota/Scala dell'overlay si seleziona
  anche l'oggetto dietro. La selezione deve essere annullata se il click cade su un pulsante dell'overlay.

### F2 — Alzare e posizionare · ✅ **FATTA 2026-07-22** (changelog 56)
- **Campo Y per le STRUTTURE (fatto)**: `StrategicTargetDef`/`TargetEntry` ha ora `y` = altezza sopra il
  suolo (0 = a terra, retro-compatibile), cablato editor↔runtime (`groundHeightAt + y`), con gizmo Sposta
  su Y, slider e anteprima. Effetto verificato via telemetria.
- **Perché NON comandante/veicoli/settori (decisione)**: comandante e veicoli sono unità con **gravità**
  → una Y "in aria" cadrebbe: sarebbe un dato senza effetto ([[verify-effect-not-data]]). Per metterli
  in alto si usa una piattaforma. I settori sono aree **2D** (XZ). Quindi la Y ha senso solo per le
  strutture statiche — ed è lì che l'ho messa. Gli slider Y esistono già ovunque siano efficaci
  (spawn, posizioni, danger, route, box, ora target).
- **Piazzamento davanti alla camera (fatto)**: `FreeCameraViewport::groundFocusPoint()` (sguardo→suolo,
  ripiego a distanza fissa); tutti i "+ Aggiungi" creano l'oggetto dove guardi, non al centro.
- *Rimane possibile* (non fatto, non bloccante): piazzamento sotto il **mouse** (raycast del cursore),
  più preciso del centro-sguardo; da valutare se serve dopo il collaudo.

### F3 — Vedere le superfici · ✅ **FATTA 2026-07-22** (changelog 57)
- Facce piene ombreggiate sui box (muri/piattaforme/cover), oltre al wireframe, con `glPolygonOffset` per
  spigoli nitidi. Opaco, nessun blending, stesso pipeline (ADR-003 rispettato). Toggle "Solido" (default
  ON) in toolbar.
- *Rimane possibile* (non fatto): riempimento semitrasparente delle AREE 2D (settori/danger/raggi) —
  richiederebbe alpha/blending (un uniform nello shader). Le aree restano per ora contorni. Da valutare
  dopo il collaudo se serve; il grosso ("le superfici invisibili") era la geometria, ora piena.

### F4 — Metadata senza attrito · ✅ **FATTA 2026-07-22** (changelog 58)
- **Duplica generalizzato** (`duplicateSelected`): duplica qualsiasi metadato con tutti i suoi campi
  (posizione/settore/danger/bersaglio/post/percorso/veicolo) — "autora una volta, posa una serie".
- Default di creazione già sensati; pannelli con tooltip. Nessun altro attrito evidente da rimuovere ora.
- *Rimane possibile* (non fatto): un vero "pennello" click-per-posare; la duplicazione ne è la versione
  pragmatica.

### F5 — Training Ground come banco di prova (il "salto di complessità") · ✅ OBIETTIVO RAGGIUNTO (2026-07-23)
- Con l'editor a posto (F1-F4), la mappa più grande (Training Ground) ora esercita **tutti** i sistemi
  INSIEME. Misura finale (`--sim` 6v6): overwatch 4, hold 543, osservazione 597, route 54, manovre 19,
  combattimento (contatti 295), comando multi-fronte (3), `fermi=0`. I tre gap emersi sono chiusi
  (danger aggiunte, hold, overwatch). Resta authoring incrementale (l'utente affina pose/valori) e la
  rifinitura del *feel*, ma il banco di prova voluto esiste.

**Censimento (2026-07-22)** — la mappa è già ricca: ~136 posizioni tattiche (108 cover, 14 vantage, 6
defensive, 4 chokepoint, 4 observation), **15+ settori** multi-fronte (Alpha/Bravo/Charlie/Delta/Echo),
5 command post, ~12 route, 3 strutture (2 comms + 1 control), roster B1/B1 Heavy vs Clone/Heavy Trooper.
Floor 70×92. **Buco netto: 0 danger zones** → cover-evita-pericolo (ADR-046) non ha nulla da esercitare.

**Misurazione `--sim` (ConquestMode, 6v6, 90 s) — la mappa FUNZIONA bene**:
- Comandante **attivo**: `cmd_obiettivo` = Alpha/Bravo-Charlie/Delta-Echo, `cmd_fronti = 3` costante
  (comando multi-fronte OK).
- **Combattimento** reale: `contatti_vivi` sale 0→10→30→54→…→83 (le forze si trovano e ingaggiano).
- `su_route` 5-7 (route OK), `obs_vista_estesa` 184-190 nelle fasi vicino ai punti d'osservazione (OK),
  `manovre_avviate` 2-4 (aggiramenti OK). **I sistemi lavorano insieme.**

> ⚠️ **Falso allarme corretto**: una prima misura dava "tutti i contatori a 0 / nessun-comandante". Era
> un artefatto dell'harness: `Start-Process -ArgumentList "--map","Training Ground"` non quota lo spazio,
> il processo riceveva `--map Training Ground` → id troncato a "Training" → mappa non trovata → girava
> su un'altra mappa. Con l'id quotato correttamente la mappa carica e tutto scatta. Vedi
> [[verify-effect-not-data]] e KI #77: confermare SEMPRE quale mappa è viva, non fidarsi del flag.

**Gap reali rimasti (opportunità F5, non rotture)**:
- ~~0 danger zones~~ → **✅ AGGIUNTE 7 (2026-07-22)**: set simmetrico che esercita cover-evita-pericolo —
  artiglieria sul centro conteso Alpha (0,0 r9 lvl0.55), due corsie di fuoco sugli assi d'avvicinamento
  nord/sud (0,±13 r7 lvl0.6), quattro chokepoint minati ai obiettivi d'angolo Bravo/Charlie/Delta/Echo
  (±20,±22 r5.5 lvl0.6). Poste vicino al cover esistente così la scelta della copertura le evita davvero.
  Mappa valida (7 danger), sim funziona (comando 3 fronti, combattimento, route). **Effetto AI da
  osservare a mano** (nessun contatore per "cover che evita danger"): l'utente approva/aggiusta le pose.
- `overwatch_avviati` → **✅ RISOLTO (2026-07-23, changelog 61)**. Non era il grafo (ben popolato: 1364
  link): il **segnale d'avanzata viveva un solo tick** mentre la manovra dura ~6 s → il copritore non lo
  vedeva mai. Fix: TTL (~5 s) sull'`Advance` → persiste finché non scade. `overwatch_avviati` ora scatta
  (4 in 90 s): chi non avanza copre il compagno in avanzata via il grafo `positionCovers`.
- `hold_su_posizione` → **✅ RISOLTO (2026-07-22, changelog 60)**. Due parti:
  1. **Comando**: il Hold ora scatta quando i droidi **possiedono un command post** minacciato (condizione
     stabile, prima chiedeva una maggioranza di unità nel settore, raro) → `cmd_tieni` scatta.
  2. **Droidi (opzione A)**: un droide in TIENI si **àncora** alla miglior posizione difensiva/chokepoint
     e ci **combatte da lì senza inseguire** (campo `holdX/Z/Radius` + clamp come il leash; valutato a
     inizio tick, così vale anche durante il combattimento). → `hold_su_posizione` scatta (318/225 quando
     il Hold è su un obiettivo d'angolo con chokepoint).
  Combattimento sano (contatti fino a 55), `fermi=0`, Advance dominante: nessuna passività. Resta un
  gancio di **authoring**: perché il presidio scatti anche su Alpha (centro) servono posizioni
  `defensive`/`chokepoint` vicine — pose dell'utente, non codice.

## Out of Scope (per ora)
- Riscrivere il viewport con un gizmo library esterno (ImGuizmo): si migliora quello a mano che c'è.
- Editor multi-mappa / undo-redo generalizzato: utile ma è un progetto a sé.
- Geometrie oltre il box (mesh arbitrarie, prefab): resta futuro (già annotato in 06_Todo).

## Dependencies
- ADR-001 (id = filename), ADR-002 (due binari), ADR-003 (rendering compat Intel), ADR-010 (save RMW +
  rename command). Ogni cambio di schema mappa (aggiunta `y`) impatta `DefinitionRegistry` (load) e il
  runtime che legge quei campi → tracciare l'impatto globale come da CLAUDE.md §1.4.

---

# Audit di COERENZA fra i moduli e regole vincolanti (2026-08-02)

Segnalazione utente: *"spesso le stesse funzioni non stanno su un modulo e stanno su un altro, alcune cose
che dovrebbero essere uguali per tutto l'editor a volte cambiano tra moduli o alcuni proprio non ce l'hanno."*
Verificato: è vero ed è **misurabile**. I quattro bug del changelog 106 (gizmo assente sui prefab, creazione
prefab mancante, pannello non riallargabile, scroll tagliato) non sono incidenti isolati — sono **sintomi**
di assenza di regole comuni.

## 1. Inventario (conteggio grezzo delle occorrenze; 0 = certamente assente)

| Modulo | Duplica | Rinomina | Elimina | Pannello ridimensionabile |
|---|---|---|---|---|
| BalanceEditor  | ❌ | ❌ | ❌ | ✅ |
| ClassEditor    | ❌ | ✅ | ❌ | ❌ |
| EntityEditor   | ✅ | ✅ | ❌ | ✅ |
| MapEditor      | ✅ | ✅ | ✅ | ✅ (corretto in 106) |
| MissionEditor  | ✅ | ✅ | ✅ | ❌ |
| VehicleEditor  | ✅ | ✅ | ❌ | ✅ |
| WeaponEditor   | ❌ | ✅ | ❌ | ✅ |

**Buchi principali**: **Elimina manca in 5 moduli su 7**; **Duplica in 3**; il ridimensionamento del pannello
in 2. Un autore che impara un modulo non può trasferire l'abitudine a un altro: è il costo nascosto.

## 2. REGOLE VINCOLANTI (valgono per ogni modulo, presente e futuro)

Stessa natura delle regole già in vigore sui dati (id = filename, RMW, dropdown dal registry): non sono
preferenze estetiche, sono **contratti**. Violarle è un errore di implementazione.

**R1 — Ciclo di vita completo.** Ogni modulo che gestisce definizioni su file espone **Crea, Duplica,
Rinomina, Elimina**. Se una di queste non ha senso per quel tipo, va scritto perché nel codice — non
semplicemente omessa. *(Oggi il buco maggiore.)*

**R2 — Rinominare è un COMANDO, non un salvataggio con nome nuovo** (ADR-010). Mai creare un file nuovo
lasciando il vecchio orfano: è la causa dei near-duplicate di KI #7.

**R3 — Ogni scrittura passa da `saveJsonRMW`** (ADR-010). Rispettata ovunque oggi: mantenerla.

**R4 — Un riferimento a un'altra definizione è SEMPRE un dropdown dal registry**, mai testo libero
(CLAUDE.md). Vale anche per i campi nuovi.

**R5 — Layout a tre pannelli**: lista | contenuto/viewport | proprietà. Il pannello ancorato a un bordo si
ridimensiona con uno **splitter esplicito**, mai con `ImGuiChildFlags_ResizeX` (il suo grip finisce sul bordo
finestra e il pannello non si riallarga più — bug reale, changelog 106). Sempre con **clamp** min/max: nessuno
stato irreversibile.

**R6 — Il contenuto posizionato con coordinate assolute deve estendere l'area scrollabile** (un `Dummy`
finale). Altrimenti lo scroll si ferma sull'ultimo elemento e la schermata sembra tagliata (changelog 106).

**R7 — Ogni elemento selezionabile ha il suo gizmo dichiarato.** Aggiungendo un tipo selezionabile si
aggiorna il dispatch del gizmo *nello stesso change set*: se un tipo non è trasformabile, va detto — la
sparizione silenziosa dei tool si legge come un bug (changelog 106).
**Corollario (fragilità nota)**: i codici di selezione del Map Editor sono un unico range crescente con rami
`<= soglia` usati come catch-all (`<= -2000` per i settori cattura anche -4000 dei prefab). Aggiungendo un
tipo, il suo ramo va messo **prima** del catch-all. Da irrobustire quando se ne aggiungerà un altro.

**R8 — Niente comandi tagliati**: quando un pannello cresce, si raggruppa in sezioni/tendine invece di far
uscire i comandi dalla vista ([[ui-no-clipping-use-dropdowns]]). Le tendine ricordano lo stato aperto/chiuso.

**R9 — I difetti si mostrano dove si autora.** Un modulo che può produrre contenuto invalido mostra i propri
problemi in loco (modello: "Salute tattica" del Map Editor, changelog 103-104), non solo nel pannello
Validazione. Le regole stanno in `ContentValidation` (condivise col gate headless), mai duplicate nella UI.

## 2-bis. Lo SCHELETRO comune (ADR-049) — come le regole diventano strutturali

Le regole di §2 restano disciplina finché ogni modulo rifà la struttura da sé. La soluzione (proposta
dall'utente, 2026-08-02) è raccoglierle in componenti condivisi:

| Componente | Copre | Stato |
|---|---|---|
| `FreeCameraViewport` | viewport 3D + gizmo sposta/ruota/scala | ✅ **già esisteva**, usato da 4 moduli |
| `DefinitionRename` · `saveJsonRMW` · `UiWidgets` | R2, R3, widget | ✅ già esistevano |
| **`ModuleShell`** | R5 (splitter + clamp), R6 (scroll), layout a 3 pannelli | ✅ fatto (changelog 107), pilota `VehicleEditor` |
| **`AssetBrowser`** | R1 (Crea/Duplica/Rinomina/Elimina) | ⚠️ costruito (changelog 108) ma **non ancora adottato** → finché non lo è, è debito |

**Regola di adozione**: un modulo si migra **quando lo si tocca per un motivo funzionale**, mai "per
allineamento". Motivo: l'editor è GUI e non è verificabile con `--sim`; sette migrazioni insieme non sarebbero
testabili in un colpo solo, e il rischio di regressioni supererebbe il beneficio.

**Prova che il metodo funziona**: migrando il pilota è emerso che `VehicleEditor` replicava **lo stesso bug
del pannello** già corretto nel Map Editor, e nessuno l'aveva notato. Una funzione scritta due volte è un bug
scritto due volte.

## 3. Piano di allineamento (proposto, non ancora applicato)

| Priorità | Intervento | Costo |
|---|---|---|
| **Alta** | **R1**: aggiungere *Elimina* ai 5 moduli che non ce l'hanno (con conferma esplicita) | Medio |
| **Alta** | **R5**: splitter esplicito nei moduli senza ridimensionamento (Class, Mission) | Basso |
| Media | **R1**: *Duplica* in Balance, Class, Weapon | Basso |
| Media | **R9**: portare i difetti in loco anche negli altri moduli | Medio |
| Bassa | **R7-corollario**: irrobustire i codici di selezione (range espliciti, non catch-all) | Medio |

> Nota di metodo: queste regole vanno applicate **quando si tocca un modulo**, non con un refactor unico di
> tutti e sette. Un refactor totale dell'editor non è verificabile in un colpo solo (è GUI: niente `--sim`),
> e il rischio di regressioni supera il beneficio.

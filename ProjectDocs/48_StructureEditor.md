# 48 — Editor Strutture (Planned Feature)

> Stato: **Planned Feature**. Scope definito, implementazione in corso (changelog 163).
> Prerequisiti già in essere: ADR-053 (primitive parametriche), ADR-054 (navmesh vero
> nell'editor), ADR-049 (scheletro comune dei moduli), doc 47 (metriche e libreria).

## Overview

Un **tab dentro il Map Editor** — non un modulo nuovo — in cui si creano e si modificano
**tipi di struttura**: preset con nome delle primitive di ADR-053, che portano con sé
**quali misure sono modificabili** e **entro quali limiti**, e che si verificano da soli
costruendo il navmesh vero sulla struttura isolata.

Richiesta dell'utente (2026-08-05), verbatim sui punti che decidono la forma:
> *"andando in strutture, in fondo al menu bisogna aggiungere il tasto 'editor strutture'
> … che apre un altro tab nel map editor, tipo i tab di google, così non apre un altro
> modulo, ma posso rimanere in map editor avendo una viewport separata e nuove opzioni
> diverse"*, con *"funzioni per scegliere quali grandezze possono modificare, per impostare
> dei minimi o dei massimi"* e *"la possibilità di verificare che il navmesh funzioni e che
> tutte le misure vadano bene"*.

## Goal

Che la libreria di forme con cui si costruisce una mappa sia **autorata, nominata e
verificata** invece che ricordata a memoria — e che una forma entri in libreria solo dopo
aver dimostrato, sul navmesh vero, di essere percorribile.

## Problem Solved

1. **I vincoli sono murati nel codice.** Oggi `minWidthFor(kind)` e i clamp di alzata/pedata
   vivono in `MapStructures.hpp`. Sono giusti come **pavimento fisico**, ma non esiste modo
   di dire *"in questa mappa le passerelle stanno fra 2,4 e 3,0"* senza ricompilare.
2. **Nove primitive non sono una libreria.** Per la mappa 300 × 200 servono forme
   riconoscibili e riusabili ("torre con scala interna", "passerella stretta"), non nove
   ricette da riparametrizzare a mano ogni volta.
3. **La verifica arriva troppo tardi.** Oggi si scopre che una scala non genera navmesh
   *dopo* averla piazzata in mappa, in mezzo ad altre 167 box (è la storia di KI #97, e delle
   scale con pianerottolo ritirate dal menu). Su una struttura **isolata** la stessa domanda
   ha una risposta immediata e non ambigua.
4. **Una forma sbagliata si ripete.** Senza un posto dove una forma sia *definita*, ogni
   riuso è una nuova occasione di sbagliarla.

## Riferimenti professionali (regola dell'utente: guardare chi l'ha già risolto)

- **Revit — famiglie: parametri di TIPO vs di ISTANZA.** *"Instance and type parameters can
  be changed without editing the family"*. È la spina dorsale che adottiamo: il **tipo**
  dichiara quali parametri esistono e con che limiti; l'**istanza** in mappa ne fissa i
  valori. Modificare i valori di una struttura in mappa non richiede di aprire il tipo.
- **Unity — Prefab Mode.** Si entra nel prefab **da dove lo si usa**, si edita in
  **isolamento**, e all'uscita c'è un contratto esplicito su cosa è stato salvato. Da qui:
  il tasto "Editor strutture" sta in fondo al menu **+ Struttura**, e il tab si apre con la
  struttura già in vista, isolata.
- **AutoCAD — REFEDIT / Block Editor.** Ridefinire un blocco **ridefinisce tutte le sue
  inserzioni**. È il rischio da rendere visibile: cambiare un tipo tocca ogni struttura che
  lo usa, quindi l'editor deve dire *quante* prima, non dopo.

## Scope (questo giro)

- **Barra tab nel Map Editor**: `Mappa` sempre presente; i tipi aperti diventano tab
  chiudibili accanto, in stile browser. Nessun modulo nuovo (vincolo esplicito dell'utente).
- **Viewport separata** per il tab struttura: mostra **solo** la struttura, su un piano
  neutro, con la figura di scala di doc 47 §E6 accanto.
- **Nuovo tipo di definizione**: `data/structures/<id>.json`, id = filename stem (ADR-001),
  caricato da `DefinitionRegistry` come tutti gli altri.
- **Authoring dei vincoli**: per ogni parametro della primitiva, `editable` sì/no e
  `min`/`max`. Il **pavimento fisico resta invalicabile** (vedi Invarianti).
- **Verifica integrata**: navmesh vero (ADR-054) sulla struttura isolata + controllo delle
  metriche di `MapMetrics.hpp`, con l'esito in chiaro: percorribile / isole / superfici
  perse, e *perché*.
- **Il menu `+ Struttura` si popola dai tipi** oltre che dalle 9 primitive nude.

## Out of Scope (esplicito — non anticipare)

- **L'editor prefab.** L'utente lo ha messo dopo, e condizionato: *"Una volta fatto bene in
  maniera funzionante creiamo una cosa del genere anche con il prefab"*.
- **Ordinamento in categorie delle liste del Map Editor.** È il terzo lavoro chiesto, separato.
- **Nuove primitive geometriche.** Un *tipo* è un preset vincolato di una primitiva
  esistente; una primitiva nuova è codice in `mapstructures::expand` e resta una decisione a
  parte. (Vedi Assunzione dichiarata.)
- **Strutture annidate** (una struttura composta da altre). Serve per le torri, ma è un
  sistema suo: prima si valuta se la composizione è di tipi o di istanze.
- **Modifica del navmesh in tempo reale a ogni tasto**: la costruzione costa ~0,1 s, quindi
  la verifica resta **su richiesta** come in ADR-054, con il rilevamento di stantio.

## ⚠ Assunzione SMENTITA dall'utente (2026-08-06) — vedi doc 50

L'assunzione qui sotto era dichiarata proprio perché potesse essere smentita, ed è successo:
*"l'editor strutture mi permette di modificare le strutture ma non di crearne di nuove … un sistema
per far sì che io possa creare strutture che rispettino il navmesh, per fare anche magari strutture
un po' più complesse"*. Serve un livello di **assemblaggio** (più parti in un tipo solo), non solo
preset di primitive singole. Analisi e piano in **doc 50 §2**; la decisione da prendere prima del
codice è se assemblaggio e **prefab (ADR-048)** debbano restare due sistemi o diventarne uno.

## Assunzione dichiarata (superata, tenuta per memoria)

*"Creare strutture"* è letto come **creare TIPI (preset vincolati) delle primitive
esistenti**, non nuove geometrie parametriche. Motivo: una primitiva nuova è un ramo di
`expand()`, cioè codice, mentre il valore che l'utente chiede — nomi, limiti, verifica,
riuso — sta tutto nel livello del tipo. Se servirà una geometria che nessuna delle nove
primitive esprime, sarà una richiesta esplicita e un ADR a sé.

## Invarianti (non negoziabili)

1. **Il pavimento fisico vince sempre.** `minWidthFor(kind)`, il clamp di `STEP_HEIGHT`
   sull'alzata e `STAIR_TREAD` sulla pedata **non** sono preferenze: derivano dall'erosione
   di Recast e da `minRegionArea` (doc 47 §4, e KI #97 ne è la conferma sul campo). Un tipo
   può essere **più severo**, mai più permissivo. Un `min` autorato sotto il pavimento viene
   alzato e l'editor lo dice.
2. **Fallback documentato durante la transizione** (CLAUDE.md §2). Una struttura in mappa
   **senza** campo `type` continua a comportarsi esattamente come oggi. Nessuna mappa
   esistente cambia comportamento.
3. **I box espansi non si salvano mai**, né qui né altrove: la ricetta è la verità, i box si
   rigenerano (ADR-053, ADR-033).
4. **Una sola espansione.** Il tab struttura usa `mapstructures::expand`, la stessa del
   registry, del Map Editor e del gate. Un'anteprima con codice proprio divergerebbe.
5. **Il rename passa dal comando di rename** (ADR-010), non da "salva con altro nome".

## Osservabilità (ADR-050, §5-bis — nasce con il sistema)

- **Sintomo**: per il tipo aperto, `superficie_persa_%` — quanta della superficie calpestabile
  dichiarata sopravvive al navmesh. È il sintomo diretto (una scala che non si sale), non
  l'esito lontano.
- **Funnel**: `box_espansi → box_nel_navmesh → triangoli → componenti connesse`. Dove muore,
  e su quale base.
- **Singola entità**: quale box della struttura non produce superficie, con la causa fra le
  quattro possibili (erosione · ciglio · altezza libera · area minima di regione).
- **Gate di authoring**: un tipo che non passa la verifica si può salvare, ma resta **marcato
  non verificato** nella libreria — e il menu `+ Struttura` lo mostra come tale. Un dato
  sbagliato in silenzio è il difetto che il gate esiste per impedire.

## Dependencies

- `include/mini/game/MapStructures.hpp` — espansione (invariata, si aggiunge la validazione
  dei limiti).
- `include/mini/game/data/Definitions.hpp` — `StructureTypeDef` + `StructureDef.type`.
- `src/game/data/DefinitionRegistry.cpp` — `loadStructureTypes` (+ `loadAll`).
- `editor/src/modules/MapEditor.cpp` — barra tab, tab struttura, menu `+ Struttura`.
- `mini::NavManager` — già linkato nell'editor da ADR-054.
- `src/game/data/ContentValidation.cpp` — il gate segnala i tipi non verificati.

## Fasi

- **S1 ✅** — Barra tab nel Map Editor (`Mappa` + tab chiudibili), viewport separata.
- **S2 ✅** — `StructureTypeDef` + loader + fallback (nessuna mappa cambia comportamento).
- **S3 ✅** — Editor dei parametri: valori, `editable`, `min`/`max`, col pavimento fisico visibile
  e invalicabile.
- **S4 ✅** — Verifica integrata (navmesh isolato) con sintomo, funnel e box muto.
- **S5 ✅** — `+ Struttura` si popola dai tipi, con i non verificati in giallo.
- **S6 ✅** — ProjectDocs + verifica (changelog 163).
- **Gate `--validate` sui tipi non verificati**: NON fatto. Oggi la marcatura vive nel menu
  dell'editor; portarla anche nel gate serve quando i tipi saranno davvero in uso su una mappa.

## Cosa è emerso implementando (da non ri-scoprire)

- **`width` della piattaforma non è un parametro**: l'espansione fissa le scale d'accesso a
  `STAIR_MIN_WIDTH` (`st.width = ...` in `MapStructures.hpp`) e ignora il campo. Esporlo darebbe un
  comando inerte. Renderlo efficace **cambierebbe la geometria delle piattaforme già in mappa**:
  è una decisione dell'utente.
- **Le primitive si autoproteggono già sulla larghezza.** Una scala autorata a 0,80 m esce
  comunque a 1,60: l'espansione la clampa. Quindi la verifica navmesh **non serve** a intercettare
  quel caso — serve per ciò che i clamp non coprono (accessi mancanti, altezza libera, giunzioni).
- **Il blocco `defaults` parla il vocabolario delle ISTANZE** (`y`, non `elev`): è una ricetta
  completa, riletta dallo stesso `parseStructure` delle mappe. Una chiave inventata qui verrebbe
  ignorata in silenzio — è già successo in fase di sviluppo.
- **Per un ostacolo puro il criterio è rovesciato.** Muri, porte e barricate non dichiarano
  superficie calpestabile: si verifica che **non ostruiscano**, e il numero di componenti > 1 è
  atteso, non un difetto.

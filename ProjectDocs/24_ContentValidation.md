# 24 — Content Validation & Error Model (Planned Feature)

**Status: Planned Feature — non ancora implementato in codice.**
Descrive un sistema che NON esiste ancora (la validazione oggi è sparsa nei loader e nelle UI,
e in gran parte assente). Esiste per dare a Claude Code un bersaglio preciso. Non trattare
nessun nome di funzione/campo come già presente finché 05_CurrentState non lo conferma e questo
header non diventa "Implementato". Vedi ADR-018.

## Overview
Un **gate di validazione** che gira nello stesso identico codice in tre punti — caricamento
runtime, console dell'editor, e verifica headless — e che rifiuta (o degrada esplicitamente) i
contenuti non validi **prima** che diventino un bug in partita.

## Problem Solved
Il progetto ha già pagato ripetutamente la stessa classe di problema: **dati sbagliati che non
falliscono, ma si degradano in silenzio.** Storico confermato:
- **KI #7 (P0):** near-duplicate weapon/enemy JSON da rename manuale → doppioni nel loadout.
  Mitigato dal rename tool (ADR-010), ma nessuno *verifica* che non esistano duplicati.
- **KI #25:** campi editabili ma mai consumati dal runtime ("campi fantasma") — mitigato
  marcandoli "(non attivo)" a mano, ma nulla impedisce che ne nascano altri.
- **KI #26:** dati/codice morti che creano fallback problematici.
- **2026-07-09 (#2):** cambiare `hitbox_profile` senza ricaricare le zone → scritte le zone del
  profilo precedente su quello nuovo (B1 svuotato). Salvato dal `.bak`.
- **ADR-007:** fallback id hardcoded nei game mode, rimossi solo dopo che avevano già fatto danno.

Il pattern è sempre lo stesso: **un riferimento rotto o un dato incoerente non blocca nulla, e
il sintomo appare molto dopo, in un punto lontano dalla causa.** ADR-010 ha reso strutturale la
*scrittura* sicura (`saveJsonRMW`); questo doc fa lo stesso per la *correttezza* del contenuto.

## Goal
Rendere impossibile far partire una partita con contenuto critico invalido, e rendere l'errore
**azionabile** (cosa, dove, cosa fare) invece che silenzioso.

## Scope

### 1. Error model (`Result`-style)
Introdurre un tipo di ritorno per i fallimenti **attesi** (file assente, riferimento invalido,
schema incompatibile) distinto dalle assertion (violazioni di invarianti interne = bug di codice).

```cpp
// include/mini/core/Result.hpp  (nuovo)
struct Diagnostic {
    Level       severity;    // Error | Warning
    std::string category;    // "Content" | "Asset" | "Map" | ...  (categorie telemetria, doc 21)
    std::string file;        // path del JSON incriminato
    std::string message;     // cosa non va
    std::string suggestion;  // cosa fare (azionabile)
};
```
Ogni `Diagnostic` DEVE essere emessa anche come evento telemetria JSONL (doc 21) con
`event(Level, "Content", msg, json)` → così la validazione è leggibile da un LLM e da un tool
senza aprire l'editor.

### 2. I gate (cosa verificare prima di far partire una missione)
Derivati dai problemi reali già visti, non teorici:
- **Riferimenti risolti:** ogni `EnemyDef.weaponIds[]`, `aiProfileId`, `hitboxProfileId`,
  `MapDef.enemyTypes/allyTypes`, `abilities[]` esiste nel registry ed è del **tipo giusto**.
- **Nessun orfano/duplicato:** nessun file dati non referenziato che duplichi un id vicino
  (chiude KI #7 in modo strutturale invece che manuale).
- **Mappa:** `spawnTeam1/2` presenti; `commandPosts` riferiscono zone valide; `geometry` non vuota.
- **Arma:** i campi consumati dal runtime sono presenti e sensati (rateo > 0, gittata > 0).
- **Unità:** ha hitbox profile valido, mesh risolvibile, profilo AI valido.
- **Abilità:** i `param` usati dal tipo di abilità sono presenti.
- **Asset:** ogni path mesh/texture referenziato esiste su disco (evita il "modello invisibile").
- **Campi fantasma:** chiavi presenti nel JSON che **nessun loader legge** → **Warning**
  automatico. **IMPLEMENTATO 2026-07-15** con l'opzione (a), decisa dall'utente: i loader
  registrano le chiavi sconosciute in `DefinitionRegistry::unknownKeys()` mentre il JSON è
  ancora in mano — dopo il parsing quell'informazione non esiste più. Nessun I/O nuovo,
  nessun re-parse. Gli elenchi delle chiavi note stanno **accanto al parser** che le legge:
  è l'unico posto in cui non possono divergere dal codice reale.
  Copre `weapons`, `ai`, `abilities`, `enemies`, `allies`, `objectives`, `missions`
  (chiavi di primo livello; gli oggetti annidati come `stats`/`attach_points` non sono coperti).
  Il campo `id`/`profile_id` ha un messaggio dedicato: non è un refuso, è il campo che ADR-001
  ignora di proposito ed è ciò che causò KI #21.

  > **Cosa questo gate NON cattura — distinzione importante, verificata sul codice 2026-07-15.**
  > Cattura le chiavi che il loader **ignora** (refusi come `"fire_rat"`, che non falliscono:
  > il runtime usa il default e il sintomo appare lontano dalla causa; e campi obsoleti che un
  > editor scrive ma nessuno legge).
  > **Non** cattura la lista storica di KI #25 (`min_range`, `fov_deg`, `hearing_range`,
  > `reposition_chance`): quei campi il loader **li legge** e li mette nella struct — è più a
  > valle che nessun sistema li consuma. Nessun gate che guardi i *dati* può vederlo: è un fatto
  > sul *codice*, e servirebbe analisi statica o reflection. Resta quindi annotato a mano negli
  > editor ("(non attivo)", A9). La formulazione originale di questo punto ("non consumato da
  > nessun lettore → rende KI #25 auto-diagnosticato") confondeva i due casi.

### 3. Dove gira (stesso codice, tre consumatori)
```
validateContent(registry) -> std::vector<Diagnostic>
 ├── Runtime  : al load della missione → Error = blocca (o degrada esplicito + log)
 ├── Editor   : pannello "Validazione" → lista cliccabile, doppio click = apri file/campo
 └── Headless : GFEngine.exe --validate  → stampa + JSONL + exit code ≠ 0
```
L'editor **non deve avere una copia più debole delle regole**: un contenuto che l'editor accetta
ma il runtime rifiuta è esattamente il bug che stiamo eliminando.

### 4. Politica di fallimento
- **Contenuto critico invalido → blocca** con diagnostica azionabile. Niente fallback silenzioso.
- **Non critico (es. cosmetico) → Warning** loggato con il riferimento mancante, mai silenzio.
- Il fallback geometria firebase (ADR-004) e gli altri fallback **documentati** restano, ma
  devono loggare che sono stati usati.

## Out of Scope
- Riscrivere i loader esistenti: la validazione si aggiunge **dopo** `loadAll()`, leggendo il
  registry già popolato. Nessun cambio al pattern `gets/geti/getf` (04_CodingStandards).
- Schema versioning / migrazioni: utile ma è un problema diverso, e oggi non c'è pressione
  (id = filename stem, schema stabile). Aprire un ADR separato quando servirà.
- Validazione semantica di bilanciamento ("quest'arma è troppo forte") — non è un gate tecnico.
- Test automatici: il progetto non ne ha per scelta (doc 12). `--validate` è verifica headless,
  coerente con il canale telemetria già in uso.

## Architecture
`validateContent()` vive nel layer **game/data** accanto a `DefinitionRegistry` (è il nodo
centrale dei dati, doc 01), non nell'editor: così entrambi i binari la linkano senza violare il
contratto two-binary (ADR-002 — l'editor può dipendere dagli header engine, mai il contrario).

Consuma solo il registry già caricato → **zero I/O nuovo**, testabile in isolamento, e
riutilizzabile dal `--validate` headless.

## Technical Decisions
- **Perché non eccezioni:** il codebase non le usa; `Result`/vector di diagnostiche si integra
  col pattern esistente e con la telemetria.
- **Perché non bloccare TUTTO:** bloccare su un cosmetico mancante renderebbe l'authoring
  ostile. La distinzione critico/non-critico è la parte che va decisa con cura, per definizione.
- **Perché la validazione dei campi fantasma è automatica:** mantenerla a mano (KI #25) ha già
  mostrato di non scalare.

## Acceptance
Stato al 2026-07-15 (in force). Numeri e comandi in 07_Changelog.
- [x] `GFEngine.exe --validate` esce ≠ 0 su un dato rotto ad arte (es. `aiProfileId` inesistente).
      — verificato con guasti deliberati: 6 errori, 3 warning, **exit 1**. Sui dati reali: 0/0, exit 0.
- [x] Ogni Error ha categoria + file + suggerimento, ed è nel `session_latest.jsonl`. — il
      `suggestion` è un campo obbligatorio di `Diagnostic`, non una convenzione.
- [x] Rompere un riferimento **blocca** il load invece di degradare. — verificato: "Avvio
      BLOCCATO: contenuto critico invalido". *Nota:* il blocco è all'avvio (dopo `loadAll`), che
      è prima e più a monte del load della singola missione.
- [x] Il pannello editor mostra gli stessi errori del runtime (stesso codice). — `Moduli →
      Validazione contenuti`; `ContentValidation.cpp` è nella source list di **entrambi** i
      target, quindi una copia più debole è impossibile per costruzione.
      **Build-verified ma non ancora aperto a mano**: serve uno smoke in GFEditor.
- [x] Un near-duplicate (KI #7) viene segnalato automaticamente. — sui **nomi visualizzati**
      (identici o l'uno prefisso dell'altro), che è come KI #7 si manifestò davvero.

- [x] **Campi fantasma** (aggiunto 2026-07-15, opzione (a)): verificato con un file di prova con
      `"fire_rat"` (refuso), `campo_obsoleto` e `id` → 3 Warning distinti. **Sui dati reali ha
      trovato subito un caso vero**: `data/ai/B1 Heavy Droid.json` conteneva `profile_id`, residuo
      pre-ADR-001, rimosso. Vedi il limite documentato nella sezione 2.

### Non implementato
- **Rilevare i campi letti-ma-non-consumati** (la lista storica di KI #25): impossibile per
  costruzione da un gate sui dati — vedi la nota in sezione 2. Resta l'annotazione manuale (A9).
- **Nested keys**: il gate campi fantasma copre solo il primo livello (non `stats`,
  `attach_points`, `geometry[]`...).
- **Doppio click = apri file/campo** nel pannello editor: oggi la riga mostra file, problema e
  correzione, ma non naviga. Utile, non essenziale al gate.

## Interconnessioni
Legge il `DefinitionRegistry` (doc 03) · emette via telemetria (doc 21) · usata dagli editor
(doc 01/03) · prerequisito sano per 14_ClassSystem (una classe con loadout invalido deve
fallire subito) e per 25/27/28, che aggiungono nuovi tipi di riferimento incrociato.

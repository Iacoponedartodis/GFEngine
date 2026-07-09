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

## 6. Testing e verifica

Non esistono test automatici che bloccano le modifiche (12_TestingStrategy). Ogni modifica
significativa richiede almeno:
- Build pulita (`cmake --build build/windows-debug --config Debug`, 0 errori).
- Lo smoke test manuale rilevante da 12_TestingStrategy.md (liste editor senza duplicati,
  cross-reference dei dropdown risolte, partita/sandbox avviata senza crash, viewport
  corretto).
Dichiara sempre esplicitamente quali di questi hai eseguito e quali restano da verificare
manualmente dallo sviluppatore.

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
# 14 — Class System

**Status: MISTO — leggere con attenzione, le due metà sono in stati diversi.**

| Metà | Stato | Dove |
|---|---|---|
| **Classe per gli NPC** (cloni alleati, nemici) | **Current Implementation** — verificata sul codice live | qui sotto, §Metà NPC |
| **Classe per il GIOCATORE** (livelli, XP, perk) | **Planned Feature** — zero righe di codice | qui sotto, §Metà giocatore → doc 27 |
| **Specializzazioni** (ARC, Commando) | **Planned Feature** — nemmeno progettata | ADR-022 §5 |

> **Riscritto integralmente il 2026-07-17 su base ADR-022 (Accepted).**
> La stesura precedente modellava la classe come *"un pacchetto di armi assegnabile a `PlayerDef`"*
> e in Out of Scope **vietava** di legarla al comportamento IA. Entrambe le cose sono superate:
> il GDD (cap. 12) apre con *"professioni militari, **non semplici categorie di armi**"*, e
> ADR-022 **è** l'ADR separato che quel divieto richiedeva. Il vecchio doc si dichiarava anche
> *"not yet implemented"* mentre la metà NPC era già in produzione: una doc che mente sullo stato
> è peggio di una doc assente, perché viene creduta.

## Overview
Una **Classe** è una **professione militare**: cosa un clone *sa fare*, non cosa *impugna*.
Il modello ha **tre parti** (ADR-022), e confonderle è l'errore storico di questo sistema:

| Parte | Chi | Come si ottiene | Stato |
|---|---|---|---|
| **Classe NPC** | cloni alleati, nemici | **istanziata**: l'unità referenzia una classe | **implementata** |
| **Classe giocatore** | il personaggio | **si livella giocando** — mai scelta | pianificata (doc 27) |
| **Specializzazione** | il personaggio | **sbloccata** da obiettivi specifici — non si livella | non progettata |

`ClassDef` è **una sola definizione usata in due modi**. Tenerne una sola è una decisione
(ADR-022 §1): due definizioni gemelle divergerebbero.

## Problem Solved
Senza classi, "che unità è questa" si esprime ripetendo `weapons[]` + `ai_profile` + `abilities[]`
su ogni entità. Due unità che condividono il corpo e differiscono per mestiere diventano **due
entità**, e la composizione di squadra del GDD 12.3 (Trooper+Heavy+Recon+Engineer+Leader che si
comportano **diversamente**) non è esprimibile.
Caso reale nei dati: `Clone Trooper` e `Heavy Clone Trooper` hanno **la stessa mesh, la stessa
scala e lo stesso `hitbox_profile`** — sono **un corpo e due professioni**, modellate come due
entità perché le classi non c'erano.

## Metà NPC — Current Implementation (verificata sul codice live)

### Schema `ClassDef` (`include/mini/game/data/Definitions.hpp`)
| Campo | JSON | Note |
|---|---|---|
| `id` | *filename stem* | ADR-001: **mai** scritto dentro il JSON |
| `name` | `name` | nome visualizzato |
| `primaryWeaponId` | `primary_weapon` | dropdown da `registry.weapons()` |
| `secondaryWeaponId` | `secondary_weapon` | dropdown, opzionale |
| `abilityIds[]` | `abilities` | dropdown da `registry.abilities()` |
| `aiProfileId` | `ai_profile` | **è ciò che rende la classe una professione** (ADR-022 §2) |
| `role` | `role` | tag descrittivo — **nessun sistema lo consuma** (vedi Debiti) |

### Come si applica (`ConquestMode::resolveUnitArchetype`)
Un'unità (`EnemyDef`, che è la stessa struct per alleati e nemici — `team=1` vs `team=2`)
referenzia una classe col campo `class`. Alla risoluzione dell'archetipo:

```
se l'unità ha class:
    arma        ← classe (se valorizzata)
    profilo AI  ← classe (se valorizzata)
    abilità     ← classe (se valorizzate)
```

**Ogni campo della classe vince solo se è valorizzato**: un'unità può referenziare una classe e
tenersi comunque una particolarità. Nessuna classe → tutto come prima: il sistema è **additivo**.
Vale **identicamente per alleati e nemici** (`resolveUnitArchetype(registry, allyId, 1)`).

### Cosa NON prende dalla classe, e perché
**`hitbox_profile` resta sempre dell'entità**: è il **corpo**, non il mestiere. Un Heavy e un
Trooper con lo stesso modello hanno la stessa sagoma colpibile — è esattamente la separazione che
rende `Clone Trooper`/`Heavy Clone Trooper` un'entità sola con due classi.
Idem mesh, scala, attach point, fazione.

### Raffinazione — ADR-023 (Accepted/in force, 2026-07-19)
**Implementata.** Il modello sopra era **incompleto**: era l'**entità** a referenziare una classe (`classId`), quindi
l'entità resta il tipo-unità nei roster e "un corpo, molte professioni" NON è ancora esprimibile
senza duplicare entità (è il motivo per cui `Heavy Clone Trooper` è ancora un'entità separata).
**ADR-023** rovescia la relazione per la metà NPC: la **classe** guadagna `baseEntityId` (il corpo da
cui prende modello/hitbox/stat-base) + **moltiplicatori** di stat (`hpMult`/`speedMult`/`damageMult`);
i roster referenziano **classi** come tipo-unità; le **entità si riservano ai corpi veri**
(B1/B2/Droideka). Heavy/Sniper/Medic diventano classi del corpo clone. **Fatto**: `Heavy Clone
Trooper` e `B1 Heavy Battle Droid` sono ora **classi** (corpo `Clone Trooper`/`B1 Battle Droid`); le
entità ridondanti sono eliminate; i roster referenziano classi. Vedi ADR-023 per il modello completo.

### Authoring
Modulo **Classi** dell'editor (dropdown-only per arma/abilità/profilo AI, ADR-010 RMW per il save).
L'**Entity Editor** mostra il selettore di classe in cima al tab "Statistiche" e marca
`[deciso dalla CLASSE]` i campi che la classe sovrascrive — perché un campo editabile che viene
ignorato a runtime è KI #25 daccapo.

### Verificato dal gate (ADR-018)
Riferimenti rotti su arma primaria/secondaria, profilo AI e abilità; classe inesistente
referenziata da un'unità; arma primaria assente; primaria == secondaria.

## Metà giocatore — Planned Feature (Fase 3, doc 27)

**Il giocatore NON sceglie una classe.** Ogni classe esiste **contemporaneamente** per lui e si
**livella** con azioni coerenti (GDD 11.3: *"un'identità che emerge dal comportamento"*).
Salendo di livello si sbloccano **perk**. Il gameplay decide quali classi crescono.

### Decisione applicata il 2026-07-17: il selettore è stato RIMOSSO
La riga **"Classe"** del PreMatch faceva scegliere una classe al giocatore — cioè esattamente ciò
che GDD 11.3 nega. Faceva anche danno concreto: **sovrascriveva in silenzio** le righe
"Arma primaria/secondaria" dello stesso menu (due posti che decidono lo stesso dato, uno vince
senza dirlo). **Rimossa**, insieme a `setClassList`/`getSelectedClassId`/`ClassEntry`: senza i
metodi la regola è **strutturale**, non una convenzione da ricordare.
Il loadout del giocatore **sono** le righe Arma primaria/secondaria, che esistevano già: non si è
persa nessuna funzione.
`MatchSettings.classId` **sopravvive** come **override di test** via `--class <id>`, dichiarato tale
nel codice e annunciato in telemetria. Non è una scelta offerta al giocatore.

### Scope della metà giocatore (da implementare in doc 27, non qui)
- `classXp[classId]` + livello per classe; **nessuna classe "attiva"**.
- XP da **azioni coerenti**, con gli assi già scritti nel GDD 11.3: Heavy (armi pesanti,
  distruzione mezzi, difesa posizioni) · Medic (cure, rianimazioni) · Recon (ricognizione,
  eliminazioni precise) · Engineer (hacking, sabotaggio).
- Perk sbloccati per livello.
- **Fondamenta già pronte**: `World::missionStats` (kill, obiettivi, tempo, perdite) e
  `tier`/`type` sugli obiettivi rendono "completare obiettivi di un certo tipo" **già osservabile**.
- **Vincolo non negoziabile ereditato da doc 27**: un run kill-focused non deve superare un run
  objective-focused. L'XP legge gli **obiettivi**, non il combat — così il pilastro è imposto
  dall'architettura, non sperato dal bilanciamento.

## Out of Scope
- **Perk, XP e livelli**: sono doc 27. Qui si definisce la classe, non la carriera.
- **Specializzazioni** (ARC, Commando): terzo asse, `SpecializationDef`, sbloccato da obiettivi e
  **senza livelli** (ADR-022 §5). Non progettarlo prima dei perk.
- **Persistenza**: doc 28.
- **Aspetto/variazioni di armatura**: previsto dal modello (*"e in caso l'aspetto"*), non ancora
  nello schema. Additivo quando servirà.
- **`role` come enum tattico**: finché nessun sistema lo consuma resta un tag descrittivo.

## Debiti noti di questo sistema
- **`role` è un campo fantasma di secondo tipo**: il loader lo legge, l'editor lo scrive
  (`InputText` libero), **nessun sistema lo consuma**. È la classe di problema di KI #25, che il
  gate di validazione **non può vedere** (guarda il registry, non i consumatori). Diventerà un enum
  quando la metà NPC userà i ruoli tattici (ADR-022, Consequences).
- **`abilityIds` per il giocatore non ha effetto**: le abilità del giocatore non le applica nessuno
  (KI #32). È un problema a valle, non di questo sistema.

## Dependencies
`DefinitionRegistry` · `WeaponDef`/`AbilityDef`/`AiProfileDef` (per id) · `ConquestMode`
(risoluzione archetipo) · editor (modulo Classi, Entity Editor).

## Acceptance
- [x] `ClassDef` con `ai_profile`: la classe determina il **comportamento**, non solo il loadout.
- [x] Un'unità referenzia una classe invece di ripetere loadout+profilo+abilità.
- [x] Vale per alleati **e** nemici.
- [x] Il corpo (mesh, hitbox) resta dell'entità.
- [x] Il giocatore non può scegliere una classe da nessuna UI.
- [ ] Una squadra multi-classe si comporta **osservabilmente** diversamente da una monoclasse
      (GDD 12.3) — richiede che il contenuto assegni le classi.
- [ ] Metà giocatore (XP/livelli/perk) → doc 27.

## Interconnessioni
ADR-022 (il modello) · doc 27 (metà giocatore; il suo criterio #1 è *"14 implementato prima"* — la
**metà NPC** lo soddisfa) · doc 26 (composizione di squadra) · doc 16 (profili AI) · doc 24 (gate) ·
GDD cap. 12 (cosa sono) e 11.3 (come si ottengono — **due capitoli**, ADR-022 Vincolo di metodo).

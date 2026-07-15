# 27 — Progression & Career (Planned Feature)

**Status: Planned Feature — non ancora implementato in codice. Fase 3.**
Descrive un sistema che NON esiste. **Non iniziarlo prima che 14_ClassSystem sia implementato**
e che il livello tattico (25/26) sia solido: la Vision è esplicita — la progressione è Fase 3,
dopo il core e il sistema tattico. Vedi ADR-021.

## Overview
La carriera del clone: da recluta a veterano a figura di comando. **Tre assi indipendenti**
(non una barra di livello):

| Asse | Rappresenta | Determina |
|---|---|---|
| **Grado** | Posizione nella gerarchia GAR | Livello di comando, responsabilità, accesso a missioni |
| **Classe** | Funzione sul campo (doc 14) | Loadout, comportamento, contributo alla squadra |
| **Specializzazione** | Percorsi d'élite (ARC, Commando) | Capacità uniche, equipaggiamenti avanzati |

Separarli permette di essere *Sergente (grado) + Heavy (classe) + ARC in sblocco
(specializzazione)* — tre dimensioni ortogonali, non tre nomi per la stessa cosa.

## Problem Solved
Il pilastro #1 del GDD ("sei un clone qualunque che cresce") non ha oggi alcun supporto. Ma il
motivo per cui questo doc è **Fase 3 e non prima** è strutturale: la progressione si aggancia
alla **Classe**, e la classe non esiste ancora (doc 14, KI #10). Costruire la carriera adesso
significherebbe agganciarla a `weaponIds[]` — cioè rifare il lavoro dopo.

## Goal
Il grado deve cambiare **come si gioca** (più comando), non solo un numero che sale.

## Scope
- `ProgressionDef` / `RankDef`: soglie XP, requisiti, sblocchi (classi, specializzazioni, equip).
- **Gerarchia GAR canonica** (doc 23) — il grado mappa su un livello di comando reale:
  Trooper (1) → Caporale (fireteam ~4) → Sergente (squadra 9) → Tenente (plotone 36) →
  Capitano (compagnia 144) → Maggiore/Comandante (battaglione 576 / reggimento 2.304) →
  Marshal Commander (corpo) → gradi alti = narrativi/astratti.
- **Valutazione di fine missione** su tre dimensioni: efficienza militare (obiettivi, tempo,
  risorse), prestazione tattica (uso della squadra, scelte), capacità individuale.
- **XP per classe** (progressione diegetica del GDD: "diventi ciò che giochi") — usare spesso
  armi pesanti fa crescere l'Heavy.
- Sblocco del **livello di comando** (doc 26) dal grado.

## Regola non negoziabile: gli obiettivi contano più delle uccisioni
Il pilastro #3 del GDD diventa qui un **criterio di accettazione misurabile**, non un'aspirazione:
> Un run kill-focused NON deve superare un run objective-focused.

L'XP pesa: completamento obiettivi, successo missione, supporto alla squadra, sopravvivenza degli
alleati, decisioni tattiche efficaci. Le kill sono un mezzo, non il punteggio. Se il
bilanciamento non regge questo test, il sistema è sbagliato — non il test.

## Out of Scope
- Galactic Conquest / stato della guerra (Fase 4/5) — qui solo la carriera personale.
- Specializzazioni d'élite complete (ARC/Commando): definire lo *slot*, non il contenuto.
- Persistenza: è doc 28. Questo doc definisce **cosa** progredisce, non come si salva.
- Qualunque lavoro prima che 14 sia implementato e 25/26 siano solidi.

## Architecture
Definizioni pure nel `DefinitionRegistry` (stesso layer di 14). La valutazione consuma gli eventi
di fine missione dagli obiettivi (doc 25) e dalla squadra (doc 26) — **non** legge il combat
direttamente: altrimenti l'XP diventa inevitabilmente kill-based, per costruzione.

## Technical Decisions
- **Perché tre assi e non un livello:** un solo numero collasserebbe grado/classe/specializzazione
  e renderebbe impossibile la fantasia del GDD.
- **Perché la valutazione legge gli obiettivi e non le kill:** vedi sopra — è una scelta
  architetturale che *impone* il pilastro invece di sperarlo dal bilanciamento.

## Acceptance
- [ ] 14_ClassSystem implementato **prima** di iniziare.
- [ ] I tre assi sono separati nei dati; nessuno è un rinominare di un altro.
- [ ] Un run kill-focused non supera un run objective-focused (test misurabile in `--sim`).
- [ ] Il grado sblocca ampiezza di comando (doc 26), non solo numeri.
- [ ] Grafo degli sblocchi aciclico (gate di validazione, doc 24).

## Interconnessioni
Dipende da 14 (classe) e 25/26 (da cui riceve la valutazione) · sblocca il comando (26) ·
serializzata da 28 · realizza i pilastri #1 e #3 del GDD (doc 23) · Fase 3 della Vision.

# 43 — Scalabilità del rendering e simulazione a distanza (Planned Feature)

> **Stato: PIANIFICATO — zero righe di codice.** Documento di scope, non di implementazione
> (CLAUDE.md §5). Nasce da una causa **misurata**, non da un'intuizione: vedi KI #87.

## Overview

Il gioco è limitato dal **rendering**, non dalla simulazione. Misurato il 2026-08-04 su Training
Ground: scena 3D **95,1% del frame** (38,6 ms), intera simulazione **2,9%** (1,17 ms). La causa
diretta è il volume di vertici — **1,45 milioni per frame** su 205 draw call — amplificata dal
rendering client-side-array (ADR-003), che senza VBO rispedisce i vertici alla GPU a **ogni** draw
call. Il singolo mesh del B1 Battle Droid (**161.304 vertici**, dieci volte il Clone Trooper) vale
da solo i due terzi del traffico.

Questo documento raccoglie le direzioni decise con l'utente il 2026-08-04 e ne fissa lo scope.

## Goal

Rendere il costo per unità in campo **basso e prevedibile**, così che il numero di AI simultanee sia
limitato dal design tattico e non dal numero di vertici di un modello.

## Problem Solved

1. **Ogni AI aggiunta costa 161k vertici/frame.** Il tetto al numero di unità è oggi un fatto
   accidentale di un asset, non una scelta.
2. **Si disegna tutto, sempre**: 205 mesh disegnate su 206 esaminate — nessun frustum culling,
   nessuna soglia di distanza.
3. **Non esiste una nozione di "lontano"**: un'unità a 60 m costa quanto una a 3 m, sia in
   rendering sia in simulazione.

## Scope

**R1 — LOD degli asset (contenuto, non motore).** Versioni semplificate delle mesh, con almeno un
livello "bot/lontano" molto più leggero. È il guadagno maggiore al rischio minore e non tocca il
motore. Richiede: un campo LOD nelle definizioni + selezione per distanza al draw.

**R2 — Frustum culling.** Non disegnare ciò che è fuori dal cono della camera. Guardia già pronta:
il funnel di rendering espone `entita_esaminate` vs `mesh_disegnate` — oggi 206/205, e il rapporto
dirà subito se il culling funziona.

**R3 — Soglia di distanza per il disegno**, con LOD a scaglioni (vicino / medio / lontano).

**R4 — Simulazione a distanza ("AI LOD").** Unità lontane dal giocatore e non in vista aggiornano la
loro logica a cadenza ridotta, mantenendo comportamento plausibile. **Attenzione**: la simulazione è
il 2,9% del frame — questo NON è un lavoro di performance oggi, ma di *scalabilità futura* (mappe
grandi, molti fronti). Va fatto dopo R1-R3 e solo se il profilo lo giustifica.

## Out of Scope (per ora)

- **Passaggio a VBO / revisione di ADR-003.** È l'**ultima risorsa**: il client-side-array è un
  workaround deliberato per il driver Intel di questa macchina. Decisione dell'utente (2026-08-04):
  *per adesso va bene che la build sia ottimizzata per il PC su cui lavoriamo*; la compatibilità
  universale su Windows è un obiettivo successivo e separato.
- Ombre, illuminazione, post-processing: non sono la causa misurata.
- Instancing / batching: prematuro finché un singolo mesh vale due terzi del traffico.

## Dependencies

- **Osservabilità già pronta** (ADR-050): il funnel di rendering (`entita_esaminate`,
  `mesh_disegnate`, `draw_call`, `vertici_per_frame`), le zone `render.scena` / `render.ui` e
  l'`inventario asset` sono le guardie con cui si misura ogni passo. Nessun lavoro qui va fatto
  senza confrontare quei numeri prima e dopo.
- **R1 dipende dagli asset**, quindi dall'utente (Blender). Il motore deve solo saper scegliere il
  livello.
- Relazione con ADR-047 (pipeline Blender): il LOD è un secondo output della stessa pipeline.

## Contesto di macchina — da tenere presente in ogni misura

L'utente ha dichiarato (2026-08-04) che **il PC attuale è molto vecchio e verrà sostituito a breve**.
Conseguenze operative, non trascurabili:
- I numeri assoluti di questo documento (38,6 ms di scena) sono **specifici di questa macchina**.
  I *rapporti* (chi domina il frame, quanti vertici) restano validi; i millisecondi no.
- Non si progetta l'architettura attorno ai limiti di una macchina in uscita. R1-R3 sono giusti in
  assoluto — un mesh da 161k vertici per un fante è sbagliato ovunque — ma la **fretta** con cui
  affrontarli va pesata sapendo che l'hardware cambierà.
- Quando la macchina cambia, **ri-misurare prima di concludere**: `--sim-ticks 4200` e confronto
  dell'ultima finestra del profilo.

# 18 — AI Map Consumption: l'AI usa i Map Metadata (Planned Feature)

Status: **Implementato (scope 1-4)** — 2026-07-10. Le voci in Out of Scope restano
pianificate (pose alle coperture → Todo #24; route multi-segmento; pathfinding).
Template come 14/15/16/17.
Chiude il cerchio tra 15_MapMetadata (dati, implementato) e 16_AiBehavior
(comportamento tattico, implementato): questo documento definisce il PRIMO
consumo runtime dei metadata, in forma minima e verificabile.

## Overview
15_MapMetadata ha dato alle mappe cover point, patrol route e danger zone
autorabili; 16_AiBehavior ha dato alle AI un vocabolario tattico (peek/hide,
aggression, ritirata). Oggi i due sistemi non si parlano: la fase "hide" è solo
uno strafe evasivo e le pattuglie sono generate proceduralmente verso i post.

## Goal
Ogni metadato autorato nel Map Editor deve avere un effetto osservabile sul
comportamento AI nella simulazione sandbox, senza riscrivere l'AiSystem.

## Problem Solved
- L'autore piazza cover/route/danger e non succede nulla → i metadata sembrano
  decorativi e non si può iterare sul level design tattico.
- "Hide" senza copertura vera = l'AI balla sul posto invece di ripararsi.

## Scope
1. **Canale dati**: `World::activeMap` (puntatore opaco a `MapDef`, settato dai
   game mode in `start()`, azzerato da `World::initialize()`). L'ECS non
   include header di gioco: forward declaration, pattern mailbox come
   `combatFeedback`/`eventFeed`.
2. **Cover point in ingaggio**: entrando in fase evasiva (hide), l'AI cerca il
   cover point più vicino (≤ ~12m) il cui fronte guarda verso il nemico; se lo
   trova si muove lì e ci resta finché non torna in peek. Senza cover: strafe
   evasivo attuale (fallback). `height` per ora non cambia la posa (niente
   crouch nel runtime): influenzerà solo la scelta quando esisteranno le pose.
3. **Danger zone nel movimento**: in Patrol/Hunt/Search il movimento riceve una
   repulsione dalle danger zone (pesata su `dangerLevel` e distanza). In Alert
   non si applica: sotto ingaggio si combatte.
4. **Patrol route come pattuglie**: se la mappa ha `patrolRoutes`, ConquestMode
   assegna alle unità segmenti consecutivi delle route (round-robin) al posto
   dei waypoint procedurali verso i post. Limite attuale documentato:
   `AiComponent` supporta due waypoint (A/B) → ogni unità pattuglia UN segmento
   della route, non l'intera sequenza.

## Out of Scope (per ora)
- Pose alle coperture (crouch, mira da copertura, peek-over vs peek-around
  guidati da `height`): richiedono animazioni/pose che il runtime non ha —
  annotato anche in 15_MapMetadata Future Expansion (richiesta utente
  2026-07-10).
- Pathfinding: il movimento resta steering diretto + anti-stuck; niente navmesh.
  *Limite osservato dall'utente (2026-07-10):* con un ostacolo tra due punti di una
  route l'AI inverte semplicemente la marcia (anti-stuck) invece di aggirarlo — si
  risolve solo con un vero pathfinding, non con altri hint.
- Route multi-segmento per singola unità (richiede lista waypoint in
  AiComponent — estensione futura naturale).
- Pesatura danger zone nella SCELTA delle destinazioni (solo repulsione locale).

## Dependencies
- 15_MapMetadata (dati, fatto), 16_AiBehavior (stati AI, fatto),
  ConquestMode spawn (waypoint), simulazione sandbox (verifica, 17_SandboxTools).

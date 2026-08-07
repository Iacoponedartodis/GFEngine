#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include <imgui.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <glm/glm.hpp>

namespace editor
{

// ── STRATO DI EDITING DEL VIEWPORT (doc 52 F1, correzione ad ADR-049) ───────
//
// ADR-049 diceva *"il viewport resta FreeCameraViewport: già condiviso, non si
// tocca"*. Vero per la CLASSE, falso per la CAPACITÀ: il viewport sa fare
// ray-picking e disegnare il gizmo, ma tradurre un clic in una selezione e un delta
// in una modifica lo riscriveva ogni modulo. Da lì la divergenza segnalata
// dall'utente — *"la viewport del map editor è evidentemente molto più avanti di
// quella dell'editor strutture"*.
//
// ── IL CONFINE, dichiarato ───────────────────────────────────────────────────
// Questo componente instrada EVENTI. **Non decide semantica.**
// Il Map Editor, ruotando un gruppo, fa ORBITARE ogni elemento attorno al
// baricentro comune (altrimenti un edificio "gira sul posto" pezzo per pezzo); il
// tab strutture ruota la singola parte; l'Entity Editor lavora in model space. Sono
// tre politiche diverse, e tutte e tre giuste per il loro modulo.
// Portarle qui dentro avrebbe fatto di questo file un contenitore di casi
// particolari — cioè il framework rigido contro cui mette in guardia ADR-049.
// Perciò le operazioni ricevono **l'intera selezione** e il modulo decide cosa
// significhi: qui si sa solo QUANDO è successo qualcosa, non COSA voglia dire.
//
// Nessuna callback sopravvive alla chiamata: le funzioni si passano a `tick()` e
// valgono solo lì. Stessa disciplina di `UndoStack`, e per lo stesso motivo (un
// `std::function` che cattura un elemento di `vector` diventa un puntatore
// penzolante alla prima riallocazione).
class ViewportEditing
{
public:
    // I codici sono quelli che il modulo ha messo nei `pickId` dei box: il loro
    // significato lo decide lui (il Map Editor usa intervalli negativi per tipo,
    // il tab strutture l'indice della parte).
    struct Ops
    {
        // Il codice è selezionabile? Sostituisce un "0 <= i < count", che presume
        // codici contigui — presunzione falsa nel Map Editor.
        std::function<bool(int)> valid;
        // Dove sta il gizmo per questa selezione (il baricentro, di norma).
        std::function<glm::vec3(const std::vector<int>&)> anchor;
        std::function<void(const std::vector<int>&, const glm::vec3&)> move;
        std::function<void(const std::vector<int>&, const glm::vec3&)> rotate;
        std::function<void(const std::vector<int>&, const glm::vec3&)> scale;
        // Chiamata UNA volta all'inizio di un gesto: è lì che il modulo fotografa
        // per l'annullamento. Separata dalle altre perché il gizmo produce un delta
        // per frame, e fotografare a ogni delta riempirebbe la pila.
        std::function<void()> beginGesture;
    };

    // Ritorna true se lo stato del modulo è cambiato. `selection` entra ed esce: il
    // componente non la possiede, la aggiorna.
    bool tick(FreeCameraViewport& vp, std::vector<int>& selection, const Ops& ops)
    {
        bool changed = false;
        bool gestureOpened = false;
        auto openGesture = [&]() {
            if (!gestureOpened && ops.beginGesture) { ops.beginGesture(); gestureOpened = true; }
        };

        // 1. Selezione dal viewport.
        int picked = 0;
        if (vp.popClickedMapBox(picked) && (!ops.valid || ops.valid(picked)))
        {
            const bool additive = ImGui::GetIO().KeyCtrl;
            if (additive)
            {
                auto it = std::find(selection.begin(), selection.end(), picked);
                if (it == selection.end()) selection.push_back(picked);
                else                       selection.erase(it);
            }
            else if (selection.size() != 1 || selection[0] != picked)
            { selection.assign(1, picked); }
            changed = true;
        }

        // Codici diventati non validi (elementi cancellati altrove) escono da soli:
        // una selezione che punta a ciò che non c'è più è la sorgente naturale dei
        // "ha spostato l'elemento sbagliato".
        if (ops.valid)
        {
            const auto before = selection.size();
            selection.erase(std::remove_if(selection.begin(), selection.end(),
                                           [&](int c) { return !ops.valid(c); }),
                            selection.end());
            if (selection.size() != before) changed = true;
        }

        if (selection.empty())
        {
            vp.setGizmoTarget({0, 0, 0}, false);
            return changed;
        }

        // 2. Il gizmo. La fotografia si prende all'INIZIO del gesto.
        if (vp.gizmoDragging()) openGesture();

        glm::vec3 d;
        if (ops.move && vp.popGizmoDelta(d))
        { openGesture(); ops.move(selection, d); changed = true; }

        glm::vec3 rd;
        if (vp.popGizmoRotDelta(rd) && ops.rotate)
        { openGesture(); ops.rotate(selection, rd); changed = true; }

        glm::vec3 sd;
        if (vp.popGizmoScaleDelta(sd) && ops.scale)
        { openGesture(); ops.scale(selection, sd); changed = true; }

        // 3. Il gizmo segue sempre la selezione, anche quando la muove altro (un
        //    campo numerico, un annullamento).
        if (ops.anchor) vp.setGizmoTarget(ops.anchor(selection), true);
        return changed;
    }
};

} // namespace editor

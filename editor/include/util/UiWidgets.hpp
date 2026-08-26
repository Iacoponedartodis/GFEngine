#pragma once
// Widget UI condivisi tra i moduli dell'editor.
// sliderRow: riga "slider + campo numerico + etichetta" — permette regolazione
// rapida (slider) mantenendo l'inserimento numerico preciso (drag/doppio click).

#include <imgui.h>

namespace editor::ui
{

// ── LO SLIDER È STATO TOLTO (2026-08-11, richiesta dell'utente) ─────────────
//
// Il nome resta `sliderRow` perché lo chiamano ~70 punti in cinque moduli, e
// cambiarlo ovunque sarebbe stato un rinominare senza guadagno. Quello che disegna
// è cambiato: **etichetta + campo numerico**, niente slider.
//
// Perché. Gli slider erano nati per vedere l'effetto di un valore in tempo reale
// senza scrivere numeri a mano. Da quando c'è il gizmo quel mestiere è del gizmo, e
// lo slider era rimasto solo con i suoi difetti: mappato su un intervallo largo
// (0,1 → 120 m) dentro poche decine di pixel, un pixel valeva più di un metro,
// quindi si muoveva a scatti grossi e irregolari. Testuale: *"non posso mettere una
// box larga 2.40, ma solo 2.50 … la funzione slider non ci serve più"*.
//
// Il campo di trascinamento non ha intervallo mappato: la velocità è in unità al
// pixel, quindi la precisione non dipende da quanto è largo il pannello. E il
// **Ctrl+clic** apre la digitazione diretta — è il modo con cui si mette un valore
// esatto, e va detto (vedi il suggerimento).
//
// `vmin`/`vmax` restano e restano CLAMP: molti sono pavimenti fisici (le soglie del
// navmesh), non preferenze. Cambiano solo di ruolo: prima erano gli estremi di una
// corsa, adesso sono limiti.
//
// ADATTIVA: sotto una certa larghezza l'etichetta va sopra invece di essere tagliata
// dal bordo. In Dear ImGui non esiste layout a vincoli: il ramo si scrive a mano
// interrogando `GetContentRegionAvail`.
inline bool sliderRow(const char* label, float& v,
                      float vmin, float vmax, float dragSpeed,
                      const char* fmt = "%.3f", float labelW = 58.0f)
{
    bool changed = false;
    ImGui::PushID(label);
    const float avail = ImGui::GetContentRegionAvail().x;
    // Il testo visibile è quello prima di "##": è quello che deve starci.
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    const float textW = ImGui::CalcTextSize(label, end).x;

    auto field = [&](float w) {
        if (w < 50.0f) w = 50.0f;
        ImGui::SetNextItemWidth(w);
        if (ImGui::DragFloat("##dg", &v, dragSpeed, vmin, vmax, fmt)) changed = true;
        // Il modo per mettere un valore ESATTO non si indovina: si dice.
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Trascina per regolare · Ctrl+clic per scrivere il valore esatto"
                              "\nlimiti: %.2f … %.2f", vmin, vmax);
    };

    // Serve spazio per l'etichetta e per un campo ancora usabile; sotto, si impila.
    if (avail < textW + 90.0f)
    {
        ImGui::TextUnformatted(label, end);
        field(avail);
    }
    else
    {
        const float lw = (textW > labelW) ? textW : labelW;
        ImGui::TextUnformatted(label, end);
        ImGui::SameLine(0, 4);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (lw - textW));
        field(ImGui::GetContentRegionAvail().x);
    }
    ImGui::PopID();
    return changed;
}

// ── Splitter di pannello (2026-08-06) ────────────────────────────────────────
// `ImGuiChildFlags_ResizeX` mette il grip sul bordo DESTRO del child. Quando quel
// bordo coincide col bordo della finestra — cioè per ogni pannello di destra — una
// volta stretto non c'è più nulla da afferrare e il pannello non si riallarga più.
// Il Map Editor l'aveva già risolto con uno splitter esplicito a SINISTRA; qui la
// soluzione diventa condivisa, così non va riscoperta modulo per modulo.
// Da chiamare PRIMA del pannello di destra, fra `SameLine()`.
inline void panelSplitter(const char* id, float& width, float height,
                          float minW = 180.0f, float maxW = 0.0f)
{
    ImGui::InvisibleButton(id, ImVec2(6.0f, height));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
        width -= ImGui::GetIO().MouseDelta.x;   // trascinando a sinistra si ALLARGA
    if (width < minW) width = minW;
    if (maxW > 0.0f && width > maxW) width = maxW;
}

// Riga "etichetta a sinistra + campo che riempie il resto". L'etichetta è
// disegnata PER PRIMA, quindi non viene mai tagliata dal bordo del pannello
// (il difetto di DragFloat(label) a piena larghezza, che disegna l'etichetta
// a destra e la fa uscire dal pannello). Ritorna true se il valore cambia.
inline bool dragRow(const char* label, float& v, float speed,
                    float lo, float hi, const char* fmt = "%.2f")
{
    ImGui::PushID(label);
    // Mostra solo la parte prima di "##" (id ImGui), come nei widget nativi.
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextUnformatted(label, end);
    ImGui::SameLine();
    float w = ImGui::GetContentRegionAvail().x;
    if (w < 60.0f) w = 60.0f;
    ImGui::SetNextItemWidth(w);
    const bool ch = ImGui::DragFloat("##v", &v, speed, lo, hi, fmt);
    ImGui::PopID();
    return ch;
}

// Come dragRow ma con uno SLIDER. **È l'unico posto in cui lo slider è rimasto**,
// e di proposito: qui i valori sono FATTORI normalizzati (aggressività, precisione,
// preferenza di copertura… tutti 0..1). Su un intervallo così stretto uno slider è
// il controllo giusto — un pixel vale 0,005 — e il difetto che ha fatto togliere gli
// altri (intervallo largo schiacciato in pochi pixel, quindi scatti grossi) qui non
// esiste. Per il valore esatto c'è comunque **Ctrl+clic**, che ora è scritto nel
// suggerimento invece di doverlo sapere.
inline bool sliderRowLR(const char* label, float& v, float lo, float hi,
                        const char* fmt = "%.2f")
{
    ImGui::PushID(label);
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextUnformatted(label, end);
    ImGui::SameLine();
    float w = ImGui::GetContentRegionAvail().x;
    if (w < 60.0f) w = 60.0f;
    ImGui::SetNextItemWidth(w);
    const bool ch = ImGui::SliderFloat("##v", &v, lo, hi, fmt);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Trascina · Ctrl+clic per scrivere il valore esatto (%.2f … %.2f)",
                          lo, hi);
    ImGui::PopID();
    return ch;
}

// Etichetta a SINISTRA + DragInt che riempie il resto. Ritorna true se cambia.
inline bool intRow(const char* label, int& v, float speed, int lo, int hi)
{
    ImGui::PushID(label);
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextUnformatted(label, end);
    ImGui::SameLine();
    float w = ImGui::GetContentRegionAvail().x;
    if (w < 60.0f) w = 60.0f;
    ImGui::SetNextItemWidth(w);
    const bool ch = ImGui::DragInt("##v", &v, speed, lo, hi);
    ImGui::PopID();
    return ch;
}

// Etichetta a SINISTRA + widget che riempie il resto. Helper comune per non
// ripetere lo stesso boilerplate (label, SameLine, larghezza, id nascosto).
namespace detail {
inline void rowLabel(const char* label)
{
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextUnformatted(label, end);
    ImGui::SameLine();
    float w = ImGui::GetContentRegionAvail().x;
    if (w < 60.0f) w = 60.0f;
    ImGui::SetNextItemWidth(w);
}
} // namespace detail

// Combo con etichetta a sinistra (items = array di const char*).
inline bool comboRow(const char* label, int& index, const char* const items[], int count)
{
    ImGui::PushID(label);
    detail::rowLabel(label);
    const bool ch = ImGui::Combo("##c", &index, items, count);
    ImGui::PopID();
    return ch;
}

// InputText con etichetta a sinistra.
inline bool textRow(const char* label, char* buf, size_t bufSize)
{
    ImGui::PushID(label);
    detail::rowLabel(label);
    const bool ch = ImGui::InputText("##t", buf, bufSize);
    ImGui::PopID();
    return ch;
}

// InputFloat con etichetta a sinistra (mantiene il typing preciso, non un drag).
inline bool inputFloatRow(const char* label, float& v, const char* fmt = "%.2f")
{
    ImGui::PushID(label);
    detail::rowLabel(label);
    const bool ch = ImGui::InputFloat("##if", &v, 0.0f, 0.0f, fmt);
    ImGui::PopID();
    return ch;
}

// InputInt con etichetta a sinistra.
inline bool inputIntRow(const char* label, int& v)
{
    ImGui::PushID(label);
    detail::rowLabel(label);
    const bool ch = ImGui::InputInt("##ii", &v);
    ImGui::PopID();
    return ch;
}

// ColorEdit3 con etichetta a sinistra.
inline bool colorRow(const char* label, float* rgb)
{
    ImGui::PushID(label);
    detail::rowLabel(label);
    const bool ch = ImGui::ColorEdit3("##col", rgb,
                        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha);
    ImGui::PopID();
    return ch;
}

// Tripla riga X/Y/Z per un vettore. Ritorna true se un componente è cambiato.
inline bool sliderRow3(const char* baseLabel, float* v,
                       float vmin, float vmax, float dragSpeed,
                       const char* fmt = "%.3f")
{
    bool changed = false;
    ImGui::PushID(baseLabel);
    static const char* axisNames[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i)
    {
        ImGui::PushID(i);
        changed |= sliderRow(axisNames[i], v[i], vmin, vmax, dragSpeed, fmt, 18.0f);
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

// Barra di selezione modalità gizmo: [Sposta][Ruota][Scala].
// Ritorna true se la modalità è cambiata. allowRotate/allowScale disabilitano
// i pulsanti quando il target non supporta la modalità.
template <typename ViewportT>
inline bool gizmoModeBar(ViewportT& vp, bool allowRotate = true, bool allowScale = true)
{
    using Mode = typename ViewportT::GizmoMode;
    Mode cur = vp.getGizmoMode();
    bool changed = false;

    auto modeButton = [&](const char* lbl, Mode m, bool enabled)
    {
        const bool active = (cur == m);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.80f, 1.0f));
        ImGui::BeginDisabled(!enabled);
        if (ImGui::Button(lbl) && !active) { vp.setGizmoMode(m); changed = true; }
        ImGui::EndDisabled();
        if (active) ImGui::PopStyleColor();
    };

    modeButton("Sposta (1)", Mode::Translate, true);           ImGui::SameLine(0, 2);
    modeButton("Ruota (2)",  Mode::Rotate,    allowRotate);    ImGui::SameLine(0, 2);
    modeButton("Scala (3)",  Mode::Scale,     allowScale);

    vp.setGizmoCanRotateScale(allowRotate, allowScale);
    return changed;
}

} // namespace editor::ui

#pragma once
// Widget UI condivisi tra i moduli dell'editor.
// sliderRow: riga "slider + campo numerico + etichetta" — permette regolazione
// rapida (slider) mantenendo l'inserimento numerico preciso (drag/doppio click).

#include <imgui.h>

namespace editor::ui
{

// Ritorna true se il valore è cambiato. labelW = spazio riservato all'etichetta.
inline bool sliderRow(const char* label, float& v,
                      float vmin, float vmax, float dragSpeed,
                      const char* fmt = "%.3f", float labelW = 58.0f)
{
    bool changed = false;
    ImGui::PushID(label);
    const float avail = ImGui::GetContentRegionAvail().x;
    float sliderW = avail - 66.0f - labelW;
    if (sliderW < 40.0f) sliderW = 40.0f;

    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::SliderFloat("##sl", &v, vmin, vmax, fmt)) changed = true;
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##dg", &v, dragSpeed, vmin, vmax, fmt)) changed = true;
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
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

#pragma once
// ── ModuleShell — scheletro di layout comune ai moduli editor (ADR-049) ──────
//
// PERCHÉ ESISTE. Ogni modulo si costruiva il proprio layout, e la deriva è stata
// misurata (doc 39, 2026-08-02): il pannello ridimensionabile c'era in 5 moduli su 7 e
// in uno era pure rotto (`ImGuiChildFlags_ResizeX` mette il grip sul bordo destro, che
// sul pannello ancorato a destra coincide col bordo finestra: una volta stretto non si
// riallargava più). Le correzioni fatte una volta qui valgono per tutti i moduli che
// adottano lo shell — è il punto dell'ADR-049.
//
// COMPOSIZIONE, NON EREDITARIETÀ: non è una classe base da estendere. È un oggetto che
// un modulo tiene come membro e usa per aprire/chiudere i pannelli. I moduli restano
// liberi di essere diversi (viewport 3D, tabelle, form) senza combattere un framework.
//
// USO TIPICO:
//     m_shell.begin(toolbarHeight);
//     if (m_shell.beginList())      { ...lista...;        m_shell.endList(); }
//     if (m_shell.beginContent())   { ...viewport/form...; m_shell.endContent(); }
//     if (m_shell.beginProperties()){ ...proprietà...;    m_shell.endProperties(); }
//     m_shell.end();
//
// Un modulo che non ha uno dei tre pannelli semplicemente non lo apre.

#include <imgui.h>
#include <algorithm>

namespace editor
{

class ModuleShell
{
public:
    // Larghezze iniziali; il pannello proprietà è ridimensionabile dall'utente.
    explicit ModuleShell(float listW = 200.0f, float propW = 260.0f)
        : m_listW(listW), m_propW(propW) {}

    // `reservedTop` = altezza già consumata sopra (toolbar del modulo).
    void begin(float reservedTop = 0.0f)
    {
        m_totalW = ImGui::GetContentRegionAvail().x;
        m_height = ImGui::GetContentRegionAvail().y - reservedTop;
        if (m_height < 80.0f) m_height = 80.0f;
        ImGui::BeginChild("##shell", ImVec2(m_totalW, m_height), ImGuiChildFlags_None);
        m_open = true;
    }

    bool beginList(bool border = true)
    {
        if (!m_open) return false;
        ImGui::BeginChild("##shell_list", ImVec2(m_listW, 0),
                          border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None);
        return true;
    }
    void endList()
    {
        ImGui::EndChild();
        m_listActualW = ImGui::GetItemRectSize().x;
        ImGui::SameLine();
    }

    // Pannello centrale: prende lo spazio che resta. `noScroll` per i viewport 3D,
    // che gestiscono il mouse da sé.
    bool beginContent(bool noScroll = false)
    {
        if (!m_open) return false;
        float w = m_totalW - m_listActualW - m_propW - ImGui::GetStyle().ItemSpacing.x * 2;
        if (w < 120.0f) w = 120.0f;
        m_contentW = w;
        ImGui::BeginChild("##shell_content", ImVec2(w, 0), ImGuiChildFlags_None,
                          noScroll ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
                                   : 0);
        return true;
    }
    void endContent() { ImGui::EndChild(); ImGui::SameLine(); }

    // Pannello destro, ridimensionabile con una maniglia ESPLICITA sul lato sinistro.
    // Mai `ChildFlags_ResizeX` su un pannello ancorato a un bordo (vedi commento in
    // testa): il clamp garantisce che non possa incastrarsi in uno stato irreversibile.
    bool beginProperties(bool border = true)
    {
        if (!m_open) return false;
        ImGui::InvisibleButton("##shell_split", ImVec2(6.0f, m_height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
            m_propW -= ImGui::GetIO().MouseDelta.x;   // trascinando a sinistra si allarga
        const float maxW = (m_totalW > 400.0f) ? m_totalW * 0.5f : 200.0f;
        m_propW = std::clamp(m_propW, 180.0f, maxW);
        ImGui::SameLine();
        ImGui::BeginChild("##shell_props", ImVec2(m_propW, 0),
                          border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None);
        return true;
    }
    void endProperties() { ImGui::EndChild(); }

    void end() { if (m_open) { ImGui::EndChild(); m_open = false; } }

    [[nodiscard]] float contentWidth()    const { return m_contentW; }
    [[nodiscard]] float propertiesWidth() const { return m_propW; }
    [[nodiscard]] float height()          const { return m_height; }

    // Margine finale per il contenuto piazzato con coordinate ASSOLUTE
    // (`SetCursorPos`): senza, l'area scrollabile finisce sull'ultimo elemento e la
    // schermata sembra tagliata — bug reale della home (changelog 106), regola R6.
    static void scrollPadding(float h = 24.0f) { ImGui::Dummy({1.0f, h}); }

private:
    float m_listW = 200.0f, m_propW = 260.0f;
    float m_totalW = 0.0f, m_height = 0.0f;
    float m_listActualW = 0.0f, m_contentW = 0.0f;
    bool  m_open = false;
};

} // namespace editor

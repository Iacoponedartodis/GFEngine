#include "ui/HomeScreen.hpp"
#include <imgui.h>
#include <cstdio>

namespace editor
{

struct ModuleCard
{
    const char*  id;
    const char*  label;
    const char*  description;
    ActiveModule module;
    bool         available;
    float        r, g, b;
};

static const ModuleCard k_modules[] = {
    {"cam",     "Free Camera\nViewport",
     "Naviga la mappa in 3D con\ncamera libera. Posiziona e\nmodifica metadata tattici.",
     ActiveModule::FreeCameraViewport, true,  0.25f, 0.65f, 1.0f},

    {"hitbox",  "Hitbox\nEditor",
     "Modifica le zone di hitbox\nper ogni tipo di personaggio.\nSalva come JSON.",
     ActiveModule::HitboxEditor,       true,  1.0f,  0.55f, 0.2f},  // <-- era false

    {"balance", "Balance\nEditor",
     "Regola parametri di armi,\nnemici e AI con slider.\nSalva preset.",
     ActiveModule::BalanceEditor,      true,  0.4f,  0.9f,  0.4f},  // <-- era false

    {"assets",  "Asset\nManager",
     "Esplora e assegna modelli,\ntexture e materiali alle\ndefinizioni entità.",
     ActiveModule::AssetManager,       false, 0.9f,  0.75f, 0.2f},

    {"ai",      "AI Editor /\nDebugger",
     "Visualizza e modifica profili\nAI. Debug stati, cover, target\ne percorsi in real-time.",
     ActiveModule::AiEditor,           false, 0.85f, 0.3f,  0.9f},

    {"meta",    "Map Metadata\nEditor",
     "Posiziona spawn, cover, patrol\ne zone tattiche sulla mappa.\nSalva come JSON.",
     ActiveModule::MapMetadataEditor,  false, 1.0f,  0.35f, 0.35f},
};
static constexpr int k_moduleCount = 6;

ActiveModule HomeScreen::draw(bool& wantsLaunchGame)
{
    wantsLaunchGame = false;
    ActiveModule selected = ActiveModule::Home;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    constexpr ImGuiWindowFlags f =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##Home", nullptr, f);
    ImGui::PopStyleVar();

    const float W = vp->WorkSize.x;
    const float H = vp->WorkSize.y;

    // ── Titolo ────────────────────────────────────────────────────────
    ImGui::SetCursorPos({W * 0.5f - 90.0f, H * 0.06f});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.75f, 0.30f, 1.0f));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::Text("GFEngine Editor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({W * 0.5f - 60.0f, H * 0.06f + 34.0f});
    ImGui::TextDisabled("v0.1 — Stage 1");

    // ── Pulsante avvia gioco ──────────────────────────────────────────
    const float btnW = 220, btnH = 44;
    ImGui::SetCursorPos({W * 0.5f - btnW * 0.5f, H * 0.14f});
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.50f, 0.20f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.30f, 1.0f));
    if (ImGui::Button("▶  Avvia GFEngine", ImVec2(btnW, btnH)))
        wantsLaunchGame = true;
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos({W * 0.5f - 110.0f, H * 0.14f + btnH + 4});
    ImGui::TextDisabled("(apre la partita con --direct-prematch)");

    // ── Griglia moduli ────────────────────────────────────────────────
    const float cardW  = 220, cardH = 160;
    const float gapX   = 24,  gapY  = 20;
    const int   cols   = 3;
    const float gridW  = cols * cardW + (cols - 1) * gapX;
    const float startX = (W - gridW) * 0.5f;
    const float startY = H * 0.28f;

    for (int i = 0; i < k_moduleCount; ++i)
    {
        const auto& m = k_modules[i];
        int   col = i % cols;
        int   row = i / cols;
        float x = startX + col * (cardW + gapX);
        float y = startY + row * (cardH + gapY);

        ImGui::SetCursorPos({x, y});

        float alpha = m.available ? 0.85f : 0.45f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImVec4(0.10f, 0.12f, 0.18f, alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

        char childId[32]; std::snprintf(childId, sizeof(childId), "##card%d", i);
        ImGui::BeginChild(childId, ImVec2(cardW, cardH), true);

        ImVec2 p = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            {p.x, p.y}, {p.x + cardW, p.y + 4},
            ImGui::ColorConvertFloat4ToU32(ImVec4(m.r, m.g, m.b, alpha))
        );
        ImGui::Dummy({0, 8});

        ImGui::PushStyleColor(ImGuiCol_Text,
            m.available ? ImVec4(0.9f,0.9f,0.95f,1.0f) : ImVec4(0.5f,0.5f,0.55f,1.0f));
        ImGui::TextUnformatted(m.label);
        ImGui::PopStyleColor();

        ImGui::Dummy({0, 4});

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.55f,0.60f,1.0f));
        ImGui::TextUnformatted(m.description);
        ImGui::PopStyleColor();

        if (m.available)
        {
            ImGui::SetCursorPosY(cardH - 32);
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(m.r*0.4f, m.g*0.4f, m.b*0.4f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(m.r*0.6f, m.g*0.6f, m.b*0.6f, 1.0f));
            char btnId[32]; std::snprintf(btnId, sizeof(btnId), "Apri##%d", i);
            if (ImGui::Button(btnId, ImVec2(cardW - 16, 22)))
                selected = m.module;
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::SetCursorPosY(cardH - 28);
            ImGui::TextDisabled("  — in arrivo —");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::End();
    return selected;
}

} // namespace editor
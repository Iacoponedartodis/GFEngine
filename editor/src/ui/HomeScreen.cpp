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

// Ordine per COSA SI FA, non storico. Le descrizioni dicono anche cosa il modulo
// NON fa: la sovrapposizione fra moduli (arma e profilo AI editabili sia
// sull'unità sia sulla classe) è la confusione segnalata dall'utente il
// 2026-07-16 — e un editor che non dice chi decide cosa la alimenta.
static const ModuleCard k_modules[] = {
    // ── Contenuto di gioco ───────────────────────────────────────────
    {"mission", "Missioni e\nObiettivi",
     "Missioni, obiettivi e le loro\nCONSEGUENZE sulla battaglia.\nRegole di vittoria/sconfitta.",
     ActiveModule::MissionEditor,      true,  0.95f, 0.80f, 0.35f},

    {"class",   "Classi",
     "La PROFESSIONE: loadout,\ncomportamento (profilo AI)\ne abilità dei cloni.",
     ActiveModule::ClassEditor,        true,  0.55f, 0.75f, 0.95f},

    {"entity",  "Entity\nEditor",
     "Il CORPO dell'unità: mesh,\nscale, attach point e hitbox.\nArma/AI: le decide la Classe.",
     ActiveModule::EntityEditor,       true,  0.55f, 0.80f, 0.35f},

    {"weapon",  "Weapon\nEditor",
     "Configura armi: mesh, scala,\npunti di attacco, bilanciamento\ne parametri di fuoco.",
     ActiveModule::WeaponEditor,       true,  1.0f,  0.45f, 0.15f},

    {"map",     "Map\nEditor",
     "Geometria, command post,\nspawn e zone tattiche.\nSalva come JSON.",
     ActiveModule::MapEditor,          true,  1.0f,  0.35f, 0.35f},

    {"vehicle", "Vehicle\nEditor",
     "Crea e configura i veicoli:\nmodello, statistiche di guida,\nbox di collisione.",
     ActiveModule::VehicleEditor,      true,  0.85f, 0.55f, 0.2f},

    // ── Strumenti ────────────────────────────────────────────────────
    {"balance", "Balance\nEditor",
     "Slider di bilanciamento:\nprofili AI, mappe, personaggio\ne abilità.",
     ActiveModule::BalanceEditor,      true,  0.4f,  0.9f,  0.4f},

    {"validate","Validazione\ncontenuti",
     "Trova riferimenti rotti, asset\nmancanti e duplicati. Stesse\nregole del gioco.",
     ActiveModule::ContentValidation,  true,  0.95f, 0.45f, 0.45f},

    {"cam",     "Free Camera\nViewport",
     "Naviga la scena in 3D con\ncamera libera. Anteprima\nmodelli e animazioni.",
     ActiveModule::FreeCameraViewport, true,  0.25f, 0.65f, 1.0f},

    {"ai",      "AI Editor /\nDebugger",
     "Debug stati, cover, target e\npercorsi in real-time. I profili\nAI si editano in Balance.",
     ActiveModule::AiEditor,           false, 0.85f, 0.3f,  0.9f},

    {"assets",  "Asset\nManager",
     "Esplora e assegna modelli,\ntexture e materiali alle\ndefinizioni entità.",
     ActiveModule::AssetManager,       false, 0.9f,  0.75f, 0.2f},
};
// Conteggio derivato dall'array: mai più out-of-bounds per una card rimossa.
static constexpr int k_moduleCount =
    (int)(sizeof(k_modules) / sizeof(k_modules[0]));

ActiveModule HomeScreen::draw(bool& wantsLaunchGame, bool& wantsLaunchSandbox)
{
    wantsLaunchGame = false;
    wantsLaunchSandbox = false;
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

    // ── Pulsanti avvia ────────────────────────────────────────────────
    const float btnW = 200, btnH = 44;
    const float gapBtn = 16.0f;
    const float totalBtnW = btnW * 2 + gapBtn;
    float bx = W * 0.5f - totalBtnW * 0.5f;
    float by = H * 0.14f;

    ImGui::SetCursorPos({bx, by});
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.50f, 0.20f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.30f, 1.0f));
    if (ImGui::Button("▶  Avvia Partita", ImVec2(btnW, btnH)))
        wantsLaunchGame = true;
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos({bx + btnW + gapBtn, by});
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.35f, 0.60f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.50f, 0.85f, 1.0f));
    if (ImGui::Button("▶  Sandbox", ImVec2(btnW, btnH)))
        wantsLaunchSandbox = true;
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos({W * 0.5f - 130.0f, by + btnH + 4});
    ImGui::TextDisabled("Partita: --direct-prematch          Sandbox: modalita' libera");

    // ── Griglia moduli ────────────────────────────────────────────────
    const float cardW  = 200, cardH = 160;
    const float gapX   = 18,  gapY  = 20;
    const int   cols   = 4;
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

    // Margine sotto l'ultima riga: le card sono piazzate con coordinate ASSOLUTE
    // (SetCursorPos), quindi il contenuto della finestra finiva esattamente sul loro
    // bordo inferiore — lo scroll si fermava di netto e la griglia sembrava tagliata
    // (segnalato dall'utente). Un elemento invisibile sotto l'ultima riga estende
    // l'area scrollabile e restituisce il respiro visivo.
    {
        const int lastRow = (k_moduleCount - 1) / cols;
        ImGui::SetCursorPos({startX, startY + (lastRow + 1) * (cardH + gapY)});
        ImGui::Dummy({gridW, 24.0f});
    }

    ImGui::End();
    return selected;
}

} // namespace editor
#pragma once
#include <imgui.h>
#include <string>

namespace editor::dialogs
{

// ── FINESTRE MODALI CONDIVISE (doc 52 F4) ───────────────────────────────────
//
// Perché esistono: c'erano 14 popup nel solo Map Editor, ciascuno con la sua forma,
// e nessun contratto comune per "conferma distruttiva" o "salva/scarta/annulla".
// Da lì è nato il difetto peggiore della settimana (changelog 164): `OpenPopup`
// chiamata dentro `BeginTabBar` — che spinge un proprio livello di ID — e
// `BeginPopupModal` chiamata fuori. Due identificatori diversi, finestra
// registrata come aperta e **mai disegnata**, clic bloccati ovunque.
//
// LA DIFESA È NELLA FORMA, non nella disciplina: qui `OpenPopup` e
// `BeginPopupModal` stanno nella **stessa funzione**, quindi nello stesso livello
// di ID **per costruzione**. Il difetto non è "da evitare": è inesprimibile.
//
// L'apertura si chiede con un `bool&` che la funzione azzera da sola quando la
// finestra si chiude: nessuno stato da ricordare fuori, nessun flag che resta
// appeso a bloccare l'input.

enum class Choice { None, Yes, No, Cancel };

// Conferma di un'azione distruttiva. `wanted` a true la apre; torna Yes/Cancel.
inline Choice confirmDestructive(const char* id, bool& wanted,
                                 const char* message,
                                 const char* confirmLabel = "Elimina")
{
    if (!wanted) return Choice::None;
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);

    Choice out = Choice::None;
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(message);
        ImGui::Separator();
        // Il pulsante distruttivo NON è il primo né quello di default: si conferma
        // per scelta, non per inerzia.
        if (ImGui::Button("Annulla", {110, 0})) { out = Choice::Cancel; }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.16f, 1.0f));
        if (ImGui::Button(confirmLabel, {130, 0})) { out = Choice::Yes; }
        ImGui::PopStyleColor();
        if (out != Choice::None) { wanted = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    else
    {
        // Rete di sicurezza: se per qualunque ragione la finestra non è stata
        // disegnata, l'intenzione non resta appesa. È la ricaduta del difetto del
        // modale invisibile, resa impossibile anche in caso di uso improprio.
        wanted = false;
    }
    return out;
}

// Salva / Scarta / Annulla. Torna Yes = salva, No = scarta, Cancel = resta.
inline Choice saveDiscardCancel(const char* id, bool& wanted,
                                const std::string& message,
                                const std::string& detail = {},
                                bool canSave = true)
{
    if (!wanted) return Choice::None;
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);

    Choice out = Choice::None;
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(message.c_str());
        if (!detail.empty()) ImGui::TextDisabled("%s", detail.c_str());
        ImGui::Separator();
        if (canSave)
        {
            if (ImGui::Button("Salva ed esci", {140, 0})) out = Choice::Yes;
            ImGui::SameLine();
        }
        if (ImGui::Button("Esci senza salvare", {160, 0})) out = Choice::No;
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {110, 0})) out = Choice::Cancel;
        if (out != Choice::None) { wanted = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    else wanted = false;
    return out;
}

// Messaggio d'errore. Torna true quando è stato chiuso.
inline bool errorBox(const char* id, bool& wanted, const std::string& message)
{
    if (!wanted) return false;
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);

    bool closed = false;
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", message.c_str());
        ImGui::Separator();
        if (ImGui::Button("Ho capito", {130, 0}))
        { closed = true; wanted = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    else wanted = false;
    return closed;
}

} // namespace editor::dialogs

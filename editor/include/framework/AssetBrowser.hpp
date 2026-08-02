#pragma once
// ── AssetBrowser — ciclo di vita di una definizione su file (ADR-049, R1) ────
//
// PERCHÉ ESISTE. L'audit di coerenza (doc 39, 2026-08-02) ha misurato che *Elimina*
// manca in 5 moduli su 7 e *Duplica* in 3, e che ogni modulo si riscrive la scansione
// della cartella e i propri pulsanti. Qui il ciclo di vita completo — **Crea, Duplica,
// Rinomina, Elimina** — sta in un posto solo: aggiungerlo a un modulo è una riga, e
// migliorarlo migliora tutti i moduli che lo adottano.
//
// COSA NON FA (deliberato): non conosce il CONTENUTO delle definizioni. Il modulo gli
// dice come si chiama un asset nuovo (`makeDefault`) e cosa fare quando la selezione
// cambia; tutto il resto — form, viewport, validazione — resta del modulo. È
// composizione (ADR-049), non un framework che impone la forma del modulo.
//
// REGOLE CHE FA RISPETTARE PER COSTRUZIONE:
//  · id = filename stem (ADR-001): l'id NON si legge mai dal contenuto del file — è
//    esattamente l'errore di KI #21 e #84, dove l'elenco mostrava un id stantio;
//  · rinominare è un COMANDO (ADR-010): usa `renameDefinition`, che sposta il file e
//    aggiorna i riferimenti — mai "salva con nome nuovo", che lascia orfani (KI #7);
//  · ogni scrittura passa da `saveJsonRMW` (ADR-010);
//  · eliminare CHIEDE CONFERMA e dice cosa sta per succedere.

#include "util/DataPath.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace editor
{

class AssetBrowser
{
public:
    struct Config
    {
        std::string      folder;        // sottocartella di data/ (es. "vehicles")
        std::string      title;         // etichetta mostrata (es. "Veicoli")
        rename::Category category = rename::Category::Map;   // per il comando di rinomina
        // Contenuto di un asset appena creato. Il browser non sa cosa sia un veicolo:
        // lo chiede al modulo.
        std::function<nlohmann::json(const std::string& newId)> makeDefault;
        bool allowDelete = true;
    };

    void configure(Config c) { m_cfg = std::move(c); refresh(); }

    // Riscansiona la cartella. id = filename stem, SEMPRE (ADR-001).
    void refresh()
    {
        namespace fs = std::filesystem;
        m_ids.clear();
        std::error_code ec;
        const fs::path folder = fs::path(editor::datapath::dir()) / m_cfg.folder;
        if (fs::exists(folder, ec))
            for (auto& e : fs::directory_iterator(folder, ec))
                if (e.path().extension() == ".json")
                    m_ids.push_back(e.path().stem().string());
        std::sort(m_ids.begin(), m_ids.end());
        if (std::find(m_ids.begin(), m_ids.end(), m_selected) == m_ids.end())
            m_selected = m_ids.empty() ? std::string() : m_ids.front();
    }

    // Disegna lista + comandi. Ritorna true se la SELEZIONE è cambiata in questo frame
    // (il modulo ricarica ciò che mostra).
    bool draw()
    {
        bool selectionChanged = false;
        ImGui::TextDisabled("%s (%d)", m_cfg.title.c_str(), (int)m_ids.size());

        // Lista: lascia spazio ai comandi sotto, che non devono mai finire fuori vista
        // (R8: niente comandi tagliati).
        ImGui::BeginChild("##ab_items", ImVec2(0, -118), ImGuiChildFlags_None);
        for (const auto& id : m_ids)
            if (ImGui::Selectable(id.c_str(), id == m_selected) && id != m_selected)
            { m_selected = id; selectionChanged = true; }
        ImGui::EndChild();

        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(w);
        ImGui::InputTextWithHint("##ab_name", "nome...", m_nameBuf, sizeof(m_nameBuf));

        // ── Crea ─────────────────────────────────────────────────────────
        if (ImGui::Button("Nuovo", {w * 0.5f - 2.0f, 0}))
        {
            std::string err;
            if (createAsset(m_nameBuf, err)) { m_nameBuf[0] = '\0'; selectionChanged = true; }
            else m_msg = err;
        }
        ImGui::SameLine();
        // ── Duplica ──────────────────────────────────────────────────────
        if (ImGui::Button("Duplica", {w * 0.5f - 2.0f, 0}))
        {
            std::string err;
            if (duplicateSelected(m_nameBuf, err)) { m_nameBuf[0] = '\0'; selectionChanged = true; }
            else m_msg = err;
        }
        // ── Rinomina (COMANDO, ADR-010) ──────────────────────────────────
        if (ImGui::Button("Rinomina", {w * 0.5f - 2.0f, 0}))
        {
            std::string err;
            if (renameSelected(m_nameBuf, err)) { m_nameBuf[0] = '\0'; selectionChanged = true; }
            else m_msg = err;
        }
        ImGui::SameLine();
        // ── Elimina (con conferma) ───────────────────────────────────────
        ImGui::BeginDisabled(!m_cfg.allowDelete || m_selected.empty());
        if (ImGui::Button("Elimina", {w * 0.5f - 2.0f, 0}))
            ImGui::OpenPopup("##ab_del");
        ImGui::EndDisabled();

        if (ImGui::BeginPopup("##ab_del"))
        {
            ImGui::Text("Eliminare '%s'?", m_selected.c_str());
            // Onestà: si dice cosa NON viene fatto. I riferimenti da altri file non
            // vengono ripuliti — il gate `--validate` li segnalerà come rotti.
            ImGui::TextDisabled("Il file viene cancellato. Eventuali riferimenti");
            ImGui::TextDisabled("da altre definizioni resteranno rotti (--validate li segnala).");
            if (ImGui::Button("Elimina", {110, 0}))
            {
                std::string err;
                if (deleteSelected(err)) selectionChanged = true; else m_msg = err;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (!m_msg.empty())
            ImGui::TextColored({0.95f, 0.45f, 0.35f, 1.0f}, "%s", m_msg.c_str());

        return selectionChanged;
    }

    [[nodiscard]] const std::string& selectedId() const { return m_selected; }
    [[nodiscard]] const std::vector<std::string>& ids() const { return m_ids; }
    void select(const std::string& id) { m_selected = id; }
    void clearMessage() { m_msg.clear(); }

private:
    [[nodiscard]] std::string pathOf(const std::string& id) const
    {
        return (std::filesystem::path(editor::datapath::dir()) / m_cfg.folder / (id + ".json")).string();
    }
    // Un id è un NOME DI FILE: i caratteri non validi vanno fermati qui, non scoperti
    // quando la scrittura fallisce a metà.
    static bool validId(const std::string& id, std::string& err)
    {
        if (id.empty()) { err = "Serve un nome."; return false; }
        for (char c : id)
            if (!(std::isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-'))
            { err = "Usa lettere, numeri, spazio, _ o -"; return false; }
        return true;
    }

    bool createAsset(const std::string& id, std::string& err)
    {
        if (!validId(id, err)) return false;
        if (std::filesystem::exists(pathOf(id))) { err = "Esiste gia'."; return false; }
        nlohmann::json content = m_cfg.makeDefault ? m_cfg.makeDefault(id)
                                                   : nlohmann::json::object();
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(editor::datapath::dir()) / m_cfg.folder, ec);
        if (!jsonsave::saveJsonRMW(pathOf(id), [&](nlohmann::json& j) { j = content; return true; }))
        { err = "Scrittura fallita."; return false; }
        refresh(); m_selected = id; m_msg.clear();
        return true;
    }

    bool duplicateSelected(const std::string& newId, std::string& err)
    {
        if (m_selected.empty()) { err = "Nessun elemento selezionato."; return false; }
        if (!validId(newId, err)) return false;
        if (std::filesystem::exists(pathOf(newId))) { err = "Esiste gia'."; return false; }
        std::error_code ec;
        std::filesystem::copy_file(pathOf(m_selected), pathOf(newId), ec);
        if (ec) { err = "Copia fallita."; return false; }
        // Il nome VISUALIZZATO segue il nuovo id, altrimenti nascono i near-duplicate
        // "stesso nome, due file" di KI #7.
        jsonsave::saveJsonRMW(pathOf(newId), [&](nlohmann::json& j) {
            j["name"] = newId; j.erase("id"); return true; }, /*backup=*/false);
        refresh(); m_selected = newId; m_msg.clear();
        return true;
    }

    bool renameSelected(const std::string& newId, std::string& err)
    {
        if (m_selected.empty()) { err = "Nessun elemento selezionato."; return false; }
        if (!validId(newId, err)) return false;
        // COMANDO condiviso (ADR-010): sposta il file e aggiorna i cross-reference.
        const std::string e = rename::renameDefinition(editor::datapath::dir(), m_cfg.category,
                                                       m_selected, newId);
        if (!e.empty()) { err = e; return false; }
        refresh(); m_selected = newId; m_msg.clear();
        return true;
    }

    bool deleteSelected(std::string& err)
    {
        if (m_selected.empty()) { err = "Nessun elemento selezionato."; return false; }
        std::error_code ec;
        if (!std::filesystem::remove(pathOf(m_selected), ec) || ec)
        { err = "Cancellazione fallita."; return false; }
        m_selected.clear(); refresh(); m_msg.clear();
        return true;
    }

    Config                   m_cfg;
    std::vector<std::string> m_ids;
    std::string              m_selected;
    std::string              m_msg;
    char                     m_nameBuf[64] = "";
};

} // namespace editor

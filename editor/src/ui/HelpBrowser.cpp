#include "ui/HelpBrowser.hpp"
#include "util/DataPath.hpp"

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace editor
{

void HelpBrowser::reload()
{
    m_chapters.clear();
    // `root()` è già la radice di `data/` (senza slash finale), non la radice del
    // progetto: aggiungere "data" avrebbe cercato in `data/data/help`.
    const fs::path dir = fs::path(editor::datapath::root()) / "help";
    std::error_code ec;
    if (!fs::exists(dir, ec)) { m_loaded = true; return; }

    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".md") files.push_back(e.path());
    // Ordine per NOME FILE: il prefisso numerico decide la sequenza dei capitoli,
    // così l'ordine di lettura è una scelta d'autore e non l'ordine del filesystem.
    std::sort(files.begin(), files.end());

    for (const auto& f : files)
    {
        std::ifstream in(f);
        if (!in.is_open()) continue;
        Chapter ch;
        ch.file  = f.filename().string();
        ch.title = ch.file;
        std::string line;
        Section cur;
        bool haveSection = false;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("# ", 0) == 0)        { ch.title = line.substr(2); continue; }
            if (line.rfind("## ", 0) == 0)
            {
                if (haveSection) ch.sections.push_back(cur);
                cur = Section{ line.substr(3), {} };
                haveSection = true;
                continue;
            }
            if (haveSection) { cur.body += line; cur.body += '\n'; }
        }
        if (haveSection) ch.sections.push_back(cur);
        m_chapters.push_back(std::move(ch));
    }
    m_loaded = true;
    if (m_sel >= (int)m_chapters.size()) m_sel = 0;
}

void HelpBrowser::ensureLoaded() { if (!m_loaded) reload(); }

int HelpBrowser::sectionCount() const
{
    int n = 0;
    for (const auto& c : m_chapters) n += (int)c.sections.size();
    return n;
}

// Renderer volutamente MINIMO: grassetto `**`, elenchi `-`, codice `` ` ``, righe
// vuote. Un parser markdown completo sarebbe una dipendenza e un problema in più da
// mantenere, e qui serve solo che il testo si legga bene.
void HelpBrowser::drawMarkdownLite(const std::string& text)
{
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty()) { ImGui::Spacing(); continue; }
        if (line.rfind("### ", 0) == 0)
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", line.substr(4).c_str());
            continue;
        }
        if (line.rfind("- ", 0) == 0)
        {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", line.substr(2).c_str());
            continue;
        }
        if (line.rfind("> ", 0) == 0)
        {
            ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.35f, 1.0f), "%s", line.substr(2).c_str());
            continue;
        }
        ImGui::TextWrapped("%s", line.c_str());
    }
}

void HelpBrowser::draw(bool* open)
{
    if (!open || !*open) return;
    ensureLoaded();

    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Guida dell'editor", open))
    { ImGui::End(); return; }

    if (m_chapters.empty())
    {
        ImGui::TextWrapped("Nessun contenuto in 'data/help/'.");
        if (ImGui::Button("Ricarica")) reload();
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##helpfilter", "cerca nel testo...", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::SmallButton("Ricarica")) reload();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Rilegge 'data/help/': i capitoli si correggono\n"
                          "senza ricompilare l'editor.");
    ImGui::Separator();

    const bool filtering = (m_filter[0] != '\0');
    std::string needle = m_filter;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto contains = [&](const std::string& hay) {
        std::string h = hay;
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        return h.find(needle) != std::string::npos;
    };

    // Indice a sinistra, contenuto a destra: la struttura di ogni manuale.
    ImGui::BeginChild("##helpnav", ImVec2(240, 0), ImGuiChildFlags_Borders);
    for (int i = 0; i < (int)m_chapters.size(); ++i)
    {
        const auto& ch = m_chapters[i];
        if (filtering)
        {
            bool hit = contains(ch.title);
            for (const auto& s : ch.sections)
                if (!hit && (contains(s.title) || contains(s.body))) hit = true;
            if (!hit) continue;
        }
        if (ImGui::Selectable(ch.title.c_str(), m_sel == i)) m_sel = i;
        if (m_sel == i)
            for (const auto& s : ch.sections)
                ImGui::TextDisabled("   %s", s.title.c_str());
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##helpbody", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (m_sel >= 0 && m_sel < (int)m_chapters.size())
    {
        const auto& ch = m_chapters[m_sel];
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s", ch.title.c_str());
        ImGui::Separator();
        for (const auto& s : ch.sections)
        {
            // Col filtro attivo si mostrano SOLO le sezioni che contengono il testo:
            // una ricerca che poi obbliga a cercare a occhio non ha cercato nulla.
            if (filtering && !contains(s.title) && !contains(s.body)) continue;
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), "%s", s.title.c_str());
            ImGui::Separator();
            drawMarkdownLite(s.body);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace editor

#pragma once
#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

// ── BARRA DEI COMANDI CHE NON PUÒ TAGLIARE ──────────────────────────────────
//
// Perché esiste. La regola "mai far tagliare i comandi" è nel progetto da mesi e
// **è stata violata tre volte**, sempre allo stesso modo: si aggiunge un comando
// utile, la barra supera la larghezza del pannello, e l'ultimo comando smette di
// esistere per chi lo usa. L'ultima volta è toccato a "Prova da qui", consegnato
// e dichiarato fatto — l'utente non l'ha trovato.
//
// La causa non è la distrazione: **io non vedo lo schermo**. Una regola che
// richiede di guardare il risultato non può essere rispettata da chi non guarda.
// Quindi non basta una regola: serve che il taglio sia *inesprimibile* e che
// l'eccedenza sia *misurabile senza occhi*.
//
// Due pezzi, e sono separati apposta:
//   1. `fitCount` — decisione PURA (quante voci stanno, dato lo spazio). Non tocca
//      ImGui, quindi si collauda headless, che è l'unico modo in cui io possa
//      verificarla.
//   2. `Toolbar` — misura le voci con ImGui e applica la decisione, mandando
//      l'eccedenza in un menu «altro» invece che oltre il bordo.
//
// È il pattern "priority+" (PatternFly, Brad Frost): le voci più importanti
// restano in barra, le altre si raccolgono dietro una voce sola. La differenza
// rispetto a nasconderle è tutta lì — nessun comando sparisce, cambia solo dove
// si trova.
namespace editor::toolbar
{

// Quante delle prime N voci stanno nello spazio disponibile.
//
// `overflowW` è la larghezza del pulsante «altro»: va RISERVATA appena una voce
// non ci sta, altrimenti il pulsante che dovrebbe salvare le voci in eccesso
// finisce lui stesso oltre il bordo. È l'errore classico di questo pattern, e la
// ragione per cui questa funzione esiste separata invece che scritta a mano dentro
// il ciclo di disegno.
//
// Ritorna un numero fra 0 e widths.size(). Le voci da `fitCount` in poi vanno nel
// menu: **nessuna sparisce**, ed è l'invariante collaudata.
[[nodiscard]] inline int fitCount(const std::vector<float>& widths, float avail,
                                  float spacing, float overflowW)
{
    if (widths.empty()) return 0;

    // Prima ipotesi: ci stanno tutte, quindi niente pulsante «altro» da riservare.
    float used = 0.0f;
    int   n    = 0;
    for (float w : widths)
    {
        const float next = used + (n > 0 ? spacing : 0.0f) + w;
        if (next > avail) break;
        used = next;
        ++n;
    }
    if (n == (int)widths.size()) return n;

    // Serve il menu: si riprova riservandogli lo spazio. Si toglie dalla coda finché
    // il pulsante «altro» ci sta davvero — riservare "a occhio" è come non riservare.
    const float budget = avail - spacing - overflowW;
    used = 0.0f; n = 0;
    for (float w : widths)
    {
        const float next = used + (n > 0 ? spacing : 0.0f) + w;
        if (next > budget) break;
        used = next;
        ++n;
    }
    return n;
}

// Larghezza che occuperebbe un pulsante con questa etichetta, con lo stile corrente.
[[nodiscard]] inline float buttonWidth(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x
         + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Una voce della barra.
//
// `draw` disegna la voce IN BARRA, `menu` la disegna come voce del menu «altro».
// Sono due perché un pulsante e una riga di menu non si disegnano allo stesso modo,
// ma **fanno la stessa cosa**: l'azione sta in `action`, chiamata da entrambe. Se
// l'azione vivesse in due posti, la voce funzionerebbe in barra e non nel menu —
// che è esattamente il difetto che questa classe esiste per impedire.
struct Item
{
    std::string label;                 // usato per la misura e per il menu
    std::string tooltip;
    std::function<void()> action;      // cosa fa (una sola definizione)
    // Voci che NON devono mai finire nel menu: il selettore della mappa, l'indicatore
    // di modifiche non salvate. Sono contesto, non comandi.
    bool pinned = false;
    // Larghezza dichiarata (combo, testo di stato). 0 = si misura dall'etichetta.
    float fixedWidth = 0.0f;
    // Disegno personalizzato in barra (combo, spunte, testo colorato). Se assente
    // si disegna un pulsante con `label` che invoca `action`.
    std::function<void()> custom;
    // Etichetta diversa nel menu, quando quella della barra è abbreviata per spazio.
    std::string menuLabelOverride;
};

// Esito dell'ultimo disegno: serve a DIRE quanto sta rientrando, invece di
// lasciarlo scoprire a chi guarda. Vedi §5-bis di CLAUDE.md — un sistema senza
// osservabilità non è finito.
struct Report
{
    int   items      = 0;   // voci totali
    int   inBar      = 0;   // quante disegnate in barra
    int   inOverflow = 0;   // quante finite nel menu
    float required   = 0.0f;// larghezza che servirebbe per averle tutte in barra
    float available  = 0.0f;
};

// Disegna la barra. `avail` = larghezza utile; <= 0 = tutta quella del pannello.
inline Report draw(const char* id, std::vector<Item>& items, float avail = 0.0f)
{
    Report rep;
    if (items.empty()) return rep;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float spacing = st.ItemSpacing.x;
    if (avail <= 0.0f) avail = ImGui::GetContentRegionAvail().x;

    std::vector<float> w;
    w.reserve(items.size());
    for (const auto& it : items)
        w.push_back(it.fixedWidth > 0.0f ? it.fixedWidth : buttonWidth(it.label.c_str()));

    rep.items     = (int)items.size();
    rep.available = avail;
    for (std::size_t i = 0; i < w.size(); ++i)
        rep.required += w[i] + (i ? spacing : 0.0f);

    // Le voci fissate stanno sempre in barra e non entrano nella contesa: si toglie
    // il loro spazio dal budget e si decide sul resto.
    float pinnedW = 0.0f;
    int   nPinned = 0;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].pinned) { pinnedW += w[i] + spacing; ++nPinned; }

    std::vector<float> freeW;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (!items[i].pinned) freeW.push_back(w[i]);

    const float overflowW = buttonWidth("...");
    const int   fits = fitCount(freeW, avail - pinnedW, spacing, overflowW);
    rep.inBar      = nPinned + fits;
    rep.inOverflow = (int)freeW.size() - fits;

    auto drawItem = [&](Item& it) {
        if (it.custom) { it.custom(); }
        else if (ImGui::Button(it.label.c_str()) && it.action) it.action();
        if (!it.tooltip.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", it.tooltip.c_str());
    };

    bool first = true;
    int  freeSeen = 0;
    for (auto& it : items)
    {
        if (!it.pinned && freeSeen++ >= fits) continue;   // va nel menu
        if (!first) ImGui::SameLine();
        first = false;
        drawItem(it);
    }

    if (rep.inOverflow > 0)
    {
        if (!first) ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("...")) ImGui::OpenPopup("##overflow");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%d comandi non entrano nella barra a questa larghezza.\n"
                              "Sono qui dentro, non spariti. Allarga la finestra e\n"
                              "tornano al loro posto.", rep.inOverflow);
        if (ImGui::BeginPopup("##overflow"))
        {
            int seen = 0;
            for (auto& it : items)
            {
                if (it.pinned) continue;
                if (seen++ < fits) continue;
                if (it.menuLabelOverride.empty()
                    ? ImGui::MenuItem(it.label.c_str())
                    : ImGui::MenuItem(it.menuLabelOverride.c_str()))
                    if (it.action) it.action();
                if (!it.tooltip.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", it.tooltip.c_str());
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    return rep;
}

} // namespace editor::toolbar

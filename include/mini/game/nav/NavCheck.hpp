#pragma once
#include "mini/game/nav/NavManager.hpp"
#include "mini/game/data/Definitions.hpp"
#include "mini/game/MapStructures.hpp"
#include "mini/core/GameConfig.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ostream>
#include <string>
#include <vector>
#include <array>

// ── IL NAVMESH DI UNA MAPPA, SENZA APRIRE UNA FINESTRA (doc 53 L5) ──────────
//
// Perché esiste. `MapEditor::validateNavmesh` costruisce il navmesh vero, ma vive
// nell'editor e richiede un contesto grafico. Chi non può aprire una finestra — la
// CI, e soprattutto **io** — del navmesh non sa niente. Il risultato l'abbiamo
// visto: per due giri ho ragionato sulle isole basandomi sulla descrizione a voce
// dell'utente, e ho sbagliato la scala di un ordine di grandezza (ho chiamato
// "schegge" zone da 40 m²).
//
// Testuale dell'utente (2026-08-11): *"il fatto che tu non riesca ad esaminare bene
// cose come il navmesh è un problema … meno vedi più fai cose su basi sbagliate e
// quindi commetti errori"*.
//
// Regole: STESSO `NavManager` del gioco e dell'editor (nessuna terza costruzione),
// e ogni numero accompagnato dalla sua grandezza — un conteggio di triangoli su un
// navmesh grossolano non dice niente, l'area sì.
namespace mini::navcheck
{

struct Island
{
    float     area = 0.0f;
    int       tris = 0;
    glm::vec3 center{0.0f};
    // Il caso più frequente e più innocuo: pavimento **sotto** un ostacolo. È
    // navmesh legittimo (c'è l'altezza per starci) ma chiuso dentro, quindi
    // irraggiungibile — e non è un difetto da correggere, è una conseguenza di
    // avere un cubo appoggiato sopra. Distinguerlo evita di mandare l'autore a
    // caccia di un problema che non c'è.
    bool      underCover = false;
    float     covered = 0.0f;     // frazione di superficie sotto un ostacolo (0..1)
    // ── QUANTO DISTA DAL NAVMESH BUONO ───────────────────────────────────
    // È la misura che dice CHE TIPO di isola è, e mancava:
    //   · sotto il metro → una **sacca** separata solo dall'erosione (il navmesh si
    //     ritira di 0,40 m per lato da ogni ostacolo, e fra due cubi vicini la
    //     striscia in mezzo resta scollegata pur essendo a un passo);
    //   · molti metri → una **zona** davvero staccata, che va collegata.
    // Le due cose hanno rimedi opposti — allargare un varco, oppure costruire un
    // accesso — e senza questo numero si confondono.
    float     distToMain = -1.0f;
};

struct Result
{
    bool  built = false;
    int   polys = 0, components = 0;
    int   islandTris = 0;
    float islandArea = 0.0f;
    std::vector<Island> islands;          // ordinate per area
    std::vector<int> badPositions, badPosts;
};

// Espande le strutture parametriche come fa il caricamento vero: senza, il navmesh
// headless vedrebbe una mappa priva di scale e piattaforme, cioè un'altra mappa.
inline void collectGeometry(const MapDef& map, std::vector<MapGeometryBox>& out,
                            const mapstructures::TypeResolver& resolve = {})
{
    out = map.geometry;
    for (const auto& s : map.structures)
        mapstructures::expandInstance(s, nullptr, out, resolve);
}

// C'è un collider SOPRA questo punto? Serve a distinguere il pavimento chiuso
// sotto un cubo (normale, non un difetto) da una zona scollegata vera.
[[nodiscard]] inline bool coveredFromAbove(const std::vector<MapGeometryBox>& geo,
                                           const glm::vec3& p)
{
    for (const auto& b : geo)
    {
        if (!b.collider) continue;
        if (std::fabs(p.x - b.x) > b.sx * 0.5f) continue;
        if (std::fabs(p.z - b.z) > b.sz * 0.5f) continue;
        if (b.y - b.sy * 0.5f > p.y + 0.05f) return true;   // sta sopra
    }
    return false;
}

// ── QUANTA DELLA SUPERFICIE STA SOTTO QUALCOSA (frazione 0..1) ──────────────
// Due versioni sbagliate prima di questa, e sono istruttive:
//  1. **il solo baricentro dell'isola** — un'isola ad ANELLO (il terreno attorno a
//     una piattaforma centrale) ha il baricentro nel buco in mezzo, dove non c'è
//     copertura: 566 m² classificati "isola vera" per colpa di un punto solo;
//  2. **maggioranza dei baricentri dei triangoli** — su un navmesh grossolano un
//     triangolo può valere 94 m², quindi "un triangolo un voto" pesa un fazzoletto
//     quanto un piazzale, e con 2 triangoli la maggioranza non esiste nemmeno.
// Si campiona DENTRO ogni triangolo e si pesa per AREA. Il risultato è una misura
// (78% sotto ostacoli) invece di un verdetto, e la soglia resta una riga sola.
[[nodiscard]] inline float coveredFraction(const std::vector<MapGeometryBox>& geo,
                                           const std::vector<std::array<glm::vec3,3>>& tris)
{
    // Baricentriche fisse: centro + tre punti a un terzo verso i vertici + tre punti
    // a metà dei lati. Sette campioni per triangolo bastano a distinguere un
    // fazzoletto coperto da un piazzale scoperto, e non dipendono dalla forma.
    static const float bc[7][3] = {
        {1/3.f, 1/3.f, 1/3.f},
        {2/3.f, 1/6.f, 1/6.f}, {1/6.f, 2/3.f, 1/6.f}, {1/6.f, 1/6.f, 2/3.f},
        {0.5f, 0.5f, 0.0f},    {0.0f, 0.5f, 0.5f},    {0.5f, 0.0f, 0.5f},
    };
    float total = 0.0f, covered = 0.0f;
    for (const auto& t : tris)
    {
        const float a = 0.5f * glm::length(glm::cross(t[1] - t[0], t[2] - t[0]));
        if (a <= 0.0f) continue;
        int hit = 0;
        for (const auto& w : bc)
            if (coveredFromAbove(geo, t[0]*w[0] + t[1]*w[1] + t[2]*w[2])) ++hit;
        total   += a;
        covered += a * (float)hit / 7.0f;
    }
    return total > 0.0f ? covered / total : 0.0f;
}

// Ritorna 0 se la mappa è sana, 1 se ha isole VERE (non sottopassi) o elementi
// autorati irraggiungibili: così `--navcheck` è usabile anche come gate.
inline int report(const std::string& mapId, const MapDef& map, std::ostream& os,
                  const mapstructures::TypeResolver& resolve = {})
{
    std::vector<MapGeometryBox> geo;
    collectGeometry(map, geo, resolve);
    MapDef flat = map;
    flat.geometry = geo;
    flat.structures.clear();      // già espanse: espanderle due volte le duplica

    NavManager nav;
    const NavBuildStats st = nav.build(flat);
    char buf[256];
    if (!st.ok)
    {
        os << "[navcheck] " << mapId << ": COSTRUZIONE FALLITA\n";
        return 1;
    }

    std::vector<NavManager::DebugTri> tris;
    int nComp = 0;
    nav.debugTriangles(tris, &nComp);
    const glm::vec3 spawn = {map.spawnTeam1[0], map.spawnTeam1[1], map.spawnTeam1[2]};
    const int main = nav.componentAt(spawn);

    std::vector<float> area(nComp > 0 ? nComp : 0, 0.0f);
    std::vector<int>   cnt (nComp > 0 ? nComp : 0, 0);
    std::vector<glm::vec3> cen(nComp > 0 ? nComp : 0, glm::vec3(0.0f));
    std::vector<std::vector<std::array<glm::vec3,3>>> samples(nComp > 0 ? nComp : 0);
    float totalArea = 0.0f;
    for (const auto& t : tris)
    {
        const float a = 0.5f * glm::length(glm::cross(t.b - t.a, t.c - t.a));
        totalArea += a;
        if (t.component == main || t.component < 0 || t.component >= nComp) continue;
        const glm::vec3 c3 = (t.a + t.b + t.c) / 3.0f;
        area[t.component] += a;
        cnt [t.component] += 1;
        cen [t.component] += c3;
        samples[t.component].push_back({t.a, t.b, t.c});
    }

    // I vertici del navmesh BUONO: servono a misurare quanto dista ogni isola.
    std::vector<glm::vec3> mainPts;
    for (const auto& t : tris)
        if (t.component == main) { mainPts.push_back(t.a); mainPts.push_back(t.b);
                                   mainPts.push_back(t.c); }

    Result r;
    for (int c = 0; c < nComp; ++c)
    {
        if (cnt[c] == 0) continue;
        Island i;
        i.area = area[c]; i.tris = cnt[c]; i.center = cen[c] / (float)cnt[c];
        i.covered = coveredFraction(geo, samples[c]);
        i.underCover = (i.covered >= 0.70f);
        // Distanza minima fra un vertice dell'isola e uno del navmesh buono, in
        // TRE dimensioni. La prima versione misurava solo in pianta e dava 0,00 m
        // a ogni chiazza che stesse sopra o sotto un'altra superficie — cioè la
        // maggioranza dei casi, e con l'etichetta "basta allargare il varco"
        // attaccata a zone che stanno cinque metri più in basso. Un numero
        // sbagliato con un consiglio sopra è peggio di nessun numero.
        float best = 1e9f;
        for (const auto& t : samples[c])
            for (const auto& v : t)
                for (const auto& m : mainPts)
                {
                    const glm::vec3 d = v - m;
                    const float d2 = d.x*d.x + d.y*d.y + d.z*d.z;
                    if (d2 < best) best = d2;
                }
        i.distToMain = (best < 1e8f) ? std::sqrt(best) : -1.0f;
        r.islandArea += i.area; r.islandTris += i.tris;
        r.islands.push_back(i);
    }
    std::sort(r.islands.begin(), r.islands.end(),
              [](const Island& a, const Island& b) { return a.area > b.area; });

    int realIslands = 0;
    for (const auto& i : r.islands) if (!i.underCover) ++realIslands;

    std::snprintf(buf, sizeof(buf),
                  "[navcheck] %s: %d poligoni, %.0f m2 navigabili, %d componenti\n",
                  mapId.c_str(), st.polyCount, totalArea, nComp);
    os << buf;
    std::snprintf(buf, sizeof(buf),
                  "           isole: %d (%.0f m2), di cui %d SOTTO un ostacolo "
                  "(pavimento chiuso sotto un cubo: normale) e %d vere\n",
                  (int)r.islands.size(), r.islandArea,
                  (int)r.islands.size() - realIslands, realIslands);
    os << buf;
    for (const auto& i : r.islands)
    {
        // La PERCENTUALE accanto al verdetto: è la misura da cui il verdetto viene,
        // e mostrarla è ciò che permette di accorgersi se la soglia è tarata male
        // invece di fidarsi dell'etichetta. Un verdetto senza la sua misura è
        // esattamente il difetto che questo giro ha già pagato due volte.
        std::snprintf(buf, sizeof(buf),
                      "             %s %6.1f m2 a %7.1f,%6.1f,%7.1f  "
                      "(%3.0f%% coperta, %.2f m dal navmesh buono)%s\n",
                      i.underCover ? "sotto-ostacolo" : "ISOLA VERA    ",
                      i.area, i.center.x, i.center.y, i.center.z,
                      i.covered * 100.0f, i.distToMain,
                      (!i.underCover && i.distToMain >= 0.0f
                       && i.distToMain < 2.0f * mapmetrics::AGENT_RADIUS + 0.4f)
                        ? "  ← SACCA: la separa solo l'erosione, basta allargare il varco"
                        : "");
        os << buf;
    }

    for (std::size_t i = 0; i < map.tacticalPositions.size(); ++i)
    {
        const auto& p = map.tacticalPositions[i];
        if (!nav.isReachable(spawn, {p.x, p.y, p.z})) r.badPositions.push_back((int)i);
    }
    for (std::size_t i = 0; i < map.commandPosts.size(); ++i)
    {
        const auto& c = map.commandPosts[i];
        if (!nav.isReachable(spawn, {c.x, c.y, c.z})) r.badPosts.push_back((int)i);
    }
    if (!r.badPositions.empty() || !r.badPosts.empty())
    {
        std::snprintf(buf, sizeof(buf),
                      "           irraggiungibili: %d posizioni tattiche, %d command post\n",
                      (int)r.badPositions.size(), (int)r.badPosts.size());
        os << buf;
        for (int i : r.badPosts)
        {
            std::snprintf(buf, sizeof(buf), "             post '%s' a %.1f, %.1f — "
                          "INCATTURABILE\n", map.commandPosts[i].label.c_str(),
                          map.commandPosts[i].x, map.commandPosts[i].z);
            os << buf;
        }
    }
    return (realIslands > 0 || !r.badPosts.empty()) ? 1 : 0;
}

} // namespace mini::navcheck

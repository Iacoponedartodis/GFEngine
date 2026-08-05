#pragma once

#include "mini/game/MapMetrics.hpp"
#include "mini/game/data/Definitions.hpp"

#include <cmath>
#include <string>
#include <vector>

// ── Primitive parametriche di costruzione (ADR-053, doc 47 §3) ──────────────
//
//   Una scala non è un oggetto: è una RICETTA.
//
// L'autore dichiara l'INTENTO ("da qui, salendo di 3 m, larga 4"); questa funzione
// emette i `MapGeometryBox` rispettando `STEP_HEIGHT`. **L'alzata sbagliata diventa
// inesprimibile**: non è più possibile disegnare una scala che non è una scala, che
// è il difetto che ha prodotto KI #95 (alzate 0,68-1,21 m contro un massimo di 0,55).
//
// UN'IMPLEMENTAZIONE, TRE CONSUMATORI (come `WeaponHandPose.hpp`): il registry al
// load, il Map Editor per l'anteprima nel viewport, e il gate per la verifica. Se
// l'espansione vivesse in due posti, l'anteprima e il gioco divergerebbero al primo
// campo aggiunto.
//
// I PARAMETRI si salvano; i BOX espansi **mai** — si rigenerano al load, esattamente
// come i prefab (ADR-048) e come ogni dato derivato (ADR-033). Così non possono
// diventare stantii rispetto alla ricetta.
//
// PERCHÉ LE RAMPE SONO SCALETTATE e non inclinate: `MapGeometryBox` ha solo `ry`
// (nessun pitch), e aggiungerlo romperebbe lo slab test analitico della LOS, il
// navmesh e la collisione — cioè la fondazione tattica (ADR-047) — per una feature
// di authoring. La scalettatura fine risolve meglio: le pedate sono orizzontali,
// quindi il limite di pendenza non si applica mai, e con l'alzata multipla di
// `kCellHeight` (0,10) il campo di altezza di Recast la rappresenta esattamente.
// Il visivo liscio resta a carico di Blender, come già dice ADR-047.

namespace mini::mapstructures
{

// I quattro tipi decisi per il primo giro (doc 47 §12.2, scelta dell'utente:
// "anche un po' più del minimo utile"). La RICETTA (`StructureDef`) vive in
// Definitions.hpp con tutte le altre definizioni; qui c'è solo come si espande.
using Kind         = StructureKind;
using StructureDef = mini::StructureDef;

inline Kind parseKind(const std::string& s)
{
    if (s == "ramp")       return Kind::Ramp;
    if (s == "wall")       return Kind::Wall;
    if (s == "platform")   return Kind::Platform;
    if (s == "switchback") return Kind::Switchback;
    if (s == "doorway")    return Kind::Doorway;
    if (s == "room")       return Kind::Room;
    if (s == "catwalk")    return Kind::Catwalk;
    if (s == "barricade")  return Kind::Barricade;
    return Kind::Stair;
}

inline const char* kindName(Kind k)
{
    switch (k) {
        case Kind::Ramp:       return "ramp";
        case Kind::Wall:       return "wall";
        case Kind::Platform:   return "platform";
        case Kind::Switchback: return "switchback";
        case Kind::Doorway:    return "doorway";
        case Kind::Room:       return "room";
        case Kind::Catwalk:    return "catwalk";
        case Kind::Barricade:  return "barricade";
        default:               return "stair";
    }
}

// Etichetta leggibile per l'editor: le liste devono dire cosa fa la primitiva,
// non il suo identificatore.
inline const char* kindLabel(Kind k)
{
    switch (k) {
        case Kind::Ramp:       return "Rampa";
        case Kind::Wall:       return "Muro";
        case Kind::Platform:   return "Piattaforma con accessi";
        case Kind::Switchback: return "Vano scala (a rampe)";
        case Kind::Doorway:    return "Muro con apertura";
        case Kind::Room:       return "Stanza (guscio)";
        case Kind::Catwalk:    return "Passerella";
        case Kind::Barricade:  return "Linea di coperture";
        default:               return "Scala";
    }
}

inline bool isFlight(Kind k) { return k == Kind::Stair || k == Kind::Ramp; }

// Alzata e pedata normative del tipo (doc 47 §4.3).
inline float defaultRiser(Kind k)
{
    return (k == Kind::Ramp) ? mapmetrics::RAMP_RISER : mapmetrics::STAIR_RISER;
}
inline float defaultTread(Kind k)
{
    return (k == Kind::Ramp) ? mapmetrics::RAMP_TREAD : mapmetrics::STAIR_TREAD;
}

// L'alzata effettivamente usata: quella richiesta, ma **mai oltre `STEP_HEIGHT`**.
// È il clamp che rende il difetto inesprimibile: qualunque cosa scriva l'autore,
// la scala che esce è percorribile.
inline float effectiveRiser(const StructureDef& s)
{
    float r = (s.riser > 0.001f) ? s.riser : defaultRiser(s.kind);
    if (r > config::STEP_HEIGHT) r = config::STEP_HEIGHT;
    if (r < 0.05f)               r = 0.05f;   // sotto 5 cm sono migliaia di box
    return r;
}

// La pedata effettiva: libera verso l'alto (una scala più dolce è sempre lecita),
// limitata verso il basso alla PEDATA NORMATIVA. Sotto quella soglia il navmesh
// fatica a collegare i gradini — ed è la stessa soglia che usa il gate.
inline float effectiveTread(const StructureDef& s)
{
    const float t = (s.tread > 0.001f) ? s.tread : defaultTread(s.kind);
    return (t < mapmetrics::STAIR_TREAD) ? mapmetrics::STAIR_TREAD : t;
}

namespace detail
{
    struct Frame
    {
        float ox, oz, cs, sn;
        // (lx,lz) locale → mondo, con la stessa convenzione di rotazione dei prefab.
        float wx(float lx, float lz) const { return ox + lx * cs + lz * sn; }
        float wz(float lx, float lz) const { return oz - lx * sn + lz * cs; }
    };
    inline Frame frameOf(float x, float z, float ryDeg)
    {
        const float rad = ryDeg * 3.14159265358979f / 180.0f;
        return { x, z, std::cos(rad), std::sin(rad) };
    }

    // Un gradino: box pieno dalla base fino al proprio ripiano (niente box
    // sospesi, e il solido sotto la scala è geometria vera come l'autore la
    // costruirebbe a mano).
    inline MapGeometryBox stepBox(const Frame& f, const StructureDef& s,
                                  float lx, float lz, float w, float depth,
                                  float baseY, float topY)
    {
        MapGeometryBox b;
        b.x  = f.wx(lx, lz);
        b.z  = f.wz(lx, lz);
        b.sy = topY - baseY;
        b.y  = baseY + b.sy * 0.5f;
        b.sx = w;
        b.sz = depth;
        b.ry = s.ry;
        b.r = s.color[0]; b.g = s.color[1]; b.b = s.color[2];
        b.collider = true;
        b.type = BoxType::Floor;        // ci si cammina sopra
        b.fromStructure = true;         // DERIVATO: non si salva (ADR-033/053)
        return b;
    }

    // Un box generico nel frame della struttura, con tipo dichiarato.
    inline MapGeometryBox slab(const Frame& f, const StructureDef& s,
                               float lx, float lz, float w, float depth,
                               float baseY, float topY, BoxType type)
    {
        MapGeometryBox b;
        b.x = f.wx(lx, lz);  b.z = f.wz(lx, lz);
        b.sy = topY - baseY; b.y = baseY + b.sy * 0.5f;
        b.sx = w;            b.sz = depth;
        b.ry = s.ry;
        b.r = s.color[0]; b.g = s.color[1]; b.b = s.color[2];
        b.collider = true;
        b.type = type;
        b.fromStructure = true;
        return b;
    }

    // Un muro CON un'apertura: si emettono gli stipiti e l'architrave (più il
    // parapetto se è una finestra). Un'apertura non è un box mancante — è tre box
    // messi giusti, ed è esattamente il lavoro che nessuno vuole rifare a mano per
    // ogni porta di una mappa.
    inline void emitWallWithOpening(const Frame& f, const StructureDef& s,
                                    float lx, float lz, float length, float ry,
                                    float baseY, float height, float thick,
                                    float openW, float openH, float sill,
                                    float openOff, std::vector<MapGeometryBox>& out)
    {
        StructureDef ws = s; ws.ry = ry;
        const Frame wf = frameOf(f.wx(lx, lz), f.wz(lx, lz), ry);
        if (openW <= 0.01f || openW >= length - 0.01f)
        {   // nessuna apertura (o apertura più larga del muro): muro pieno
            out.push_back(slab(wf, ws, 0.0f, 0.0f, length, thick,
                               baseY, baseY + height, BoxType::Wall));
            return;
        }
        const float half = length * 0.5f;
        const float oL = openOff - openW * 0.5f;   // bordo sinistro dell'apertura
        const float oR = openOff + openW * 0.5f;
        // Stipite sinistro e destro (se resta muro da quel lato).
        if (oL > -half + 0.01f)
        {
            const float w = oL - (-half);
            out.push_back(slab(wf, ws, -half + w * 0.5f, 0.0f, w, thick,
                               baseY, baseY + height, BoxType::Wall));
        }
        if (oR < half - 0.01f)
        {
            const float w = half - oR;
            out.push_back(slab(wf, ws, oR + w * 0.5f, 0.0f, w, thick,
                               baseY, baseY + height, BoxType::Wall));
        }
        // Architrave sopra l'apertura.
        const float top = baseY + sill + openH;
        if (top < baseY + height - 0.01f)
            out.push_back(slab(wf, ws, openOff, 0.0f, openW, thick,
                               top, baseY + height, BoxType::Wall));
        // Parapetto sotto: è una FINESTRA, e il parapetto è copertura vera —
        // ci si ripara dietro e si spara sopra.
        if (sill > 0.01f)
            out.push_back(slab(wf, ws, openOff, 0.0f, openW, thick,
                               baseY, baseY + sill, BoxType::Cover));
    }

    // Genera una rampa di gradini che sale lungo +Z locale, dal piede (0,0).
    // `solidFrom` = quota da cui il gradino è PIENO. Serve al vano scala: le rampe
    // sopra la prima devono essere massicce dal suolo, non solai sospesi. Un gradino
    // sospeso ha il vuoto su tutti i lati, e `rcFilterLedgeSpans` scarta le celle di
    // bordo come strapiombo — su rampe larghe 1,6 m ne resta troppo poco perché il
    // navmesh le colleghi. Un vano scala in muratura è un volume pieno: costruirlo
    // così non è solo più realistico, è ciò che lo rende percorribile.
    inline void emitFlight(const Frame& f, const StructureDef& s,
                           float baseY, float rise, float width,
                           float startLz, std::vector<MapGeometryBox>& out,
                           float solidFrom = 1e9f)
    {
        if (rise <= 0.001f) return;
        const float riser = effectiveRiser(s);
        const int   n     = mapmetrics::stepsFor(rise, riser);
        const float step  = rise / (float)n;          // ripartita, così l'ultima combacia
        const float tread = effectiveTread(s);
        const float bot   = (solidFrom < baseY) ? solidFrom : baseY;
        for (int i = 0; i < n; ++i)
        {
            const float lz  = startLz + ((float)i + 0.5f) * tread;
            const float top = baseY + step * (float)(i + 1);
            out.push_back(stepBox(f, s, 0.0f, lz, width, tread, bot, top));
        }
    }
}

// Lunghezza in pianta che una scala/rampa occuperà: serve all'editor per mostrarne
// l'ingombro **prima** di piazzarla, e a chi progetta la mappa per sapere quanto
// spazio serve. Una salita di 3 m a 0,20/0,30 sono 4,5 m di sviluppo.
inline float flightRun(const StructureDef& s)
{
    return (float)mapmetrics::stepsFor(s.rise, effectiveRiser(s)) * effectiveTread(s);
}

// ── L'ESPANSIONE ────────────────────────────────────────────────────────────
// Ricetta → box. Chiamata al load dal registry e dall'editor per l'anteprima.
inline void expand(const StructureDef& s, std::vector<MapGeometryBox>& out)
{
    const detail::Frame f = detail::frameOf(s.x, s.z, s.ry);

    switch (s.kind)
    {
    case Kind::Stair:
    case Kind::Ramp:
    {
        const float w = (s.width > 0.1f) ? s.width : mapmetrics::STAIR_MIN_WIDTH;
        detail::emitFlight(f, s, s.y, s.rise, w, 0.0f, out);
        break;
    }

    case Kind::Wall:
    case Kind::Doorway:
    {
        const float h  = (s.height > 0.01f) ? s.height : mapmetrics::WALL_HEIGHT;
        const float th = (s.thickness > 0.01f) ? s.thickness : mapmetrics::WALL_THICKNESS;
        const float ln = (s.length > 0.01f) ? s.length : 4.0f;
        const float ow = (s.kind == Kind::Doorway)
                       ? ((s.openW > 0.01f) ? s.openW : mapmetrics::DOOR_WIDTH) : 0.0f;
        const float oh = (s.openH > 0.01f) ? s.openH : mapmetrics::DOOR_HEIGHT;
        detail::emitWallWithOpening(f, s, 0.0f, 0.0f, ln, s.ry, s.y, h, th,
                                    ow, oh, s.openSill, s.openOff, out);
        break;
    }

    case Kind::Switchback:
    {
        // ⚠ NON ANCORA AFFIDABILE — fuori dal menu dell'editor (2026-08-05).
        // Il codice resta perché la struttura è giusta e serve; è il RISULTATO che
        // non è ancora garantito. Misurato su sei torri (4/8/12/20 m, ry 0/90/215):
        // **tre percorribili fino in cima, tre no**, e il gate sui dati non se ne
        // accorge (dice 0 problemi) perché il difetto nasce nella VOXELIZZAZIONE,
        // non nella geometria dichiarata.
        //
        // Cosa è già stato trovato e corretto lungo la strada — tutto vero, e tutto
        // insufficiente da solo:
        //   · il pianerottolo deve coprire ENTRAMBE le corsie (2w), o l'erosione le
        //     stacca di 0,80 m;
        //   · ma non deve essere profondo il doppio all'indietro, o seppellisce gli
        //     ultimi gradini della rampa che arriva;
        //   · le rampe vanno lasciate SOSPESE: renderle piene dal suolo fa sì che la
        //     rampa di ritorno murasse il pianerottolo da cui parte;
        //   · il muro d'anima va SOLO fra le rampe, mai dentro i pianerottoli: lì si
        //     attraversa da una corsia all'altra;
        //   · la corsia vuole larghezza da CORRIDOIO (2,40), non da scala (1,60).
        //
        // Restano irrisolti tre casi: due sole rampe (l'uscita finisce sopra
        // l'ingresso e il franco cade sul limite di `walkableHeight`), molte rampe,
        // e le rotazioni non ortogonali. Vanno affrontati con una passata dedicata,
        // non a tentativi. Nel frattempo una torre si costruisce con
        // `platform` + `stair` per livello, che sono verificate.
        //
        // ── VANO SCALA a rampe alternate ──────────────────────────────────
        // Non "due rampanti": **quante rampe servono**, dentro un ingombro in
        // pianta che NON cambia con l'altezza. È ciò che lo rende il pezzo con cui
        // si fa la scala interna di una torre — 4 m o 20 m, la pianta è la stessa.
        //
        // Pianta: due corsie affiancate (larghe `w` ciascuna) dentro 2w × (R + w).
        // Le rampe pari salgono lungo +Z sulla corsia sinistra, le dispari tornano
        // lungo −Z sulla destra; fra una e l'altra un pianerottolo che occupa
        // **tutta** la larghezza, così c'è spazio vero per girarsi.
        //
        // La versione precedente metteva la seconda rampa *di taglio* contro il
        // bordo del pianerottolo: ci si saliva solo di fianco, e due vani scala non
        // si potevano impilare. Segnalato dall'utente e rifatto (2026-08-05).
        // Larghezza CLAMPATA al minimo di vano scala: sotto quella soglia la torre
        // non e percorribile, quindi il difetto va reso inesprimibile come l alzata.
        float w = (s.width > 0.1f) ? s.width : mapmetrics::STAIRWELL_MIN_WIDTH;
        if (w < mapmetrics::STAIRWELL_MIN_WIDTH) w = mapmetrics::STAIRWELL_MIN_WIDTH;
        const float tr = effectiveTread(s);
        const float maxF = (s.flightRise > 0.1f) ? s.flightRise : 3.0f;
        int nf = (int)std::ceil(s.rise / maxF);
        if (nf < 2) nf = 2;                       // un vano scala ha almeno due rampe
        const float rF = s.rise / (float)nf;      // dislivello per rampa
        const float R  = (float)mapmetrics::stepsFor(rF, effectiveRiser(s)) * tr;

        const float laneL = -w * 0.5f, laneR = w * 0.5f;
        for (int k = 0; k < nf; ++k)
        {
            const bool  upZ  = (k % 2 == 0);
            const float base = s.y + rF * (float)k;
            // Pianerottolo di PARTENZA della rampa: occupa tutta la larghezza (2w),
            // profondo `w`. La rampa ci sale sopra sovrapponendosi — due superfici
            // a quote diverse che si toccano solo sul bordo verrebbero separate
            // dall'erosione del navmesh (0,40 m per lato).
            // I pianerottoli sono SOLAI SOTTILI, non blocchi pieni dal suolo: un
            // vano scala è cavo. Con blocchi pieni il pianerottolo d'uscita — che in
            // un numero pari di rampe torna sopra l'ingresso, com'è giusto — murava
            // l'ingresso stesso, e tutte e tre le torri di prova risultavano
            // irraggiungibili (misurato 2026-08-05).
            constexpr float kLand = 0.30f;
            // Il pianerottolo SBORDA di due pedate oltre l'inizio della rampa
            // successiva. Non è margine estetico: il primo gradino di una rampa che
            // parte sul CIGLIO ha il vuoto davanti, `rcFilterLedgeSpans` lo tratta
            // come strapiombo e lo scarta — la salita si fermava lì (misurato con
            // cinque sonde lungo la torre: pianerottolo raggiungibile, prima rampa
            // successiva no). Con il pianerottolo che lo circonda, il vicino oltre il
            // gradino è pavimento 0,20 m più in basso, non un salto nel vuoto.
            // Pianerottolo QUADRATO 2w × 2w, esteso **oltre** la fine della rampa che
            // arriva (mai all'indietro: estenderlo indietro seppellirebbe gli ultimi
            // gradini sotto il proprio ripiano). Serve tutto questo spazio perché sul
            // pianerottolo bisogna **attraversare da una corsia all'altra**, e la
            // striscia libera dev'essere più larga di quanto l'erosione del navmesh
            // toglie (0,40 m per lato): con un pianerottolo stretto la traversata
            // spariva e la torre si interrompeva alla seconda rampa.
            const float wL = w * 2.0f;
            if (k > 0)
                out.push_back(detail::slab(f, s, 0.0f,
                                           upZ ? (w - wL * 0.5f) : (R + wL * 0.5f),
                                           w * 2.0f, wL, base - kLand, base, BoxType::Floor));
            StructureDef fl = s;
            fl.ry = s.ry + (upZ ? 0.0f : 180.0f);
            // Origine della rampa: sulla propria corsia, al bordo del pianerottolo.
            const float lx = upZ ? laneL : laneR;
            const float lz = upZ ? 0.0f  : (R + w);
            fl.x = f.wx(lx, lz);
            fl.z = f.wz(lx, lz);
            const detail::Frame ff = detail::frameOf(fl.x, fl.z, fl.ry);
            // Rampe SOSPESE, non piene dal suolo: rendendole massicce (provato
            // 2026-08-05) la rampa di ritorno seppelliva il pianerottolo da cui
            // parte, e la salita si interrompeva ancora prima. Un vano scala è cavo.
            detail::emitFlight(ff, fl, base, rF, w, 0.0f, out);
        }
        // ── MURO D'ANIMA fra le due corsie ────────────────────────────────
        // Non è decorazione: senza, ogni gradino ha il VUOTO di fianco (la corsia
        // accanto sta 1-2 m più in basso), `rcFilterLedgeSpans` tratta le celle di
        // bordo come strapiombo e le toglie; sommata all'erosione, una rampa da
        // 1,6 m restava troppo sottile perché il navmesh la percorresse, e la torre
        // si interrompeva alla terza rampa. Un vano scala vero ha il muro d'anima
        // proprio lì: la soluzione strutturale coincide con quella architettonica.
        // Il muro sta SOLO fra le rampe (lz da w a R), mai dentro i pianerottoli:
        // è lì che si attraversa da una corsia all'altra, e un muro che li taglia
        // chiude proprio il passaggio che deve restare libero (provato: la torre si
        // bloccava al primo pianerottolo).
        const float spineLo = w, spineHi = R;
        if (spineHi - spineLo > 0.4f)
        {
            const float th = mapmetrics::WALL_THICKNESS;
            const float d  = spineHi - spineLo;
            out.push_back(detail::slab(f, s, 0.0f, (spineLo + spineHi) * 0.5f, th, d,
                                       s.y, s.y + s.rise + 0.2f, BoxType::Wall));
            // Pareti esterne del vano (opzionali): chiudono la tromba e tolgono anche
            // i cigli esterni. Servono quando il vano sta dentro una torre.
            if (s.railing)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                    out.push_back(detail::slab(f, s, sgn * (w + th * 0.5f),
                                               (spineLo + spineHi) * 0.5f, th, d,
                                               s.y, s.y + s.rise + 0.2f, BoxType::Wall));
        }
        // Pianerottolo di ARRIVO: è l'uscita del vano scala, e senza di lui l'ultimo
        // gradino sarebbe un ripiano profondo 30 cm su cui non ci si sta.
        const bool lastUp = ((nf - 1) % 2 == 0);
        const float wLe = w * 2.0f;
        out.push_back(detail::slab(f, s, 0.0f,
                                   lastUp ? (R + wLe * 0.5f) : (w - wLe * 0.5f),
                                   w * 2.0f, wLe,
                                   s.y + s.rise - 0.30f, s.y + s.rise, BoxType::Floor));
        break;
    }

    case Kind::Room:
    {
        // Il modulo più ripetuto di qualunque interno (doc 47 §2.3): pavimento,
        // quattro muri, soffitto opzionale. `access[i]` apre una PORTA sul lato i,
        // con le misure normative — così un interno non nasce senza vie d'ingresso.
        const float h   = (s.height > 0.01f) ? s.height : mapmetrics::WALL_HEIGHT;
        const float th  = (s.thickness > 0.01f) ? s.thickness : mapmetrics::WALL_THICKNESS;
        const float ow  = (s.openW > 0.01f) ? s.openW : mapmetrics::DOOR_WIDTH;
        const float oh  = (s.openH > 0.01f) ? s.openH : mapmetrics::DOOR_HEIGHT;
        constexpr float kSlabT = 0.30f;
        const float hx = s.sizeX * 0.5f, hz = s.sizeZ * 0.5f;

        out.push_back(detail::slab(f, s, 0.0f, 0.0f, s.sizeX, s.sizeZ,
                                   s.y - kSlabT, s.y, BoxType::Floor));
        // Lati: -Z, +Z, -X, +X (stessa convenzione di Platform).
        const float sx[4] = { 0.0f, 0.0f, -hx, hx };
        const float sz[4] = { -hz,  hz,  0.0f, 0.0f };
        const float sr[4] = { 0.0f, 0.0f, 90.0f, 90.0f };
        const float sl[4] = { s.sizeX, s.sizeX, s.sizeZ, s.sizeZ };
        for (int i = 0; i < 4; ++i)
            detail::emitWallWithOpening(f, s, sx[i], sz[i], sl[i], s.ry + sr[i],
                                        s.y, h, th,
                                        s.access[i] ? ow : 0.0f, oh,
                                        s.access[i] ? s.openSill : 0.0f,
                                        s.openOff, out);
        if (s.ceiling)
            out.push_back(detail::slab(f, s, 0.0f, 0.0f, s.sizeX, s.sizeZ,
                                       s.y + h, s.y + h + kSlabT, BoxType::Platform));
        break;
    }

    case Kind::Catwalk:
    {
        // Una passerella non è decorazione: è un CORRIDOIO IN QUOTA, cioè una corsia
        // tattica che domina il piano di sotto. `y` è la quota calpestabile.
        constexpr float kDeck = 0.25f;
        const float w  = (s.width > 0.1f) ? s.width : mapmetrics::CORRIDOR_MIN;
        const float ln = (s.length > 0.01f) ? s.length : 8.0f;
        out.push_back(detail::slab(f, s, 0.0f, 0.0f, ln, w,
                                   s.y - kDeck, s.y, BoxType::Platform));
        if (s.railing)
        {
            // Parapetto ad altezza di copertura BASSA: ripara accovacciati e si
            // spara sopra. ⚠ Un parapetto toglie però la visuale verso il basso
            // (KI #83, "posizione cieca verso le altre quote"): è opzionale apposta.
            const float rh = mapmetrics::COVER_LOW;
            for (int side = -1; side <= 1; side += 2)
                out.push_back(detail::slab(f, s, 0.0f, side * (w * 0.5f - 0.1f),
                                           ln, 0.20f, s.y, s.y + rh, BoxType::Cover));
        }
        break;
    }

    case Kind::Barricade:
    {
        // Linea di coperture a intervalli: la struttura da campo di battaglia per
        // eccellenza. Emette box di tipo `cover`, che è ciò che la derivazione dei
        // metadata (doc 46) cerca — quindi non è solo geometria, è terreno tattico.
        const float ln   = (s.length > 0.01f) ? s.length : 8.0f;
        const float h    = (s.height > 0.01f) ? s.height : mapmetrics::COVER_LOW;
        const float th   = (s.thickness > 0.01f) ? s.thickness : 0.6f;
        const float seg  = (s.width > 0.1f) ? s.width : 2.0f;    // lunghezza di un elemento
        const float step = (s.spacing > 0.01f) ? (seg + s.spacing) : seg;
        const int   n    = (int)std::floor(ln / step + 0.001f);
        for (int i = 0; i < std::max(1, n); ++i)
        {
            const float lx = -ln * 0.5f + step * ((float)i + 0.5f);
            out.push_back(detail::slab(f, s, lx, 0.0f, seg, th,
                                       s.y, s.y + h, BoxType::Cover));
        }
        break;
    }

    case Kind::Platform:
    {
        // Il ripiano.
        constexpr float kSlab = 0.40f;
        MapGeometryBox top;
        top.x  = s.x; top.z = s.z;
        top.sy = kSlab;  top.y = s.y - kSlab * 0.5f;   // `y` = quota CALPESTABILE
        top.sx = s.sizeX; top.sz = s.sizeZ;
        top.ry = s.ry;
        top.r = s.color[0]; top.g = s.color[1]; top.b = s.color[2];
        top.collider = true;
        top.type = BoxType::Platform;
        top.fromStructure = true;
        out.push_back(top);

        // Gli ACCESSI: una scala per lato dichiarato, che parte da `baseY` e arriva
        // esattamente al ripiano. Non è un accessorio — è ciò che impedisce a una
        // piattaforma di nascere irraggiungibile.
        const float rise = s.y - s.baseY;
        if (rise <= config::STEP_HEIGHT) break;       // ci si sale già camminando

        StructureDef st = s;
        st.kind  = Kind::Stair;
        st.rise  = rise;
        st.width = mapmetrics::STAIR_MIN_WIDTH;
        const float run = flightRun(st);

        // Lati locali: 0 = -Z, 1 = +Z, 2 = -X, 3 = +X. La scala sale VERSO il ripiano,
        // quindi parte fuori dal bordo e finisce contro di esso.
        const float halfZ = s.sizeZ * 0.5f, halfX = s.sizeX * 0.5f;
        const float sideYaw[4] = { 0.0f, 180.0f, 90.0f, 270.0f };
        const float offX[4]    = { 0.0f, 0.0f, -halfX - run, halfX + run };
        const float offZ[4]    = { -halfZ - run, halfZ + run, 0.0f, 0.0f };

        for (int i = 0; i < 4; ++i)
        {
            if (!s.access[i]) continue;
            StructureDef fl = st;
            fl.ry = s.ry + sideYaw[i];
            fl.x  = f.wx(offX[i], offZ[i]);
            fl.z  = f.wz(offX[i], offZ[i]);
            fl.y  = s.baseY;
            const detail::Frame ff = detail::frameOf(fl.x, fl.z, fl.ry);
            detail::emitFlight(ff, fl, fl.y, rise, fl.width, 0.0f, out);
        }
        break;
    }
    }
}

} // namespace mini::mapstructures

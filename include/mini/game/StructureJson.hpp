#pragma once
#include "mini/game/data/Definitions.hpp"
#include "mini/game/MapStructures.hpp"
#include <nlohmann/json.hpp>

// ── LA RICETTA DI UNA STRUTTURA ⇄ JSON, IN UN POSTO SOLO ────────────────────
//
// Perché esiste: il campo `type` (ADR-056) era stato aggiunto al lettore del
// registry e **non** a quello dell'editor. Conseguenza: una struttura composita
// caricata in mappa perdeva il legame col tipo, tornava a essere la primitiva nuda,
// e il salvataggio successivo rendeva la perdita PERMANENTE. Non un difetto di
// visualizzazione — perdita di dati.
//
// Era la terza volta che un campo nuovo arrivava in un lettore su due. La causa non
// è la distrazione: sono due lettori. Qui ce n'è **uno**, e chi aggiunge un campo
// lo aggiunge dove entrambi lo vedono.
//
// Stesso principio di ADR-018 (un solo gate), ADR-032 (una sola LOS), ADR-053 (una
// sola espansione): *una verità sola*.
namespace mini::structjson
{

// Dichiarate qui perché una struttura può portare le sue PARTI LOCALI (istanza
// modificata, ADR-056 rivisto) e una parte porta uno StructureDef: le due funzioni
// si chiamano a vicenda. La ricorsione termina perché la posa dentro una parte non
// ha mai parti locali proprie.
inline StructurePart partFromJson(const nlohmann::json& pj);
inline nlohmann::json partToJson(const StructurePart& p);

inline StructureDef fromJson(const nlohmann::json& s)
{
    StructureDef d;
    d.kind  = mapstructures::parseKind(s.value("kind", std::string("stair")));
    d.label = s.value("label", std::string(""));
    d.type  = s.value("type", std::string(""));   // ADR-056: il legame col TIPO
    d.x = s.value("x", 0.0f);  d.y = s.value("y", 0.0f);  d.z = s.value("z", 0.0f);
    d.ry         = s.value("ry", 0.0f);
    d.rise       = s.value("rise", 2.0f);
    d.width      = s.value("width", 2.0f);
    d.riser      = s.value("riser", 0.0f);
    d.tread      = s.value("tread", 0.0f);
    d.length     = s.value("length", 4.0f);
    d.height     = s.value("height", 0.0f);
    d.thickness  = s.value("thickness", 0.0f);
    d.sizeX      = s.value("size_x", 6.0f);
    d.sizeZ      = s.value("size_z", 6.0f);
    d.baseY      = s.value("base_y", 0.0f);
    d.openW      = s.value("open_w", 0.0f);
    d.openH      = s.value("open_h", 0.0f);
    d.openSill   = s.value("open_sill", 0.0f);
    d.openOff    = s.value("open_off", 0.0f);
    d.flightRise = s.value("flight_rise", 0.0f);
    d.spacing    = s.value("spacing", 0.0f);
    d.ceiling    = s.value("ceiling", false);
    d.railing    = s.value("railing", false);
    if (s.contains("access") && s["access"].is_array())
        for (std::size_t i = 0; i < 4 && i < s["access"].size(); ++i)
            d.access[i] = s["access"][i].get<bool>();
    d.color[0] = s.value("r", 0.35f);
    d.color[1] = s.value("g", 0.32f);
    d.color[2] = s.value("b", 0.28f);
    // Parti LOCALI: questa istanza è una versione modificata del suo tipo. Assente =
    // istanza normale, e nessuna mappa esistente cambia di un byte.
    if (s.contains("local_parts") && s["local_parts"].is_array())
        for (const auto& pj : s["local_parts"]) d.localParts.push_back(partFromJson(pj));
    return d;
}

inline nlohmann::json toJson(const StructureDef& s)
{
    nlohmann::json o;
    o["kind"]  = mapstructures::kindName(s.kind);
    o["label"] = s.label;
    // Solo se c'è: un'istanza senza tipo resta scritta come prima, e le mappe
    // esistenti non cambiano di un byte (ADR-056, fallback).
    if (!s.type.empty()) o["type"] = s.type;
    o["x"] = s.x;  o["y"] = s.y;  o["z"] = s.z;  o["ry"] = s.ry;
    o["rise"] = s.rise;  o["width"] = s.width;
    o["riser"] = s.riser;  o["tread"] = s.tread;
    o["length"] = s.length;  o["height"] = s.height;  o["thickness"] = s.thickness;
    o["size_x"] = s.sizeX;  o["size_z"] = s.sizeZ;  o["base_y"] = s.baseY;
    o["open_w"] = s.openW;  o["open_h"] = s.openH;
    o["open_sill"] = s.openSill;  o["open_off"] = s.openOff;
    o["flight_rise"] = s.flightRise;  o["spacing"] = s.spacing;
    o["ceiling"] = s.ceiling;  o["railing"] = s.railing;
    o["access"] = { s.access[0], s.access[1], s.access[2], s.access[3] };
    o["r"] = s.color[0];  o["g"] = s.color[1];  o["b"] = s.color[2];
    if (!s.localParts.empty())
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& p : s.localParts) arr.push_back(partToJson(p));
        o["local_parts"] = std::move(arr);
    }
    return o;
}

// ── UN BOX ⇄ JSON ───────────────────────────────────────────────────────────
// Stesso schema che stia in una mappa, in un prefab o dentro una parte di
// assemblaggio: era già la regola (ADR-048), ma il lettore stava nel registry e lo
// scrittore nell'editor. Qui stanno accanto, così un campo nuovo si vede mancante.
inline MapGeometryBox boxFromJson(const nlohmann::json& gb)
{
    MapGeometryBox b;
    b.x  = gb.value("x",  0.0f);   b.y  = gb.value("y",  0.0f);
    b.z  = gb.value("z",  0.0f);   b.ry = gb.value("ry", 0.0f);
    b.sx = gb.value("sx", 2.0f);   b.sy = gb.value("sy", 2.0f);
    b.sz = gb.value("sz", 2.0f);
    b.r  = gb.value("r", 0.35f);   b.g  = gb.value("g", 0.32f);
    b.b  = gb.value("b", 0.28f);
    b.collider = gb.value("collider", true);
    b.type = parseBoxType(gb.value("type", std::string("wall")));
    return b;
}

inline nlohmann::json boxToJson(const MapGeometryBox& b)
{
    nlohmann::json o;
    o["x"] = b.x;  o["y"] = b.y;  o["z"] = b.z;  o["ry"] = b.ry;
    o["sx"] = b.sx;  o["sy"] = b.sy;  o["sz"] = b.sz;
    o["r"] = b.r;  o["g"] = b.g;  o["b"] = b.b;
    o["collider"] = b.collider;
    o["type"] = boxTypeName(b.type);
    return o;
}

// ── UNA PARTE DI ASSEMBLAGGIO ⇄ JSON ────────────────────────────────────────
// Le parti avevano DUE serializzatori (registry in lettura, editor in scrittura):
// la stessa configurazione che ha già causato la perdita del campo `type`. Sono
// diventati uno solo prima di aggiungere `ref` — perché `ref` sarebbe stato il
// quarto campo ad arrivare in un lettore su due.
//
// Discriminatore `part`, NON `type`: `type` è già preso due volte — dalla semantica
// del box (`floor`/`wall`/...) e dall'id del tipo di una struttura.
inline StructurePart partFromJson(const nlohmann::json& pj)
{
    StructurePart p;
    p.label   = pj.value("label", std::string(""));
    p.isBox   = (pj.value("part", std::string("prim")) == "box");
    // Le parti locali di un riferimento ISOLATO E MODIFICATO stanno accanto al `ref`,
    // non dentro la posa: `fromJson` legge la posa e non deve vederle due volte.
    if (pj.contains("local_parts") && pj["local_parts"].is_array())
        for (const auto& sp : pj["local_parts"]) p.localParts.push_back(partFromJson(sp));
    // `ref` = questa parte È un altro tipo composito, non una copia delle sue parti
    // (ADR-056 rivisto). La posa sta comunque in `prim.x/y/z/ry`, che si rileggono
    // con il parser di sempre: un riferimento è una primitiva con un nome sopra.
    p.refType = pj.value("ref", std::string(""));
    if (p.isBox) p.box  = boxFromJson(pj);
    else         p.prim = fromJson(pj);
    // La posa di una parte condivide l'oggetto JSON con la parte stessa, quindi
    // `fromJson` ha appena riletto le STESSE `local_parts` dentro `prim`. Lì non
    // significano niente e le espanderebbero due volte: si azzerano subito. È il
    // prezzo di riusare un solo parser invece di scriverne un secondo.
    p.prim.localParts.clear();
    return p;
}

inline nlohmann::json partToJson(const StructurePart& p)
{
    nlohmann::json o = p.isBox ? boxToJson(p.box) : toJson(p.prim);
    o["label"] = p.label;
    o["part"]  = p.isBox ? "box" : "prim";
    if (!p.refType.empty()) o["ref"] = p.refType;
    if (!p.localParts.empty())
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& sp : p.localParts) arr.push_back(partToJson(sp));
        o["local_parts"] = std::move(arr);
    }
    return o;
}

} // namespace mini::structjson

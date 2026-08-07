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
    return o;
}

} // namespace mini::structjson

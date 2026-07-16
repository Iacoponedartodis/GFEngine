#pragma once
namespace mini
{

struct HealthComponent
{
    float current = 100.0f;
    float max     = 100.0f;
    // Divisore del danno subito. 1 = nessuna riduzione (default = comportamento
    // storico). Popolato dalle stat del personaggio (PlayerDef.armor_rating,
    // KI #35); generico per costruzione, così un giorno può valere anche per le AI.
    float armor   = 1.0f;
};

} // namespace mini
#pragma once

namespace mini
{

// Scudo energetico (ability type "shield", 16_AiBehavior).
// Assorbe il danno prima degli HP; si rigenera dopo regenDelay
// secondi dall'ultimo colpo subito. Parametri risolti allo spawn
// dall'AbilityDef (param1=HP scudo, param2=regen/s, param3=delay).
struct ShieldComponent
{
    float max        = 0.0f;
    float current    = 0.0f;
    float regenRate  = 0.0f;   // HP scudo recuperati al secondo
    float regenDelay = 3.0f;   // attesa dopo un colpo prima della regen
    float timer      = 0.0f;   // countdown verso l'inizio della regen
};

} // namespace mini

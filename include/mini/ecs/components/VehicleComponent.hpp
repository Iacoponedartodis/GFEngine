#pragma once
#include "mini/ecs/Entity.hpp"

namespace mini
{

// Veicolo guidabile (19_Vehicles, Fase A). Parametri risolti allo spawn
// dal VehicleDef; stato di guida runtime. Il movimento usa lo stesso
// slide/step-up della fanteria (nessuna fisica dedicata in Fase A).
struct VehicleComponent
{
    // Dal VehicleDef
    float maxSpeed    = 12.0f;
    float accel       = 10.0f;
    float turnRateDeg = 90.0f;
    float halfX = 0.7f, halfY = 0.5f, halfZ = 1.2f;

    // Stato runtime
    float    speed  = 0.0f;   // velocità corrente (negativa in retro)
    EntityId driver = 0;      // 0 = libero
    float    velY   = 0.0f;   // gravità
};

} // namespace mini

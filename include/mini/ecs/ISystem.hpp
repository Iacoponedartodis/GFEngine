#pragma once

namespace mini
{

class World;

class ISystem
{
public:
    virtual ~ISystem() = default;

    virtual void update(World& world, float deltaTime) = 0;

    // Nome della zona di profilazione (ADR-050). Il profiler usa il PUNTATORE
    // come chiave, quindi deve essere un letterale: un `typeid().name()` non lo
    // garantisce ed è illeggibile. Il default rende visibile chi non l'ha
    // dichiarato invece di nasconderlo in un'etichetta plausibile.
    virtual const char* name() const { return "system(senza nome)"; }
};

} // namespace mini
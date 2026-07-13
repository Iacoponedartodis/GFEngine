#pragma once
// VehicleDrive.hpp — fisica e camera della guida veicoli (19_Vehicles).
// Estratto da Application.cpp (R2): Application gestisce solo mount/dismount
// e i messaggi; qui vive il "come si muove" un mezzo guidato.

#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/core/Telemetry.hpp"
#include "mini/render/Camera.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace mini::vehicledrive
{

// Un tick di guida: input WASD, sterzo, slide/step-up (escludendo il proprio
// collider), gravità e camera (FPS al posto di guida o TPS dietro il mezzo).
// Ritorna false se il veicolo non esiste più (distrutto sotto il pilota).
inline bool update(World& world, EntityId vehicle, Camera& cam,
                   float dt, bool thirdPerson, int& traceCounter)
{
    if (!world.isValidEntity(vehicle)) return false;
    auto* vc = world.getVehicle(vehicle);
    auto* vt = world.getTransform(vehicle);
    if (!vc || !vt) return false;

    const Uint8* kb = SDL_GetKeyboardState(nullptr);
    float target = 0.0f;
    if (kb[SDL_SCANCODE_W]) target =  vc->maxSpeed;
    if (kb[SDL_SCANCODE_S]) target = -vc->maxSpeed * 0.5f;
    // Accelera verso il target, frena più in fretta
    if (vc->speed < target)
        vc->speed = std::min(target, vc->speed + vc->accel * dt);
    else
        vc->speed = std::max(target, vc->speed - vc->accel * 1.5f * dt);

    // Sterzo (efficace solo in movimento; invertito in retro)
    const float steer = (kb[SDL_SCANCODE_A] ? 1.0f : 0.0f)
                      - (kb[SDL_SCANCODE_D] ? 1.0f : 0.0f);
    if (std::abs(vc->speed) > 0.3f)
        vt->ry += steer * vc->turnRateDeg * dt
                * (vc->speed < 0.0f ? -1.0f : 1.0f);

    const float yr = glm::radians(vt->ry);
    const glm::vec3 fwd  = {std::sin(yr), 0.0f, std::cos(yr)};
    const glm::vec3 prev = {vt->x, vt->y, vt->z};
    const glm::vec3 next = prev + fwd * (vc->speed * dt);

    // Box di collisione che tiene conto dello YAW: la fisica tratta il box in
    // movimento come allineato agli assi del mondo, ma lo speeder è lungo
    // (halfZ >> halfX). Senza questo, i 5m di lunghezza restano puntati lungo
    // l'asse Z del mondo anche quando il mezzo gira → "muro invisibile" ai
    // lati. Usa l'AABB avvolgente della sagoma ruotata (esatto a 0/90°).
    const float ca = std::abs(std::cos(yr)), sa = std::abs(std::sin(yr));
    const glm::vec3 H = { vc->halfX * ca + vc->halfZ * sa,
                          vc->halfY,
                          vc->halfX * sa + vc->halfZ * ca };
    const glm::vec3 r    = physics::slideMoveWithStepUp(
        prev, {next.x, prev.y, next.z}, H, world,
        config::STEP_HEIGHT, vehicle);
    vt->x = r.x; vt->y = r.y; vt->z = r.z;

    // Gravità (stesso modello della fanteria)
    vc->velY += config::GRAVITY * dt;
    const float ny = vt->y + vc->velY * dt;
    if (!physics::hasCollision({vt->x, ny, vt->z}, H, world, vehicle))
        vt->y = ny;
    else if (vc->velY < 0.0f)
        vc->velY = 0.0f;

    // Camera: SEMPRE orientata lungo la direzione di marcia (fwd), sia in
    // prima che in terza persona. Prima, in prima persona, la camera restava
    // col mouse: lo sterzo sembrava invertito perché "destra/sinistra" erano
    // relativi allo sguardo, non al mezzo (bug 2026-07-11). Ora la vista
    // segue il veicolo, quindi A = sinistra, D = destra, coerenti.
    if (thirdPerson)
    {
        const glm::vec3 back = {-fwd.x, 0.0f, -fwd.z};
        cam.setPosition({vt->x + back.x * 6.0f,
                         vt->y + 3.0f,
                         vt->z + back.z * 6.0f});
        cam.lookAt({vt->x, vt->y + 1.0f, vt->z});
    }
    else
    {
        const glm::vec3 eye{vt->x, vt->y + config::VEHICLE_EYE_HEIGHT, vt->z};
        cam.setPosition(eye);
        cam.lookAt({eye.x + fwd.x, eye.y, eye.z + fwd.z});
    }

    // Telemetria di guida (~2 volte/s): rende diagnosticabile
    // "il veicolo non si muove" direttamente dal log.
    if (++traceCounter >= 30)
    {
        traceCounter = 0;
        const bool blocked = std::abs(vc->speed) > 1.0f
            && std::abs(r.x - prev.x) < 0.001f
            && std::abs(r.z - prev.z) < 0.001f;
        telemetry::logTrace("drive: v=" + std::to_string(vc->speed)
            + " pos=(" + std::to_string(vt->x) + ","
            + std::to_string(vt->z) + ")"
            + (blocked ? " BLOCCATO" : ""));
    }
    return true;
}

} // namespace mini::vehicledrive

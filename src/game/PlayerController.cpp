#include "mini/game/PlayerController.hpp"
#include "mini/core/Audio.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/core/InputManager.hpp"
#include "mini/ecs/World.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Mesh.hpp"

#include <iostream>

namespace mini
{

void PlayerController::reset(EntityId e, float hp, const glm::vec3& spawnPos, Camera& cam)
{
    entity       = e;
    velY         = 0.0f;
    onGround     = true;
    airVelX      = 0.0f;
    airVelZ      = 0.0f;
    prevHp       = hp;
    isDead       = false;
    respawnTimer = -1.0f;
    weapon.reset();
    cam.setPosition(spawnPos);
    cam.lookAt({spawnPos.x, spawnPos.y, spawnPos.z - 10.0f});
}

void PlayerController::updateMovement(Camera& cam, const InputManager& input,
                                       World& world, float elapsed)
{
    if (isDead) return;

    using namespace config;
    const glm::vec3 prevPos = cam.getPosition();
    const glm::vec3 PHALF   = playerHalf();

    if (onGround)
    {
        cam.processKeyboard(
            input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
            input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
            false, false, elapsed);
        cam.setPosition({cam.getPosition().x, prevPos.y, cam.getPosition().z});

        const glm::vec3 movePos = cam.getPosition();
        if (elapsed > 0.0001f)
        {
            airVelX = (movePos.x - prevPos.x) / elapsed;
            airVelZ = (movePos.z - prevPos.z) / elapsed;
        }

        if (input.isDown(Action::Jump))
        {
            velY     = JUMP_IMPULSE;
            onGround = false;
        }
    }
    else
    {
        // In aria: mantiene direzione del salto
        cam.setPosition({prevPos.x + airVelX * elapsed,
                         prevPos.y,
                         prevPos.z + airVelZ * elapsed});
    }

    // Gravità + sliding
    velY += GRAVITY * elapsed;
    const glm::vec3 target = {cam.getPosition().x,
                               cam.getPosition().y + velY * elapsed,
                               cam.getPosition().z};
    const glm::vec3 final_ = physics::slideMoveWithStepUp(
        prevPos, target, PHALF, world, STEP_HEIGHT);

    if (velY < 0.0f && final_.y > target.y + 0.001f)
    {
        velY     = 0.0f;
        onGround = true;
    }
    cam.setPosition(final_);
}

bool PlayerController::updateHealth(World& world, Audio& audio)
{
    if (isDead || !world.isValidEntity(entity)) return false;

    auto* pt = world.getTransform(entity);
    if (pt)
    {
        // Sincronizza posizione camera → entità
        // (non usiamo cam qui, la posizione è già stata aggiornata in updateMovement)
        // Application deve chiamare questo DOPO updateMovement
    }

    const auto* hp = world.getHealth(entity);
    if (!hp) return false;

    if (hp->current <= 0.0f && prevHp > 0.0f)
    {
        isDead = true;
        prevHp = hp->current;
        return true; // appena morto
    }

    if (hp->current < prevHp - 0.5f)
        audio.playHit();

    prevHp = hp->current;
    return false;
}

bool PlayerController::updateShooting(World& world, Camera& cam, const InputManager& input,
                                       Audio& audio, Mesh* bulletMesh, bool mouseCaptured)
{
    if (isDead || !mouseCaptured) return false;
    if (!input.isDown(Action::Shoot)) return false;
    if (!weapon.tryFire()) return false;

    audio.playShoot();
    const glm::vec3 org = cam.getPosition();
    const glm::vec3 fwd = cam.getForward();

    EntityId b = world.createEntity();
    world.addTransform(b, TransformComponent{
        .x = org.x, .y = org.y, .z = org.z,
        .sx = weapon.bulletScale, .sy = weapon.bulletScale, .sz = weapon.bulletScale
    });
    world.addVelocity(b, {
        fwd.x * weapon.bulletSpeed,
        fwd.y * weapon.bulletSpeed,
        fwd.z * weapon.bulletSpeed
    });
    world.addTeam(b, {1});
    world.addBullet(b, {weapon.bulletDamage, weapon.bulletLifetime, 1});
    if (bulletMesh)
        world.addMeshRenderer(b, {bulletMesh, nullptr,
                                   weapon.bulletR, weapon.bulletG, weapon.bulletB});
    return true;
}

bool PlayerController::updateRespawn(World& world, Camera& cam,
                                      float respawnDelay, const glm::vec3& spawnPos, float maxHp)
{
    // Il timer è già stato decrementato e verificato da Application.
    // Questa funzione esegue il respawn effettivo.
    respawnTimer = -1.0f;
    isDead       = false;

    entity = world.createEntity();
    world.addTransform(entity, {spawnPos.x, spawnPos.y, spawnPos.z});
    world.addTeam(entity, {1});
    world.addHealth(entity, {maxHp, maxHp});
    prevHp   = maxHp;
    velY     = 0.0f;
    onGround = true;
    cam.setPosition(spawnPos);

    std::cout << "[Respawn] Giocatore rinato!" << std::endl;
    return true;
}

} // namespace mini
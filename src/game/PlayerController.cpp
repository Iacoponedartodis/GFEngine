#include "mini/game/PlayerController.hpp"
#include "mini/core/Audio.hpp"
#include "mini/core/GameConfig.hpp"
#include "mini/core/InputManager.hpp"
#include "mini/ecs/World.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Mesh.hpp"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <iostream>

namespace mini
{

static constexpr float DEG2RAD = glm::pi<float>() / 180.0f;

// Costanti gameplay
static constexpr float SPRINT_MULT    = 1.65f;
static constexpr float CROUCH_MULT    = 0.55f;
static constexpr float AIM_MULT       = 0.75f;
static constexpr float ROLL_DURATION  = 0.32f;  // secondi
static constexpr float ROLL_SPEED     = 9.0f;   // m/s durante roll
static constexpr float CROUCH_HEIGHT  = 0.50f;  // PHALF.y accovacciato
static constexpr float STAND_HEIGHT   = config::PLAYER_HALF_Y;
static constexpr float ADS_FOV        = 35.0f;
static constexpr float NORMAL_FOV     = config::CAMERA_FOV;

// ── Reset ────────────────────────────────────────────────────────────────────

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

    isSprinting  = false;
    isCrouching  = false;
    isAiming     = false;
    isRolling    = false;
    rollTimer    = 0.0f;
    activeWeapon = 0;
    weapon       = weapons[0];   // ripristina primaria

    tpsPlayerPos = spawnPos;
    tpsYaw       = cam.getYaw();
    tpsPitch     = -10.0f;

    cam.setFov(NORMAL_FOV);
    cam.setPosition(spawnPos);
    cam.lookAt({spawnPos.x, spawnPos.y, spawnPos.z - 10.0f});
}

// ── Toggle TPS/FPS ────────────────────────────────────────────────────────────

void PlayerController::toggleThirdPerson(Camera& cam)
{
    thirdPerson = !thirdPerson;
    if (thirdPerson)
    {
        tpsPlayerPos = cam.getPosition();
        tpsYaw       = cam.getYaw();
        tpsPitch     = -10.0f;
        std::cout << "[Camera] Terza persona ON\n";
    }
    else
    {
        cam.setPosition({tpsPlayerPos.x, tpsPlayerPos.y, tpsPlayerPos.z});
        std::cout << "[Camera] Prima persona ON\n";
    }
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void PlayerController::processMouse(Camera& cam, float dx, float dy, float sensitivity)
{
    if (thirdPerson)
    {
        tpsYaw   += dx * sensitivity;
        tpsPitch -= dy * sensitivity;
        if (tpsPitch >  45.0f) tpsPitch =  45.0f;
        if (tpsPitch < -60.0f) tpsPitch = -60.0f;
    }
    else
    {
        cam.processMouse(dx, dy, sensitivity);
    }
}

// ── Camera TPS ───────────────────────────────────────────────────────────────

static void applyTpsCamera(Camera& cam, const glm::vec3& playerPos,
                            float yawDeg, float pitchDeg,
                            float distance, float height)
{
    float y  = yawDeg   * DEG2RAD;
    float p  = pitchDeg * DEG2RAD;
    float hDist = distance * std::cos(p);
    float vDist = distance * std::sin(p);

    glm::vec3 camPos = {
        playerPos.x - std::sin(y) * hDist,
        playerPos.y + height - vDist,
        playerPos.z + std::cos(y) * hDist
    };

    cam.setPosition(camPos);
    cam.lookAt({ playerPos.x, playerPos.y + 1.0f, playerPos.z });
}

// ── Movimento ─────────────────────────────────────────────────────────────────

void PlayerController::updateMovement(Camera& cam, const InputManager& input,
                                       World& world, float elapsed)
{
    if (isDead) return;

    using namespace config;

    // ── Cambia arma (pressed = bordo) ────────────────────────────────────────
    if (input.isPressed(Action::SwitchWeapon))
    {
        // passa all'altro slot solo se ha un'arma caricata (nome non vuoto)
        int other = activeWeapon == 0 ? 1 : 0;
        if (!weapons[other].name.empty())
        {
            activeWeapon = other;
            weapon = weapons[activeWeapon];
        }
    }

    // ── Crouch (toggle) ───────────────────────────────────────────────────────
    if (input.isPressed(Action::Crouch) && onGround && !isRolling)
        isCrouching = !isCrouching;

    // ── Aim / ADS ─────────────────────────────────────────────────────────────
    isAiming = input.isAimHeld();
    cam.setFov(isAiming ? ADS_FOV : NORMAL_FOV);

    // ── Roll (schivata) ───────────────────────────────────────────────────────
    const bool canRoll = onGround && !isRolling && !isCrouching;
    if (canRoll && input.isPressed(Action::Roll))
    {
        // Direzione roll = direzione movimento attuale o avanti se fermo
        float ry = thirdPerson ? tpsYaw * DEG2RAD : cam.getYaw() * DEG2RAD;
        float fwdX =  std::sin(ry), fwdZ = -std::cos(ry);
        float rgtX =  std::cos(ry), rgtZ =  std::sin(ry);

        rollVelX = 0.0f; rollVelZ = 0.0f;
        if (input.isDown(Action::MoveForward)) { rollVelX += fwdX; rollVelZ += fwdZ; }
        if (input.isDown(Action::MoveBack))    { rollVelX -= fwdX; rollVelZ -= fwdZ; }
        if (input.isDown(Action::MoveLeft))    { rollVelX -= rgtX; rollVelZ -= rgtZ; }
        if (input.isDown(Action::MoveRight))   { rollVelX += rgtX; rollVelZ += rgtZ; }
        // Se nessun tasto direzionale, schivata all'indietro
        if (rollVelX == 0.0f && rollVelZ == 0.0f) { rollVelX = -fwdX; rollVelZ = -fwdZ; }

        float len = std::sqrt(rollVelX*rollVelX + rollVelZ*rollVelZ);
        rollVelX = rollVelX / len * ROLL_SPEED;
        rollVelZ = rollVelZ / len * ROLL_SPEED;

        isRolling = true;
        rollTimer = ROLL_DURATION;
        isCrouching = false; // il roll si fa in piedi
    }

    if (isRolling)
    {
        rollTimer -= elapsed;
        if (rollTimer <= 0.0f) { isRolling = false; rollTimer = 0.0f; }
    }

    // ── Calcola moltiplicatore velocità ───────────────────────────────────────
    // Sprint: solo se in piedi, non mirano, non in roll, si sta muovendo in avanti
    const bool sprintInput = input.isDown(Action::Sprint);
    isSprinting = sprintInput && !isCrouching && !isAiming && !isRolling && onGround;

    float speedMult = 1.0f;
    if (isRolling)        speedMult = 1.0f;  // roll ha velocità propria
    else if (isSprinting) speedMult = SPRINT_MULT;
    else if (isCrouching) speedMult = CROUCH_MULT;
    else if (isAiming)    speedMult = AIM_MULT;

    // Hitbox height dinamico per crouch
    glm::vec3 PHALF = { PLAYER_HALF_X,
                         isCrouching ? CROUCH_HEIGHT : STAND_HEIGHT,
                         PLAYER_HALF_Z };

    if (thirdPerson)
    {
        // ── TPS ──────────────────────────────────────────────────────────────
        const float y = tpsYaw * DEG2RAD;
        const float fwdX =  std::sin(y), fwdZ = -std::cos(y);
        const float rgtX =  std::cos(y), rgtZ =  std::sin(y);

        glm::vec3 move = {0, 0, 0};

        if (isRolling)
        {
            move.x = rollVelX * elapsed;
            move.z = rollVelZ * elapsed;
        }
        else
        {
            const float spd = PLAYER_SPEED * speedMult;
            if (input.isDown(Action::MoveForward)) { move.x += fwdX; move.z += fwdZ; }
            if (input.isDown(Action::MoveBack))    { move.x -= fwdX; move.z -= fwdZ; }
            if (input.isDown(Action::MoveLeft))    { move.x -= rgtX; move.z -= rgtZ; }
            if (input.isDown(Action::MoveRight))   { move.x += rgtX; move.z += rgtZ; }

            const float len2 = move.x*move.x + move.z*move.z;
            if (len2 > 0.0001f)
            {
                const float inv = 1.0f / std::sqrt(len2);
                move.x *= inv * spd * elapsed;
                move.z *= inv * spd * elapsed;
            }
        }

        if (onGround && input.isDown(Action::Jump) && !isRolling && !isCrouching)
        {
            velY     = JUMP_IMPULSE;
            onGround = false;
            airVelX  = (elapsed > 0.0f) ? move.x / elapsed : 0.0f;
            airVelZ  = (elapsed > 0.0f) ? move.z / elapsed : 0.0f;
        }

        glm::vec3 prevPos  = tpsPlayerPos;
        glm::vec3 proposed;
        if (onGround)
            proposed = { prevPos.x + move.x, prevPos.y, prevPos.z + move.z };
        else
            proposed = { prevPos.x + airVelX * elapsed, prevPos.y, prevPos.z + airVelZ * elapsed };

        velY += GRAVITY * elapsed;
        glm::vec3 target = { proposed.x, proposed.y + velY * elapsed, proposed.z };
        glm::vec3 final_ = physics::slideMoveWithStepUp(prevPos, target, PHALF, world, STEP_HEIGHT);

        if (velY < 0.0f && final_.y > target.y + 0.001f)
        {
            velY     = 0.0f;
            onGround = true;
        }

        tpsPlayerPos = final_;
        applyTpsCamera(cam, tpsPlayerPos, tpsYaw, tpsPitch, tpsDistance, tpsHeight);
    }
    else
    {
        // ── FPS ──────────────────────────────────────────────────────────────
        const glm::vec3 prevPos = cam.getPosition();

        // Eye height: calcolata dal pavimento, non come offset accumulato ogni frame.
        // floor_y = cam.y - altezza_occhi_corrente (usando STAND_HEIGHT perché
        // physX lavora sempre con la posizione intera, non col crouch)
        const float eyeHeight = isCrouching ? CROUCH_HEIGHT : STAND_HEIGHT;
        const float floorY    = prevPos.y - STAND_HEIGHT;   // pavimento stimato
        const float targetEyeY = floorY + eyeHeight;

        if (onGround)
        {
            if (isRolling)
            {
                cam.setPosition({ prevPos.x + rollVelX * elapsed,
                                  targetEyeY,
                                  prevPos.z + rollVelZ * elapsed });
            }
            else
            {
                cam.setSpeed(PLAYER_SPEED * speedMult);
                cam.processKeyboard(
                    input.isDown(Action::MoveForward), input.isDown(Action::MoveBack),
                    input.isDown(Action::MoveLeft),    input.isDown(Action::MoveRight),
                    false, false, elapsed);
                cam.setSpeed(PLAYER_SPEED);
            }

            // Forza la Y alla eye height corretta (non si accumula tra frame)
            const glm::vec3 afterMove = cam.getPosition();
            cam.setPosition({ afterMove.x, targetEyeY, afterMove.z });

            const glm::vec3 movePos = cam.getPosition();
            if (elapsed > 0.0001f)
            {
                airVelX = (movePos.x - prevPos.x) / elapsed;
                airVelZ = (movePos.z - prevPos.z) / elapsed;
            }

            if (input.isDown(Action::Jump) && !isRolling && !isCrouching)
            {
                velY     = JUMP_IMPULSE;
                onGround = false;
            }
        }
        else
        {
            cam.setPosition({ prevPos.x + airVelX * elapsed,
                               prevPos.y,
                               prevPos.z + airVelZ * elapsed });
        }

        velY += GRAVITY * elapsed;
        const glm::vec3 target = { cam.getPosition().x,
                                    cam.getPosition().y + velY * elapsed,
                                    cam.getPosition().z };
        const glm::vec3 final_ = physics::slideMoveWithStepUp(prevPos, target, PHALF, world, STEP_HEIGHT);

        if (velY < 0.0f && final_.y > target.y + 0.001f)
        {
            velY     = 0.0f;
            onGround = true;
        }
        cam.setPosition(final_);
        tpsPlayerPos = final_;
    }
}

// ── Salute ────────────────────────────────────────────────────────────────────

bool PlayerController::updateHealth(World& world, Audio& audio)
{
    if (isDead || !world.isValidEntity(entity)) return false;

    const auto* hp = world.getHealth(entity);
    if (!hp) return false;

    if (hp->current <= 0.0f && prevHp > 0.0f)
    {
        isDead = true;
        prevHp = hp->current;
        return true;
    }

    if (hp->current < prevHp - 0.5f)
        audio.playHit();

    prevHp = hp->current;
    return false;
}

// ── Sparo ─────────────────────────────────────────────────────────────────────

bool PlayerController::updateShooting(World& world, Camera& cam, const InputManager& input,
                                       Audio& audio, Mesh* bulletMesh, bool mouseCaptured)
{
    if (isDead || !mouseCaptured) return false;
    if (!input.isDown(Action::Shoot)) return false;
    if (isRolling) return false;      // non si spara durante il roll
    if (!weapon.tryFire()) return false;

    audio.playShoot();

    glm::vec3 org, fwd;
    if (thirdPerson)
    {
        const float y = tpsYaw * DEG2RAD;
        org = { tpsPlayerPos.x, tpsPlayerPos.y + 1.0f, tpsPlayerPos.z };
        fwd = { std::sin(y), 0.0f, -std::cos(y) };
    }
    else
    {
        org = cam.getPosition();
        fwd = cam.getForward();
    }

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

// ── Respawn ───────────────────────────────────────────────────────────────────

bool PlayerController::updateRespawn(World& world, Camera& cam,
                                      float respawnDelay, const glm::vec3& spawnPos, float maxHp)
{
    respawnTimer = -1.0f;
    isDead       = false;
    isCrouching  = false;
    isRolling    = false;
    isAiming     = false;
    isSprinting  = false;

    entity = world.createEntity();
    world.addTransform(entity, {spawnPos.x, spawnPos.y, spawnPos.z});
    world.addTeam(entity, {1});
    world.addHealth(entity, {maxHp, maxHp});
    prevHp       = maxHp;
    velY         = 0.0f;
    onGround     = true;
    tpsPlayerPos = spawnPos;

    cam.setFov(NORMAL_FOV);
    if (thirdPerson)
        applyTpsCamera(cam, tpsPlayerPos, tpsYaw, tpsPitch, tpsDistance, tpsHeight);
    else
        cam.setPosition(spawnPos);

    std::cout << "[Respawn] Giocatore rinato!\n";
    return true;
}

} // namespace mini

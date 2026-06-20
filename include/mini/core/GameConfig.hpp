#pragma once

#include <glm/glm.hpp>

namespace mini::config
{

// ── Fisica ────────────────────────────────────────────────────────────────
constexpr float GRAVITY       = -14.0f;   // m/s²
constexpr float STEP_HEIGHT   =   0.55f;  // altezza max scalino percorribile
constexpr float JUMP_IMPULSE  =   6.0f;   // velocità iniziale salto (m/s)

// ── Player ────────────────────────────────────────────────────────────────
constexpr float PLAYER_SPEED  =   5.0f;   // m/s (camminata)
constexpr float PLAYER_HALF_X =   0.35f;
constexpr float PLAYER_HALF_Y =   0.85f;  // altezza occhi da y=0
constexpr float PLAYER_HALF_Z =   0.35f;

constexpr float PLAYER_BULLET_SPEED  = 18.0f;
constexpr float PLAYER_BULLET_DAMAGE = 25.0f;
constexpr float PLAYER_BULLET_LIFE   =  3.0f;

// ── AI ────────────────────────────────────────────────────────────────────
constexpr float AI_HALF_X     = 0.40f;
constexpr float AI_HALF_Y     = 0.50f;
constexpr float AI_HALF_Z     = 0.40f;
constexpr float AI_GRAVITY    = -14.0f;
constexpr float AI_STUCK_TIME =   1.2f;   // secondi prima di anti-stuck

// ── Combat ────────────────────────────────────────────────────────────────
constexpr float HIT_RADIUS    = 0.7f;     // raggio collisione proiettile

// ── Camera ────────────────────────────────────────────────────────────────
constexpr float CAMERA_FOV    = 60.0f;    // gradi
constexpr float CAMERA_NEAR   =  0.1f;
constexpr float CAMERA_FAR    = 100.0f;

// Helper per avere i glm::vec3 a runtime (non constexpr in GLM pre-1.0)
inline glm::vec3 playerHalf() { return {PLAYER_HALF_X, PLAYER_HALF_Y, PLAYER_HALF_Z}; }
inline glm::vec3 aiHalf()     { return {AI_HALF_X, AI_HALF_Y, AI_HALF_Z}; }

} // namespace mini::config
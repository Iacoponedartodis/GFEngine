#include "mini/render/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace mini
{

Camera::Camera(float fovDeg, float aspect, float nearPlane, float farPlane)
    : m_fov(fovDeg)
    , m_aspect(aspect)
    , m_near(nearPlane)
    , m_far(farPlane)
{
    updateFront();
}

// ============================================================
// Setup
// ============================================================

void Camera::setPosition(const glm::vec3& pos)
{
    m_position = pos;
}

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up)
{
    m_up = up;

    // Converte la direzione target->posizione in yaw/pitch
    // cosi' processKeyboard e processMouse possono continuare da li'
    const glm::vec3 dir = glm::normalize(target - m_position);
    m_pitch = glm::degrees(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)));
    m_yaw   = glm::degrees(std::atan2(dir.z, dir.x));
    updateFront();
}

void Camera::setAspect(float aspect)
{
    m_aspect = aspect;
}

void Camera::setSpeed(float speed)
{
    m_speed = speed;
}

// ============================================================
// Input FPS
// ============================================================

void Camera::processKeyboard(bool fwd, bool bwd, bool lft, bool rgt,
                              bool moveUp, bool moveDown, float dt)
{
    const float      dist  = m_speed * dt;
    const glm::vec3  right = glm::normalize(glm::cross(m_front, m_up));

    if (fwd)      m_position += m_front * dist;
    if (bwd)      m_position -= m_front * dist;
    if (lft)      m_position -= right   * dist;
    if (rgt)      m_position += right   * dist;
    if (moveUp)   m_position += m_up    * dist;
    if (moveDown) m_position -= m_up    * dist;
}

void Camera::processMouse(float dx, float dy, float sensitivity)
{
    m_yaw   += dx * sensitivity;
    m_pitch -= dy * sensitivity;   // invertito: mouse su = sguardo su

    // Clamp per evitare il gimbal lock
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    updateFront();
}

// ============================================================
// Matrici
// ============================================================

glm::mat4 Camera::getView() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjection() const
{
    return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}

glm::mat4 Camera::getViewProjection() const
{
    return getProjection() * getView();
}

const glm::vec3& Camera::getPosition() const { return m_position; }
float            Camera::getFov()       const { return m_fov; }

// ============================================================
// Privato
// ============================================================

void Camera::updateFront()
{
    const float yawRad   = glm::radians(m_yaw);
    const float pitchRad = glm::radians(m_pitch);

    m_front = glm::normalize(glm::vec3{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    });
}

} // namespace mini